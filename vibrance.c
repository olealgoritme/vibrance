// vibrance: NVIDIA Digital Vibrance on Linux, under X11 and Wayland alike. It is the same
// setting as "Digital vibrance" in the Windows NVIDIA Control Panel, applied by the display
// hardware: no shader, no extra frame of latency. It talks to the driver's modeset interface
// (/dev/nvidia-modeset) with the API headers of the loaded driver version (headers.sh).

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "nvkms-api.h"
#include "nvkms-ioctl.h"

#define VMIN (-1024)
#define VMAX 1023

static int fd = -1;

static int nvkms(NvU32 cmd, void *p, NvU32 size)
{
	struct NvKmsIoctlParams io = { .cmd = cmd, .size = size, .address = (NvU64)(NvUPtr)p };
	int r;
	do
		r = ioctl(fd, NVKMS_IOCTL_IOWR, &io);
	while (r < 0 && errno == EINTR);
	return r;
}

struct dpy {
	NvKmsDeviceHandle dev;
	NvKmsDispHandle disp;
	NVDpyId id;
	char name[NVKMS_DPY_NAME_SIZE];
};

static struct dpy dpys[64];
static int ndpys;
static NvKmsDeviceHandle devs[NV_MAX_DEVICES];
static int ndevs;

static int driver_version(char *v, size_t n)
{
	FILE *f = fopen("/sys/module/nvidia_modeset/version", "r");
	if (!f)
		f = fopen("/sys/module/nvidia/version", "r");
	if (!f || !fgets(v, n, f)) {
		if (f)
			fclose(f);
		return -1;
	}
	fclose(f);
	v[strcspn(v, "\n")] = 0;
	return 0;
}

// Every connected display of every NVIDIA GPU.
static int scan(void)
{
	char ver[64];
	if (driver_version(ver, sizeof ver) < 0) {
		fprintf(stderr, "vibrance: no NVIDIA driver loaded\n");
		return -1;
	}
	if (strcmp(ver, NV_VERSION) != 0) {
		fprintf(stderr, "vibrance: built for driver %s, loaded is %s: run `make` again\n", NV_VERSION, ver);
		return -1;
	}
	fd = open("/dev/nvidia-modeset", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("vibrance: /dev/nvidia-modeset");
		return -1;
	}
	for (NvU32 gpu = 0; gpu < NV_MAX_DEVICES; gpu++) {
		struct NvKmsAllocDeviceParams *a = calloc(1, sizeof *a);
		if (!a)
			return -1;
		snprintf(a->request.versionString, sizeof a->request.versionString, "%s", NV_VERSION);
#ifdef NVKMS_DEVICE_ID_STRUCT
		a->request.deviceId.rmDeviceId = gpu;
#else
		a->request.deviceId = gpu; // before 575 the id is a plain number
#endif
		a->request.no3d = NV_TRUE;
		if (nvkms(NVKMS_IOCTL_ALLOC_DEVICE, a, sizeof *a) < 0 || a->reply.status != NVKMS_ALLOC_DEVICE_STATUS_SUCCESS) {
			free(a);
			if (gpu == 0) {
				fprintf(stderr, "vibrance: the driver refused the device (%s)\n", strerror(errno));
				return -1;
			}
			break;
		}
		devs[ndevs++] = a->reply.deviceHandle;
		for (NvU32 d = 0; d < a->reply.numDisps; d++) {
			struct NvKmsQueryDispParams q = { 0 };
			q.request.deviceHandle = a->reply.deviceHandle;
			q.request.dispHandle = a->reply.dispHandles[d];
			if (nvkms(NVKMS_IOCTL_QUERY_DISP, &q, sizeof q) < 0)
				continue;
			// validDpys has every display the GPU can drive, MST ones behind a hub included.
			NVDpyId id;
			FOR_ALL_DPY_IDS(id, q.reply.validDpys) {
				if (ndpys == 64)
					break;
				struct NvKmsQueryDpyDynamicDataParams *dd = calloc(1, sizeof *dd);
				if (!dd)
					return -1;
				dd->request.deviceHandle = q.request.deviceHandle;
				dd->request.dispHandle = q.request.dispHandle;
				dd->request.dpyId = id;
				if (nvkms(NVKMS_IOCTL_QUERY_DPY_DYNAMIC_DATA, dd, sizeof *dd) == 0 && dd->reply.connected) {
					struct dpy *p = &dpys[ndpys++];
					p->dev = q.request.deviceHandle;
					p->disp = q.request.dispHandle;
					p->id = id;
					snprintf(p->name, sizeof p->name, "%s", dd->reply.name);
				}
				free(dd);
			}
		}
		free(a);
	}
	return 0;
}

