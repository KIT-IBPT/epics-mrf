"""
Generate `.db` and `.req` files for EVRs.
"""

import dataclasses
import typing

from .common import MRFCommon

from .constants import (
    DescriptionRecordType,
    Device,
    DeviceClass,
    FileType,
)

from . import evg as _evg
from . import evr as _evr


class _FanoutConcentrator(MRFCommon):
    """
    Generates code for the fanout / concentrator embedded within EVM devices.

    Args:
        address_offset: Offset that is added to all mememory addresses.
        config: device configuration of the EVM for which the code shall be
            generated.
        description_record_type: Record type that is used for description
            records. In general, `lso` records are the best choice, but they
            are only supported in recent releases of EPICS Base.
        file_type: Type of the file that is being generated.
        output_file: file object to which the output is written.
        pv_prefix: Prefix that is used for all record names.
    """

    def __init__(  # pylint: disable=too-many-arguments
        self,
        *,
        address_offset: int,
        config: "EVMConfig",
        description_record_type: DescriptionRecordType,
        file_type: FileType,
        output_file: typing.TextIO,
        pv_prefix: str,
    ):
        super().__init__(
            file_type,
            output_file,
            address_offset=address_offset,
            description_record_type=description_record_type,
            pv_prefix=pv_prefix,
        )
        self._config = config

    def generate(self) -> None:
        """
        Generate all code needed for the specified device.
        """
        self.write_template("evm-fct-common")

        # The fanout / concentrator has 8 SFP modules which are labeled 1
        # through 8. The EVM has 9 SFP modules in total, but the last one
        # (labeled “U”) is considered a part of the EVG.
        for sfp_index in range(0, 8):
            self.sfp_module(f"SFP{sfp_index + 1}", 0x1000 + sfp_index * 0x200)


class EVM(MRFCommon):
    """
    Generates code for EVM devices.

    Args:
        file_type: Type of the file that is being generated.
        output_file: file object to which the output is written.
        device_config: device configuration for which the code shall be
            generated.
        address_offset: Offset that is added to all mememory addresses.
        description_record_type: Record type that is used for description
            records. In general, `lso` records are the best choice, but they
            are only supported in recent releases of EPICS Base.
        pv_prefix: Prefix that is used for all record names.
    """

    def __init__(
        self,
        file_type: FileType,
        output_file: typing.TextIO,
        config: "EVMConfig",
        *,
        address_offset: int = 0,
        description_record_type: DescriptionRecordType = (
            DescriptionRecordType.LSO
        ),
        pv_prefix: str = "$(P)$(R)",
    ):
        super().__init__(
            file_type,
            output_file,
            address_offset=address_offset,
            description_record_type=description_record_type,
            pv_prefix=pv_prefix,
        )
        self._config = config

    def generate(self) -> None:
        """
        Generate all code needed for the specified device.
        """
        # Check that a supported device has been passed.
        if self._config.device.device_class != DeviceClass.EVM:
            raise ValueError(
                f"Cannot generate EVM code for {self._config.device}."
            )

        # Ensure that we start with a clean slate.
        self.clear_write_all_pvs_list()

        # Generate the code for the embedded EVG. The address space for the EVG
        # is right at the start of the address space for the EVM, so the offset
        # is zero.
        _evg.EVG(
            self._file_type,
            self._output_file,
            self._config.evg,
            address_offset=self._address_offset,
            description_record_type=self._description_record_type,
            pv_prefix=self._pv_prefix + "G:",
        ).generate()
        self.add_to_write_all_pvs_list("G:WriteAll")

        # Generate the code for the embedded fanout-concentrator (FCT) unit.
        # The address space for the FCT is in the range from 0x10000 to
        # 0x1ffff.
        _FanoutConcentrator(
            address_offset=self._address_offset + 0x10000,
            config=self._config,
            description_record_type=self._description_record_type,
            file_type=self._file_type,
            output_file=self._output_file,
            pv_prefix=f"{self._pv_prefix}FCT:",
        ).generate()

        # Generate the code for the embedded EVRs. There are two EVRs. The
        # address space for the downstream EVR is in the range from 0x20000 to
        # 0x2ffff and the address space for the upstream EVR is in the range
        # from 0x30000 to 0x3ffff.
        _evr.EVR(
            self._file_type,
            self._output_file,
            self._config.downstream_evr,
            address_offset=self._address_offset + 0x20000,
            description_record_type=self._description_record_type,
            pv_prefix=self._pv_prefix + "RD:",
        ).generate()
        self.add_to_write_all_pvs_list("RD:WriteAll")
        _evr.EVR(
            self._file_type,
            self._output_file,
            self._config.upstream_evr,
            address_offset=self._address_offset + 0x30000,
            description_record_type=self._description_record_type,
            pv_prefix=self._pv_prefix + "RU:",
        ).generate()
        self.add_to_write_all_pvs_list("RU:WriteAll")

        # Generate records that are related to the EVM in general and not
        # specific to one of the embedded devices.
        self.write_template("evm-common")

        # Generate record that links to all output records.
        self.fanout_for_write_all_pvs("Intrnl:WriteAll")


