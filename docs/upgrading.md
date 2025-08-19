Upgrading from version 1.x to 2.x
=================================

The MRF device support introduced a few breaking changes in version 2.x. This
means that the configuration of existing IOCs needs to be updated when
upgrading to version 2.x.

New signature for IOC shell functions for UDP/IP devices
--------------------------------------------------------

The signature of the IOC shell functions that are used to configure devices
that are controleld via UDP/IP have changed. This change affects the following
functions:

- `mrfUdpIpEvgDevice`
- `mrfUdpIpEvrDevice`

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
