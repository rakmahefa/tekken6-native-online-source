#!/usr/bin/env python3
"""Build the native Linux game from the player's own Tekken 6 Xbox 360 copy.

This script never downloads or ships game data. The extracted copy, generated
sources, SDK outputs and compiler objects remain under the ignored native/
and generated/default/ directories. CMake/Ninja keep their object cache across
builds so incremental rebuilds do not recompile the entire project.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / "native"
GAME = NATIVE / "game"
GAME_SOURCE_STAMP = NATIVE / "game-source.json"
BUILD = NATIVE / "build"
XDVDFS_BUILD = NATIVE / "xdvdfs-build"
XDVDFS = XDVDFS_BUILD / "xdvdfs_tool"
SDK = ROOT / "third_party" / "rexglue-sdk"
XEX_SHA256 = "ee138a4fe8d32a0edc131451377f4377041a49626153fa992db24f5420a7f15c"
PATCH_TOOLS = ("pagemap_patch", "rawstore_patch", "sndcall_patch", "faststore_patch")
VERSION = (ROOT / "builder" / "build.version").read_text(encoding="utf-8").strip()


def say(message: str) -> None:
    print(message, flush=True)


def run(args: list[str | os.PathLike[str]], what: str, cwd: Path = ROOT) -> None:
    command = [str(a) for a in args]
    say("$ " + " ".join(command))
    result = subprocess.run(command, cwd=cwd)
    if result.returncode:
        raise RuntimeError(f"{what} failed with exit code {result.returncode}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def find_xex(folder: Path) -> Path | None:
    try:
        return next(p for p in folder.iterdir() if p.is_file() and p.name.lower() == "default.xex")
    except (OSError, StopIteration):
        return None


def check_game_folder(folder: Path) -> tuple[Path | None, str]:
    if not folder.is_dir():
        return None, f"game folder not found: {folder}"
    xex = find_xex(folder)
    if not xex:
        return None, "no Default.xex found at the root of the selected folder"
    if not any(p.is_file() and p.name.upper().startswith("DATA") for p in folder.iterdir()):
        return None, "the game data (DATA*.BIN) is missing next to Default.xex"
    digest = sha256(xex)
    if digest != XEX_SHA256:
        return None, ("unsupported image; expected Tekken 6 USA/Europe Rev 1 (Xbox 360), "
                      f"Default.xex SHA-256 is {digest}")
    return xex, "Tekken 6 USA/Europe Rev 1"


def ensure_xdvdfs() -> None:
    if XDVDFS.is_file():
        return
    NATIVE.mkdir(parents=True, exist_ok=True)
    run(["cmake", "-S", ROOT / "tools" / "xdvdfs", "-B", XDVDFS_BUILD,
         "-DCMAKE_BUILD_TYPE=Release"], "configure XDVDFS extractor")
    run(["cmake", "--build", XDVDFS_BUILD, "--parallel"], "build XDVDFS extractor")
    if not XDVDFS.is_file():
        raise RuntimeError(f"XDVDFS extractor was not produced: {XDVDFS}")


def iso_file_list(iso: Path) -> list[str]:
    ensure_xdvdfs()
    with tempfile.TemporaryDirectory(prefix="t6-xdvdfs-") as temp:
        manifest = Path(temp) / "files.txt"
        run([XDVDFS, "list", iso, manifest], "list ISO contents")
        paths: list[str] = []
        for line in manifest.read_text(encoding="utf-8", errors="replace").splitlines():
            if "\t" in line:
                paths.append(line.split("\t", 1)[1].strip())
        return paths


def check_iso(iso: Path) -> tuple[str | None, str]:
    if not iso.is_file():
        return None, f"ISO not found: {iso}"
    try:
        paths = iso_file_list(iso)
    except (OSError, RuntimeError) as exc:
        return None, str(exc)
    xex_rel = next((p for p in paths if Path(p).name.lower() == "default.xex"), None)
    if not xex_rel:
        return None, "this is not a readable Xbox 360 ISO: Default.xex is missing"
    if not any(Path(p).name.upper().startswith("DATA") for p in paths):
        return None, "the ISO does not contain DATA*.BIN game files"
    ensure_xdvdfs()
    with tempfile.TemporaryDirectory(prefix="t6-xex-check-") as temp:
        extracted = Path(temp) / "Default.xex"
        run([XDVDFS, "extract", iso, xex_rel, extracted], "extract Default.xex for validation")
        digest = sha256(extracted)
    if digest != XEX_SHA256:
        return None, ("unsupported ISO; expected Tekken 6 USA/Europe Rev 1 (Xbox 360), "
                      f"Default.xex SHA-256 is {digest}")
    return xex_rel, "Tekken 6 USA/Europe Rev 1"


def iso_identity(source: Path) -> dict[str, str | int]:
    stat = source.stat()
    return {
        "kind": "iso",
        "path": str(source.resolve()),
        "size": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
    }


def read_game_source_stamp() -> dict[str, object] | None:
    try:
        value = json.loads(GAME_SOURCE_STAMP.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else None
    except (OSError, json.JSONDecodeError):
        return None


def write_game_source_stamp(source: Path) -> None:
    xex = find_xex(GAME)
    if not xex:
        raise RuntimeError("cannot record game source: native/game has no Default.xex")
    stamp = iso_identity(source) if source.is_file() else {
        "kind": "folder",
        "path": str(source.resolve()),
        "mtime_ns": source.stat().st_mtime_ns,
    }
    stamp["default_xex_sha256"] = sha256(xex)
    GAME_SOURCE_STAMP.parent.mkdir(parents=True, exist_ok=True)
    GAME_SOURCE_STAMP.write_text(json.dumps(stamp, indent=2) + "\\n", encoding="utf-8")


def prepare_game(source: Path, force_extract: bool = False) -> None:
    if source.is_dir():
        xex, info = check_game_folder(source)
        if not xex:
            raise RuntimeError(info)
        if source.resolve() == GAME.resolve():
            say(f"Game data already staged: {info}")
            return
        incoming = NATIVE / "game.incoming"
        shutil.rmtree(incoming, ignore_errors=True)
        say("Copying the player's game folder into native/game …")
        shutil.copytree(source, incoming, symlinks=True)
        staged_xex, staged_info = check_game_folder(incoming)
        if not staged_xex:
            shutil.rmtree(incoming, ignore_errors=True)
            raise RuntimeError(staged_info)
        shutil.rmtree(GAME, ignore_errors=True)
        incoming.replace(GAME)
        GAME_SOURCE_STAMP.unlink(missing_ok=True)
        say(f"Validated game data: {staged_info}")
        return

    if not source.is_file():
        raise RuntimeError(f"ISO not found: {source}")

    # The common rebuild path should only revalidate the staged XEX and the ISO's
    # cheap file identity, not unpack a 6+ GB disc on every invocation.
    existing_xex, existing_info = check_game_folder(GAME)
    existing_stamp = read_game_source_stamp()
    identity = iso_identity(source)
    if not force_extract and existing_xex and existing_stamp:
        stamp_identity = {k: existing_stamp.get(k) for k in ("kind", "path", "size", "mtime_ns")}
        if (stamp_identity == identity and
                existing_stamp.get("default_xex_sha256") == sha256(existing_xex)):
            say(f"Reusing validated native/game from unchanged ISO: {existing_info}")
            return

    xex_rel, info = check_iso(source)
    if not xex_rel:
        raise RuntimeError(info)

    # Older builder versions extracted the game but did not record the ISO identity.
    # Preserve those already validated local files on the first upgraded run. If the
    # user intentionally wants a fresh extraction, --force-extract is explicit.
    if not force_extract and existing_xex and existing_stamp is None:
        write_game_source_stamp(source)
        say(f"Reusing existing validated native/game (legacy unstamped extraction): {existing_info}")
        say("Recorded the selected ISO as the source; use --force-extract <ISO> to replace the staged data.")
        return

    NATIVE.mkdir(parents=True, exist_ok=True)
    incoming = NATIVE / "game.incoming"
    shutil.rmtree(incoming, ignore_errors=True)
    ensure_xdvdfs()
    say("Extracting the player's ISO into native/game …")
    run([XDVDFS, "extract-all", source, incoming], "extract game ISO")
    staged_xex, staged_info = check_game_folder(incoming)
    if not staged_xex:
        shutil.rmtree(incoming, ignore_errors=True)
        raise RuntimeError(staged_info)
    shutil.rmtree(GAME, ignore_errors=True)
    incoming.replace(GAME)
    write_game_source_stamp(source)
    say(f"Validated extracted game: {staged_info}")


def apply_sdk_patches() -> None:
    if not SDK.is_dir() or not (SDK / "CMakeLists.txt").is_file():
        raise RuntimeError("ReXGlue SDK missing; initialize third_party/rexglue-sdk first")
    patch_tool = ROOT / "tools" / "apply_rexglue_patches.py"
    run([sys.executable, patch_tool, "--sdk", SDK], "apply project ReXGlue patches")


def write_if_changed(path: Path, content: str) -> None:
    if path.is_file() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def write_manifest() -> Path:
    xex = find_xex(GAME)
    if not xex:
        raise RuntimeError("native/game does not contain Default.xex; choose your own game copy first")
    config_source = ROOT / "builder" / "project" / "tekken6_config.toml"
    config_target = ROOT / "tekken6_config.toml"
    write_if_changed(config_target, config_source.read_text(encoding="utf-8"))
    manifest = ROOT / "tekken6_manifest.toml"
    content = (
        '[project]\\n'
        'name = "tekken6"\\n'
        'sdk_version = "0.10.0"\\n'
        'game_root = "native/game"\\n\\n'
        '[entrypoint]\\n'
        f'file_path = "native/game/{xex.name}"\\n'
        'out_directory_path = "generated/default"\\n'
        'includes = ["tekken6_config.toml"]\\n'
    )
    write_if_changed(manifest, content)
    return manifest


def configure() -> None:
    run(["cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release",
         "-DCMAKE_C_COMPILER=clang",
         "-DCMAKE_CXX_COMPILER=clang++",
         f"-DREXSDK_DIR={SDK}",
         "-DREXGLUE_USE_VULKAN=ON",
         "-DREXGLUE_ENABLE_TRACY=OFF"], "configure native Linux game build")


def build_game() -> None:
    apply_sdk_patches()
    write_manifest()
    configure()

    say("STEP 1/4 Generating native sources from the player's Default.xex")
    run(["cmake", "--build", BUILD, "--target", "tekken6_codegen", "--parallel"], "ReXGlue code generation")

    say("STEP 2/4 Applying project netplay patches")
    gen = ROOT / "generated" / "default"
    for name in PATCH_TOOLS:
        run([sys.executable, "-I", ROOT / "tools" / f"{name}.py", gen], f"apply {name}")

    say("STEP 3/4 Reconfiguring to include generated C++ sources")
    configure()

    say("STEP 4/4 Compiling and linking (CMake/Ninja cache retained for the next run)")
    run(["cmake", "--build", BUILD, "--target", "tekken6", "--parallel"], "compile native Linux game")

    executable = BUILD / "tekken6"
    if not executable.is_file():
        raise RuntimeError(f"build succeeded but executable is missing: {executable}")
    # The ReXGlue target stages the Xenos GPU plugin next to the host executable.
    # Linux runtime libraries are produced in the SDK's platform output directory.
    staged: set[str] = set()
    for folder in (BUILD, SDK / "out" / "linux-amd64"):
        if not folder.is_dir():
            continue
        for library in folder.glob("*.so*"):
            target = NATIVE / library.name
            if library.resolve() == target.resolve() if target.exists() else False:
                continue
            shutil.copy2(library, target)
            staged.add(library.name)
    target_exe = NATIVE / "tekken6"
    shutil.copy2(executable, target_exe)
    target_exe.chmod(target_exe.stat().st_mode | 0o111)
    (NATIVE / "build.version").write_text(VERSION + "\n", encoding="utf-8")
    say(f"Linux executable ready: {target_exe}")
    if staged:
        say("Runtime modules staged: " + ", ".join(sorted(staged)))
    say(f"DONE {target_exe}")


def main(argv: list[str]) -> int:
    args = argv[1:]
    if len(args) == 2 and args[0] == "--check":
        source = Path(args[1]).expanduser().resolve()
        if source.is_dir():
            _, message = check_game_folder(source)
        else:
            _, message = check_iso(source)
        print(("OK " if message == "Tekken 6 USA/Europe Rev 1" else "ERROR ") + message, flush=True)
        return 0 if message == "Tekken 6 USA/Europe Rev 1" else 1

    force_extract = False
    if args == ["--update"] or args == ["--relink"]:
        if not check_game_folder(GAME)[0]:
            print("ERROR no validated game copy is staged in native/game", flush=True)
            return 1
        source = GAME
    elif len(args) == 2 and args[0] == "--force-extract":
        source = Path(args[1]).expanduser().resolve()
        force_extract = True
    elif len(args) == 1 and not args[0].startswith("--"):
        source = Path(args[0]).expanduser().resolve()
    elif not args:
        print("Usage: python3 builder/build_native_linux.py <Tekken 6 ISO or extracted folder>")
        print("       python3 builder/build_native_linux.py --check <ISO or folder>")
        print("       python3 builder/build_native_linux.py --update | --relink")
        print("       python3 builder/build_native_linux.py --force-extract <Tekken 6 ISO>")
        return 1
    else:
        print("ERROR invalid arguments; run without arguments for usage", flush=True)
        return 1

    NATIVE.mkdir(parents=True, exist_ok=True)
    log = NATIVE / "build.log"
    started = time.monotonic()
    try:
        NATIVE.mkdir(parents=True, exist_ok=True)
        if (args and not (args == ["--update"] or args == ["--relink"])):
            prepare_game(source, force_extract=force_extract)
        with log.open("a", encoding="utf-8") as stream:
            stream.write(f"\\n[{time.strftime('%Y-%m-%d %H:%M:%S')}] build source: {source}\\n")
        build_game()
        say(f"Build completed in {time.monotonic() - started:.0f}s")
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        say(f"ERROR {exc}")
        try:
            with log.open("a", encoding="utf-8") as stream:
                stream.write(f"ERROR {exc}\n")
        except OSError:
            pass
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
