#!/usr/bin/env python3

"""
Discover and configure the network interface of MRF devices that use the Ubicom
IP2022.

In particular, this utilitiy should work for the VME-EVG-230, VME-EVR-230, and
VME-EVR-230RF series devices.
"""

import abc
import argparse
import dataclasses
import enum
import socket
import struct
import sys
import time
import types
import typing

try:
    import netifaces as netifaces_module
except ImportError:
    netifaces_module = None

BROADCAST_ADDRESS = "255.255.255.255"
"""
IPv4 broadcast address.
"""

MESSAGE_INTERLUDE1 = b"\x01\x00\xc0\x01"
"""
First interlude that is part of every message.
"""

MESSAGE_INTERLUDE2 = b"\x00\x01\x00\x01"
"""
Second interlude that is part of every message.
"""

MESSAGE_STRUCT = struct.Struct(">H6sH6s4sB4sH")
"""
Structure for UDP messages.

- Bytes 0..1: recipient address type (``0x0001``: MAC address, ``0x0002``: IP
  address and port, ``0x0100``: broadcast)
- Bytes 2..7: recipient address (MAC address or IPv4 address and port)
- Bytes 8..9: sender address type (``0x0001: MAC address``, ``0x0002``: IP
  address and port)
- Bytes 10..15: sender address (MAC address or IPv4 address and port)
- Bytes 16..19: interlude 1 (always ``0x0100c001``)
- Byte 20: message type (``0x00``: response, ``0x01``: request)
- Bytes 21..24: interlude 2 (always ``0x00010001``)
- Bytes 25..26: command (``0x0001``: identify, ``0x0002``: get network config,
  ``0x0003``: set network config)

Additional bytes may follow depending on the command.
"""

UBICOM_UDP_PORT = 17784
"""
UDP port used for communication with the Ubicom IP2022.
"""


class Address(abc.ABC):
    """
    Abstract base class for addresses.
    """

    @property
    @abc.abstractmethod
    def address_type(self) -> "AddressType":
        """
        Adddress type.
        """

    @classmethod
    @abc.abstractmethod
    def decode(cls, data: bytes) -> typing.Self:
        """
        Decode an address from the on-wire representation.
        """

    @abc.abstractmethod
    def encode(self) -> bytes:
        """
        Return the on-wire representation of the address.
        """


class AddressType(enum.Enum):
    """
    Address types.
    """

    BROADCAST_ADDRESS = 0x0100
    """
    Broadcast address.
    """

    IPV4_ADDRESS_WITH_PORT = 0x0002
    """
    IPv4 address with port.
    """

    MAC_ADDRESS = 0x0001
    """
    MAC address.
    """


