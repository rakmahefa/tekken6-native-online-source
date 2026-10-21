#!/usr/bin/env python3
"""Regression tests for source-archive SDK portability and incremental ISO staging."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


patcher = load_module("t6_apply_rexglue_patches", ROOT / "tools" / "apply_rexglue_patches.py")
builder = load_module("t6_build_native_linux", ROOT / "builder" / "build_native_linux.py")


class SDKArchivePortabilityTests(unittest.TestCase):
    def make_sdk(self, cmake_text: str, root: Path) -> Path:
        sdk = root / "rexglue-sdk"
        (sdk / "thirdparty").mkdir(parents=True)
        (sdk / "thirdparty" / "CMakeLists.txt").write_text(cmake_text, encoding="utf-8")
        source = sdk / "src" / "core" / "logging.cpp"
        source.parent.mkdir(parents=True)
        source.write_text(
            "// SDK source\n#include <rex/logging.h>\n"
            'extern "C" __declspec(dllexport) void rex_trace_everything(bool on) {}\n',
            encoding="utf-8",
        )
        return sdk

    def test_git_submodule_check_accepts_source_archive_and_exports_are_portable(self):
        cmake = (
            "set(REQUIRED_SUBMODULES cli11)\n"
            "foreach(submodule ${REQUIRED_SUBMODULES})\n"
            '    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")\n'
            '        message(FATAL_ERROR "submodule not initialized")\n'
            "    endif()\n"
            "endforeach()\n"
        )
        with tempfile.TemporaryDirectory() as temp:
            sdk = self.make_sdk(cmake, Path(temp))
            self.assertTrue(patcher.apply_portable_sdk_exports(sdk))
            updated_cmake = (sdk / "thirdparty" / "CMakeLists.txt").read_text(encoding="utf-8")
            self.assertIn('${CMAKE_CURRENT_SOURCE_DIR}/../.git', updated_cmake)
            self.assertIn('NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git"', updated_cmake)
            source = (sdk / "src" / "core" / "logging.cpp").read_text(encoding="utf-8")
            self.assertIn("#include <rex/t6_export.h>", source)
            self.assertIn('extern "C" REX_T6_EXPORT void rex_trace_everything', source)
            self.assertNotIn("__declspec(dllexport)", source)
            header = (sdk / "include" / "rex" / "t6_export.h").read_text(encoding="utf-8")
            self.assertIn('#define REX_T6_EXPORT __declspec(dllexport)', header)
            self.assertIn('__attribute__((visibility("default")))', header)
            self.assertTrue(patcher.apply_portable_sdk_exports(sdk))

    def test_restores_the_manual_commented_out_submodule_loop(self):
        cmake = (
            "set(REQUIRED_SUBMODULES cli11)\n"
            "# foreach(submodule ${REQUIRED_SUBMODULES})\n"
            '#     if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${submodule}/.git")\n'
            '#         message(FATAL_ERROR "submodule not initialized")\n'
            "#     endif()\n"
            "# endforeach()\n"
        )
        with tempfile.TemporaryDirectory() as temp:
            sdk = self.make_sdk(cmake, Path(temp))
            self.assertTrue(patcher.apply_portable_sdk_exports(sdk))
            updated = (sdk / "thirdparty" / "CMakeLists.txt").read_text(encoding="utf-8")
            self.assertIn("foreach(submodule ${REQUIRED_SUBMODULES})", updated)
            self.assertIn('if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.git" AND', updated)
            self.assertNotIn("# foreach(submodule", updated)


class IncrementalGameStagingTests(unittest.TestCase):
    def test_unchanged_iso_reuses_staged_game_without_checking_or_extracting_iso(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            iso = root / "Tekken.iso"
            iso.write_bytes(b"fake ISO for cache-path unit test")
            staged_xex = root / "native" / "game" / "Default.xex"
            staged_xex.parent.mkdir(parents=True)
            staged_xex.write_bytes(b"already validated XEX")
            stat = iso.stat()
            stamp = {
                "kind": "iso",
                "path": str(iso.resolve()),
                "size": stat.st_size,
                "mtime_ns": stat.st_mtime_ns,
                "default_xex_sha256": builder.sha256(staged_xex),
            }
            with (
                patch.object(builder, "check_game_folder", return_value=(staged_xex, "validated game")),
                patch.object(builder, "read_game_source_stamp", return_value=stamp),
                patch.object(builder, "check_iso") as check_iso,
                patch.object(builder, "run") as run,
            ):
                builder.prepare_game(iso)
            check_iso.assert_not_called()
            run.assert_not_called()

    def test_legacy_staged_game_gets_a_stamp_without_full_reextraction(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            iso = root / "Tekken.iso"
            iso.write_bytes(b"fake ISO for legacy-stamp test")
            staged_xex = root / "native" / "game" / "Default.xex"
            staged_xex.parent.mkdir(parents=True)
            staged_xex.write_bytes(b"already validated XEX")
            with (
                patch.object(builder, "check_game_folder", return_value=(staged_xex, "validated game")),
                patch.object(builder, "read_game_source_stamp", return_value=None),
                patch.object(builder, "check_iso", return_value=("Default.xex", "supported ISO")) as check_iso,
                patch.object(builder, "write_game_source_stamp") as write_stamp,
                patch.object(builder, "run") as run,
            ):
                builder.prepare_game(iso)
            check_iso.assert_called_once_with(iso)
            write_stamp.assert_called_once_with(iso)
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main(verbosity=2)
