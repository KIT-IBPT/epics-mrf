Limitations of the device support
=================================

This device support is currently limited to the following devices:

- MTCA-EVR-300
- VME-EVG-230
- VME-EVR-230RF

However, it can easily be [extended](extending.md) to support additional
devices.

For the VME-based devices, only control over the Ethernet interface (via UDP/IP)
is currently supported. When using this protocol, the device support cannot
detect when a device may have been reset (e.g. switched off and on), so it may
present settings that actually are not active in the hardware any longer.

On a more general note, there are a few settings for which it is not possible to
read the current value from the hardware (in particular the fine delay set for
an output module that supports such a delay). This means that when starting the
IOC, the record's value will not represent the actual value used by the
hardware.
