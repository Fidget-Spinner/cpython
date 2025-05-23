"""Target-specific code generation, parsing, and processing."""

import asyncio
import dataclasses
import hashlib
import json
import os
import pathlib
import re
import sys
import tempfile
import typing

import _llvm
import _stencils
import _writer
import _ir

if sys.version_info < (3, 11):
    raise RuntimeError("Building the JIT compiler requires Python 3.11 or newer!")

TOOLS_JIT_BUILD = pathlib.Path(__file__).resolve()
TOOLS_JIT = TOOLS_JIT_BUILD.parent
TOOLS = TOOLS_JIT.parent
CPYTHON = TOOLS.parent
EXTERNALS = CPYTHON / "externals"
PYTHON_EXECUTOR_CASES_C_H = CPYTHON / "Python" / "executor_cases.c.h"
TOOLS_JIT_TEMPLATE_C = TOOLS_JIT / "template.c"

ASYNCIO_RUNNER = asyncio.Runner()


@dataclasses.dataclass
class Target:

    stable: bool = False
    debug: bool = False
    pyconfig_dir: str = ""
    verbose: bool = False


    async def _parse(self, path: pathlib.Path) -> _stencils.Stencil:
        pass


    async def _compile(
        self, opname: str, c: pathlib.Path, tempdir: pathlib.Path
    ) -> _stencils.Stencil:
        ll = tempdir / f"{opname}.ll"
        ir = tempdir / f"{opname}.ir"
        args = [
            "-DPy_BUILD_CORE_MODULE",
            "-D_DEBUG" if self.debug else "-DNDEBUG",
            f"-D_JIT_OPCODE={opname}",
            "-D_PyJIT_ACTIVE",
            "-D_Py_JIT",
            f"-I{self.pyconfig_dir}",
            f"-I{CPYTHON / 'Include'}",
            f"-I{CPYTHON / 'Include' / 'internal'}",
            f"-I{CPYTHON / 'Include' / 'internal' / 'mimalloc'}",
            f"-I{CPYTHON / 'Python'}",
            f"-I{CPYTHON / 'Tools' / 'jit'}",
            "-O2",
            "-fno-vectorize",
            "-fno-slp-vectorize",
            "-c",
            # Shorten full absolute file paths in the generated code (like the
            # __FILE__ macro and assert failure messages) for reproducibility:
            f"-ffile-prefix-map={CPYTHON}=.",
            f"-ffile-prefix-map={tempdir}=.",
            # This debug info isn't necessary, and bloats out the JIT'ed code.
            # We *may* be able to re-enable this, process it, and JIT it for a
            # nicer debugging experience... but that needs a lot more research:
            "-fno-asynchronous-unwind-tables",
            # Don't call built-in functions that we can't find or patch:
            "-fno-builtin",
            # Emit relaxable 64-bit calls/jumps, so we don't have to worry about
            # about emitting in-range trampolines for out-of-range targets.
            # We can probably remove this and emit trampolines in the future:
            "-fno-plt",
            # Don't call stack-smashing canaries that we can't find or patch:
            "-fno-stack-protector",
            "-std=c11",
            "-emit-llvm",
            "-S",
            "-o",
            f"{ll}",
            f"{c}",
        ]
        await _llvm.run("clang", args, echo=self.verbose)
        ir_args = [
            "-fno-inline",
            "--llvm-asm",
            f"{ll}",
            "--save",
            f"{ir}"
        ]
        # assert False, c.read_text()
        await _ir.run(ir_args)
        assert False, ir.read_text()
        # return await self._parse(o)

    async def _build_stencils(self) -> dict[str, _stencils.Stencil]:
        generated_cases = PYTHON_EXECUTOR_CASES_C_H.read_text()
        cases_and_opnames = sorted(
            re.findall(
                r"\n {8}(case (\w+): \{\n.*?\n {8}\})", generated_cases, flags=re.DOTALL
            )
        )
        tasks = []
        with tempfile.TemporaryDirectory() as tempdir:
            work = pathlib.Path(tempdir).resolve()
            async with asyncio.TaskGroup() as group:
                # coro = self._compile("shim", TOOLS_JIT / "shim.c", work)
                # tasks.append(group.create_task(coro, name="shim"))
                template = TOOLS_JIT_TEMPLATE_C.read_text()
                for case, opname in cases_and_opnames:
                    if opname != "_BINARY_OP_ADD_INT":
                        continue
                    # Write out a copy of the template with *only* this case
                    # inserted. This is about twice as fast as #include'ing all
                    # of executor_cases.c.h each time we compile (since the C
                    # compiler wastes a bunch of time parsing the dead code for
                    # all of the other cases):
                    c = work / f"{opname}.c"
                    c.write_text(template.replace("CASE", case))
                    coro = self._compile(opname, c, work)
                    tasks.append(group.create_task(coro, name=opname))
        stencil_groups = {task.get_name(): task.result() for task in tasks}

        return stencil_groups

    def build(
        self,
        *,
        comment: str = "",
        force: bool = False,
        jit_stencils: pathlib.Path,
    ) -> None:
        """Build jit_stencils.h in the given directory."""
        jit_stencils.parent.mkdir(parents=True, exist_ok=True)
        if not self.stable:
            warning = f"JIT support is still experimental!"
            request = "Please report any issues you encounter.".center(len(warning))
            outline = "=" * len(warning)
            print("\n".join(["", outline, warning, request, outline, ""]))

        stencil_groups = ASYNCIO_RUNNER.run(self._build_stencils())
        jit_stencils_new = jit_stencils.parent / "jit_ng_stencils.h.new"
        try:
            with jit_stencils_new.open("w") as file:
                if comment:
                    file.write(f"// {comment}\n")
                file.write("\n")
                for line in _writer.dump(stencil_groups, self.known_symbols):
                    file.write(f"{line}\n")
            try:
                jit_stencils_new.replace(jit_stencils)
            except FileNotFoundError:
                # another process probably already moved the file
                if not jit_stencils.is_file():
                    raise
        finally:
            jit_stencils_new.unlink(missing_ok=True)

