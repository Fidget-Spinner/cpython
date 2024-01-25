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
import textwrap
import typing

import _jit_c
import _jit_defines_h
import _llvm
import _schema
import _supernode
import _stencils
import _template
import _writer

if sys.version_info < (3, 11):
    raise RuntimeError("Building the JIT compiler requires Python 3.11 or newer!")

TOOLS_JIT_BUILD = pathlib.Path(__file__).resolve()
TOOLS_JIT = TOOLS_JIT_BUILD.parent
TOOLS = TOOLS_JIT.parent
CPYTHON = TOOLS.parent
PYTHON_EXECUTOR_CASES_C_H = CPYTHON / "Python" / "executor_cases.c.h"
TOOLS_JIT_TEMPLATE_C = TOOLS_JIT / "template.c"


_S = typing.TypeVar("_S", _schema.COFFSection, _schema.ELFSection, _schema.MachOSection)
_R = typing.TypeVar(
    "_R", _schema.COFFRelocation, _schema.ELFRelocation, _schema.MachORelocation
)


@dataclasses.dataclass
class _Target(typing.Generic[_S, _R]):
    triple: str
    _: dataclasses.KW_ONLY
    alignment: int = 1
    prefix: str = ""
    debug: bool = False
    verbose: bool = False

    def _compute_digest(self, out: pathlib.Path) -> str:
        hasher = hashlib.sha256()
        hasher.update(self.triple.encode())
        hasher.update(self.alignment.to_bytes())
        hasher.update(self.prefix.encode())
        # These dependencies are also reflected in _JITSources in regen.targets:
        hasher.update(PYTHON_EXECUTOR_CASES_C_H.read_bytes())
        hasher.update((out / "pyconfig.h").read_bytes())
        for dirpath, _, filenames in sorted(os.walk(TOOLS_JIT)):
            for filename in filenames:
                hasher.update(pathlib.Path(dirpath, filename).read_bytes())
        return hasher.hexdigest()

    async def _parse(self, path: pathlib.Path) -> _stencils.StencilGroup:
        group = _stencils.StencilGroup()
        args = ["--disassemble", "--reloc", f"{path}"]
        output = await _llvm.maybe_run("llvm-objdump", args, echo=self.verbose)
        if output is not None:
            group.code.disassembly.extend(
                line.expandtabs().strip()
                for line in output.splitlines()
                if not line.isspace()
            )
        args = [
            "--elf-output-style=JSON",
            "--expand-relocs",
            # "--pretty-print",
            "--section-data",
            "--section-relocations",
            "--section-symbols",
            "--sections",
            f"{path}",
        ]
        output = await _llvm.run("llvm-readobj", args, echo=self.verbose)
        # --elf-output-style=JSON is only *slightly* broken on Mach-O...
        output = output.replace("PrivateExtern\n", "\n")
        output = output.replace("Extern\n", "\n")
        # ...and also COFF:
        output = output[output.index("[", 1, None) :]
        output = output[: output.rindex("]", None, -1) + 1]
        sections: list[dict[typing.Literal["Section"], _S]] = json.loads(output)
        for wrapped_section in sections:
            self._handle_section(wrapped_section["Section"], group)
        assert group.symbols["_JIT_ENTRY"] == (_stencils.HoleValue.CODE, 0)
        if group.data.body:
            line = f"0: {str(bytes(group.data.body)).removeprefix('b')}"
            group.data.disassembly.append(line)
        group.process_relocations()
        return group

    def _handle_section(self, section: _S, group: _stencils.StencilGroup) -> None:
        raise NotImplementedError(type(self))

    def _handle_relocation(
        self, base: int, relocation: _R, raw: bytes
    ) -> _stencils.Hole:
        raise NotImplementedError(type(self))

    async def _compile(
        self, c: pathlib.Path, tempdir: pathlib.Path, opnames: typing.Iterable[str]
    ) -> _stencils.StencilGroup:
        o = tempdir / f"{_superop_name(opnames)}.o"
        args = [
            f"--target={self.triple}",
            "-DPy_BUILD_CORE",
            "-D_DEBUG" if self.debug else "-DNDEBUG"]
        
        for i, op in enumerate(opnames):
            args.append(f"-D_JIT_OPCODE{i}={op}")

        args.extend([
            "-D_PyJIT_ACTIVE",
            "-D_Py_JIT",
            "-I.",
            f"-I{CPYTHON / 'Include'}",
            f"-I{CPYTHON / 'Include' / 'internal'}",
            f"-I{CPYTHON / 'Include' / 'internal' / 'mimalloc'}",
            f"-I{CPYTHON / 'Python'}",
            "-O3",
            "-c",
            "-fno-asynchronous-unwind-tables",
            # SET_FUNCTION_ATTRIBUTE on 32-bit Windows debug builds:
            "-fno-jump-tables",
            # Position-independent code adds indirection to every load and jump:
            "-fno-pic",
            # Don't make calls to weird stack-smashing canaries:
            "-fno-stack-protector",
            # We have three options for code model:
            # - "small": the default, assumes that code and data reside in the
            #   lowest 2GB of memory (128MB on aarch64)
            # - "medium": assumes that code resides in the lowest 2GB of memory,
            #   and makes no assumptions about data (not available on aarch64)
            # - "large": makes no assumptions about either code or data
            "-mcmodel=large",
            "-o",
            f"{o}",
            "-std=c11",
            f"{c}",
        ])
        await _llvm.run("clang", args, echo=self.verbose)
        return await self._parse(o)

    async def _build_stencils(self,  ops: typing.Iterable[typing.Iterable[str]] | None = None) -> dict[str, _stencils.StencilGroup]:
        generated_cases = PYTHON_EXECUTOR_CASES_C_H.read_text()
        opnames = sorted(re.findall(r"\n {8}case (\w+): \{\n", generated_cases))
        tasks = []
        with tempfile.TemporaryDirectory() as tempdir:
            work = pathlib.Path(tempdir).resolve()
            template_path = work / f"template_1.c"
            if not template_path.exists():
                _template.create_template_file(1, template_path)
            async with asyncio.TaskGroup() as group:
                for opname in opnames:
                    coro = self._compile(template_path, work, [opname,])
                    tasks.append(group.create_task(coro, name=opname))

        if ops:
            supernodes_stencils = await self._build_multiple_ops(ops)
        else:
            supernodes_stencils = []

        result = {task.get_name(): task.result() for task in tasks}
        if supernodes_stencils: result.update(supernodes_stencils)
        return result
    
    async def _build_multiple_ops(self, supernodes: typing.Iterable[_supernode.SuperNode]) -> dict[str, _stencils.StencilGroup]:
        tasks = []
        with tempfile.TemporaryDirectory() as tempdir:
            work = pathlib.Path(tempdir).resolve()
            async with asyncio.TaskGroup() as group:
                for node in supernodes:
                    template_path = work / f"template_{node.length}.c"
                    if not template_path.exists():
                        _template.create_template_file(node.length, template_path)
                    coro = self._compile(template_path, work, node.ops)
                    tasks.append(group.create_task(coro, name=node.name))
        return {task.get_name(): task.result() for task in tasks}

    def build(self, out: pathlib.Path, supernodes: list[_supernode.SuperNode] | None, force=False) -> None:
        """Build jit_stencils.h in the given directory."""
        digest = f"// {self._compute_digest(out)}\n"

        # Assign indices to supernodes here
        # Don't do it when they're loaded/created, to allow for more dynamic
        # customization later
        max_id = max_uop_id()
        for i, s in enumerate(supernodes):
            s.index = i + max_id + 1 

        _jit_c._patch_jit_c(supernodes)
        _jit_defines_h.create_jit_defines_h(supernodes)

        jit_stencils = out / "jit_stencils.h"
        # TODO make this check all touched files - jit_stencils, jit_defines, in future jit.c
        if force or not jit_stencils.exists() or not jit_stencils.read_text().startswith(digest):
            stencil_groups = asyncio.run(self._build_stencils(supernodes))
            with jit_stencils.open("w") as file:
                file.write(digest)
                max_id = max_uop_id()
                for line in _writer.dump(stencil_groups, supernodes=supernodes):
                    file.write(f"{line}\n")


