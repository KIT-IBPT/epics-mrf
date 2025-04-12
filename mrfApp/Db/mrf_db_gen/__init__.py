"""
Library code for generating ``.db`` and ``.req`` files for the MRF device
support.
"""

import typing

from . import common, constants, evg, evr, template

__all__ = [
    "common",
    "constants",
    "device_for_name",
    "evg",
    "evr",
    "generate_file",
    "template",
]


def device_for_name(name: str) -> constants.Device:
    """
    Return the device definition for the specified name.

    Args:
        name: name of the device (e.g. “VME-EVR-230”).

    Returns:
        device definition for the specified name.

    Raises:
        KeyError: if ``name`` is not a recognized device name.
    """
    for device in constants.Device:
        # We have to ignore upper / lower case because this might differ
        # depending on where the name is used.
        if str(device).lower() == name.lower():
            return device
    raise KeyError(name)


def generate_file(
    output_file: typing.TextIO,
    device: constants.Device,
    file_type: constants.FileType,
    description_record_type: constants.DescriptionRecordType,
) -> None:
    """
    Generate a ``.db`` or ``.req`` file for the specified device.

    Args:
        output_file: file object to which the output is written.
        device: device for which the file shall be generated.
        file_type: type of the file that shall be generated.
        description_record_type: Record type that is used for description
            records. In general, `lso` records are the best choice, but they
            are only supported in recent releases of EPICS Base.
    """
    if device.device_class == constants.DeviceClass.EVG:
        evg.generate_file(
            output_file, device, file_type, description_record_type
        )
    elif device.device_class == constants.DeviceClass.EVR:
        evr.generate_file(
            output_file, device, file_type, description_record_type
        )
    else:
        raise NotImplementedError(
            f"Support for {device} is not implemented yet."
        )
