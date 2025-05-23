"""Build an experimental just-in-time compiler for CPython."""

import argparse
import pathlib
import shlex
import sys

import _targets

if __name__ == "__main__":
    comment = f"$ {shlex.join([pathlib.Path(sys.executable).name] + sys.argv)}"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-d", "--debug", action="store_true", help="compile for a debug build of Python"
    )
    parser.add_argument(
        "-f", "--force", action="store_true", help="force the entire JIT to be rebuilt"
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        help="where to output generated files",
        required=True,
        type=lambda p: pathlib.Path(p).resolve(),
    )
    parser.add_argument(
        "-p",
        "--pyconfig-dir",
        help="where to find pyconfig.h",
        required=True,
        type=lambda p: pathlib.Path(p).resolve(),
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="echo commands as they are run"
    )
    args = parser.parse_args()
    _targets.Target(debug=args.debug, pyconfig_dir=args.pyconfig_dir).build(
        comment=comment,
        force=args.force,
        jit_stencils=args.output_dir / f"jit_ng_stencils.h",
    )
