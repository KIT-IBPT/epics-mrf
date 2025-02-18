MRF Linux kernel module
=======================

The Linux kernel modue is needed to control EVG and EVR device that are directly
installed in the computer running the IOC. In this case, the devices are
typically controlled over a PCI or PCI Express bus.

This kernel module has originally been developed by Jukka Pietarinen
(Microresearch Finland) and has since then be modified by Sebastian Marsching
(aquenos GmbH) to support udev (so that device nodes are created automatically),
to make it compatible with recent versions of the Linux kernel, and to support
DKMS for automatically building the module on systems that are based on Debian.

The kernel module is distributed under the terms of the GNU General Public
License version 2.

The original source tree can be found on [GitHub](
https://github.com/jpietari/mrf-linux-driver/).


Building on Debian (or derivatives)
-----------------------------------

On Debian, you can simply run `dpkg-buildpackage`. You might have to install a
few dependencies, but `dpkg-buildpackage` will tell you which ones. After
running that tool, you will get a Debian package (`.deb` file) that you can
install with `dpkg`.

This Debian package uses DKMS to automatically rebuilt and install the module
each time a new Linux kernel is installed. It also includes configuration files
for udev, so that device nodes for EVGs and EVRs are created automatically.


Building on other systems
-------------------------

On other systems, you should be able to build the module using the supplied
`Makefile`, but you have to make sure that the required kernel headers and other
dependencies are installed. You will also have to install the udev configuration
file (`60-mrf-pci.rules`) manually.