class Client:
    """
    Client for communicating with the Ubicom IP2022.

    :param local_address: local IP address used when sending packets.
    :param local_port: local port used when sending packets.
    :param remote_port: remote port used when sending packets.
    """

    _SET_NETWORK_CONFIG_REQUEST_PAYLOAD_STRUCT = struct.Struct(">4s4s4sB")

    def __init__(
        self,
        local_address: "IPv4Address | str",
        local_port: int = UBICOM_UDP_PORT,
        remote_port: int = UBICOM_UDP_PORT,
    ):
        if isinstance(local_address, IPv4Address):
            self._local_address = local_address
        else:
            self._local_address = IPv4Address(local_address)
        self._local_port = local_port
        self._remote_port = remote_port
        self._sender_address = IPv4AddressWithPort(
            self._local_address, self._local_port
        )
        recv_socket = None
        send_socket = None
        success = False
        try:
            recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            send_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            recv_socket.settimeout(1.0)
            send_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            send_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            send_socket.settimeout(1.0)
            recv_socket.bind(("", self._local_port))
            send_socket.bind((str(self._local_address), self._local_port))
            success = True
        finally:
            if not success:
                if recv_socket is not None:
                    recv_socket.close()
                if send_socket is not None:
                    send_socket.close()
        assert recv_socket is not None
        assert send_socket is not None
        self._recv_socket = recv_socket
        self._send_socket = send_socket

    def __enter__(self) -> typing.Self:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: types.TracebackType | None,
    ) -> bool | None:
        self.close()

    def _identify_request_message(self) -> "Message":
        return Message(
            recipient_address=BroadcastAddress(),
            sender_address=self._sender_address,
            message_type=MessageType.REQUEST,
            command=Command.IDENTIFY,
        )

    @classmethod
    def _parse_payload_fields(
        cls, payload: bytes
    ) -> dict["PayloadField", bytes]:
        # For most commands, the payload is a sequence of fields, where each
        # field starts with a byte that specified the field ID, followed by a
        # byte that is the field length in bytes, followed by the actual field
        # value.
        if not payload:
            return {}
        field_id = PayloadField(payload[0])
        try:
            field_len = payload[1]
            field_bytes = payload[2 : 2 + field_len]
        except IndexError as err:
            raise ValueError("Malformed payload") from err
        skip_bytes = 2 + field_len
        return {field_id: field_bytes} | cls._parse_payload_fields(
            payload[skip_bytes:]
        )

    def _receive_identify_response(self) -> "FoundDevice | None":
        message, remote_address = self._receive_response_message()
        if message is None:
            return None
        if message.command != Command.IDENTIFY:
            # Ignore packets that are not responses to the identify command.
            return None
        if not message.payload:
            # A valid response must have a payload.
            return None
        fields = self._parse_payload_fields(message.payload)
        try:
            device_id1 = fields[PayloadField.DEVICE_ID1].decode(
                "ascii", errors="ignore"
            )
            device_id2 = fields[PayloadField.DEVICE_ID2].decode(
                "ascii", errors="ignore"
            )
        except KeyError:
            # A valid response must contain the two ID fields.
            return None
        # The _receive_response_message method already verified that the sender
        # address is a MAC address.
        assert isinstance(message.sender_address, MACAddress)
        return FoundDevice(
            id1=device_id1,
            id2=device_id2,
            ip_address=IPv4Address(remote_address),
            mac_address=message.sender_address,
        )

    def _receive_get_network_config_response(
        self,
    ) -> tuple["NetworkConfiguration | None", "MACAddress | None"]:
        message, _ = self._receive_response_message()
        if message is None:
            return None, None
        # The _receive_response_message method already verified that the sender
        # address is a MAC address.
        assert isinstance(message.sender_address, MACAddress)
        if message.command != Command.GET_NETWORK_CONFIG:
            # Ignore packets that are not responses to the get-network-config
            # command.
            return None, message.sender_address
        if not message.payload:
            # A valid response must have a payload.
            return None, message.sender_address
        fields = self._parse_payload_fields(message.payload)
        try:
            dhcp_enabled_bytes = fields[PayloadField.NET_CONFIG_DHCP_ENABLED]
            gateway_bytes = fields[PayloadField.NET_CONFIG_STATIC_GATEWAY]
            ip_address_bytes = fields[
                PayloadField.NET_CONFIG_STATIC_IP_ADDRESS
            ]
            netmask_bytes = fields[PayloadField.NET_CONFIG_STATIC_NETMASK]
        except KeyError:
            # A valid response must contain the four configuration fields.
            return None, message.sender_address
        return (
            NetworkConfiguration(
                dhcp_enabled=bool(dhcp_enabled_bytes[0]),
                gateway=IPv4Address.decode(gateway_bytes),
                ip_address=IPv4Address.decode(ip_address_bytes),
                network_mask=IPv4Address.decode(netmask_bytes),
            ),
            message.sender_address,
        )

    def _receive_response_message(self) -> tuple["Message | None", str]:
        message_bytes, (remote_address, _) = self._recv_socket.recvfrom(2048)
        try:
            message = Message.decode(message_bytes)
        except ValueError:
            # Ignore malformed packets.
            return None, remote_address
        if message.recipient_address != self._sender_address:
            # Ignore packets that are not intended for this client.
            return None, remote_address
        if not isinstance(message.sender_address, MACAddress):
            # Ignore packets that do not specify a MAC address as the sender as
            # expected.
            return None, remote_address
        if message.message_type != MessageType.RESPONSE:
            # Ignore packets that do not represent a response.
            return None, remote_address
        return message, remote_address

    def _receive_set_network_config_response(
        self,
    ) -> "MACAddress | None":
        message, _ = self._receive_response_message()
        if message is None:
            return None
        if message.command != Command.SET_NETWORK_CONFIG:
            # Ignore packets that are not responses to the set-network-config
            # command.
            return None
        if message.payload:
            # A valid response must not have a payload.
            return None
        # The _receive_response_message method already verified that the sender
        # address is a MAC address.
        assert isinstance(message.sender_address, MACAddress)
        return message.sender_address

    def _send_get_network_config_request(
        self, device_mac_address: "MACAddress"
    ) -> None:
        message = Message(
            recipient_address=device_mac_address,
            sender_address=self._sender_address,
            message_type=MessageType.REQUEST,
            command=Command.GET_NETWORK_CONFIG,
        )
        self._send_socket.sendto(
            message.encode(), (BROADCAST_ADDRESS, self._remote_port)
        )

    def _send_set_network_config_request(
        self,
        device_mac_address: "MACAddress",
        network_configuration: "NetworkConfiguration",
    ) -> None:
        payload = self._SET_NETWORK_CONFIG_REQUEST_PAYLOAD_STRUCT.pack(
            network_configuration.ip_address.encode(),
            network_configuration.network_mask.encode(),
            network_configuration.gateway.encode(),
            1 if network_configuration.dhcp_enabled else 0,
        )
        message = Message(
            recipient_address=device_mac_address,
            sender_address=self._sender_address,
            message_type=MessageType.REQUEST,
            command=Command.SET_NETWORK_CONFIG,
            payload=payload,
        )
        self._send_socket.sendto(
            message.encode(), (BROADCAST_ADDRESS, self._remote_port)
        )

    def close(self) -> None:
        """
        Close the sockets associated with this client.
        """
        try:
            self._recv_socket.close()
        except OSError:
            pass
        try:
            self._send_socket.close()
        except OSError:
            pass

    def get_network_configuration(
        self, device_mac_address: "MACAddress"
    ) -> "NetworkConfiguration":
        """
        Retrieve the network configuration from a device.

        :param device_mac_address: MAC address of the device.
        :return: network configuration for the specified device.
        :raise socket.timeout: when the device does not reply in time.
        """
        self._send_get_network_config_request(device_mac_address)
        network_configuration = None
        while network_configuration is None:
            network_configuration, sender_mac_address = (
                self._receive_get_network_config_response()
            )
            if device_mac_address != sender_mac_address:
                network_configuration = None
        return network_configuration

    def scan(self) -> list["FoundDevice"]:
        """
        Scan the network for online devices.

        :return: list of found devices.
        """
        found_devices = {}
        identify_request_message = self._identify_request_message()
        original_recv_timeout = self._recv_socket.gettimeout()
        start_time = time.monotonic()
        try:
            self._recv_socket.settimeout(0.5)
            # We try to find devices for five seconds, sending the identify
            # request several times.
            while time.monotonic() - start_time < 5.0:
                self._send_socket.sendto(
                    identify_request_message.encode(),
                    (BROADCAST_ADDRESS, self._remote_port),
                )
                # Receive responses until there is a socket timeout, which
                # happens when there is no further response for more than
                # 0.5 seconds.
                try:
                    while True:
                        found_device = self._receive_identify_response()
                        if found_device is None:
                            continue
                        found_devices[found_device.mac_address] = found_device
                except TimeoutError:
                    pass
        finally:
            self._recv_socket.settimeout(original_recv_timeout)
        return list(found_devices.values())

    def set_network_configuration(
        self,
        device_mac_address: "MACAddress",
        network_configuration: "NetworkConfiguration",
    ) -> None:
        """
        Update the network configuration of a device.

        :param device_mac_address: MAC address of the device.
        :param network_configuration: configuration for the specified device.
        :raise socket.timeout: when the device does not reply in time.
        """
        self._send_set_network_config_request(
            device_mac_address, network_configuration
        )
        # Receive messages until we either get the expected response or no more
        # messages are received and there is a socket timeout.
        while True:
            sender_mac_address = self._receive_set_network_config_response()
            if device_mac_address == sender_mac_address:
                return


