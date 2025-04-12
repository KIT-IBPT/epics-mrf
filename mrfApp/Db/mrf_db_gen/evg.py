"""
Generate `.db` and `.req` files for EVGs.
"""

import dataclasses
import typing

from .common import MRFCommon
from .constants import (
    DescriptionRecordType,
    Device,
    DeviceClass,
    FileType,
    FormFactor,
)


class EVG(MRFCommon):
    """
    Generates code for EVG devices.

    Args:
        file_type: Type of the file that is being generated.
        output_file: file object to which the output is written.
        device_config: device configuration for which the code shall be
            generated.
        address_offset: Offset that is added to all mememory addresses. This is
            useful if the register map of a device is mapped into the device’s
            memory at an offset (like the virtual EVRs inside an EVM).
        description_record_type: Record type that is used for description
            records. In general, `lso` records are the best choice, but they
            are only supported in recent releases of EPICS Base.
        pv_prefix: Prefix that is used for all record names.
    """

    def __init__(  # pylint: disable=too-many-arguments
        self,
        file_type: FileType,
        output_file: typing.TextIO,
        config: "EVGConfig",
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

    def _common(self) -> None:
        self.write_template("evg-common")

        # The record type that is used for descriptions depends on a
        # compile-time option, so we cannot include the description records in
        # the “evg-common” template and generate them here instead.
        for index in range(0, 8):
            self.description(
                f"DBus:B{index}:Description",
                f"Description for dist. bus bit {index}",
            )
        for index in range(0, 8):
            self.description(
                f"Event:EvTrig{index}:Description",
                f"Description for event trigger {index}",
            )

        # We have to include the PV that triggers writing of all output records
        # in the “evg-common” template.
        self.add_to_write_all_pvs_list("Intrnl:WriteAll:Common")

    def _front_panel_input(self, input_number: int) -> None:
        address = 0x0500 + 4 * input_number
        self._input(f"FPIn{input_number}", f"FP input {input_number}", address)

    def _front_panel_output(self, output_number: int) -> None:
        address = 0x0400 + 2 * output_number
        self._output(
            f"FPOut{output_number}",
            f"front-panel output {output_number}",
            address,
        )

    def _input(  # pylint: disable=too-many-arguments
        self,
        input_name: str,
        input_description: str,
        input_address: int,
        *,
        input_installed_macro: str = "",
        input_installed_description: str = "",
    ) -> None:
        self.write_template(
            "evg-template-input",
            variables={
                "INPUT_ADDR": self.render_address(input_address),
                "INPUT_DESCRIPTION": input_description,
                "INPUT_NAME": input_name,
            },
        )
        if input_installed_macro:
            self.write_template(
                "evg-template-input-installed",
                variables={
                    "INPUT_INSTALLED_DESCRIPTION": input_installed_description,
                    "INPUT_INSTALLED_MACRO": input_installed_macro,
                    "INPUT_NAME": input_name,
                },
            )
        self.description(
            f"{input_name}:Description", f"Description for {input_description}"
        )
        self.add_to_write_all_pvs_list(f"Intrnl:WriteAll:{input_name}")

    def _multiplexed_counter(self, mxc_number: int) -> None:
        mxc_addr = 0x0180 + 8 * mxc_number
        mxc_prescaler_addr = 0x0184 + 8 * mxc_number
        self.write_template(
            "evg-template-mxc",
            variables={
                "MXC_ADDR": self.render_address(mxc_addr),
                "MXC_NUM": str(mxc_number),
                "MXC_PRESCALER_ADDR": self.render_address(mxc_prescaler_addr),
            },
        )
        self.description(
            f"MXC{mxc_number}:Description", f"Description for MXC {mxc_number}"
        )
        self.add_to_write_all_pvs_list(f"Intrnl:WriteAll:MXC{mxc_number}")

    def _output(  # pylint: disable=too-many-arguments
        self,
        output_name: str,
        output_description: str,
        output_address: int,
        *,
        output_installed_macro: str = "",
        output_installed_description: str = "",
    ) -> None:
        self.write_template(
            "evg-template-output",
            variables={
                "OUTPUT_ADDR": self.render_address(output_address),
                "OUTPUT_DESCRIPTION": output_description,
                "OUTPUT_NAME": output_name,
            },
        )
        if output_installed_macro:
            self.write_template(
                "evg-template-output-installed",
                variables={
                    "OUTPUT_INSTALLED_DESCRIPTION": output_installed_description,
                    "OUTPUT_INSTALLED_MACRO": output_installed_macro,
                    "OUTPUT_NAME": output_name,
                },
            )
        self.description(
            f"{output_name}:Description",
            f"Description for {output_description}",
        )
        self.add_to_write_all_pvs_list(f"{output_name}:Map")

    def _universal_input(self, input_number: int) -> None:
        address = 0x0540 + 4 * input_number
        univ_in_module_first_input_num = input_number - input_number % 2
        univ_in_module_second_input_num = univ_in_module_first_input_num + 1
        self._input(
            f"UnivIn{input_number}",
            f"univ. input {input_number}",
            address,
            input_installed_macro=(
                f"UNIV_IN_{univ_in_module_first_input_num}_"
                f"{univ_in_module_second_input_num}_INSTALLED="
                "$(UNIV_IN_INSTALLED=0)"
            ),
            input_installed_description=(
                f"Universal input module {univ_in_module_first_input_num}/"
                f"{univ_in_module_second_input_num}"
            ),
        )

    def _universal_output(self, output_number: int) -> None:
        address = 0x0440 + 2 * output_number
        univ_out_module_first_output_num = output_number - output_number % 2
        univ_out_module_second_output_num = (
            univ_out_module_first_output_num + 1
        )
        self._output(
            f"UnivOut{output_number}",
            f"universal output {output_number}",
            address,
            output_installed_macro=(
                f"UNIV_OUT_{univ_out_module_first_output_num}_"
                f"{univ_out_module_second_output_num}_INSTALLED="
                "$(UNIV_OUT_INSTALLED=0)"
            ),
            output_installed_description=(
                f"Universal output module {univ_out_module_first_output_num}/"
                f"{univ_out_module_second_output_num}"
            ),
        )

    def _transition_board_input(self, input_number: int) -> None:
        address = 0x0600 + 4 * input_number
        tb_module_first_input_num = input_number - input_number % 2
        tb_module_second_input_num = tb_module_first_input_num + 1
        self._input(
            f"TBIn{input_number}",
            f"TB u. input {input_number}",
            address,
            input_installed_macro=(
                f"TB_UNIV_IN_{tb_module_first_input_num}_"
                f"{tb_module_second_input_num}_INSTALLED="
                "$(TB_UNIV_IN_INSTALLED=0)"
            ),
            input_installed_description=(
                f"TB univ. input module {tb_module_first_input_num}/"
                f"{tb_module_second_input_num}"
            ),
        )

    def _vme(self) -> None:
        self.write_template("evg-vme")
        self.add_to_write_all_pvs_list("Intrnl:WriteAll:VME")

    def generate(self) -> None:
        """
        Generate all code needed for the specified device.
        """
        # Check that a supported device has been passed.
        if self._config.device.device_class != DeviceClass.EVG:
            raise ValueError(
                f"Cannot generate EVG code for {self._config.device}."
            )

        # Ensure that we start with a clean slate.
        self.clear_write_all_pvs_list()

        # Generate records, starting with the ones that are common to all
        # devices.
        self._common()

        # Multiplexed counters.
        for index in range(0, 8):
            self._multiplexed_counter(index)

        # Universal outputs.
        for index in range(0, self._config.universal_outputs):
            self._universal_output(index)

        # Front-panel inputs.
        for index in range(0, self._config.front_panel_inputs):
            self._front_panel_input(index)

        # Universal inputs.
        for index in range(0, self._config.universal_inputs):
            self._universal_input(index)

        # There also are a couple of records that are specific to the VME form
        # factor.
        if self._config.device.form_factor == FormFactor.VME:
            self._vme()
            # TODO The number of front-panel outputs and transition board
            # inputs depends on the form factor, but this code should probably
            # still be placed somewhere else.
            for index in range(0, self._config.front_panel_outputs):
                self._front_panel_output(index)
            for index in range(0, 16):
                self._transition_board_input(index)

        # SFP module (there only is a single one, so we do not include a
        # number).
        self.sfp_module("SFP", 0x1200)

        # Generate record that links to all output records.
        self.fanout_for_write_all_pvs("Intrnl:WriteAll")


@dataclasses.dataclass
class EVGConfig:
    """
    Configuration for an EVG device.

    Contains information about the registers provided for the specific device
    type.
    """

    # Device to which this configuration applies.
    device: Device

    # Number of front-panel inputs.
    front_panel_inputs: int

    # Number of front-panel outputs.
    front_panel_outputs: int

    # Number of universal inputs.
    universal_inputs: int

    # Number of universal outputs.
    universal_outputs: int


# Configuration for specific EVG devices.
_EVG_CONFIGS = {
    Device.VME_EVG_230: EVGConfig(
        device=Device.VME_EVG_230,
        front_panel_inputs=2,
        front_panel_outputs=4,
        universal_inputs=4,
        universal_outputs=4,
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
        config = _EVG_CONFIGS[device]
    except KeyError:
        raise ValueError(  # pylint: disable=raise-missing-from
            f"Unsupported device: {device}"
        )
    EVG(
        file_type,
        output_file,
        config,
        description_record_type=desciption_record_type,
    ).generate()
