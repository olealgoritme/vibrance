# Builds against the API headers of the loaded driver (or VERSION=...); headers.sh fetches
# them once per version.
VERSION ?= $(shell cat /sys/module/nvidia/version)
CFLAGS ?= -O2 -Wall -Wextra
# Since 575 the GPU id in NvKmsAllocDeviceRequest is a struct.
IDSTRUCT = $(if $(shell grep -s 'struct NvKmsDeviceId {' nvkms/$(VERSION)/nvkms-api.h),-DNVKMS_DEVICE_ID_STRUCT)
PREFIX ?= /usr/local

vibrance: vibrance.c nvkms/$(VERSION)/nvkms-api.h
	$(CC) $(CFLAGS) -std=gnu11 -Invkms/$(VERSION) -DNV_VERSION='"$(VERSION)"' $(IDSTRUCT) -o $@ vibrance.c

nvkms/$(VERSION)/nvkms-api.h:
	./headers.sh $(VERSION)

install: vibrance
	install -m 755 vibrance $(PREFIX)/bin/vibrance

clean:
	rm -f vibrance

.PHONY: install clean