class Command(enum.Enum):
    """
    Commands.
    """

    GET_NETWORK_CONFIG = 0x0002
    """
    Get the current network configuration.
    """

    IDENTIFY = 0x0001
    """
    Identify devices.
    """

    SET_NETWORK_CONFIG = 0x0003
    """
    Set a new network configuration.
    """


class CommandLineInterface:  # pylint: disable=too-few-public-methods
    """
    Command-line interface for the client.

    :param args: command-line arguments.
    """

    def __init__(self, args: list[str]):
        self._client: Client | None = None
        self._command_map = {
            "get-network-config": self._get_network_config,
            "reset-network-config-dhcp": self._reset_network_config_dhcp,
            "scan": self._scan,
            "set-network-config-dhcp": self._set_network_config_dhcp,
            "set-network-config-static": self._set_network_config_static,
        }
        parsed_args = self._parse_args(args)
        self._command = self._command_map[parsed_args.command]
        self._command_args: list[str] = parsed_args.command_args
        self._interface: str = parsed_args.interface

    @classmethod
    def _determine_local_address(cls, interface: str | None) -> "IPv4Address":
        if interface is not None:
            try:
                return IPv4Address(interface)
            except ValueError:
                if netifaces_module is None:
                    print(
                        "The netifaces module is not available, so the "
                        "--interface option must specify a local IP address, "
                        "not an interface name.",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                try:
                    candidate_interfaces = {
                        interface: cls._suitable_addresses_for_interface(
                            interface
                        )
                    }
                except ValueError as err:
                    print(f"Error: {err}", file=sys.stderr)
                    sys.exit(1)
        elif netifaces_module is None:
            print(
                "The netifaces module is not available, so the --interface "
                "option must be specified.",
                file=sys.stderr,
            )
            sys.exit(1)
        else:
            candidate_interfaces = {}
            for interface_name in netifaces_module.interfaces():
                suitable_addresses = cls._suitable_addresses_for_interface(
                    interface_name
                )
                if suitable_addresses:
                    candidate_interfaces[interface_name] = suitable_addresses
        if len(candidate_interfaces) == 0:
            print("No suitable network interfaces found.", file=sys.stderr)
            sys.exit(1)
        if len(candidate_interfaces) > 1:
            print(
                "There is more than one suitable network interfaces. "
                "Please specify which interface should be used through "
                "the --interface argument.",
                file=sys.stderr,
            )
            sys.exit(1)
        address_info = next(iter(candidate_interfaces.values()))[0]
        return IPv4Address(address_info["addr"])

    def _get_network_config(self) -> None:
        assert self._client is not None
        device_mac_address = MACAddress(self._command_args[0])
        network_config = self._client.get_network_configuration(
            device_mac_address
        )
        print(
            f"Network configuration for {device_mac_address}:\n"
            "\n"
            f"DHCP enabled: {"yes" if network_config.dhcp_enabled else "no"}\n"
            f"Static IP address: {network_config.ip_address}\n"
            f"Static network mask: {network_config.network_mask}\n"
            f"Static gateway: {network_config.gateway}"
        )

    @staticmethod
    def _parse_args(raw_args: list[str]) -> argparse.Namespace:
        parser = argparse.ArgumentParser(
            description=(
                "Find IP2022-based MRF devices on the network and change "
                "their network configuration."
            )
        )
        parser.add_argument(
            "--interface", help="local network interface name or IP address"
        )
        parser.add_argument(
            "command",
            nargs="?",
            default="scan",
            help=(
                "operation to be performed. Can be “get-network-config”, "
                "“reset-network-config-dhcp”, “scan” (the default), "
                "“set-network-config-dhcp”, or “set-network-config-static”."
            ),
        )
        parser.add_argument(
            "command_args",
            nargs="*",
            help=(
                "extra arguments for specific commands. The "
                "“get-network-config”, “reset-network-config-dhcp”, and "
                "“set-network-config-dhcp” commands take a single argument, "
                "which is the device MAC address. The "
                "”set-network-config-static” command takes four arguments, "
                "which are the device MAC address, the new IP address, the "
                "new network mask and the new gateway address."
            ),
        )
        args = parser.parse_args(raw_args)
        if args.command not in (
            "get-network-config",
            "reset-network-config-dhcp",
            "scan",
            "set-network-config-dhcp",
            "set-network-config-static",
        ):
            print(
                f"Error: Unsupported command: {args.command}", file=sys.stderr
            )
            sys.exit(1)
        if args.command == "scan" and args.command_args:
            print(
                "Error: The scan command does not accept any arguments.",
                file=sys.stderr,
            )
            sys.exit(1)
        if (
            args.command == "set-network-config-static"
            and len(args.command_args) != 4
        ):
            print(
                "Error: The set-network-config command expects four "
                "arguments:\n"
                "\n"
                f"usage: {parser.prog} set-network-config \\\n"
                "  <device MAC address> \\\n"
                "  <new IP address> \\\n"
                "  <new network mask> \\\n"
                "  <new gateway address>",
                file=sys.stderr,
            )
            sys.exit(1)
        if (
            args.command
            in (
                "get-network-config",
                "reset-network-config-dhcp",
                "set-network-config-dhcp",
            )
            and len(args.command_args) != 1
        ):
            print(
                f"Error: The {args.command} command expects exactly one "
                "argument:\n"
                "\n"
                f"usage: {parser.prog} {args.command} <device MAC address>",
                file=sys.stderr,
            )
            sys.exit(1)
        return args

    def _reset_network_config_dhcp(self) -> None:
        assert self._client is not None
        device_mac_address = MACAddress(self._command_args[0])
        network_config = self._client.get_network_configuration(
            device_mac_address
        )
        if not network_config.dhcp_enabled:
            print(
                "Error: DHCP is not enabled for this device.", file=sys.stderr
            )
            sys.exit(1)
        network_config.dhcp_enabled = False
        # We switch the device to a static configuration, wait a moment, and
        # set it back to DHCP.
        print("Setting device to static mode...", file=sys.stderr)
        self._client.set_network_configuration(
            device_mac_address, network_config
        )
        time.sleep(5.0)
        network_config.dhcp_enabled = True
        print("Setting device back to DHCP mode...", file=sys.stderr)
        self._client.set_network_configuration(
            device_mac_address, network_config
        )
        print("Done.", file=sys.stderr)

    def _scan(self) -> None:
        assert self._client is not None
        found_devices = self._client.scan()
        for found_device in found_devices:
            print(
                f"Found device {found_device.mac_address} of type "
                f"{found_device.id1} ({found_device.id2}) at "
                f"{found_device.ip_address}."
            )
        if not found_devices:
            print("No devices found.", file=sys.stderr)

    def _set_network_config_dhcp(self) -> None:
        assert self._client is not None
        device_mac_address = MACAddress(self._command_args[0])
        network_config = self._client.get_network_configuration(
            device_mac_address
        )
        network_config.dhcp_enabled = True
        print("Setting device to DHCP mode...", file=sys.stderr)
        self._client.set_network_configuration(
            device_mac_address, network_config
        )
        print("Done.", file=sys.stderr)

    def _set_network_config_static(self) -> None:
        assert self._client is not None
        device_mac_address = MACAddress(self._command_args[0])
        ip_address = IPv4Address(self._command_args[1])
        network_mask = IPv4Address(self._command_args[2])
        gateway = IPv4Address(self._command_args[3])
        network_config = NetworkConfiguration(
            dhcp_enabled=False,
            gateway=gateway,
            ip_address=ip_address,
            network_mask=network_mask,
        )
        print("Setting device to static IP mode...", file=sys.stderr)
        self._client.set_network_configuration(
            device_mac_address, network_config
        )
        print("Done.", file=sys.stderr)

    @staticmethod
    def _suitable_addresses_for_interface(
        interface_name: str,
    ) -> list[dict[str, str]]:
        assert netifaces_module is not None
        interface_info = netifaces_module.ifaddresses(interface_name)
        ipv4_info = interface_info.get(netifaces_module.AF_INET, [])
        # Only use address configurations that include all the necessary
        # attributes. This should help us with ignoring the loopback address.
        return [
            addr_info
            for addr_info in ipv4_info
            if addr_info.get("addr")
            and addr_info.get("netmask")
            and addr_info.get("broadcast")
        ]

    def main(self):
        """
        Entrypoint for the program.
        """
        local_address = self._determine_local_address(self._interface)
        with Client(local_address) as client:
            self._client = client
            try:
                self._command()
            except ValueError as err:
                print(f"Error: {err}", file=sys.stderr)
                sys.exit(1)
            except TimeoutError:
                print("The device did not respond in time.", file=sys.stderr)
                sys.exit(1)


class BroadcastAddress(Address):
    """
    Broadcast address.
    """

    @property
    def address_type(self) -> AddressType:
        return AddressType.BROADCAST_ADDRESS

    @classmethod
    def decode(cls, data: bytes) -> typing.Self:
        if len(data) != 6:
            raise ValueError("Broadcast address must have exactly six bytes.")
        return cls()

    def encode(self) -> bytes:
        return b"\x00\x00\x00\x00\x00\x00"

    def __eq__(self, other: object) -> bool:
        return isinstance(other, BroadcastAddress)


@dataclasses.dataclass(frozen=True)
class FoundDevice:
    """
    Device that was found when scanning the network.
    """

    id1: str
    """
    First device identifier string.
    """

    id2: str
    """
    Second device identifier string.
    """

    ip_address: "IPv4Address"
    """
    Device IP address.
    """

    mac_address: "MACAddress"
    """
    Device MAC address.
    """


class IPv4Address:
    """
    IPv4 address.

    :param ip_address: IPv4 address.
    """

    _IPV4_ADDRESS_STRUCT = struct.Struct(">BBBB")

    def __init__(self, ip_address: str | tuple[int, int, int, int]):
        if isinstance(ip_address, str):
            octets = ip_address.split(".")
            if len(octets) != 4:
                raise ValueError("IPv4 address must have exactly four octets.")
            ip_address = (
                int(octets[0]),
                int(octets[1]),
                int(octets[2]),
                int(octets[3]),
            )
        else:
            if len(ip_address) != 4:
                raise ValueError("IPv4 address must have exactly four octets.")
        for octet in ip_address:
            if octet < 0 or octet > 255:
                raise ValueError(
                    "IPv4 address octet must be in range 0 to 255."
                )
        self._ip_address = ip_address

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, IPv4Address):
            return False
        return self._ip_address == other._ip_address

    def __str__(self) -> str:
        return ".".join(f"{octet}" for octet in self._ip_address)

    @classmethod
    def decode(cls, data: bytes) -> typing.Self:
        """
        Decode an address from the on-wire representation.
        """
        try:
            ip_octet1, ip_octet2, ip_octet3, ip_octet4 = (
                cls._IPV4_ADDRESS_STRUCT.unpack(data)
            )
        except struct.error as err:
            raise ValueError("IPv4 address have exactly four bytes.") from err
        return cls((ip_octet1, ip_octet2, ip_octet3, ip_octet4))

    def encode(self) -> bytes:
        """
        Return the on-wire representation of the address.
        """
        return self._IPV4_ADDRESS_STRUCT.pack(*self._ip_address)


