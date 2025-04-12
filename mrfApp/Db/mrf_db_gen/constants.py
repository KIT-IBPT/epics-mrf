"""
Shared constant definitions.
"""

import dataclasses
import enum


class NoValueEnum(enum.Enum):
    """
    Base class for enums where the values does not matter.
    """

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}.{self.name}"


class DescriptionRecordType(enum.Enum):
    """
    Record type used for description records.
    """

    # ``lso`` record.
    LSO = "lso"

    # ``stringout`` record.
    STRINGOUT = "stringout"

    # ``waveform`` record.
    WAVEFORM = "waveform"


class DeviceClass(NoValueEnum):
    """
    Represents the class of an MRF device (EVG, EVM, or EVR).
    """

    # Event generator.
    EVG = object()

    # Event master.
    EVM = object()

    # Event receiver.
    EVR = object()


@dataclasses.dataclass(frozen=True)
class DeviceType:
    """
    Represents the type of an MRF device.
    """

    form_factor: "FormFactor"
    device_class: DeviceClass
    device_name: str
    series: str

    def __str__(self) -> str:
        return self.device_name


class FileType(enum.Enum):
    """
    Type of the file that is being generated.
    """

    # EPICS record (``.db``) file.
    DB = ".db"

    # EPICS Autosave request (``.req``) file.
    REQ = ".req"


class FormFactor(NoValueEnum):
    """
    Represents the form-factor of an MRF device.
    """

    # µTCA form factor.
    MTCA = object()

    # VME form factor.
    VME = object()


class Device(DeviceType, enum.Enum):
    """
    Represents an MRF device.
    """

    # mTCA-EVR-300.
    MTCA_EVR_300 = FormFactor.MTCA, DeviceClass.EVR, "mTCA-EVR-300", "300"

    # VME-EVG-230.
    VME_EVG_230 = FormFactor.VME, DeviceClass.EVG, "VME-EVG-230", "230"

    # VME-EVR-230.
    VME_EVR_230 = FormFactor.VME, DeviceClass.EVR, "VME-EVR-230", "230"

    # VME-EVR-230RF.
    VME_EVR_230RF = FormFactor.VME, DeviceClass.EVR, "VME-EVR-230RF", "230"

    # VME-EVR-300.
    VME_EVR_300 = FormFactor.VME, DeviceClass.EVR, "VME-EVR-300", "300"

    def __str__(self) -> str:
        return DeviceType.__str__(self)
