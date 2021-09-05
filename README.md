# DWM status
![Picture](dwm-status.png "DWM Status Bar")<br>
A custom status utility for the dynamic window manager.

## Dependencies
* dwm
* libX11
* NVIDIA Management Library (optional)
* curl (optional)

## Configuration
Configuring some parameters is supported through the classic config.h.


## Installation
Build dwm-status with ```make```.
Installation is also simple, just type ```sudo make install```.

### NVIDIA GPU support
If you want NVIDIA GPU support you have to build with ```make NVIDIA_GPU=1``` instead
and enable ```ENABLE_NVIDIA``` in the config.h.
This feature is currently only supported with the proprietary NVIDIA driver.<br>
It may be required to change the -I and -L options in CFLAGS and LIBS respectively to the corresponding directory where CUDA is installed on your system.
