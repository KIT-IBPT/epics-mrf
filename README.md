EPICS device support for MRF EVR and EVG
========================================

This EPICS device support is intended for use with the event generator (EVG) and
event receiver (EVR) devices that are part of the timing system from
[Microresearch Finland (MRF)](http://www.mrf.fi/).

At the moment, the following devices are directly supported:

- MTCA-EVR-300
- VME-EVG-230
- VME-EVR-230
- VME-EVR-230RF

This device support implements an abstraction layer, that allows it to control
devices either via PCI(e) (using the MRF linux kernel driver) or via Ethernet,
using a UDP/IP based protocol. The second option is only available for those
devices that actually support being controlled via Ethernet (these are all the
VME based devices).

This device support only works with devices using the “new” (modular register
map) firmware, not with devices using the legacy firmware.

Ready-to-use panels for Control System Studio for the aforementioned devices are
supplied with this device support.


Documentation
-------------

- [Installing / compiling the device support](docs/installing.md)
- [Using the device support](docs/using.md)
- [Process variables for the EVG and EVR](docs/process_variables.md)
- [Linux kernel module](support/mrf-pci-dkms-0.1.20250218/README.md)
- [Extending the device support](docs/extending.md)
- [Current limitations of the device support](docs/limitations.md)


Copyright / License
-------------------

This EPICS device support is licensed under the terms of the
[GNU Lesser General Public License version 3](LICENSE-LGPL.md). It has been
developed by [aquenos GmbH](https://www.aquenos.com/) on behalf of the
[Karlsruhe Institute of Technology's Institute of Beam Physics and Technology](https://www.ibpt.kit.edu/).

This device support contains code originally developed for the s7nodave device
support. aquenos GmbH has given special permission to relicense this parts of
s7nodave device support under the terms of the GNU LGPL version 3.

The kernel module for the MRF devices (that is distributed in the `support`
sub-directory of this device support) is not covered by this license. This
kernel module is licensed under the terms of the GNU General Public License
version 2. The kernel module has been originally developed by Micro-Research
Finland and has been modified by Sebastian Marsching to make it work with more
recent Linux kernels. Please refer to the `README.md` file in the kernel
module's source tree for details.
