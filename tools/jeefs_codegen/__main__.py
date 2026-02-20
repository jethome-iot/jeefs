"""CLI entry point for JEEFS code generator.

Usage:
    python -m jeefs_codegen --specs docs/format/*.md --c-output include/jeefs_generated.h
    python -m jeefs_codegen --specs docs/format/*.md --validate-only
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .generators.c_generator import generate_c_header
from .generators.python_generator import generate_python_constants
from .generators.rust_generator import generate_rust_module
from .parser import parse_files
from .validator import validate_spec


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate C and Python code from JEEFS markdown format specs."
    )
    parser.add_argument(
        "--specs",
        nargs="+",
        type=Path,
        required=True,
        help="Markdown spec files to parse (docs/format/*.md)",
    )
    parser.add_argument(
        "--c-output",
        type=Path,
        default=None,
        help="Output path for generated C header",
    )
    parser.add_argument(
        "--py-output",
        type=Path,
        default=None,
        help="Output path for generated Python constants module",
    )
    parser.add_argument(
        "--rs-output",
        type=Path,
        default=None,
        help="Output path for generated Rust module",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Only validate specs, do not generate code",
    )

    args = parser.parse_args()

    # Check spec files exist
    for spec_path in args.specs:
        if not spec_path.exists():
            print(f"ERROR: Spec file not found: {spec_path}", file=sys.stderr)
            return 1

    # Parse
    print(f"Parsing {len(args.specs)} spec file(s)...")
    spec = parse_files(args.specs)
    print(
        f"  Found: {len(spec.structs)} struct(s), {len(spec.enums)} enum(s), "
        f"{len(spec.constants)} constant(s), {len(spec.unions)} union(s)"
    )

    # Validate
    errors = validate_spec(spec)
    if errors:
        print(f"\nValidation FAILED with {len(errors)} error(s):", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print("Validation OK.")

    if args.validate_only:
        return 0

    # Generate
    if args.c_output:
        c_code = generate_c_header(spec)
        args.c_output.parent.mkdir(parents=True, exist_ok=True)
        args.c_output.write_text(c_code, encoding="utf-8")
        print(f"Generated C header: {args.c_output} ({len(c_code)} bytes)")

    if args.py_output:
        py_code = generate_python_constants(spec)
        args.py_output.parent.mkdir(parents=True, exist_ok=True)
        args.py_output.write_text(py_code, encoding="utf-8")
        print(f"Generated Python constants: {args.py_output} ({len(py_code)} bytes)")

    if args.rs_output:
        rs_code = generate_rust_module(spec)
        args.rs_output.parent.mkdir(parents=True, exist_ok=True)
        args.rs_output.write_text(rs_code, encoding="utf-8")
        print(f"Generated Rust module: {args.rs_output} ({len(rs_code)} bytes)")

    if not args.c_output and not args.py_output and not args.rs_output:
        print("No output files specified. Use --c-output, --py-output, and/or --rs-output.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
