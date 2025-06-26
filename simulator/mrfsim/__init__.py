"""
Simulate an MRF UDP/IP device.
"""

import argparse
import asyncio
import collections.abc
import dataclasses
import logging
import struct
import typing


# Address offset used by the VME-EVG-230.
ADDRESS_OFFSET_VME_EVG_230 = 0x80000000

# Address offset used by the VME-EVM-300.
ADDRESS_OFFSET_VME_EVM_300 = 0x80000000

# Address offset used by the VME-EVR-230.
ADDRESS_OFFSET_VME_EVR_230 = 0x7A000000

# Address offset used by the VME-EVR-300.
ADDRESS_OFFSET_VME_EVR_300 = 0x80000000

# Memory size for an EVG.
MEMORY_SIZE_EVG = 0x10000

# Memory size for an EVM.
MEMORY_SIZE_EVM = 0x40000

# Memory size for an EVR.
MEMORY_SIZE_EVR = 0x38000

# Structure representing a UDP packet of the MRF protocol.
_mrf_packet_struct = struct.Struct("!BbHII")

# Logger for this module.
logger = logging.getLogger(__name__)


@dataclasses.dataclass
class _MrfPacket:

    access_type: int
    address: int
    data: int
    ref: int
    status: int

    @staticmethod
    def decode(  # pylint: disable=missing-function-docstring
        packet_data: collections.abc.Buffer,
    ) -> "_MrfPacket":
        if len(memoryview(packet_data)) != _mrf_packet_struct.size:
            raise ValueError(
                f"Packet size is {len(memoryview(packet_data))} bytes, not "
                "the expected {self._protocol_struct.size} bytes.",
            )
        access_type, status, data, address, ref = _mrf_packet_struct.unpack(
            packet_data
        )
        return _MrfPacket(access_type, address, data, ref, status)

    def encode(self) -> bytes:  # pylint: disable=missing-function-docstring
        return _mrf_packet_struct.pack(
            self.access_type, self.status, self.data, self.address, self.ref
        )


class _MrfProtocol(asyncio.DatagramProtocol):

    def __init__(self, address_offset: int, simulator: "DeviceSimulator"):
        self._address_offset = address_offset
        self._simulator = simulator
        self._transport: asyncio.DatagramTransport | None = None

    def connection_lost(self, exc: Exception | None) -> None:
        self._simulator._stopped()

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        assert isinstance(transport, asyncio.DatagramTransport)
        self._transport = transport

    def datagram_received(
        self, data: bytes, addr: tuple[str | typing.Any, int]
    ) -> None:
        try:
            packet = _MrfPacket.decode(data)
        except ValueError as err:
            logger.error(
                "Error in packet received from %s:%d: %s",
                addr[0],
                addr[1],
                err,
            )
            return
        # Apply the address offset.
        address = packet.address - self._address_offset
        # Process the packet depending on the access type.
        if packet.access_type == 1:
            # Read.
            try:
                packet.data = self._simulator.read_uint16(address)
                packet.status = 0
            except IndexError:
                packet.status = -1
        elif packet.access_type == 2:
            # Write.
            try:
                self._simulator.write_uint16(address, packet.data)
                packet.status = 0
            except (IndexError, OverflowError):
                packet.status = -1
        # Send the response.
        assert self._transport is not None
        self._transport.sendto(packet.encode(), addr)


