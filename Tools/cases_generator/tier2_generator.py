"""Generate the cases for the tier 2 interpreter.
Reads the instruction definitions from bytecodes.c.
Writes the cases to executor_cases.c.h, which is #included in ceval.c.
"""

import argparse

from analyzer import (
    Analysis,
    Instruction,
    Uop,
    Part,
    Label,
    CodeSection,
    analyze_files,
    Skip,
    Flush,
    analysis_error,
    StackItem,
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
from stack import Local, Stack, StackError, get_stack_effect, Storage

from tier3_generator import generate_tier3_cases

DEFAULT_OUTPUT = ROOT / "Python/executor_cases.c.h"


FOOTER = "#undef TIER_TWO\n"
INSTRUCTION_START_MARKER = "/* BEGIN INSTRUCTIONS */"
INSTRUCTION_END_MARKER = "/* END INSTRUCTIONS */"

class Tier2Emitter(Emitter):

    def __init__(self, out: CWriter, labels: dict[str, Label]):
        super().__init__(out, labels)
        self._replacers["DISPATCH_GOTO"] = self.dispatch_goto
        self._replacers["DISPATCH"] = self.dispatch

    def goto_error(self, offset: int, label: str, storage: Storage) -> str:
        # To do: Add jump targets for popping values.
        if offset != 0:
            storage.copy().flush(self.out)
        return f"JUMP_TO_ERROR();"

    def deopt_if(
        self,
        tkn: Token,
        tkn_iter: TokenIterator,
        uop: CodeSection,
        storage: Storage,
        inst: Instruction | None,
    ) -> bool:
        self.out.emit_at("if ", tkn)
        lparen = next(tkn_iter)
        self.emit(lparen)
        assert lparen.kind == "LPAREN"
        first_tkn = tkn_iter.peek()
        emit_to(self.out, tkn_iter, "RPAREN")
        next(tkn_iter)  # Semi colon
        self.emit(") {\n")
        self.emit("UOP_STAT_INC(uopcode, miss);\n")
        self.emit("JUMP_TO_JUMP_TARGET();\n")
        self.emit("}\n")
        return not always_true(first_tkn)

    def exit_if(  # type: ignore[override]
        self,
        tkn: Token,
        tkn_iter: TokenIterator,
        uop: CodeSection,
        storage: Storage,
        inst: Instruction | None,
    ) -> bool:
        self.out.emit_at("if ", tkn)
        lparen = next(tkn_iter)
        self.emit(lparen)
        first_tkn = tkn_iter.peek()
        emit_to(self.out, tkn_iter, "RPAREN")
        next(tkn_iter)  # Semi colon
        self.emit(") {\n")
        self.emit("UOP_STAT_INC(uopcode, miss);\n")
        self.emit("JUMP_TO_JUMP_TARGET();\n")
        self.emit("}\n")
        return not always_true(first_tkn)


    def dispatch_goto(  # type: ignore[override]
        self,
        tkn: Token,
        tkn_iter: TokenIterator,
        uop: CodeSection,
        storage: Storage,
        inst: Instruction | None,
    ) -> bool:
        next(tkn_iter)
        next(tkn_iter)
        next(tkn_iter)
        self.out.start_line()
        self.out.emit("Py_UNREACHABLE();\n")

    def dispatch(  # type: ignore[override]
        self,
        tkn: Token,
        tkn_iter: TokenIterator,
        uop: CodeSection,
        storage: Storage,
        inst: Instruction | None,
    ) -> bool:
        next(tkn_iter)
        next(tkn_iter)
        next(tkn_iter)
        self.out.start_line()
        self.out.emit("Py_UNREACHABLE();\n")

def declare_variable(var: StackItem, out: CWriter) -> None:
    type, null = type_and_null(var)
    space = " " if type[-1].isalnum() else ""
    if var.condition:
        out.emit(f"{type}{space}{var.name} = {null};\n")
    else:
        out.emit(f"{type}{space}{var.name};\n")


def declare_variables(inst: Instruction, out: CWriter) -> None:
    try:
        stack = get_stack_effect(inst)
    except StackError as ex:
        raise analysis_error(ex.args[0], inst.where) from None
    required = set(stack.defined)
    required.discard("unused")
    for part in inst.parts:
        if not isinstance(part, Uop):
            continue
        for var in part.stack.inputs:
            if var.name in required:
                required.remove(var.name)
                declare_variable(var, out)
        for var in part.stack.outputs:
            if var.name in required:
                required.remove(var.name)
                declare_variable(var, out)


def write_uop(
    uop: Part,
    emitter: Emitter,
    offset: int,
    stack: Stack,
    inst: Instruction,
    braces: bool,
) -> tuple[int, Stack]:
    # out.emit(stack.as_comment() + "\n")
    if isinstance(uop, Skip):
        entries = "entries" if uop.size > 1 else "entry"
        emitter.emit(f"/* Skip {uop.size} cache {entries} */\n")
        return (offset + uop.size), stack
    if isinstance(uop, Flush):
        emitter.emit(f"// flush\n")
        stack.flush(emitter.out)
        return offset, stack
    try:
        locals: dict[str, Local] = {}
        emitter.out.start_line()
        if braces:
            emitter.out.emit(f"// {uop.name}\n")
            emitter.emit("{\n")
        code_list, storage = Storage.for_uop(stack, uop)
        emitter._print_storage(storage)
        for code in code_list:
            emitter.emit(code)

        for cache in uop.caches:
            if cache.name != "unused":
                if cache.size == 4:
                    type = "PyObject *"
                    reader = "read_obj"
                else:
                    type = f"uint{cache.size*16}_t "
                    reader = f"read_u{cache.size*16}"
                emitter.emit(
                    f"{type}{cache.name} = {reader}(&this_instr[{offset}].cache);\n"
                )
                if inst.family is None:
                    emitter.emit(f"(void){cache.name};\n")
            offset += cache.size

        storage = emitter.emit_tokens(uop, storage, inst)
        if braces:
            emitter.out.start_line()
            emitter.emit("}\n")
        # emitter.emit(stack.as_comment() + "\n")
        return offset, storage.stack
    except StackError as ex:
        raise analysis_error(ex.args[0], uop.body[0])


def uses_this(inst: Instruction) -> bool:
    if inst.properties.needs_this:
        return True
    for uop in inst.parts:
        if not isinstance(uop, Uop):
            continue
        for cache in uop.caches:
            if cache.name != "unused":
                return True
    return False


def generate_tier2(
    filenames: list[str], analysis: Analysis, outfile: TextIO, lines: bool
) -> None:
    write_header(__file__, filenames, outfile)
    outfile.write("""
#ifdef TIER_ONE
    #error "This file is for Tier 2 only"
#endif
#define TIER_TWO 2
""")
    generate_tier2_cases(analysis, outfile, lines)
    for name, uop in dict(analysis.uops).items():
        if uop.properties.tier == 3 or uop.emitted and name != "_CHECK_PERIODIC":
            del analysis.uops[name]
    generate_tier3_cases(analysis, outfile, lines)
    outfile.write(FOOTER)

def generate_tier2_cases(
    analysis: Analysis, outfile: TextIO, lines: bool
) -> None:
    out = CWriter(outfile, 2, lines)
    emitter = Tier2Emitter(out, analysis.labels)
    out.emit("\n")
    for name, inst in sorted(analysis.instructions.items()):
        out.emit("\n")
        out.emit(f"case {name}: {{\n")
        uses_this_instr = uses_this(inst)
        if uses_this_instr:
            # The most generic instruction doesn't need to deopt.
            if inst.family is not None and inst.family.name != name:
                out.emit(f"if (this_instr->op.code != {name}) {{\n")
                out.emit(f"JUMP_TO_JUMP_TARGET();\n")
                out.emit("}\n")
        if inst.properties.needs_prev:
            out.emit(f"_Py_CODEUNIT* const prev_instr = frame->instr_ptr;\n")

        if not inst.properties.no_save_ip:
            out.emit(f"frame->instr_ptr = next_instr;\n")

        out.emit(f"INSTRUCTION_STATS({name});\n")
        if inst.properties.uses_opcode:
            out.emit(f"opcode = {name};\n")
        if inst.family is not None:
            out.emit(
                f"static_assert({inst.family.size} == {inst.size-1}"
                ', "incorrect cache size");\n'
            )
        declare_variables(inst, out)
        offset = 1  # The instruction itself
        stack = Stack()
        is_composite_call = (isinstance(inst.parts[-1], Uop) and inst.parts[-1].name == "_PUSH_FRAME")
        is_for_iter = name.startswith("FOR_ITER")
        for part in inst.parts:
            # Only emit braces if more than one uop
            insert_braces = len([p for p in inst.parts if isinstance(p, Uop)]) > 1
            offset, stack = write_uop(part, emitter, offset, stack, inst, insert_braces)
            if not is_composite_call and not is_for_iter:
                part.emitted = True
            else:
                part.emitted = False
        out.start_line()

        stack.flush(out)
        if not inst.parts[-1].properties.always_exits:
            out.emit("break;\n")
        out.start_line()
        out.emit("}")
        out.emit("\n")



arg_parser = argparse.ArgumentParser(
    description="Generate the code for the interpreter switch.",
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


def generate_tier2_from_files(
    filenames: list[str], outfilename: str, lines: bool
) -> None:
    data = analyze_files(filenames)
    with open(outfilename, "w") as outfile:
        generate_tier2(filenames, data, outfile, lines)


if __name__ == "__main__":
    args = arg_parser.parse_args()
    if len(args.input) == 0:
        args.input.append(DEFAULT_INPUT)
    data = analyze_files(args.input)
    with open(args.output, "w") as outfile:
        generate_tier2(args.input, data, outfile, args.emit_line_directives)