class IPv4AddressWithPort(Address):
    """
    IPv4 address with port number.

    :param ip_address: IPv4 address.
    :param port: port number.
    """

    _IPV4_ADDRESS_WITH_PORT_STRUCT = struct.Struct(">4sH")

    def __init__(
        self,
        ip_address: IPv4Address | str | tuple[int, int, int, int],
        port: int,
    ):
        if isinstance(ip_address, (str, tuple)):
            ip_address = IPv4Address(ip_address)
        self._ip_address = ip_address
        self._port = port

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, IPv4AddressWithPort):
            return False
        return (
            self._ip_address == other._ip_address and self._port == other._port
        )

    def __str__(self) -> str:
        return f"{self.ip_address}:{self.port}"

    @property
    def address_type(self) -> AddressType:
        return AddressType.IPV4_ADDRESS_WITH_PORT

    @classmethod
    def decode(cls, data: bytes) -> typing.Self:
        try:
            ip_address_bytes, port = cls._IPV4_ADDRESS_WITH_PORT_STRUCT.unpack(
                data
            )
        except struct.error as err:
            raise ValueError(
                "IPv4 address with port must have exactly six bytes."
            ) from err
        return cls(IPv4Address.decode(ip_address_bytes), port)

    def encode(self) -> bytes:
        ip_address_bytes = self._ip_address.encode()
        return self._IPV4_ADDRESS_WITH_PORT_STRUCT.pack(
            ip_address_bytes, self._port
        )

    @property
    def ip_address(self) -> IPv4Address:
        """
        IPv4 address as string.
        """
        return self._ip_address

    @property
    def port(self) -> int:
        """
        Port number.
        """
        return self._port


