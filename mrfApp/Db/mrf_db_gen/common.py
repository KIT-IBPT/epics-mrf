"""
Code shared by the different device classes.
"""

import typing

from .constants import DescriptionRecordType, FileType
from .template import render_template


class MRFCommon:
    """
    Common base class for EVG, EVM, and EVR.

    Args:
        file_type: Type of the file that is being generated.
        output_file: file object to which the output is written.
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
        *,
        address_offset: int = 0,
        description_record_type: DescriptionRecordType = (
            DescriptionRecordType.LSO
        ),
        pv_prefix: str = "$(P)$(R)",
    ):
        self._address_offset = address_offset
        self._description_record_type = description_record_type
        self._file_type = file_type
        self._output_file = output_file
        self._pv_prefix = pv_prefix
        self._write_all_pvs: typing.List[str] = []

    def _addr_in_template(self, argument: str) -> str:
        arguments = [arg.strip() for arg in argument.split(",")]
        if len(arguments) == 1:
            return self.render_address(int(arguments[0], 0))
        if len(arguments) == 2:
            return self.render_address(
                int(arguments[0], 0), int(arguments[1], 0)
            )
        raise ValueError(
            f"Invalid argument to @ADDR(...)@ function: {argument}"
        )

    def add_to_write_all_pvs_list(self, pv_name: str) -> None:
        """
        Add the specified PV name to the list of “write all” PVs.

        This has the effect that the specified PV is included as a link target
        when calling `write_all_pvs`. The PV prefix is automatically prepended
        to the specified PV name.

        Args:
          pv_name: name of the PV that shall be added to the list.
        """
        self._write_all_pvs.append(pv_name)

    def clear_write_all_pvs_list(self) -> None:
        """
        Clear all the PVs that were added through ``add_write_all_pvs_list``
        without writing anything to the output file.
        """
        self._write_all_pvs.clear()

    def description(self, pv_name: str, description: str):
        """
        Generate code needed for a description record.

        A description record does not have any direct use in the device
        support. It is only provided, so that the user can annotate the
        configuration and have these annotations stored with Autosave files.

        Args:
            pv_name: Name of the record for which the code is generated. The PV
                prefix is prepended to this name.
            description: Description that is set for the generated record.
        """
        self.write_template(
            f"common-description-{self._description_record_type.value}",
            variables={
                "DESCRIPTION": description,
                "PV_NAME": pv_name,
            },
        )

    def fanout(
        self, pv_name: str, target_pv_names: typing.Sequence[str]
    ) -> None:
        """
        Generate a ``fanout`` record with the specified name.

        The generated record links to all the specified target records, using
        multiple ``fanout`` records if necessary.

        The PV prefix is automatically prepended to both the ``pv_name`` and
        ``target_pv_names``.

        This function does not do anything when operating in Autosave request
        file mode.

        Args:
            pv_name: Name used for the generated fanout record. Additional
                records that are generated if there are more than six target
                PVs use the same name with ``:Fout<X>`` appended, where
                ``<X>`` is a number that is incremented for each generated
                record.
            target_pv_names: List of PV names to which the generated record
                shall link.
        """
        if self._file_type == FileType.REQ:
            return
        fanout_index = 0
        link_index = 1
        pv_name = self._pv_prefix + pv_name
        self.write(f'record(fanout, "{pv_name}") {{\n')
        for target_pv_name in target_pv_names:
            target_pv_name = self._pv_prefix + target_pv_name
            # The fanout record only has six target links, so when we hit the
            # seventh target, we have to generate an additional record.
            if link_index == 7:
                fanout_index += 1
                link_index = 1
                self.write(f'  field(FLNK, "{pv_name}:Fout{fanout_index}")\n')
                self.write("}\n")
                self.write("\n")
                self.write(
                    f'record(fanout, "{pv_name}:Fout{fanout_index}") {{\n'
                )
            self.write(f'  field(LNK{link_index}, "{target_pv_name}")\n')
            link_index += 1
        self.write("}\n")
        self.write("\n")

    def fanout_for_write_all_pvs(self, pv_name: str) -> None:
        """
        Generate a `fanout` record (or series of `fanout` records) that links
        to all PVs that have previously been added using
        `add_to_write_all_pvs_list`. As a side effect, the internal list of PVs
        is cleared, so that subsequent calls to this function do not include the
        same target PVs in a newly generated record.

        The PV prefix is automatically prepended to the ``pv_name``.

        This function does not do anything when operating in Autosave request
        file mode.

        Args:
            pv_name: Name used for the generated fanout record. Additional
                records that are generated if there are more than six target
                PVs use the same name with ``:Fout<X>`` appended, where
                ``<X>`` is a number that is incremented for each generated
                record.

        Returns:
            Record definitions or the empty string, depending on the file type
            that is set.
        """
        self.fanout(pv_name, self._write_all_pvs)
        self._write_all_pvs.clear()

    def render_address(self, address: int, width: int = 4) -> str:
        """
        Convert the specified address to its hexadecimal representation.

        Args:
            addres: address that shall be converted. The address offset is
                automatically added to the specified address before rendering
                it.
            width: minimum number of digits that shall be rendered. The
                resulting string is two characters longer than that due to the
                “0x” prefix.
        """
        result = hex(self._address_offset + address)
        number_of_padding_zeros = width + 2 - len(result)
        if number_of_padding_zeros <= 0:
            return result
        return result[0:2] + "0" * number_of_padding_zeros + result[2:]

    def render_template(
        self,
        template_name: str,
        *,
        functions: typing.Optional[
            typing.Mapping[str, typing.Callable[[str], str]]
        ] = None,
        variables: typing.Optional[typing.Mapping[str, str]] = None,
    ) -> str:
        """
        Render a template, replacing the specified functions and variables in
        the template.

        Args:
            template_name: Name of the template file, without the `.inc` and
                type specific suffix.
            functions: Mapping from function names to the functions that shall
                be called when a corresponding string is found in the template.
                In the template, a place where a function call shall be
                substituted is written as ``@FUNCTION_NAME(ARGUMENT)@``, where
                ``FUNCTION_NAME`` is the key used in this mapping and
                ``ARGUMENT`` is the string that is passed to the function as an
                argument. The argument string must not include a closing
                parenthesis. The ``ADDR`` function is automatically added to
                this mapping.
            variables: Mapping from variable names to values that shall be
                replaced in the template. In the template, a place where a
                variable shall be substituted is written as
                ``@VARIABLE_NAME@``, where ``VARIABLE_NAME`` is the key that is
                used in this mapping. The ``PV_PREFIX`` variable is
                automatically added to this mapping.

        Returns:
            Result of rendering the template.

        Raises:
            OSError: if the template file cannot be loaded.
        """
        if functions is None:
            actual_functions = {}
        elif isinstance(functions, dict):
            actual_functions = functions.copy()
        else:
            actual_functions = {**functions}
        actual_functions["ADDR"] = self._addr_in_template
        if variables is None:
            actual_variables = {}
        elif isinstance(variables, dict):
            actual_variables = variables.copy()
        else:
            actual_variables = {**variables}
        actual_variables["PV_PREFIX"] = self._pv_prefix
        return render_template(
            template_name,
            self._file_type,
            functions=actual_functions,
            variables=actual_variables,
        )

    def sfp_module(self, sfp_name: str, sfp_base_address: int) -> None:
        """
        Return code needed for an SFP module.

        Args:
            sfp_name: Name SFP module (e.g. ``SFP0``). This is used as part of
                the record names.
            sfp_base_address: Base address of the registers for the SFP module.
                The address offset is added to this address.

        Returns:
            Record definitions or the respective PV name for the Autosave
            request file, depending on the file type that is set.
        """

        def sfp_addr(argument: str) -> str:
            arguments = [arg.strip() for arg in argument.split(",")]
            if len(arguments) == 1:
                return self.render_address(
                    sfp_base_address + int(arguments[0], 0)
                )
            if len(arguments) == 2:
                return self.render_address(
                    sfp_base_address + int(arguments[0], 0),
                    int(arguments[1], 0),
                )
            raise ValueError(
                f"Invalid argument to @SFP_ADDR(...)@ function: {argument}"
            )

        self.write_template(
            "common-sfp",
            functions={"SFP_ADDR": sfp_addr},
            variables={"SFP_NAME": sfp_name},
        )

    def write(self, output: str) -> None:
        """
        Write the specified string to the output file.

        Args:
            output: string that shall be written.

        Raises:
            OSError: if the string cannot be written to the file.
        """
        self._output_file.write(output)

    def write_template(
        self,
        template_name: str,
        *,
        functions: typing.Optional[
            typing.Mapping[str, typing.Callable[[str], str]]
        ] = None,
        variables: typing.Optional[typing.Mapping[str, str]] = None,
    ) -> None:
        """
        Write result of rendering a template to the output file.

        The the specified functions and variables are replaced the template
        before writing it to the output file.

        Args:
            template_name: Name of the template file, without the `.inc` and
                type specific suffix.
            functions: Mapping from function names to the functions that shall
                be called when a corresponding string is found in the template.
                In the template, a place where a function call shall be
                substituted is written as ``@FUNCTION_NAME(ARGUMENT)@``, where
                ``FUNCTION_NAME`` is the key used in this mapping and
                ``ARGUMENT`` is the string that is passed to the function as an
                argument. The argument string must not include a closing
                parenthesis. The ``ADDR`` function is automatically added to
                this mapping.
            variables: Mapping from variable names to values that shall be
                replaced in the template. In the template, a place where a
                variable shall be substituted is written as
                ``@VARIABLE_NAME@``, where ``VARIABLE_NAME`` is the key that is
                used in this mapping. The ``PV_PREFIX`` variable is
                automatically added to this mapping.

        Raises:
            OSError: if the template file cannot be loaded or the output cannot
                be written.
        """
        self.write(
            self.render_template(
                template_name, functions=functions, variables=variables
            )
        )
