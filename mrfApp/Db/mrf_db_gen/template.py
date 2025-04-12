"""
``.db`` and ``.req`` file templates.
"""

import functools
import pathlib
import re
import typing

from .constants import FileType


class _Template:  # pylint: disable=too-few-public-methods

    def __init__(
        self,
        template_str: str,
        function_names: typing.Set[str],
        variable_names: typing.Set[str],
    ):
        function_names_regexp = "|".join(
            re.escape(func_name) for func_name in function_names
        )
        pattern_str = "@(?:"
        if function_names:
            function_names_regexp = "|".join(
                re.escape(func_name) for func_name in function_names
            )
            pattern_str += (
                f"(?P<func_name>{function_names_regexp})"
                "\\((?P<func_arg>[^)]*)\\)"
            )
        if variable_names:
            variable_names_regexp = "|".join(
                re.escape(var_name) for var_name in variable_names
            )
            if function_names:
                pattern_str += "|"
            pattern_str += f"(?P<var_name>{variable_names_regexp})"
        pattern_str += ")@"
        self.template_pattern: typing.Optional[re.Pattern] = None
        if function_names or variable_names:
            self.template_pattern = re.compile(pattern_str)
        self.template_str = template_str

    def render(  # pylint: disable=missing-function-docstring
        self,
        functions: typing.Mapping[str, typing.Callable[[str], str]],
        variables: typing.Mapping[str, str],
    ) -> str:
        def match(match_obj: re.Match) -> str:
            match_groups = match_obj.groupdict()
            # When matching a function, we have two groups (one for the
            # function name and the other one for the argument). When matching
            # a variable, we only have a single group for the variable name.
            if "func_name" in match_groups and match_groups["func_name"]:
                return functions[match_groups["func_name"]](
                    match_groups["func_arg"]
                )
            if "var_name" in match_groups and match_groups["var_name"]:
                return variables[match_groups["var_name"]]
            # We should never get here because if neither a function nor a
            # variable matched, there should be no match at all.
            raise RuntimeError(f"Unexpected match: {match_obj}")

        if self.template_pattern is None:
            return self.template_str
        return re.sub(self.template_pattern, match, self.template_str)


def _get_template_dir() -> pathlib.Path:
    return pathlib.Path(__file__).parent.parent / "templates"


@functools.cache
def _load_template(
    template_name: str,
    file_type: FileType,
    function_names: typing.Set[str],
    variable_names: typing.Set[str],
) -> _Template:
    template_path = (
        _get_template_dir() / f"{template_name}.inc{file_type.value}"
    )
    with open(template_path, encoding="utf-8") as template_file:
        template_str = "".join(template_file.readlines())
    return _Template(template_str, function_names, variable_names)


def render_template(
    template_name: str,
    file_type: FileType,
    *,
    functions: typing.Optional[
        typing.Mapping[str, typing.Callable[[str], str]]
    ] = None,
    variables: typing.Optional[typing.Mapping[str, str]] = None,
) -> str:
    """
    Render a template, replacing the specified functions and variables in the
    template.

    Args:
        template_name: Name of the template file, without the `.inc` and type
            specific suffix.
        file_type: Type of the file that shall be rendered. This is used to
            generate the actual name of the template file.
        functions: Mapping from function names to the functions that shall be
            called when a corresponding string is found in the template. In the
            template, a place where a function call shall be substituted is
            written as ``@FUNCTION_NAME(ARGUMENT)@``, where ``FUNCTION_NAME``
            is the key used in this mapping and ``ARGUMENT`` is the string that
            is passed to the function as an argument. The argument string must
            not include a closing parenthesis.
        variables: Mapping from variable names to values that shall be replaced
            in the template. In the template, a place where a variable shall be
            substituted is written as ``@VARIABLE_NAME@``, where
            ``VARIABLE_NAME`` is the key that is used in this mapping.

    Returns:
        Result of rendering the template.

    Raises:
        IOError: if the template file cannot be loaded.
    """
    if functions is None:
        functions = {}
    if variables is None:
        variables = {}
    return _load_template(
        template_name,
        file_type,
        frozenset(functions.keys()),
        frozenset(variables.keys()),
    ).render(functions, variables)
