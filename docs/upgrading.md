Upgrading from version 1.x to 2.x
=================================

The MRF device support introduced a few breaking changes in version 2.x. This
means that the configuration of existing IOCs needs to be updated when
upgrading to version 2.x.

Renamed IOC shell functions
---------------------------------

Some of the IOC shell functions for initializing devices have been renamed. The
old names are still supported (though with different arguments, see below), but
will be removed in a future version. Therefore, you should change the function
names in IOC startup scripts now.

The following functions have been renamed:

- `mrfUdpIpEvgDevice` → `mrfUdpIpVmeEvg230Device`
- `mrfUdpIpEvrDevice` → `mrfUdpIpVmeEvr230Device` or
  `mrfUdpIpVmeEvr230RfDevice` (depending on the device type)

Please note that the number and meaning of the arguments has also changed
(refer to the next section for details).

New signature for IOC shell functions for UDP/IP devices
--------------------------------------------------------

The signature of the IOC shell functions that are used to configure devices
that are controleld via UDP/IP have changed. This change affects the following
functions:

- `mrfUdpIpEvgDevice` (deprecated, see preceding section)
- `mrfUdpIpEvrDevice` (deprecated, see preceding section)
- `mrfUdpIpVmeEvg230Device`
- `mrfUdpIpVmeEvm300Device`
- `mrfUdpIpVmeEvr230Device`
- `mrfUdpIpVmEvr230RfDevice`
- `mrfUdpIpVmEvr300Device`

The first two (mandatory) arguments have stayed the same, but the third (delay
between packets), fourth (UDP timeout), and fifths (max. number of retries)
arguments have been removed and two new arguments, which like the old arguments
are optional, have been added instead. The first of the two new arguments is
the queue timeout, the second one is the request timeout. Like with the old
optional arguments, it is rarely necessary (or advisable) to specify them.
Please refer to the [*Configuring the device support*](using.md) document for
more information about the new arguments.

This change is the consequence of a complete rewrite of the UDP/IP client code.
Thanks to these changes, communication should now be much more robust,
perfomant and reliable, and the need to specify optional parameters is greatly
reduced thanks to many key parameters (like the rate at which packets can be
sent to the device) now being determined automatically.