class MACAddress(Address):
    """
    MAC address.

    :param mac_address: MAC address as tuple of octets or ``str``.
    """

    _MAC_ADDRESS_STRUCT = struct.Struct(">BBBBBB")

    def __init__(self, mac_address: str | tuple[int, int, int, int, int, int]):
        if isinstance(mac_address, str):
            octets = mac_address.split(":")
            if len(octets) != 6:
                raise ValueError("MAC address must have exactly six octets.")
            mac_address_octets = tuple(int(octet, 16) for octet in octets)
            assert len(mac_address_octets) == 6
            mac_address = mac_address_octets
        else:
            if len(mac_address) != 6:
                raise ValueError("MAC address must have exactly six octets.")
        for octet in mac_address:
            if octet < 0 or octet > 255:
                raise ValueError(
                    "MAC address octet must be in range 0 to 255."
                )
        self._mac_address = mac_address

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, MACAddress):
            return False
        return self._mac_address == other._mac_address

    def __hash__(self) -> int:
        return hash(self._mac_address)

    def __str__(self) -> str:
        return ":".join(f"{octet:02x}" for octet in self._mac_address)

    @property
    def address_type(self) -> AddressType:
        return AddressType.MAC_ADDRESS

    @classmethod
    def decode(cls, data: bytes) -> typing.Self:
        try:
            mac_octets = cls._MAC_ADDRESS_STRUCT.unpack(data)
        except struct.error as err:
            raise ValueError(
                "MAC address must have exactly six bytes."
            ) from err
        return cls(mac_octets)

    def encode(self) -> bytes:
        return self._MAC_ADDRESS_STRUCT.pack(*self._mac_address)