static void done(void)
{
	for (int i = 0; i < ndevs; i++) {
		struct NvKmsFreeDeviceParams f = { 0 };
		f.request.deviceHandle = devs[i];
		nvkms(NVKMS_IOCTL_FREE_DEVICE, &f, sizeof f);
	}
	if (fd >= 0)
		close(fd);
}

static int get(const struct dpy *p, NvS64 *v)
{
	struct NvKmsGetDpyAttributeParams g = { 0 };
	g.request.deviceHandle = p->dev;
	g.request.dispHandle = p->disp;
	g.request.dpyId = p->id;
	g.request.attribute = NV_KMS_DPY_ATTRIBUTE_DIGITAL_VIBRANCE;
	if (nvkms(NVKMS_IOCTL_GET_DPY_ATTRIBUTE, &g, sizeof g) < 0)
		return -1;
	*v = g.reply.value;
	return 0;
}

static int set(const struct dpy *p, NvS64 v)
{
	struct NvKmsSetDpyAttributeParams s = { 0 };
	s.request.deviceHandle = p->dev;
	s.request.dispHandle = p->disp;
	s.request.dpyId = p->id;
	s.request.attribute = NV_KMS_DPY_ATTRIBUTE_DIGITAL_VIBRANCE;
	s.request.value = v;
	return nvkms(NVKMS_IOCTL_SET_DPY_ATTRIBUTE, &s, sizeof s);
}

// The Windows slider: 50 % is the default (0), 100 % the maximum, 0 % grey.
static int to_pct(NvS64 v)
{
	return (int)(v >= 0 ? 50 + (v * 50 + VMAX / 2) / VMAX : 50 + (v * 50 - 512) / 1024);
}

static int parse(const char *s, NvS64 *v)
{
	char *end;
	long n = strtol(s, &end, 10);
	if (end == s)
		return -1;
	if (*end == '%' && !end[1]) {
		if (n < 0 || n > 100)
			return -1;
		*v = n >= 50 ? (n - 50) * VMAX / 50 : (n - 50) * 1024 / 50;
		return 0;
	}
	if (*end || n < VMIN || n > VMAX)
		return -1;
	*v = n;
	return 0;
}

static void usage(void)
{
	fputs("usage: vibrance                    show every display's vibrance\n"
	      "       vibrance <value> [display]  set it: -1024..1023 (0 = default) or a Windows\n"
	      "                                   percentage (50% = default, 100% = maximum)\n"
	      "       vibrance reset [display]    back to the default\n"
	      "Without a display name every connected display changes.\n",
	      stderr);
}

int main(int argc, char **argv)
{
	NvS64 want = 0;
	const char *only = NULL;
	if (argc > 3 || (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
		usage();
		return 2;
	}
	if (argc > 1 && strcmp(argv[1], "reset") != 0 && parse(argv[1], &want) < 0) {
		usage();
		return 2;
	}
	if (argc > 2)
		only = argv[2];
	if (scan() < 0) {
		done();
		return 1;
	}
	int rc = 0, hit = 0;
	for (int i = 0; i < ndpys; i++) {
		struct dpy *p = &dpys[i];
		if (only && !strstr(p->name, only))
			continue;
		hit++;
		if (argc > 1 && set(p, want) < 0) {
			fprintf(stderr, "vibrance: %s: %s\n", p->name, strerror(errno));
			rc = 1;
			continue;
		}
		NvS64 v;
		if (get(p, &v) < 0)
			printf("%s  ?\n", p->name);
		else
			printf("%s  %lld  (%d%%)\n", p->name, (long long)v, to_pct(v));
	}
	if (only && !hit) {
		fprintf(stderr, "vibrance: no connected display %s\n", only);
		rc = 1;
	}
	done();
	return rc;
}
