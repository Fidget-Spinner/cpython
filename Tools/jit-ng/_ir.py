"""Utilities for invoking LLVM tools."""

import asyncio
import functools
import os
import re
import shlex
import subprocess
import typing

import _targets

_IR_PATH = "../ir/ir"

_P = typing.ParamSpec("_P")
_R = typing.TypeVar("_R")
_C = typing.Callable[_P, typing.Awaitable[_R]]




_CORES = asyncio.BoundedSemaphore(os.cpu_count() or 1)


async def _run(args: typing.Iterable[str], echo: bool = False) -> str | None:
    command = [_IR_PATH, *args]
    async with _CORES:
        if echo:
            print(shlex.join(command))
        try:
            process = await asyncio.create_subprocess_exec(
                *command, stdout=subprocess.PIPE
            )
        except FileNotFoundError:
            return None
        out, err = await process.communicate()
    if process.returncode:
        raise RuntimeError(f"IR exited with return code {process.returncode} {err}")
    return out.decode()

async def run(args: typing.Iterable[str], echo: bool = False) -> str:
    """Run an LLVM tool if it can be found. Otherwise, raise RuntimeError."""
    output = await _run(args, echo=echo)
    if output is None:
        raise RuntimeError(f"Can't find IR executable!")
    return output
