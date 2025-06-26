"""
Entrypoint when running this module from the command-line.
"""

import contextlib
import asyncio

from . import main


async def _main() -> None:
    # We expect a CancelledError because this is the only way how this
    # co-routine can be stopped. As the program stops right after, we do not
    # want this exception to bubble up and cause a stack trace to be printed,
    # so we ignore it.
    with contextlib.suppress(asyncio.CancelledError):
        await main()


if __name__ == "__main__":
    asyncio.run(_main())
