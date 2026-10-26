#!/bin/sh
# Fetch the NVKMS API headers of one driver version (default: the loaded one)
# from NVIDIA's open-gpu-kernel-modules into nvkms/<version>/.
set -e
v=${1:-$(cat /sys/module/nvidia/version)}
d=nvkms/$v
mkdir -p "$d"
for f in \
	src/nvidia-modeset/interface/nvkms-api.h \
	src/nvidia-modeset/interface/nvkms-api-types.h \
	src/nvidia-modeset/interface/nvkms-format.h \
	src/nvidia-modeset/interface/nvkms-ioctl.h \
	src/common/inc/nv_mig_types.h \
	src/common/unix/common/inc/nv_dpy_id.h \
	src/common/unix/common/inc/nv_mode_timings.h \
	src/common/unix/common/inc/nv_common_utils.h \
	src/common/sdk/nvidia/inc/nvgputypes.h \
	src/common/sdk/nvidia/inc/nvmisc.h \
	src/common/sdk/nvidia/inc/nvlimits.h \
	src/common/sdk/nvidia/inc/nvtypes.h \
	src/common/sdk/nvidia/inc/cpuopsys.h \
	src/common/sdk/nvidia/inc/nvstatus.h \
	src/common/sdk/nvidia/inc/nvstatuscodes.h
do
	[ -s "$d/${f##*/}" ] && continue
	# Not every version has every file (nv_mig_types.h is new); the compiler names any real gap.
	curl -fsSL "https://raw.githubusercontent.com/NVIDIA/open-gpu-kernel-modules/$v/$f" -o "$d/${f##*/}" 2>/dev/null ||
		rm -f "$d/${f##*/}"
done
echo "$d"
