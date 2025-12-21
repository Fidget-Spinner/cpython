"""Generate the cases for the tier 2 optimizer.
Reads the instruction definitions from bytecodes.c and optimizer_bytecodes.c
Writes the cases to optimizer_cases.c.h, which is #included in Python/optimizer_analysis.c.
"""

import argparse

from analyzer import (
    Analysis,
    Instruction,
    Uop,
    analyze_files,
    StackItem,
    analysis_error,
    CodeSection,
    Label,
)
from generators_common import (
    DEFAULT_INPUT,
    ROOT,
    write_header,
    Emitter,
    TokenIterator,
    always_true,
)
from optimizer_generator import validate_uop, emit_default, declare_variables
from cwriter import CWriter
from typing import TextIO
from lexer import Token
from stack import Local, Stack, StackError, Storage

DEFAULT_OUTPUT = ROOT / "Python/optimizer_backwards_cases.c.h"
DEFAULT_ABSTRACT_INPUT = (ROOT / "Python/optimizer_backwards_bytecodes.c").absolute().as_posix()

def decref_inputs(
    out: CWriter,
    tkn: Token,
    tkn_iter: TokenIterator,
    uop: Uop,
    stack: Stack,
    inst: Instruction | None,
) -> None:
    next(tkn_iter)
    next(tkn_iter)
    next(tkn_iter)
    out.emit_at("", tkn)


class OptimizerEmitter(Emitter):

    def __init__(self, out: CWriter, labels: dict[str, Label], original_uop: Uop, stack: Stack):
        super().__init__(out, labels)
        self.original_uop = original_uop
        self.stack = stack

    def emit_save(self, storage: Storage) -> None:
        storage.flush(self.out)

    def emit_reload(self, storage: Storage) -> None:
        pass

    def goto_label(self, goto: Token, label: Token, storage: Storage) -> None:
        self.out.emit(goto)
        self.out.emit(label)


def write_uop(
    override: Uop | None,
    uop: Uop,
    out: CWriter,
    stack: Stack,
    debug: bool,
    skip_inputs: bool,
) -> None:
    locals: dict[str, Local] = {}
    prototype = override if override else uop
    try:
        out.start_line()
        if override:
            storage = Storage.for_uop_backwards(stack, prototype, out, check_liveness=False)
        if debug:
            args = []
            for input in prototype.stack.outputs:
                if not input.peek or override:
                    args.append(input.name)
            out.emit(f'DEBUG_PRINTF({", ".join(args)});\n')
        if override:
            emitter = OptimizerEmitter(out, {}, uop, stack.copy())
            # No reference management of inputs needed.
            for var in storage.inputs:  # type: ignore[possibly-undefined]
                var.in_local = False
            _, storage = emitter.emit_tokens(override, storage, None, False)
            out.start_line()
            storage.flush(out)
            out.start_line()
        else:
            emit_default(out, uop.stack.outputs, uop.stack.inputs, stack)
            out.start_line()
            stack.flush(out)
    except StackError as ex:
        raise analysis_error(ex.args[0], prototype.body.open) # from None


SKIPS = ("_EXTENDED_ARG",)


def generate_abstract_interpreter(
    filenames: list[str],
    abstract: Analysis,
    base: Analysis,
    outfile: TextIO,
    debug: bool,
) -> None:
    write_header(__file__, filenames, outfile)
    out = CWriter(outfile, 2, False)
    out.emit("\n")
    base_uop_names = set([uop.name for uop in base.uops.values()])
    for abstract_uop_name in abstract.uops:
        if abstract_uop_name not in base_uop_names:
            raise ValueError(f"All abstract uops should override base uops, "
                                 "but {abstract_uop_name} is not.")

    for uop in base.uops.values():
        override: Uop | None = None
        if uop.name in abstract.uops:
            override = abstract.uops[uop.name]
            validate_uop(override, uop)
        if uop.properties.tier == 1:
            continue
        if uop.replicates:
            continue
        if uop.is_super():
            continue
        if not uop.is_viable():
            out.emit(f"/* {uop.name} is not a viable micro-op for tier 2 */\n\n")
            continue
        out.emit(f"case {uop.name}: {{\n")
        if override:
            declare_variables(override.stack.outputs, override.stack.inputs, out, skip_inputs=False)
        else:
            declare_variables(uop.stack.outputs, uop.stack.inputs, out, skip_inputs=True)
        stack = Stack(check_stack_bounds=True)
        write_uop(override, uop, out, stack, debug, skip_inputs=(override is None))
        out.start_line()
        out.emit("break;\n")
        out.emit("}")
        out.emit("\n\n")


def generate_tier2_abstract_from_files(
    filenames: list[str], outfilename: str, debug: bool = False
) -> None:
    assert len(filenames) == 2, "Need a base file and an abstract cases file."
    base = analyze_files([filenames[0]])
    abstract = analyze_files([filenames[1]])
    with open(outfilename, "w") as outfile:
        generate_abstract_interpreter(filenames, abstract, base, outfile, debug)


arg_parser = argparse.ArgumentParser(
    description="Generate the code for the JIT backwards optimizer pass.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter,
)

arg_parser.add_argument(
    "-o", "--output", type=str, help="Generated code", default=DEFAULT_OUTPUT
)


arg_parser.add_argument("input", nargs="*", help="Abstract interpreter definition file")

arg_parser.add_argument(
    "base", nargs="*", help="The base instruction definition file(s)"
)

arg_parser.add_argument("-d", "--debug", help="Insert debug calls", action="store_true")

if __name__ == "__main__":
    args = arg_parser.parse_args()
    if not args.input:
        args.base.append(DEFAULT_INPUT)
        args.input.append(DEFAULT_ABSTRACT_INPUT)
    else:
        args.base.append(args.input[-1])
        args.input.pop()
    abstract = analyze_files(args.input)
    base = analyze_files(args.base)
    with open(args.output, "w") as outfile:
        generate_abstract_interpreter(args.input, abstract, base, outfile, args.debug)
