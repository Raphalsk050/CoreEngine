#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


SOURCE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inl",
    ".ipp",
}

EXCLUDED_DIRS = {
    ".git",
    ".idea",
    ".vscode",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
    "cmake-build-relwithdebinfo",
    "cmake-build-minsizerel",
    "dist",
    "out",
    "third_party",
    "_deps",
    "CMakeFiles",
}


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def is_excluded(path: Path, root: Path) -> bool:
    relative_parts = path.relative_to(root).parts
    return any(part in EXCLUDED_DIRS for part in relative_parts)


def iter_source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if is_excluded(path, root):
            continue
        if path.suffix.lower() in SOURCE_EXTENSIONS:
            files.append(path)
    return sorted(files)


def run_command(command: list[str], root: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def check_file(clang_format: str, path: Path, root: Path) -> bool:
    result = run_command([clang_format, str(path)], root)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        return False

    original = path.read_text(encoding="utf-8", errors="surrogateescape")
    return result.stdout == original


def format_file(clang_format: str, path: Path, root: Path) -> bool:
    result = run_command([clang_format, "-i", str(path)], root)
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        return False
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Format CoreEngine C/C++ source files with clang-format.")
    parser.add_argument("--check", action="store_true", help="Check formatting without modifying files.")
    parser.add_argument("--list-files", action="store_true", help="Print files that would be formatted.")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print every formatted file.")
    parser.add_argument("--clang-format", default="clang-format", help="clang-format executable path.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = project_root()
    clang_format = args.clang_format

    if not (root / ".clang-format").exists():
        sys.stderr.write(f"Missing .clang-format in project root: {root}\n")
        return 1

    files = iter_source_files(root)
    if args.list_files:
        for path in files:
            print(path.relative_to(root))
        return 0

    if shutil.which(clang_format) is None and not Path(clang_format).exists():
        sys.stderr.write(f"clang-format was not found: {clang_format}\n")
        return 1

    if not files:
        print("No C/C++ source files found.")
        return 0

    failed = 0
    changed = 0

    for path in files:
        if args.check:
            if not check_file(clang_format, path, root):
                changed += 1
                print(f"Needs formatting: {path.relative_to(root)}")
        else:
            if args.verbose:
                print(f"Formatting: {path.relative_to(root)}")
            if not format_file(clang_format, path, root):
                failed += 1

    if args.check:
        if changed:
            print(f"{changed} file(s) need formatting.")
            return 1
        print(f"All {len(files)} file(s) are formatted.")
        return 0

    if failed:
        print(f"Formatting failed for {failed} file(s).")
        return 1

    print(f"Formatted {len(files)} file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