@dataclasses.dataclass(frozen=True)
class Message:
    """
    Message sent via UDP/IP.
    """

    recipient_address: Address
    """
    Recipient address.
    """

    sender_address: Address
    """
    Sender address.
    """

    message_type: "MessageType"
    """
    Message type.
    """

    command: Command
    """
    Command.
    """

    payload: bytes | None = None
    """
    Payload for command.
    """

    @staticmethod
    def _decode_address(
        address_type_as_int: int, address_bytes: bytes
    ) -> Address:
        address_type = AddressType(address_type_as_int)
        address_class = _CLASS_FOR_ADDRESS_TYPE[address_type]
        return address_class.decode(address_bytes)

    @classmethod
    def decode(cls, data: bytes) -> typing.Self:
        """
        Decode a message from the on-wire representation.
        """
        try:
            (
                recipient_address_type,
                recipient_address_bytes,
                sender_address_type,
                sender_address_bytes,
                interlude1,
                message_type,
                interlude2,
                command,
            ) = MESSAGE_STRUCT.unpack_from(data)
        except struct.error as err:
            raise ValueError(
                f"Message is too short: Expected {MESSAGE_STRUCT.size} bytes "
                f"but got {len(data)} bytes."
            ) from err
        payload = data[MESSAGE_STRUCT.size :]
        if interlude1 != MESSAGE_INTERLUDE1:
            raise ValueError(
                f"Expected {MESSAGE_INTERLUDE1} but got {interlude1}."
            )
        if interlude2 != MESSAGE_INTERLUDE2:
            raise ValueError(
                f"Expected {MESSAGE_INTERLUDE2} but got {interlude2}."
            )
        message = cls(
            recipient_address=cls._decode_address(
                recipient_address_type, recipient_address_bytes
            ),
            sender_address=cls._decode_address(
                sender_address_type, sender_address_bytes
            ),
            message_type=MessageType(message_type),
            command=Command(command),
            payload=(payload if payload else None),
        )
        return message

    def encode(self) -> bytes:
        """
        Return the on-wire representation of the message.
        """
        return MESSAGE_STRUCT.pack(
            self.recipient_address.address_type.value,
            self.recipient_address.encode(),
            self.sender_address.address_type.value,
            self.sender_address.encode(),
            MESSAGE_INTERLUDE1,
            self.message_type.value,
            MESSAGE_INTERLUDE2,
            self.command.value,
        ) + (self.payload if self.payload else b"")