@dataclasses.dataclass
class EVMConfig:
    """
    Configuration for an EVM device.

    Contains information about the registers provided for the specific device
    type.
    """

    # Device to which this configuration applies.
    device: Device

    # Configuration for the internal downstream EVR.
    downstream_evr: _evr.EVRConfig

    # Configuration for the internal EVG.
    evg: _evg.EVGConfig

    # Configuration for the internal upstream EVR.
    upstream_evr: _evr.EVRConfig


_EVM_CONFIGS = {
    Device.VME_EVM_300: EVMConfig(
        device=Device.VME_EVM_300,
        downstream_evr=_evr.EVRConfig(
            device=Device.VME_EVM_300,
            feature_level=_evr.FeatureLevel.GEN2,
            # TODO There are registers for all FP inputs (and some univ.
            #   inputs), but they all seem to refer to the same two values, so
            #   only FPIn0 and FPIn1 seem to be “real”.
            front_panel_inputs=8,
            front_panel_outputs=([_evr.OutputConfig()] * 8),
            prescalers=([_evr.PrescalerConfig(32)] * 8),
            pulse_generators=(
                [_evr.PulseGeneratorConfig(32, 16, 32)] * 4
                + [_evr.PulseGeneratorConfig(32, 0, 16)] * 20
            ),
            sfp_available=False,
            # TODO There are registers for 12 univ. inputs, but they only seem
            #   to duplicate FPIn0 / FPIn1.
            universal_inputs=0,
            universal_outputs=[],
            use_mmap=False,
        ),
        evg=_evg.EVGConfig(
            device=Device.VME_EVM_300,
            feature_level=_evg.FeatureLevel.GEN2,
            front_panel_inputs=3,
            front_panel_outputs=0,
            transition_board_inputs=16,
            # TODO For the VME-EVM-300, also has transition board outputs
            #   (presumably when univ. output instead of input modules are
            #   plugged into the rear transition module).
            # TODO These universal inputs are always installed (they are
            #   virtual and connect to the FP outputs of the embedded EVRs), so
            #   the installed macro should be fixed to 1.
            universal_inputs=16,
            use_mmap=False,
        ),
        upstream_evr=_evr.EVRConfig(
            device=Device.VME_EVM_300,
            feature_level=_evr.FeatureLevel.GEN2,
            # TODO There are registers for all FP inputs (and some univ.
            #   inputs), but they all seem to refer to the same two values, so
            #   only FPIn0 and FPIn1 seem to be “real”.
            front_panel_inputs=8,
            front_panel_outputs=([_evr.OutputConfig()] * 8),
            prescalers=([_evr.PrescalerConfig(32)] * 8),
            pulse_generators=(
                [_evr.PulseGeneratorConfig(32, 16, 32)] * 4
                + [_evr.PulseGeneratorConfig(32, 0, 16)] * 20
            ),
            sfp_available=False,
            # TODO There are registers for 12 univ. inputs, but they only seem
            #   to duplicate FPIn0 / FPIn1.
            universal_inputs=0,
            universal_outputs=[],
            use_mmap=False,
        ),
    )
}


def generate_file(
    output_file: typing.TextIO,
    device: Device,
    file_type: FileType,
    desciption_record_type: DescriptionRecordType,
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
    try:
        config = _EVM_CONFIGS[device]
    except KeyError:
        raise ValueError(  # pylint: disable=raise-missing-from
            f"Unsupported device: {device}"
        )
    EVM(
        file_type,
        output_file,
        config,
        description_record_type=desciption_record_type,
    ).generate()
