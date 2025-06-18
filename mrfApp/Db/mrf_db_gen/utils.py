"""
General utilities.
"""

import typing


T = typing.TypeVar("T")


def get_optional_or_default(
    optional_value: typing.Optional[T], default_value: T
) -> T:
    """
    Return an optional value if it is set or a default value otherwise.

    Args:
        optional_value: value that may be ``None``.
        dault_value: value that is returned if ``optional_value`` is ``None``.

    Returns:
        ``optional_value`` if not ``None``, ``default_value`` otherwise.

    """
    return default_value if optional_value is None else optional_value
