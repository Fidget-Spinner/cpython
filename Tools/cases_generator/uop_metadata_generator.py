"""Generate uop metadata.
Reads the instruction definitions from bytecodes.c.
Writes the metadata to pycore_uop_metadata.h by default.
"""

import argparse
import itertools

from analyzer import (
    Analysis,
    analyze_files,
)
from generators_common import (
    DEFAULT_INPUT,
    ROOT,
    write_header,
    cflags,
)
from stack import Stack
from cwriter import CWriter
from typing import TextIO

DEFAULT_OUTPUT = ROOT / "Include/internal/pycore_uop_metadata.h"


def generate_names_and_flags(analysis: Analysis, out: CWriter) -> None:
    out.emit("extern const uint64_t _PyUop_Flags[MAX_UOP_ID+1];\n")
    out.emit("extern const uint8_t _PyUop_Replication[MAX_UOP_ID+1];\n")
    out.emit("extern const char * const _PyOpcode_uop_name[MAX_UOP_ID+1];\n\n")
    out.emit("extern int _PyUop_num_popped(int opcode, int oparg);\n\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit("const uint64_t _PyUop_Flags[MAX_UOP_ID+1] = {\n")
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
    for super_name, super_uop in sorted(analysis.super_uops.items(), key=lambda t: t[0]):
        out.emit(f'[{super_name}] = "{super_name}",\n')
    out.emit("};\n")
    out.emit("int _PyUop_num_popped(int opcode, int oparg)\n{\n")
    out.emit("switch(opcode) {\n")
    for uop in analysis.uops.values():
        if uop.is_viable() and uop.properties.tier != 1:
            stack = Stack()
            for var in reversed(uop.stack.inputs):
                if var.peek:
                    break
                stack.pop(var)
            popped = (-stack.base_offset).to_c()
            out.emit(f"case {uop.name}:\n")
            out.emit(f"    return {popped};\n")
    out.emit("default:\n")
    out.emit("    return -1;\n")
    out.emit("}\n")
    out.emit("}\n\n")
    out.emit("#endif // NEED_OPCODE_METADATA\n\n")


def generate_super_uop_matcher(analysis: Analysis, out: CWriter) -> None:
    out.emit("extern int _PyUOp_superuop_matcher(_PyUOpInstruction *this_instr, int *move_forward_by);\n\n")
    out.emit("#ifdef NEED_OPCODE_METADATA\n")
    out.emit("int _PyUOp_superuop_matcher(_PyUOpInstruction *this_instr, int *move_forward_by) {\n")
    for super_name, super_uop in analysis.super_uops.items():
        not_viable = False
        for uop in super_uop:
            if not_viable:
                break
            if uop.properties.tier == 1:
                not_viable = True
                break
            if uop.properties.oparg_and_1:
                not_viable = True
                break
            if uop.is_super():
                not_viable = True
                break
            why_not_viable = uop.why_not_viable()
            if why_not_viable is not None:
                not_viable = True
                break
        if not_viable:
            continue
        for idx, uop in enumerate(super_uop):
            out.emit(f"if (this_instr[{idx}].opcode == {uop.name}) {{ \n")
        out.emit(f"*move_forward_by = {len(super_uop)};\n")
        out.emit(f"return {super_name};\n")
        for _ in super_uop:
            out.emit("}\n")
    out.emit(f"return -1;\n")
    out.emit("}\n")

    out.emit("#endif // NEED_OPCODE_METADATA\n\n")

def generate_uop_metadata(
    filenames: list[str], analysis: Analysis, outfile: TextIO
) -> None:
    write_header(__file__, filenames, outfile)
    out = CWriter(outfile, 0, False)
    with out.header_guard("Py_CORE_UOP_METADATA_H"):
        out.emit("#include <stdint.h>\n")
        out.emit('#include "pycore_uop_ids.h"\n')
        generate_names_and_flags(analysis, out)
        generate_super_uop_matcher(analysis, out)


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
