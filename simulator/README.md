Simulator for UDP/IP MRF devices
================================

This directory contains scripts that can help with simulating the UDP/IP
interface of MRF timing devices.  They are mainly intended for development use,
allowing some tests do be done without actually needing such a device on the
network.

Please note that the simulation code does not implement any of the devices’
actual behavior. It simply starts with all memory initialized to zero and
accepts write requests, updating the memory at the respective address and
returning the new value for subsequent read requests.

The simulator can be used in two ways: interactively through the `asyncio` REPL
and non-interactively through `simulator.py`.

## Interactive use

In order to use the simulator interactively, start the `asyncio` REPL in this
directory by running `python3 -m asyncio`. In the REPL, execute the following
code in order to start the simulator:

```py
import mrfsim
sim = mrfsim.vme_evr_300_simulator()
await sim.start_server()
```

The server is now available on 127.0.0.1, UDP port 2000. The memory can be
accessed through the network protocol and through the REPL. For example, the
value at the address of the firmware register can be read and written like
this:

```py
sim.read_uint32(0x002c)  # Should return 0.
sim.write_uint32(0x002c, 0x12090207)
```

When the server is not needed any longer, it can be stopped like this:

```py
sim.stop_server()
```

In addition to the `vme_evr_300_simulator` function, there are functions that
create a simulator instance that is appropriate for the other devices
supporting the UDP/IP protocol.

## Non-interactive use

When only the simulated network interface is required, the `mrfsim` module can
be run directly. Run `python3 -m mrfsim --help` for information about the
available command-line options.
