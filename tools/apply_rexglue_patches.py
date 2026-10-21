#!/usr/bin/env python3
"""Apply the project-owned ReXGlue SDK changes (patches/rexglue) to a fresh SDK checkout."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# (patch, directory inside the SDK it applies to, the upstream revision that directory must be at)
PATCHES = (
    ("0001-tekken6-sdk-changes.patch", ".", "f5337cdc947ff6d4c4196737e2c807a48f2a1fc2"),
    ("0002-ffmpeg-mingw.patch", "thirdparty/FFmpeg", "0604b464c7cb4ebc94940cf1f324a3b26b87717c"),
)


def git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(["git", "-C", str(repo), *args], check=check, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def apply(repo: Path, patch: Path, base: str) -> bool:
    """True when the patch is applied afterwards (now or already); False (with a message) when the checkout differs."""
    if not patch.is_file():
        print(f"Missing project patch: {patch}", file=sys.stderr)
        return False
    if git(repo, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
        print(f"{patch.name}: already applied")
        return True
    # Source archives used by portable packages intentionally omit Git metadata.
    # In that case, patch application is still safe: require an exact forward
    # applicability check, otherwise leave the SDK untouched and report why.
    if not (repo / ".git").exists():
        check = git(repo, "apply", "--check", str(patch), check=False)
        if check.returncode:
            print(f"{patch.name}: SDK source does not match the expected unpatched files; refusing to patch it.", file=sys.stderr)
            return False
        git(repo, "apply", str(patch))
        print(f"{patch.name}: applied to source archive")
        return True

    revision = git(repo, "rev-parse", "HEAD").stdout.strip()
    if revision != base:
        print(f"{patch.name}: expected {repo} at {base}, found {revision}; refusing to patch a different revision.", file=sys.stderr)
        return False
    if git(repo, "status", "--porcelain", "--untracked-files=no").stdout.strip():
        print(f"{patch.name}: {repo} has local changes; refusing to overwrite them.", file=sys.stderr)
        return False
    git(repo, "apply", "--check", str(patch))
    git(repo, "apply", str(patch))
    print(f"{patch.name}: applied")
    return True


def apply_portable_sdk_exports(sdk: Path) -> bool:
    """Make project-added SDK exports compile on ELF and allow source archives without nested .git metadata."""
    thirdparty_cmake = sdk / "thirdparty" / "CMakeLists.txt"
    if not thirdparty_cmake.is_file():
        print(f"Missing SDK thirdparty CMake file: {thirdparty_cmake}", file=sys.stderr)
        return False

    cmake_text = thirdparty_cmake.read_text(encoding="utf-8")
    git_only_check = 'if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")'
    archive_aware_check = (
        'if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.git" AND '
        'NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")'
    )
    if git_only_check in cmake_text:
        cmake_text = cmake_text.replace(git_only_check, archive_aware_check)
        thirdparty_cmake.write_text(cmake_text, encoding="utf-8")
        print("thirdparty/CMakeLists.txt: validate nested submodules only for a Git SDK checkout")
    elif archive_aware_check in cmake_text:
        print("thirdparty/CMakeLists.txt: source-archive submodule check already fixed")
    else:
        print("thirdparty/CMakeLists.txt: expected nested-submodule check not found; leaving it untouched",
              file=sys.stderr)
        return False

    export_header = sdk / "include" / "rex" / "t6_export.h"
    export_header.parent.mkdir(parents=True, exist_ok=True)
    export_content = (
        "#pragma once\n"
        "// Project-owned SDK hooks are exported on Windows DLL builds. On ELF, default visibility\n"
        "// is sufficient; __declspec(dllexport) is Microsoft-specific syntax.\n"
        "#if defined(_WIN32)\n"
        "#define REX_T6_EXPORT __declspec(dllexport)\n"
        "#else\n"
        "#define REX_T6_EXPORT __attribute__((visibility(\\"default\\")))\n"
        "#endif\n"
    )
    if not export_header.is_file() or export_header.read_text(encoding="utf-8") != export_content:
        export_header.write_text(export_content, encoding="utf-8")

    converted = 0
    include_line = '#include <rex/t6_export.h>\n'
    for source in sdk.rglob("*.cpp"):
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        if 'extern "C" __declspec(dllexport)' not in text:
            continue
        text = text.replace('extern "C" __declspec(dllexport)', 'extern "C" REX_T6_EXPORT')
        if include_line.strip() not in text:
            first_include = text.find("#include ")
            if first_include < 0:
                print(f"No include location found in SDK source: {source}", file=sys.stderr)
                return False
            text = text[:first_include] + include_line + text[first_include:]
        source.write_text(text, encoding="utf-8")
        converted += 1

    print(f"Portable SDK exports: updated {converted} source files (header: {export_header.relative_to(sdk)})")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", type=Path, default=Path(__file__).resolve().parents[1] / "third_party" / "rexglue-sdk",
                        help="ReXGlue SDK checkout (default: this repo's submodule)")
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    patch_dir = Path(__file__).resolve().parents[1] / "patches" / "rexglue"
    if not (sdk / "CMakeLists.txt").is_file():
        print(f"ReXGlue SDK source tree not found: {sdk}", file=sys.stderr)
        return 2
    for name, sub, base in PATCHES:
        if not apply((sdk / sub).resolve(), patch_dir / name, base):
            return 2
    if not apply_portable_sdk_exports(sdk):
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
