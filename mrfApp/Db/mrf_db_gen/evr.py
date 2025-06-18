"""
Generate `.db` and `.req` files for EVRs.
"""

import dataclasses
import enum
import typing

from .common import MRFCommon

from .constants import (
    DescriptionRecordType,
    Device,
    DeviceClass,
    FileType,
    FormFactor,
)


class EVR(MRFCommon):
    """
    Generates code for EVR devices.

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
        config: "EVRConfig",
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

    def _backplane_output(
        self,
        output_number: int,
        output_config: "OutputConfig",
    ) -> None:
        bp_output_address = 0x04C0 + 2 * output_number
        self._output(
            f"BPOut{output_number}",
            f"Backplane output {output_number}",
            bp_output_address,
            output_config,
        )

    def _common(self) -> None:
        self.write_template("evr-common")
        self.add_to_write_all_pvs_list("Intrnl:WriteAll:Common")

    def _dbus_bits_to_pulse_gens(self) -> None:
        for dbus_bit_index in range(0, 8):
            dbus_bit_to_pulse_gen_addr = 0x0180 + 4 * dbus_bit_index
            for pulse_gen_index in range(
                0, len(self._config.pulse_generators)
            ):
                self.write_template(
                    "evr-template-dbus-bit-to-pulse-gen",
                    variables={
                        "DBUS_BIT_NUM": str(dbus_bit_index),
                        "DBUS_BIT_TO_PULSE_GEN_ADDR": self.render_address(
                            dbus_bit_to_pulse_gen_addr
                        ),
                        "PULSE_GEN_NUM": str(pulse_gen_index),
                    },
                )

    def _delay_compensation(self) -> None:
        # TODO Maybe we should move this into the evr-common-gen2 template?
        self.write_template("evr-delay-compensation")
        self.add_to_write_all_pvs_list("DelayComp:Enabled")
        self.add_to_write_all_pvs_list("DelayComp:TargetDelay")

    def _front_panel_input(
        self,
        input_number: int,
    ) -> None:
        input_address = 0x0500 + 4 * input_number
        self.write_template(
            "evr-template-fp-input",
            variables={
                "FP_INPUT_ADDR": self.render_address(input_address),
                "FP_INPUT_NUM": str(input_number),
            },
        )
        if self._config.feature_level == FeatureLevel.GEN2:
            self.write_template(
                "evr-template-fp-input-read",
                variables={
                    "FP_INPUT_ADDR": self.render_address(input_address),
                    "FP_INPUT_NUM": str(input_number),
                },
            )
        self.description(
            f"FPIn{input_number}:Description",
            f"Description for front-panel input {input_number}",
        )
        self.add_to_write_all_pvs_list(f"Intrnl:WriteAll:FPIn{input_number}")

    def _front_panel_output(
        self,
        output_number: int,
        output_config: "OutputConfig",
    ) -> None:
        fp_output_address = 0x0400 + 2 * output_number
        self._output(
            f"FPOut{output_number}",
            f"Front panel output {output_number}",
            fp_output_address,
            output_config,
        )

    def _interrupts_mmap(self) -> None:
        self.write_template("evr-interrupts-mmap")
        self.add_to_write_all_pvs_list("Intrnl:WriteAll:IRQ")

    def _interrupts_udp_ip(self) -> None:
        self.write_template("evr-interrupts-udp-ip")
        self.add_to_write_all_pvs_list("Intrnl:WriteAll:IRQ")

    def _mapping_ram(self, mapping_ram_number: int) -> None:
        base_address = 0x4000 + 0x1000 * mapping_ram_number
        internal_functions_address = base_address
        trigger_pulse_generators_address = base_address + 0x04
        set_pulse_generators_address = base_address + 0x08
        reset_pulse_generators_address = base_address + 0x0C
        self.write_template(
            "evr-template-map-ram",
            variables={
                "MAP_RAM_INT_FUNCS_ADDR": self.render_address(
                    internal_functions_address
                ),
                "MAP_RAM_NUM": str(mapping_ram_number),
                "MAP_RAM_RESET_PULSE_GENS_ADDR": self.render_address(
                    reset_pulse_generators_address
                ),
                "MAP_RAM_SET_PULSE_GENS_ADDR": self.render_address(
                    set_pulse_generators_address
                ),
                "MAP_RAM_TRIG_PULSE_GENS_ADDR": self.render_address(
                    trigger_pulse_generators_address
                ),
            },
        )
        self.add_to_write_all_pvs_list(
            f"Event:MapRAM{mapping_ram_number}:WriteAll"
        )

    def _output(  # pylint: disable=too-many-arguments
        self,
        default_output_name: str,
        default_output_description: str,
        output_address: int,
        output_config: "OutputConfig",
        *,
        output_fine_delay_available_macro: typing.Optional[str] = None,
        output_installed_macro: typing.Optional[str] = None,
        output_installed_description: typing.Optional[str] = None,
    ) -> None:
        if output_config.name is None:
            output_name = default_output_name
        else:
            output_name = output_config.name
        if output_config.description is None:
            output_description = default_output_description
        else:
            output_description = output_config.description
        template_name = (
            "evr-template-output-gen2"
            if self._config.feature_level == FeatureLevel.GEN2
            else "evr-template-output"
        )
        self.write_template(
            template_name,
            variables={
                "OUTPUT_ADDR": self.render_address(output_address),
                "OUTPUT_DESCRIPTION": output_description,
                "OUTPUT_NAME": output_name,
            },
        )
        # An output is considered optional, if the optional flag is set
        # explicitly, or if it is is not set at all (None) and the calling
        # function provides the installed macro.
        optional = output_config.optional
        if optional is None:
            if (
                output_installed_macro is not None
                and output_installed_description is not None
            ):
                optional = True
        elif optional:
            if (
                output_config.installed_description is None
                or output_config.installed_macro is None
            ):
                raise ValueError(
                    "If optional is set to True, installed_macro and installed_description must also be set."
                )
            output_installed_description = output_config.installed_description
            output_installed_macro = output_config.installed_macro
        if optional:
            assert output_installed_description is not None
            assert output_installed_macro is not None
            self.write_template(
                "evr-template-output-installed",
                variables={
                    "OUTPUT_INSTALLED_DESCRIPTION": (
                        output_installed_description
                    ),
                    "OUTPUT_INSTALLED_MACRO": output_installed_macro,
                    "OUTPUT_NAME": output_name,
                },
            )
        self.description(
            f"{output_name}:Description",
            f"Description for {output_description}",
        )
        self.add_to_write_all_pvs_list(f"{output_name}:Map")
        if self._config.feature_level == FeatureLevel.GEN2:
            self.add_to_write_all_pvs_list(f"{output_name}:Map2")
        # If the output supports CML logic, we have to generate the respective
        # code.
        if output_config.cml_block_index is not None:
            if output_config.cml_block_description is None:
                output_cml_block_description = output_description
            else:
                output_cml_block_description = (
                    output_config.cml_block_description
                )
            self._output_cml_logic(
                output_name,
                output_cml_block_description,
                output_config.cml_block_index,
            )
        # If the output supports GTX logic, we have to generate the respective
        # code.
        if output_config.gtx_block_index is not None:
            if output_config.gtx_block_description is None:
                output_gtx_block_description = output_description
            else:
                output_gtx_block_description = (
                    output_config.gtx_block_description
                )
            self._output_gtx_logic(
                output_name,
                output_gtx_block_description,
                output_config.gtx_block_index,
            )
        # If the output allows setting a fine delay through the GPIO pins, we
        # have to generate the respective code.
        if output_config.fine_delay_index is not None:
            if output_config.fine_delay_available_macro is not None:
                output_fine_delay_available_macro = (
                    output_config.fine_delay_available_macro
                )
            elif output_fine_delay_available_macro is None:
                raise ValueError(
                    "If fine_delay_index is set, fine_delay_available_macro "
                    "must also be set."
                )
            if output_config.fine_delay_description is None:
                output_fine_delay_description = output_description
            else:
                output_fine_delay_description = (
                    output_config.fine_delay_description
                )
            self._output_fine_delay(
                output_name,
                output_fine_delay_description,
                output_fine_delay_available_macro,
                output_config.fine_delay_index,
            )

    def _output_cml_logic(
        self,
        output_name: str,
        output_description: str,
        cml_block_index: int,
    ) -> None:
        cml_base_address = 0x0600 + 0x0020 * cml_block_index
        cml_pattern_00_address = cml_base_address
        cml_pattern_01_address = cml_base_address + 0x04
        cml_pattern_10_address = cml_base_address + 0x08
        cml_pattern_11_address = cml_base_address + 0x0C
        cml_control_address = cml_base_address + 0x10
        cml_high_period_address = cml_base_address + 0x14
        cml_low_period_address = cml_base_address + 0x16
        cml_samples_count_address = cml_base_address + 0x18
        cml_samples_address = 0x20000 + 0x4000 * cml_block_index
        self.write_template(
            "evr-template-output-cml",
            variables={
                "OUTPUT_CML_CTRL_ADDR": self.render_address(
                    cml_control_address
                ),
                "OUTPUT_CML_HP_ADDR": self.render_address(
                    cml_high_period_address
                ),
                "OUTPUT_CML_LP_ADDR": self.render_address(
                    cml_low_period_address
                ),
                "OUTPUT_CML_PATTERN_00_ADDR": self.render_address(
                    cml_pattern_00_address
                ),
                "OUTPUT_CML_PATTERN_01_ADDR": self.render_address(
                    cml_pattern_01_address
                ),
                "OUTPUT_CML_PATTERN_10_ADDR": self.render_address(
                    cml_pattern_10_address
                ),
                "OUTPUT_CML_PATTERN_11_ADDR": self.render_address(
                    cml_pattern_11_address
                ),
                "OUTPUT_CML_SAMPLES_COUNT_ADDR": self.render_address(
                    cml_samples_count_address
                ),
                "OUTPUT_CML_SAMPLES_ADDR": self.render_address(
                    cml_samples_address
                ),
                "OUTPUT_DESCRIPTION": output_description,
                "OUTPUT_NAME": output_name,
            },
        )
        self.add_to_write_all_pvs_list(f"Intrnl:WriteAll:{output_name}:CML")

    def _output_fine_delay(
        self,
        output_name: str,
        output_description: str,
        fine_delay_available_macro: str,
        fine_delay_index: int,
    ) -> None:
        # There cannot be more than 16 fine delays. This limits is due to the
        # fact that the GPIO registers only have 32 bits, and each shift
        # register (controlling the fine delay of two outputs each) needs GPIO
        # lines and thus four bits.
        if fine_delay_index > 15:
            raise ValueError(f"Invalid fine-delay index: {fine_delay_index}")
        # The shift register controlling the first two fine delays is reached
        # through the first four GPIO lines, the shift register controlling the
        # next two fine delays through the next four GPIO lines, etc.
        #
        # Therefore, we have to shift the address of the GPIO registers by four
        # bits for each increment of two in the fine delay index. Each shift
        # register controls the fine delay ouf two outputs, so the shift for an
        # odd index is the same as for the preceding even index.
        output_fd_addr_bit_shift = 4 * (fine_delay_index // 2)
        # If the fine-delay index is even, we do not have to shift the
        # calculated value before writing it to the record. If it is odd, we
        # have to let-shift the value by 16 bits to affect the second output
        # on the shift register instead of the first one.
        output_fd_val_bit_shift = "<<16" if fine_delay_index % 2 else ""
        self.write_template(
            "evr-template-output-fine-delay",
            variables={
                "OUTPUT_DESCRIPTION": output_description,
                "OUTPUT_FD_ADDR_BIT_SHIFT": str(output_fd_addr_bit_shift),
                "OUTPUT_FD_AVAILABLE_MACRO": fine_delay_available_macro,
                "OUTPUT_FD_VAL_BIT_SHIFT": output_fd_val_bit_shift,
                "OUTPUT_NAME": output_name,
            },
        )
        self.add_to_write_all_pvs_list(f"{output_name}:FineDelay")

    def _output_gtx_logic(  # pylint: disable=too-many-locals
        self,
        output_name: str,
        output_description: str,
        gtx_block_index: int,
    ) -> None:
        gtx_base_address = 0x0600 + 0x0020 * gtx_block_index
        gtx_samples_base_address = 0x20000 + 0x4000 * gtx_block_index
        gtx_control_address = gtx_base_address + 0x10
        gtx_high_period_address = gtx_base_address + 0x14
        gtx_low_period_address = gtx_base_address + 0x16
        gtx_samples_count_address = gtx_base_address + 0x18
        gtx_samples_00_lword_address = gtx_samples_base_address
        gtx_samples_00_hword_address = gtx_samples_base_address + 0x04
        gtx_samples_01_lword_address = gtx_samples_base_address + 0x08
        gtx_samples_01_hword_address = gtx_samples_base_address + 0x0C
        gtx_samples_10_lword_address = gtx_samples_base_address + 0x10
        gtx_samples_10_hword_address = gtx_samples_base_address + 0x14
        gtx_samples_11_lword_address = gtx_samples_base_address + 0x18
        gtx_samples_11_hword_address = gtx_samples_base_address + 0x1C
        gtx_samples_exclusive_address = gtx_samples_base_address + 0x20
        self.write_template(
            "evr-template-output-gtx",
            variables={
                "OUTPUT_GTX_CTRL_ADDR": self.render_address(
                    gtx_control_address
                ),
                "OUTPUT_DESCRIPTION": output_description,
                "OUTPUT_GTX_HP_ADDR": self.render_address(
                    gtx_high_period_address
                ),
                "OUTPUT_GTX_LP_ADDR": self.render_address(
                    gtx_low_period_address
                ),
                "OUTPUT_NAME": output_name,
                "OUTPUT_GTX_SAMPLES_00_HWORD_ADDR": self.render_address(
                    gtx_samples_00_hword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_00_LWORD_ADDR": self.render_address(
                    gtx_samples_00_lword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_01_HWORD_ADDR": self.render_address(
                    gtx_samples_01_hword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_01_LWORD_ADDR": self.render_address(
                    gtx_samples_01_lword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_10_HWORD_ADDR": self.render_address(
                    gtx_samples_10_hword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_10_LWORD_ADDR": self.render_address(
                    gtx_samples_10_lword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_11_HWORD_ADDR": self.render_address(
                    gtx_samples_11_hword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_11_LWORD_ADDR": self.render_address(
                    gtx_samples_11_lword_address, 5
                ),
                "OUTPUT_GTX_SAMPLES_COUNT_ADDR": self.render_address(
                    gtx_samples_count_address
                ),
                "OUTPUT_GTX_SAMPLES_EXCLUSIVE_ADDR": self.render_address(
                    gtx_samples_exclusive_address
                ),
            },
        )
        self.add_to_write_all_pvs_list(f"Intrnl:WriteAll:{output_name}:GTX")

    def _prescaler(
        self, prescaler_number: int, config: "PrescalerConfig"
    ) -> None:
        address = 0x0100 + 4 * prescaler_number
        map_to_pulse_generator_address = 0x0140 + 4 * prescaler_number
        write_all_pvs = []
        self.write_template(
            f"evr-template-prescaler-{config.size_in_bits}-bit",
            variables={
                "PRESCALER_ADDR": self.render_address(address),
                "PRESCALER_NUM": str(prescaler_number),
            },
        )
        write_all_pvs.append(f"Prescaler{prescaler_number}")
        if self._config.feature_level == FeatureLevel.GEN2:
            for pulse_generator_index in range(
                0, len(self._config.pulse_generators)
            ):
                self.write_template(
                    "evr-template-prescaler-to-pulse-gen",
                    variables={
                        "PRESCALER_NUM": str(prescaler_number),
                        "PRESCALER_TO_PULSE_GEN_ADDR": self.render_address(
                            map_to_pulse_generator_address
                        ),
                        "PULSE_GEN_NUM": str(pulse_generator_index),
                    },
                )
                write_all_pvs.append(
                    f"Prescaler{prescaler_number}:MapTo:"
                    f"TrigPulseGen{pulse_generator_index}"
                )
        self.description(
            f"Prescaler{prescaler_number}:Description",
            f"Description for prescaler {prescaler_number}",
        )
        self.fanout(
            f"Intrnl:WriteAll:Prescaler{prescaler_number}", write_all_pvs
        )
        self.add_to_write_all_pvs_list(
            f"Intrnl:WriteAll:Prescaler{prescaler_number}"
        )

    def _pulse_generator(
        self, pulse_generator_number: int, config: "PulseGeneratorConfig"
    ) -> None:
        base_address = 0x0200 + 16 * pulse_generator_number
        control_address = base_address
        prescaler_address = base_address + 0x04
        delay_address = base_address + 0x08
        width_address = base_address + 0x0C
        write_all_pvs = []
        self.write_template(
            "evr-template-pulse-gen-generic",
            variables={
                "PULSE_GEN_CTRL_ADDR": self.render_address(control_address),
                "PULSE_GEN_NUM": str(pulse_generator_number),
            },
        )
        write_all_pvs.append(
            f"Intrnl:WriteAll:PulseGen{pulse_generator_number}:Generic"
        )
        if config.prescaler_size_in_bits:
            self.write_template(
                (
                    "evr-template-pulse-gen-prescaler-"
                    f"{config.prescaler_size_in_bits}-bit"
                ),
                variables={
                    "PULSE_GEN_NUM": str(pulse_generator_number),
                    "PULSE_GEN_PRESCALER_ADDR": self.render_address(
                        prescaler_address
                    ),
                },
            )
            write_all_pvs.append(f"PulseGen{pulse_generator_number}:Prescaler")
        self.write_template(
            f"evr-template-pulse-gen-delay-{config.delay_size_in_bits}-bit",
            variables={
                "PULSE_GEN_DELAY_ADDR": self.render_address(delay_address),
                "PULSE_GEN_NUM": str(pulse_generator_number),
            },
        )
        write_all_pvs.append(f"PulseGen{pulse_generator_number}:Delay")
        self.write_template(
            f"evr-template-pulse-gen-width-{config.width_size_in_bits}-bit",
            variables={
                "PULSE_GEN_NUM": str(pulse_generator_number),
                "PULSE_GEN_WIDTH_ADDR": self.render_address(width_address),
            },
        )
        write_all_pvs.append(f"PulseGen{pulse_generator_number}:Width")
        self.description(
            f"PulseGen{pulse_generator_number}:Description",
            f"Description for pulse gen. {pulse_generator_number}",
        )
        self.fanout(
            f"Intrnl:WriteAll:PulseGen{pulse_generator_number}", write_all_pvs
        )
        self.add_to_write_all_pvs_list(
            f"Intrnl:WriteAll:PulseGen{pulse_generator_number}"
        )

    def _pulse_generators_header(self) -> None:
        self.write_template(
            "evr-template-pulse-gens-header",
            variables={
                "NUMBER_OF_PULSE_GENERATORS": str(
                    len(self._config.pulse_generators)
                )
            },
        )

    def _transition_board_output(
        self,
        output_number: int,
        output_config: "OutputConfig",
    ) -> None:
        tb_output_address = 0x0480 + 2 * output_number
        tb_module_first_output_num = output_number - output_number % 2
        tb_module_second_output_num = tb_module_first_output_num + 1
        self._output(
            f"TBOut{output_number}",
            f"TB univ. output {output_number}",
            tb_output_address,
            output_config,
            output_installed_macro=(
                f"TB_UNIV_OUT_{tb_module_first_output_num}_"
                f"{tb_module_second_output_num}_INSTALLED="
                "$(TB_UNIV_OUT_INSTALLED=0)"
            ),
            output_installed_description=(
                f"TB univ. output module {tb_module_first_output_num}/"
                f"{tb_module_second_output_num}"
            ),
        )

    def _universal_output(
        self,
        output_number: int,
        output_config: "OutputConfig",
    ) -> None:
        univ_output_address = 0x0440 + 2 * output_number
        univ_out_module_first_output_num = output_number - output_number % 2
        univ_out_module_second_output_num = (
            univ_out_module_first_output_num + 1
        )
        self._output(
            f"UnivOut{output_number}",
            f"Universal output {output_number}",
            univ_output_address,
            output_config,
            output_fine_delay_available_macro=(
                f"UNIV_OUT_{univ_out_module_first_output_num}_"
                f"{univ_out_module_second_output_num}_FD_AVAILABLE="
                "$(UNIV_OUT_FD_AVAILABLE=0)"
            ),
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

    def generate(self) -> None:
        """
        Generate all code needed for the specified device.
        """
        # Check that a supported device has been passed.
        if self._config.device.device_class != DeviceClass.EVR:
            raise ValueError(
                f"Cannot generate EVR code for {self._config.device}."
            )

        # Ensure that we start with a clean slate.
        self.clear_write_all_pvs_list()

        # Generate records, starting with the ones that are common to all
        # devices.
        self._common()

        # If the device supports it, generate the code for supporting delay
        # compensation.
        if self._config.feature_level == FeatureLevel.GEN2:
            self._delay_compensation()

        # Mapping RAMs.
        for mapping_ram_index in range(0, 2):
            self._mapping_ram(mapping_ram_index)

        # Interrupt handling. Which one we need depends on whether the device
        # is controlled through memory-mapped I/O or through the UDP/IP.
        if self._config.use_mmap:
            self._interrupts_mmap()
        else:
            self._interrupts_udp_ip()

        # Generate code that is common to all devices using a modern firmware.
        # TODO This code should probably be placed in _common().
        if self._config.feature_level == FeatureLevel.GEN2:
            self.write_template("evr-common-gen2")
            self.add_to_write_all_pvs_list(
                "EventClock:ClkCleanerPLL:BandwidthSel"
            )
            self.add_to_write_all_pvs_list("IRQ:DataBuffer:Enabled:000To1FF")
            self.add_to_write_all_pvs_list("IRQ:DataBuffer:Enabled:200To3FF")
            self.add_to_write_all_pvs_list("IRQ:DataBuffer:Enabled:400To5FF")
            self.add_to_write_all_pvs_list("IRQ:DataBuffer:Enabled:600To7FF")

        # Generate code common to both the VME-EVR-230 and VME-EVR-230RF.
        if (
            self._config.device.form_factor == FormFactor.VME
            and self._config.device.series == "230"
        ):
            self.write_template("evr-vme-230-common")

        # Generate code specific to the mTCA-EVR-300.
        if self._config.device == Device.MTCA_EVR_300:
            self.write_template("evr-mtca-300")
            self.add_to_write_all_pvs_list("IFB300:Enabled")

        # Prescalers.
        for prescaler_index, prescaler_config in enumerate(
            self._config.prescalers
        ):
            self._prescaler(prescaler_index, prescaler_config)

        # Mapping of distributed-bus bits to pulse generators (if supported).
        if self._config.feature_level == FeatureLevel.GEN2:
            self._dbus_bits_to_pulse_gens()

        # Pulse generators.
        self._pulse_generators_header()
        for pulse_generator_index, pulse_generator_config in enumerate(
            self._config.pulse_generators
        ):
            self._pulse_generator(
                pulse_generator_index, pulse_generator_config
            )

        # Front-panel outputs.
        for output_index, output_config in enumerate(
            self._config.front_panel_outputs
        ):
            if output_config is not None:
                self._front_panel_output(output_index, output_config)

        # Backplane outputs.
        for output_index, output_config in enumerate(
            self._config.backplane_outputs
        ):
            if output_config is not None:
                self._backplane_output(output_index, output_config)

        # Universal outputs.
        for output_index, output_config in enumerate(
            self._config.universal_outputs
        ):
            if output_config is not None:
                self._universal_output(output_index, output_config)

        # Transition-board outputs.
        for output_index, output_config in enumerate(
            self._config.transition_board_outputs
        ):
            if output_config is not None:
                self._transition_board_output(output_index, output_config)

        # Front-panel inputs.
        for input_index in range(0, self._config.front_panel_inputs):
            self._front_panel_input(input_index)

        # SFP module (there only is a single one, so we do not include a
        # number).
        self.sfp_module("SFP", 0x8200)

        # Generate record that links to all output records.
        self.fanout_for_write_all_pvs("Intrnl:WriteAll")


@dataclasses.dataclass
class EVRConfig:  # pylint: disable=too-many-instance-attributes
    """
    Configuration for an EVR device.

    Contains information about the registers provided for the specific device
    type.
    """

    # Device to which this configuration applies.
    device: Device

    # Feature level supported by the device.
    feature_level: "FeatureLevel"

    # Prescaler configurations.
    prescalers: typing.List["PrescalerConfig"]

    # Pulse generator configurations.
    pulse_generators: typing.List["PulseGeneratorConfig"]

    # Tells whether the memory-mapped interface to the device is used.
    #
    # This is important because interrupt handling is implemented differently
    # for such devices when compared to devices that are controlled through
    # UDP/IP.
    use_mmap: bool

    # Number of backplane inputs.
    backplane_inputs: int = 0

    # Configuration for backplane outputs.
    backplane_outputs: typing.List[typing.Optional["OutputConfig"]] = (
        dataclasses.field(default_factory=list)
    )

    # Number of front-panel inputs.
    front_panel_inputs: int = 0

    # Configuration for front-panel outputs.
    front_panel_outputs: typing.List[typing.Optional["OutputConfig"]] = (
        dataclasses.field(default_factory=list)
    )

    # Configuration for transition-board inputs.
    transition_board_outputs: typing.List[typing.Optional["OutputConfig"]] = (
        dataclasses.field(default_factory=list)
    )

    # Number of universal inputs.
    universal_inputs: int = 0

    # Configuration for universal outputs.
    universal_outputs: typing.List[typing.Optional["OutputConfig"]] = (
        dataclasses.field(default_factory=list)
    )


class FeatureLevel(enum.Enum):
    """
    Feature level supported by a device.

    This can depend both on the hardware revision and firmware version
    installed on the hardware.
    """

    # Feature level that represents legacy firmware versions (00##) like they
    # are used for the old *-EVR-230 devices and the cPCI-EVR-300,
    # cPCI-EVRTG-300, and cRIO-EVR-300.
    GEN1 = 1

    # Feature level that represents modern firmware versions (02##) like they
    # are used for most -300 series devices (e.g. mTCA-EVR-300, PCIe-EVR-300DC,
    # and VME-EVR-300).
    #
    # Strictly speaking only firmware versions 0206 and newer are supported,
    # due to some address layout changes.
    GEN2 = 2


@dataclasses.dataclass
class OutputConfig:  # pylint: disable=too-many-instance-attributes
    """
    Configuration for an output.
    """

    # Description for the output in the CML logic.
    #
    # This is only used if `cml_block_index` is not ``None``.
    #
    # If not specified, the regular ``description`` is used.
    cml_block_description: typing.Optional[str] = None

    # Index of the CML logic block for the output.
    #
    # A value of ``None`` means that the output does not support CML logic.
    cml_block_index: typing.Optional[int] = None

    # Description for the output.
    #
    # If not specified, it is derived from the place where it is defined (e.g.
    # front-panel, transition board, etc.) and the index within this group of
    # outputs.
    description: typing.Optional[str] = None

    # Macro expression uses as the value of the “fine-delay available” record.
    #
    # Only used when `fine_delay_index` is nont ``None``.
    fine_delay_available_macro: typing.Optional[str] = None

    # Description for the output in the fine-delay records.
    #
    # This is only used if `fine_delay_index` is not ``None``.
    #
    # If not specified, the regular ``description`` is used.
    fine_delay_description: typing.Optional[str] = None

    # Index of the GPIO logic for setting the fine delay.
    #
    # A value of ``0`` means that the first four GPIO pins are connected to the
    # shift register for this output and that it is the first output (A) on
    # that shift register. A value of ``1`` means that the same GPIO pins and
    # thus shift register is used, but that it is the second output (B) on that
    # shift register. A value of ``2`` means that the next four GPIO pins are
    # connected to the shift register and that it is the first output (A) on
    # that shift register, etc.
    #
    # A value of ``None`` means that the output does not support setting a fine
    # delay through the GPIO pins.
    fine_delay_index: typing.Optional[int] = None

    # Description for the output in the GTX logic.
    #
    # This is only used if `gtx_block_index` is not ``None``.
    #
    # If not specified, the regular ``description`` is used.
    gtx_block_description: typing.Optional[str] = None

    # Index of the GTX logic block for the output.
    #
    # A value of ``None`` means that the output does not support GTX logic.
    gtx_block_index: typing.Optional[int] = None

    # Description of the output for the “installed” record.
    #
    # Only used when `optional` is ``True``.
    installed_description: typing.Optional[str] = None

    # Macro expression uses as the value of the “installed” record.
    #
    # Only used when `optional` is ``True``.
    installed_macro: typing.Optional[str] = None

    # Name of the output.
    #
    # If not specified, it is derived from the place where it is defined (e.g.
    # front-panel, transition board, etc.) and the index within this group of
    # outputs.
    name: typing.Optional[str] = None

    # Tells whether the output is optional.
    #
    # An output that is optional might be installed on a device or not,
    # typically in the form of a universal output module. A vale of ``None``
    # means that a sensible default based on the type of output is used.
    #
    # If ``True``, ``installed_description`` and ``installed_macro`` must not
    # be ``None``.
    optional: typing.Optional[bool] = None


@dataclasses.dataclass
class PrescalerConfig:
    """
    Configuration for a prescaler.

    Contains information about the register widths for a prescaler.
    """

    # Size of the prescaler value in bits.
    size_in_bits: int


@dataclasses.dataclass
class PulseGeneratorConfig:
    """
    Configuration for a pulse generator.

    Contains information about the register widths for a pulse generator.
    """

    # Size of the pulse delay value in bits.
    delay_size_in_bits: int

    # Size of the prescaler value in bits.
    prescaler_size_in_bits: int

    # Size of the pulse width in bits.
    width_size_in_bits: int


_PRESCALER_16 = PrescalerConfig(16)
_PRESCALER_32 = PrescalerConfig(32)

_PULSE_GENERATOR_32_0_16 = PulseGeneratorConfig(
    delay_size_in_bits=32, prescaler_size_in_bits=0, width_size_in_bits=16
)

_PULSE_GENERATOR_32_16_32 = PulseGeneratorConfig(
    delay_size_in_bits=32, prescaler_size_in_bits=16, width_size_in_bits=32
)

_PULSE_GENERATOR_32_8_16 = PulseGeneratorConfig(
    delay_size_in_bits=32, prescaler_size_in_bits=8, width_size_in_bits=16
)

_OUTPUT_DEFAULT = OutputConfig()

_EVR_CONFIGS = {
    Device.MTCA_EVR_300: EVRConfig(
        backplane_outputs=[
            OutputConfig(description="Backplane output RX17"),
            OutputConfig(description="Backplane output TX17"),
            OutputConfig(description="Backplane output RX18"),
            OutputConfig(description="Backplane output TX18"),
            OutputConfig(description="Backplane output RX19"),
            OutputConfig(description="Backplane output TX19"),
            OutputConfig(description="Backplane output RX20"),
            OutputConfig(description="Backplane output TX20"),
        ],
        device=Device.MTCA_EVR_300,
        feature_level=FeatureLevel.GEN2,
        front_panel_inputs=2,
        front_panel_outputs=([_OUTPUT_DEFAULT] * 4),
        prescalers=([_PRESCALER_32] * 8),
        pulse_generators=(
            [_PULSE_GENERATOR_32_16_32] * 4 + [_PULSE_GENERATOR_32_0_16] * 12
        ),
        # The device does not have regular universal outputs, but the registers
        # of universal output 16 and 17 are mapped to the backplane outputs
        # that are connected to TCLKA and TCLKB respectively. Those outputs
        # also provide GTX logic.
        universal_outputs=(
            [None] * 16
            + [
                OutputConfig(
                    description="Backplane output TCLKA",
                    gtx_block_description="Bp. out. TCLKA",
                    gtx_block_index=0,
                    name="BPOut8",
                    optional=False,
                ),
                OutputConfig(
                    description="Backplane output TCLKB",
                    gtx_block_description="Bp. out. TCLKB",
                    gtx_block_index=1,
                    name="BPOut9",
                    optional=False,
                ),
            ]
        ),
        use_mmap=True,
    ),
    Device.VME_EVR_230: EVRConfig(
        device=Device.VME_EVR_230,
        feature_level=FeatureLevel.GEN1,
        front_panel_inputs=2,
        front_panel_outputs=([_OUTPUT_DEFAULT] * 8),
        prescalers=([_PRESCALER_16] * 3),
        pulse_generators=(
            [_PULSE_GENERATOR_32_8_16] * 4 + [_PULSE_GENERATOR_32_0_16] * 12
        ),
        transition_board_outputs=([_OUTPUT_DEFAULT] * 16),
        universal_outputs=[
            OutputConfig(fine_delay_index=0),
            OutputConfig(fine_delay_index=1),
            OutputConfig(fine_delay_index=2),
            OutputConfig(fine_delay_index=3),
        ],
        use_mmap=False,
    ),
    Device.VME_EVR_230RF: EVRConfig(
        device=Device.VME_EVR_230RF,
        feature_level=FeatureLevel.GEN1,
        front_panel_inputs=2,
        front_panel_outputs=typing.cast(
            typing.List[typing.Optional[OutputConfig]],
            (
                [_OUTPUT_DEFAULT] * 4
                + [
                    OutputConfig(
                        cml_block_description="FP output 4", cml_block_index=0
                    ),
                    OutputConfig(
                        cml_block_description="FP output 5", cml_block_index=1
                    ),
                    OutputConfig(
                        cml_block_description="FP output 6", cml_block_index=2
                    ),
                ]
            ),
        ),
        prescalers=([_PRESCALER_16] * 3),
        pulse_generators=(
            [_PULSE_GENERATOR_32_8_16] * 4 + [_PULSE_GENERATOR_32_0_16] * 12
        ),
        transition_board_outputs=([_OUTPUT_DEFAULT] * 16),
        universal_outputs=[
            OutputConfig(fine_delay_index=0),
            OutputConfig(fine_delay_index=1),
            OutputConfig(fine_delay_index=2),
            OutputConfig(fine_delay_index=3),
        ],
        use_mmap=False,
    ),
    Device.VME_EVR_300: EVRConfig(
        device=Device.VME_EVR_300,
        feature_level=FeatureLevel.GEN2,
        front_panel_inputs=2,
        front_panel_outputs=(
            [
                OutputConfig(
                    gtx_block_description="FP output 0", gtx_block_index=2
                ),
                OutputConfig(
                    gtx_block_description="FP output 1", gtx_block_index=3
                ),
            ]
        ),
        prescalers=([_PRESCALER_32] * 8),
        pulse_generators=(
            [_PULSE_GENERATOR_32_16_32] * 4 + [_PULSE_GENERATOR_32_0_16] * 12
        ),
        transition_board_outputs=([_OUTPUT_DEFAULT] * 16),
        # We need to use a cast here, because the type checker detected the
        # type as List[OutputConfig], which is not compatiable with
        # List[Optional[OutputConfig]].
        universal_outputs=typing.cast(
            typing.List[typing.Optional[OutputConfig]],
            (
                [
                    OutputConfig(fine_delay_index=0),
                    OutputConfig(fine_delay_index=1),
                    OutputConfig(fine_delay_index=2),
                    OutputConfig(fine_delay_index=3),
                    OutputConfig(fine_delay_index=4),
                    OutputConfig(fine_delay_index=5),
                ]
                + [
                    OutputConfig(
                        fine_delay_index=6,
                        gtx_block_description="Univ. output 6",
                        gtx_block_index=0,
                    ),
                    OutputConfig(
                        fine_delay_index=7,
                        gtx_block_description="Univ. output 7",
                        gtx_block_index=1,
                    ),
                ]
            ),
        ),
        use_mmap=False,
    ),
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
        config = _EVR_CONFIGS[device]
    except KeyError:
        raise ValueError(  # pylint: disable=raise-missing-from
            f"Unsupported device: {device}"
        )
    EVR(
        file_type,
        output_file,
        config,
        description_record_type=desciption_record_type,
    ).generate()