class _ELF(
    _Target[_schema.ELFSection, _schema.ELFRelocation]
):  # pylint: disable = too-few-public-methods
    def _handle_section(
        self, section: _schema.ELFSection, group: _stencils.StencilGroup
    ) -> None:
        section_type = section["Type"]["Value"]
        flags = {flag["Name"] for flag in section["Flags"]["Flags"]}
        if section_type == "SHT_RELA":
            assert "SHF_INFO_LINK" in flags, flags
            assert not section["Symbols"]
            if section["Info"] in group.code.sections:
                stencil = group.code
            else:
                stencil = group.data
            base = stencil.sections[section["Info"]]
            for wrapped_relocation in section["Relocations"]:
                relocation = wrapped_relocation["Relocation"]
                hole = self._handle_relocation(base, relocation, stencil.body)
                stencil.holes.append(hole)
        elif section_type == "SHT_PROGBITS":
            if "SHF_ALLOC" not in flags:
                return
            if "SHF_EXECINSTR" in flags:
                value = _stencils.HoleValue.CODE
                stencil = group.code
            else:
                value = _stencils.HoleValue.DATA
                stencil = group.data
            stencil.sections[section["Index"]] = len(stencil.body)
            for wrapped_symbol in section["Symbols"]:
                symbol = wrapped_symbol["Symbol"]
                offset = len(stencil.body) + symbol["Value"]
                name = symbol["Name"]["Value"]
                name = name.removeprefix(self.prefix)
                group.symbols[name] = value, offset
            stencil.body.extend(section["SectionData"]["Bytes"])
            assert not section["Relocations"]
        else:
            assert section_type in {
                "SHT_GROUP",
                "SHT_LLVM_ADDRSIG",
                "SHT_NULL",
                "SHT_STRTAB",
                "SHT_SYMTAB",
            }, section_type

    def _handle_relocation(
        self, base: int, relocation: _schema.ELFRelocation, raw: bytes
    ) -> _stencils.Hole:
        match relocation:
            case {
                "Type": {"Value": kind},
                "Symbol": {"Value": s},
                "Offset": offset,
                "Addend": addend,
            }:
                offset += base
                s = s.removeprefix(self.prefix)
                value, symbol = _stencils.symbol_to_value(s)
            case _:
                raise NotImplementedError(relocation)
        return _stencils.Hole(offset, kind, value, symbol, addend)


