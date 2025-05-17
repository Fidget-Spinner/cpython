"""Generate the cases for the tier 2 interpreter.
Reads the instruction definitions from bytecodes.c.
Writes the cases to executor_cases.c.h, which is #included in ceval.c.
"""

import argparse

from analyzer import (
    Analysis,
    Instruction,
    Uop,
    Label,
    CodeSection,
    analyze_files,
    StackItem,
    analysis_error,
)
from generators_common import (
    DEFAULT_INPUT,
    ROOT,
    emit_to,
    write_header,
    type_and_null,
    Emitter,
    TokenIterator,
    always_true,
)
from cwriter import CWriter
from typing import TextIO
from lexer import Token
from stack import Local, Stack, StackError, Storage

DEFAULT_OUTPUT = ROOT / "Python/regalloc_cases.c.h"

def generate_tier2_regalloc(
    filenames: list[str], analysis: Analysis, outfile: TextIO, lines: bool
) -> None:
    write_header(__file__, filenames, outfile)
    outfile.write(
        """
#ifdef TIER_ONE
    #error "This file is for Tier 2 only"
#endif
#define TIER_TWO 2
"""
    )
    out = CWriter(outfile, 2, lines)
    out.emit("\n")
    for name, uop in analysis.uops.items():
        if uop.properties.tier == 1:
            continue
        if uop.is_super():
            continue
        if uop.replicates:
            continue
        if uop.tos_cached_version_of:
            continue
        why_not_viable = uop.why_not_viable()
        if why_not_viable is not None:
            out.emit(
                f"/* {uop.name} is not a viable micro-op for tier 2 because it {why_not_viable} */\n\n"
            )
            continue
        try:
            net_effect = int(uop.stack.net_effect())
        except ValueError:
            net_effect = None
        if net_effect is None:
            continue
        # Don't stack cache type casted items, as we expect the registers to be
        # _PyStackRef.
        skip = False
        for effect in (*uop.stack.inputs, *uop.stack.outputs):
            if effect.type or effect.size:
                skip = True
                break
        if skip:
            continue
        out.emit(f"case {uop.name}: {{\n")
        out.emit("switch(curr_regs_in) {\n")
        for i_in in range(0, 7):
            num_live_registers_out = i_in + net_effect
            if num_live_registers_out < 0 or num_live_registers_out > 6:
                continue
            if num_live_registers_out == 0 and i_in == 0:
                continue
            out.emit(f"case {i_in}:\n")
            out.emit(f"reged = {uop.name}___CACHED_{i_in}in_{num_live_registers_out}out;\n")
            out.emit(f"curr_regs_in = {num_live_registers_out};\n")
            out.emit("break;\n")
        out.emit("default: Py_UNREACHABLE();\n")
        out.emit("}\n")
        out.emit("break;\n")
        out.emit("}")
        out.emit("\n\n")
    outfile.write("#undef TIER_TWO\n")


arg_parser = argparse.ArgumentParser(
    description="Generate the code for the tier 2 register allocator.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)

arg_parser.add_argument(
    "-o", "--output", type=str, help="Generated code", default=DEFAULT_OUTPUT
)

arg_parser.add_argument(
    "-l", "--emit-line-directives", help="Emit #line directives", action="store_true"
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
        generate_tier2_regalloc(args.input, data, outfile, args.emit_line_directives)