class DeviceSimulator:
    """
    Provides a UDP/IP server simulating an MRF timing device.

    Args:
        memory_size: size of the simulated memory (in bytes). Requests to
            access memory addresses that reference memory beyond
            the specified size fail.
        network_address_offset: offset at which the address space of the
            simulated device is exposed over the network. This offset is
            substracted from the addresses that are specified when accessing
            the simulated device over the network.
    """

    def __init__(
        self,
        memory_size: int,
        network_address_offset: int,
    ):
        self._memory = bytearray(memory_size)
        self._network_address_offset = network_address_offset
        self._server_stopped_event: asyncio.Event | None = None
        self._transport: asyncio.DatagramTransport | None = None

    def _stopped(self) -> None:
        if self._server_stopped_event is None:
            return
        self._server_stopped_event.set()
        self._server_stopped_event = None

    def _validate_address(self, address, length):
        if address < 0:
            raise IndexError("Address is negative.")
        if address + length > len(self._memory):
            raise IndexError("Address is too large.")

    def read_uint16(self, address: int) -> int:
        """
        Read a 16 bit unsigned integer.

        Args:
            address: address of the 16-bit word that shall be read.

        Returns:
            value that is stored at the specified address.

        Raises:
            IndexError: if address is less than zero or if the specified word
                extends beyond the memory limit.
        """
        self._validate_address(address, 2)
        return int.from_bytes(self._memory[address : address + 2])

    def read_uint32(self, address: int) -> int:
        """
        Read a 32 bit unsigned integer.

        Args:
            address: address of the 32-bit word that shall be read.

        Returns:
            value that is stored at the specified address.

        Raises:
            IndexError: if address is less than zero or if the specified word
                extends beyond the memory limit.
        """
        self._validate_address(address, 4)
        return int.from_bytes(self._memory[address : address + 4])

    async def start_server(
        self,
        *,
        bind_address: str = "127.0.0.1",
        bind_port: int = 2000,
    ) -> None:
        """
        Start a UDP/IP server for the simulated device.

        Args:
            bind_address: host address to which the socket is bound.
            bind_port: UDP port to which the socket is bound.

        Raises:
            RuntimeError: if the server has already been started and not
                stopped since or if there is no running event loop.
        """
        if self._server_stopped_event is not None:
            raise RuntimeError("The server has already been started.")

        event_loop = asyncio.get_running_loop()

        def create_protocol():
            return _MrfProtocol(self._network_address_offset, self)

        try:
            self._server_stopped_event = asyncio.Event()
            self._transport, _ = await event_loop.create_datagram_endpoint(
                create_protocol,
                (bind_address, bind_port),
                reuse_port=True,
            )
        except:
            self._stopped()
            raise

    async def stop_server(self) -> None:
        """
        Stop the server.

        If the server is not running, this is a no-op.
        """
        if self._transport is None:
            return
        self._transport.close()
        self._transport = None

    async def wait_for_server_stopped(self) -> None:
        """
        Wait until the server has stopped.
        """
        if self._server_stopped_event is None:
            return
        await self._server_stopped_event.wait()

    def write_uint16(self, address: int, value: int) -> None:
        """
        Write a 16 bit unsigned integer.

        Args:
            address: address of the 16-bit word that shall be written.
            value: value that shall be written.

        Raises:
            IndexError: if address is less than zero or if the specified word
                extends beyond the memory limit.
            OverflowError: if the specified ``value`` exceeds the range of an
                unsigned 16-bit integer.
        """
        self._validate_address(address, 2)
        self._memory[address : address + 2] = value.to_bytes(2)

    def write_uint32(self, address: int, value: int) -> None:
        """
        Write a 32 bit unsigned integer.

        Args:
            address: address of the 32-bit word that shall be written.
            value: value that shall be written.

        Raises:
            IndexError: if address is less than zero or if the specified word
                extends beyond the memory limit.
            OverflowError: if the specified ``value`` exceeds the range of an
                unsigned 32-bit integer.
        """
        self._validate_address(address, 4)
        self._memory[address : address + 4] = value.to_bytes(4)


async def main(args: collections.abc.Sequence[str] | None = None) -> None:
    """
    Process command-line arguments and run the server.

    This function never returns normally. Instead, it can only be stopped by
    cancelling the task, which typically happens due to a keyboard interrupt.

    Args:
      args: command-line arguments. If ``None``, ``sys.argv`` is used.
    """
    device_type_to_function = {
        "vme-evg-230": vme_evg_230_simulator,
        "vme-evm-300": vme_evm_300_simulator,
        "vme-evr-230": vme_evr_230_simulator,
        "vme-evr-300": vme_evm_300_simulator,
    }

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--bind-address",
        default="127.0.0.1",
        help="Address to which the server binds",
    )
    parser.add_argument(
        "--bind-port",
        default=2000,
        type=int,
        help="UDP port to which the server binds",
    )
    parser.add_argument("device_type", choices=device_type_to_function.keys())
    parsed_args = parser.parse_args(args)
    simulator = device_type_to_function[parsed_args.device_type]()
    await simulator.start_server(
        bind_address=parsed_args.bind_address, bind_port=parsed_args.bind_port
    )
    await simulator.wait_for_server_stopped()


def vme_evg_230_simulator() -> DeviceSimulator:
    """
    Return a simulator for the VME-EVG-230.

    Returns:
        instance that simulates a VME-EVG-230.
    """
    return DeviceSimulator(MEMORY_SIZE_EVG, ADDRESS_OFFSET_VME_EVG_230)


def vme_evm_300_simulator() -> DeviceSimulator:
    """
    Return a simulator for the VME-EVM-300.

    Returns:
        instance that simulates a VME-EVG-300.
    """
    return DeviceSimulator(MEMORY_SIZE_EVM, ADDRESS_OFFSET_VME_EVM_300)


def vme_evr_230_simulator() -> DeviceSimulator:
    """
    Return a simulator for the VME-EVR-230 or VME-EVR-230RF.

    Returns:
        instance that simulates a VME-EVR-230 or VME-EVR-230RF.
    """
    return DeviceSimulator(MEMORY_SIZE_EVR, ADDRESS_OFFSET_VME_EVR_230)


def vme_evr_300_simulator() -> DeviceSimulator:
    """
    Return a simulator for the VME-EVR-300.

    Returns:
        instance that simulates a VME-EVR-300.
    """
    return DeviceSimulator(MEMORY_SIZE_EVM, ADDRESS_OFFSET_VME_EVR_300)
