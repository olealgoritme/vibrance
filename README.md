# vibrance

NVIDIA Digital Vibrance on Linux, under X11 and Wayland. It is the "Digital vibrance" slider
from the Windows NVIDIA Control Panel: the display hardware applies it, so there is no shader
and no added latency.

Works with NVIDIA driver 515 or newer (open or proprietary kernel modules) on any desktop or
compositor.

## Build

Needs a C compiler, make and curl.

```
make                 # fetches the driver's API headers once
sudo make install
```

The tool is built for the loaded driver version. After a driver update, run `make` again.

## Use

```
vibrance             show every display
vibrance 80%         like the Windows slider at 80 % (50 % is the default)
vibrance 614 DP-4    raw value -1024..1023 on one display
vibrance reset       back to the default
```

The value stays set after vibrance exits. To have it
while one game runs, put this in its Steam launch options (614 is the Windows 80 %):

```
sh -c 'vibrance 614; "$@"; vibrance reset' sh %command%
```

## How it works

It sets the driver's digital vibrance display attribute through `/dev/nvidia-modeset`, with
the API headers from NVIDIA's
[open-gpu-kernel-modules](https://github.com/NVIDIA/open-gpu-kernel-modules) for the loaded
driver version.

## Credits

Made by Ole Algoritme. MIT license.
