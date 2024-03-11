"""Generate uop metadata.
Reads the instruction definitions from bytecodes.c.
Writes the metadata to pycore_uop_metadata.h by default.
"""

import argparse

from analyzer import (
    Analysis,
    analyze_files,
    REGISTERS,
)
from generators_common import (
    DEFAULT_INPUT,
    ROOT,
    write_header,
    cflags,
)
from cwriter import CWriter
from typing import TextIO
from stack import get_uop_stack_effect, RegisterItem


DEFAULT_OUTPUT = ROOT / "Include/internal/pycore_uop_metadata.h"


def generate_names_and_flags(analysis: Analysis, out: CWriter) -> None:
    out.emit("extern const uint32_t _PyUop_Flags[MAX_UOP_ID+1];\n")
    out.emit("extern const uint8_t _PyUop_Replication[MAX_UOP_ID+1];\n")
    out.emit("extern const char * const _PyOpcode_uop_name[MAX_UOP_ID+1];\n\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit("const uint32_t _PyUop_Flags[MAX_UOP_ID+1] = {\n")
    for uop in analysis.uops.values():
        if uop.is_viable() and uop.properties.tier != 1:
            out.emit(f"[{uop.name}] = {cflags(uop.properties)},\n")

    out.emit("};\n\n")
    out.emit("const uint8_t _PyUop_Replication[MAX_UOP_ID+1] = {\n")
    for uop in analysis.uops.values():
        if uop.replicated:
            out.emit(f"[{uop.name}] = {uop.replicated},\n")

    out.emit("};\n\n")
    out.emit("const char *const _PyOpcode_uop_name[MAX_UOP_ID+1] = {\n")
    for uop in sorted(analysis.uops.values(), key=lambda t: t.name):
        if uop.is_viable() and uop.properties.tier != 1:
            out.emit(f'[{uop.name}] = "{uop.name}",\n')
    out.emit("};\n")
    out.emit("#endif // NEED_OPCODE_METADATA\n\n")

def emit_stack_effect_function(
        out: CWriter, direction: str, data: list[tuple[str, str]]
) -> None:
    out.emit(f"extern int _PyUop_num_{direction}(int opcode, int oparg);\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit(f"int _PyUop_num_{direction}(int opcode, int oparg)  {{\n")
    out.emit("switch(opcode) {\n")
    for name, effect in data:
        out.emit(f"case {name}:\n")
        out.emit(f"    return {effect};\n")
    out.emit("default:\n")
    out.emit("    return -1;\n")
    out.emit("}\n")
    out.emit("}\n\n")
    out.emit("#endif\n\n")


def generate_stack_effect_functions(analysis: Analysis, out: CWriter) -> None:
    popped_data: list[tuple[str, str]] = []
    pushed_data: list[tuple[str, str]] = []
    for uop in analysis.uops.values():
        if not(uop.is_viable() and uop.properties.tier != 1):
            continue
        stack = get_uop_stack_effect(uop)
        popped = (-stack.base_offset).to_c()
        pushed = (stack.top_offset - stack.base_offset).to_c()
        popped_data.append((uop.name, popped))
        pushed_data.append((uop.name, pushed))
    emit_stack_effect_function(out, "popped", sorted(popped_data))
    emit_stack_effect_function(out, "pushed", sorted(pushed_data))

def generate_reg_to_uop_mapping(
        analysis: Analysis, out: CWriter
) -> None:
    out.emit(f"extern int _PyUop_match_reg_to_uop(uint32_t opcode);\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit(f"int _PyUop_match_reg_to_uop(uint32_t opcode)  {{\n")
    out.emit("switch(opcode) {\n")
    for name, uop in analysis.uops.items():
        if not(uop.is_viable() and uop.properties.tier != 1):
            continue
        if uop.properties.has_register_version:
            for reg_op in uop.register_version:
                out.emit(f"case {reg_op.name}: ")
                out.emit(f"return {uop.name};\n")
    out.emit("default:\n")
    out.emit("    Py_UNREACHABLE();\n")
    out.emit("}\n")
    out.emit("}\n\n")
    out.emit("#endif\n\n")

def generate_uop_to_reg_mapping(
    analysis: Analysis, out: CWriter
) -> None:
    out.emit(f"extern int _PyUop_match_reg(uint32_t opcode, int should_spill, int reg_out_start);\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit(f"int _PyUop_match_reg(uint32_t opcode, int should_spill, int reg_out_start)  {{\n")
    out.emit("switch(opcode) {\n")
    for name, uop in analysis.uops.items():
        if not(uop.is_viable() and uop.properties.tier != 1):
            continue
        if uop.properties.has_register_version:
            out.emit(f"case {uop.name}: {{\n")
            out.emit("if (should_spill) {\n")
            out.emit(f"return {uop.name}__REG_SPILL;\n")
            out.emit("}\n")
            if uop.stack.outputs:
                out.emit("else {\n")
                out.emit("switch (reg_out_start) {\n")
                for reg in [version for version in uop.register_version if not version.name.endswith("__REG_SPILL")]:
                    register_usage = [out for out in reg.stack.outputs if isinstance(out, RegisterItem)]
                    assert register_usage
                    out.emit(f"case {register_usage[0].register}: ")
                    out.emit(f"return {reg.name};\n")
                out.emit(f"default: return {uop.name}__REG_SPILL;\n")
                out.emit("}\n")
                out.emit("}\n")
            out.emit("break;\n")
            out.emit("}\n")
    out.emit("default:\n")
    out.emit("    Py_UNREACHABLE();\n")
    out.emit("}\n")
    out.emit("}\n\n")
    out.emit("#endif\n\n")

def generate_uop_metadata(
    filenames: list[str], analysis: Analysis, outfile: TextIO
) -> None:
    write_header(__file__, filenames, outfile)
    out = CWriter(outfile, 0, False)
    with out.header_guard("Py_CORE_UOP_METADATA_H"):
        out.emit("#include <stdint.h>\n")
        out.emit('#include "pycore_uop_ids.h"\n')
        out.emit(f"#define UOP_REGISTERS_COUNT {len(REGISTERS)}\n\n")
        generate_names_and_flags(analysis, out)
        generate_stack_effect_functions(analysis, out)
        generate_uop_to_reg_mapping(analysis, out)
        generate_reg_to_uop_mapping(analysis, out)


arg_parser = argparse.ArgumentParser(
    description="Generate the header file with uop metadata.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)

arg_parser.add_argument(
    "-o", "--output", type=str, help="Generated code", default=DEFAULT_OUTPUT
)

arg_parser.add_argument(
    "input", nargs=argparse.REMAINDER, help="Instruction definition file(s)"
)

if __name__ == "__main__":
    args = arg_parser.parse_args()
    if len(args.input) == 0:
        args.input.append(DEFAULT_INPUT)
    data = analyze_files(args.input)
    with open(args.output, "w") as outfile:
        generate_uop_metadata(args.input, data, outfile)