class MessageType(enum.Enum):
    """
    Message types.
    """

    REQUEST = 0x01
    """
    Request from client to device.
    """

    RESPONSE = 0x00
    """
    Response from device to client.
    """


@dataclasses.dataclass
class NetworkConfiguration:
    """
    DHCP enabled?
    """

    dhcp_enabled: bool

    """
    Gateway IP address.
    """
    gateway: IPv4Address

    """
    Device IP address.
    """
    ip_address: IPv4Address

    """
    Device IP network mask.
    """
    network_mask: IPv4Address


class PayloadField(enum.Enum):
    """
    Payload field identifiers.
    """

    DEVICE_ID1 = 0x02
    """
    First device ID string.
    """

    DEVICE_ID2 = 0x03
    """
    Second device ID string.
    """

    NET_CONFIG_DHCP_ENABLED = 0x04
    """
    Is DHCP enabled?
    """

    NET_CONFIG_STATIC_GATEWAY = 0x07
    """
    Static IPv4 gateway.
    """

    NET_CONFIG_STATIC_IP_ADDRESS = 0x05
    """
    Static IPv4 address.
    """

    NET_CONFIG_STATIC_NETMASK = 0x06
    """
    Static IPv4 network mask.
    """


_CLASS_FOR_ADDRESS_TYPE = {
    AddressType.BROADCAST_ADDRESS: BroadcastAddress,
    AddressType.IPV4_ADDRESS_WITH_PORT: IPv4AddressWithPort,
    AddressType.MAC_ADDRESS: MACAddress,
}

if __name__ == "__main__":
    CommandLineInterface(sys.argv[1:]).main()