class _COFF(
    _Target[_schema.COFFSection, _schema.COFFRelocation]
):  # pylint: disable = too-few-public-methods
    def _handle_section(
        self, section: _schema.COFFSection, group: _stencils.StencilGroup
    ) -> None:
        flags = {flag["Name"] for flag in section["Characteristics"]["Flags"]}
        if "SectionData" in section:
            section_data_bytes = section["SectionData"]["Bytes"]
        else:
            # Zeroed BSS data, seen with printf debugging calls:
            section_data_bytes = [0] * section["RawDataSize"]
        if "IMAGE_SCN_MEM_EXECUTE" in flags:
            value = _stencils.HoleValue.CODE
            stencil = group.code
        elif "IMAGE_SCN_MEM_READ" in flags:
            value = _stencils.HoleValue.DATA
            stencil = group.data
        else:
            return
        base = stencil.sections[section["Number"]] = len(stencil.body)
        stencil.body.extend(section_data_bytes)
        for wrapped_symbol in section["Symbols"]:
            symbol = wrapped_symbol["Symbol"]
            offset = base + symbol["Value"]
            name = symbol["Name"]
            name = name.removeprefix(self.prefix)
            group.symbols[name] = value, offset
        for wrapped_relocation in section["Relocations"]:
            relocation = wrapped_relocation["Relocation"]
            hole = self._handle_relocation(base, relocation, stencil.body)
            stencil.holes.append(hole)

    def _handle_relocation(
        self, base: int, relocation: _schema.COFFRelocation, raw: bytes
    ) -> _stencils.Hole:
        match relocation:
            case {
                "Type": {"Value": "IMAGE_REL_AMD64_ADDR64" as kind},
                "Symbol": s,
                "Offset": offset,
            }:
                offset += base
                s = s.removeprefix(self.prefix)
                value, symbol = _stencils.symbol_to_value(s)
                addend = int.from_bytes(raw[offset : offset + 8], "little")
            case {
                "Type": {"Value": "IMAGE_REL_I386_DIR32" as kind},
                "Symbol": s,
                "Offset": offset,
            }:
                offset += base
                s = s.removeprefix(self.prefix)
                value, symbol = _stencils.symbol_to_value(s)
                addend = int.from_bytes(raw[offset : offset + 4], "little")
            case _:
                raise NotImplementedError(relocation)
        return _stencils.Hole(offset, kind, value, symbol, addend)


