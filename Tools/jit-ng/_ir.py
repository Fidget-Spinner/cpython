"""Utilities for invoking LLVM tools."""

import asyncio
import functools
import os
import re
import shlex
import subprocess
import typing

import _targets

IR_JIT_FRAMEWORK_PATH = "../ir/ir"

_CORES = asyncio.BoundedSemaphore(os.cpu_count() or 1)


async def _run(args: typing.Iterable[str], echo: bool = False) -> str | None:
    command = [IR_JIT_FRAMEWORK_PATH, *args]
    async with _CORES:
        if echo:
            print(shlex.join(command))
        try:
            process = await asyncio.create_subprocess_exec(
                *command, stdout=subprocess.PIPE
            )
        except FileNotFoundError:
            return None
        out, _ = await process.communicate()
    if process.returncode:
        raise RuntimeError(f"{IR_JIT_FRAMEWORK_PATH} exited with return code {process.returncode}")
    return out.decode()


async def run(args: typing.Iterable[str], echo: bool = False) -> str:
    """Run an LLVM tool if it can be found. Otherwise, raise RuntimeError."""
    await _run(args, echo=echo)