class _MachO(
    _Target[_schema.MachOSection, _schema.MachORelocation]
):  # pylint: disable = too-few-public-methods
    def _handle_section(
        self, section: _schema.MachOSection, group: _stencils.StencilGroup
    ) -> None:
        assert section["Address"] >= len(group.code.body)
        assert "SectionData" in section
        flags = {flag["Name"] for flag in section["Attributes"]["Flags"]}
        name = section["Name"]["Value"]
        name = name.removeprefix(self.prefix)
        if "SomeInstructions" in flags:
            value = _stencils.HoleValue.CODE
            stencil = group.code
            start_address = 0
            group.symbols[name] = value, section["Address"] - start_address
        else:
            value = _stencils.HoleValue.DATA
            stencil = group.data
            start_address = len(group.code.body)
            group.symbols[name] = value, len(group.code.body)
        base = stencil.sections[section["Index"]] = section["Address"] - start_address
        stencil.body.extend(
            [0] * (section["Address"] - len(group.code.body) - len(group.data.body))
        )
        stencil.body.extend(section["SectionData"]["Bytes"])
        assert "Symbols" in section
        for wrapped_symbol in section["Symbols"]:
            symbol = wrapped_symbol["Symbol"]
            offset = symbol["Value"] - start_address
            name = symbol["Name"]["Value"]
            name = name.removeprefix(self.prefix)
            group.symbols[name] = value, offset
        assert "Relocations" in section
        for wrapped_relocation in section["Relocations"]:
            relocation = wrapped_relocation["Relocation"]
            hole = self._handle_relocation(base, relocation, stencil.body)
            stencil.holes.append(hole)

    def _handle_relocation(
        self, base: int, relocation: _schema.MachORelocation, raw: bytes
    ) -> _stencils.Hole:
        symbol: str | None
        match relocation:
            case {
                "Type": {
                    "Value": "ARM64_RELOC_GOT_LOAD_PAGE21"
                    | "ARM64_RELOC_GOT_LOAD_PAGEOFF12" as kind
                },
                "Symbol": {"Value": s},
                "Offset": offset,
            }:
                offset += base
                s = s.removeprefix(self.prefix)
                value, symbol = _stencils.HoleValue.GOT, s
                addend = 0
            case {
                "Type": {"Value": kind},
                "Section": {"Value": s},
                "Offset": offset,
            } | {
                "Type": {"Value": kind},
                "Symbol": {"Value": s},
                "Offset": offset,
            }:
                offset += base
                s = s.removeprefix(self.prefix)
                value, symbol = _stencils.symbol_to_value(s)
                addend = 0
            case _:
                raise NotImplementedError(relocation)
        # Turn Clang's weird __bzero calls into normal bzero calls:
        if symbol == "__bzero":
            symbol = "bzero"
        return _stencils.Hole(offset, kind, value, symbol, addend)


def get_target(host: str) -> _COFF | _ELF | _MachO:
    """Build a _Target for the given host "triple" and options."""
    if re.fullmatch(r"aarch64-apple-darwin.*", host):
        return _MachO(host, alignment=8, prefix="_")
    if re.fullmatch(r"aarch64-.*-linux-gnu", host):
        return _ELF(host, alignment=8)
    if re.fullmatch(r"i686-pc-windows-msvc", host):
        return _COFF(host, prefix="_")
    if re.fullmatch(r"x86_64-apple-darwin.*", host):
        return _MachO(host, prefix="_")
    if re.fullmatch(r"x86_64-pc-windows-msvc", host):
        return _COFF(host)
    if re.fullmatch(r"x86_64-.*-linux-gnu", host):
        return _ELF(host)
    raise ValueError(host)

def max_uop_id():
    with open(CPYTHON / "Include" / "internal" / "pycore_uop_ids.h") as file:
        for line in file.readlines():
            if m:= re.match(r"#define MAX_UOP_ID (?P<id>\d+)", line):
                return int(m.group("id"))

def _superop_name(opset: list[str]):
    return 'plus'.join(opset)