#!/usr/bin/env python3
"""Bootstrap and build CoreEngine on Windows, Linux, and macOS.

The script always checks for tools before installing anything. By default it
only reports missing requirements and builds when the machine is ready. Pass
--install to install missing requirements, or --dry-run to print the commands.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parent
DEFAULT_BUILD_DIR = ROOT / "build"


@dataclass
class CheckResult:
    name: str
    ok: bool
    detail: str
    install_key: str | None = None
    required: bool = True


@dataclass
class Context:
    args: argparse.Namespace
    system: str
    dry_run: bool = False
    env: dict[str, str] = field(default_factory=lambda: dict(os.environ))
    package_manager: str | None = None


def log(message: str) -> None:
    print(message, flush=True)


def run(
    cmd: Sequence[str],
    *,
    ctx: Context,
    check: bool = True,
    capture: bool = False,
    env: dict[str, str] | None = None,
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    pretty = " ".join(quote_arg(part) for part in cmd)
    log(f"+ {pretty}")
    if ctx.dry_run:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    run_env = env or ctx.env
    return subprocess.run(
        resolve_command(cmd, run_env),
        cwd=str(cwd or ROOT),
        env=run_env,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def quote_arg(value: str) -> str:
    if not value:
        return '""'
    if any(ch.isspace() for ch in value):
        return '"' + value.replace('"', '\\"') + '"'
    return value


def command_path(name: str, env: dict[str, str] | None = None) -> str | None:
    values = env or os.environ
    path = values.get("PATH")
    if path is None and os.name == "nt":
        path = next((value for key, value in values.items() if key.upper() == "PATH"), None)
    return shutil.which(name, path=path)


def resolve_command(cmd: Sequence[str], env: dict[str, str] | None = None) -> list[str]:
    resolved = list(cmd)
    if not resolved:
        return resolved

    executable = resolved[0]
    if os.path.dirname(executable):
        return resolved

    found = command_path(executable, env)
    if found:
        resolved[0] = found
    return resolved


def command_version(cmd: Sequence[str], env: dict[str, str] | None = None) -> str | None:
    exe = command_path(cmd[0], env)
    if not exe:
        return None
    try:
        result = subprocess.run(
            [exe, *cmd[1:]],
            env=env or os.environ,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError:
        return exe
    first_line = (result.stdout or "").strip().splitlines()
    return first_line[0] if first_line else exe


def refresh_windows_path(ctx: Context) -> None:
    if ctx.system != "Windows":
        return
    try:
        import winreg
    except ImportError:
        return

    values: list[str] = []
    for hive, key_name in (
        (winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment"),
        (winreg.HKEY_CURRENT_USER, r"Environment"),
    ):
        try:
            with winreg.OpenKey(hive, key_name) as key:
                value, _ = winreg.QueryValueEx(key, "Path")
                if value:
                    values.append(os.path.expandvars(value))
        except OSError:
            continue
    if values:
        current = ctx.env.get("PATH", "")
        ctx.env["PATH"] = os.pathsep.join(values + [current])


def find_vswhere() -> Path | None:
    found = command_path("vswhere")
    if found:
        return Path(found)
    candidate = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    candidate = candidate / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    return candidate if candidate.exists() else None


def find_vsdevcmd() -> Path | None:
    vswhere = find_vswhere()
    if vswhere:
        try:
            result = subprocess.run(
                [
                    str(vswhere),
                    "-latest",
                    "-products",
                    "*",
                    "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-property",
                    "installationPath",
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                check=False,
            )
            install_path = result.stdout.strip()
            if install_path:
                candidate = Path(install_path) / "Common7" / "Tools" / "VsDevCmd.bat"
                if candidate.exists():
                    return candidate
        except OSError:
            pass

    root = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
    editions = ("Community", "Professional", "Enterprise", "BuildTools")
    for edition in editions:
        candidate = root / "Microsoft Visual Studio" / "2022" / edition / "Common7" / "Tools" / "VsDevCmd.bat"
        if candidate.exists():
            return candidate
    return None


def load_msvc_environment(ctx: Context) -> bool:
    if command_path("cl", ctx.env):
        return True

    vsdevcmd = find_vsdevcmd()
    if not vsdevcmd:
        return False

    # cmd.exe treats backslash-escaped quotes literally, so this command is
    # passed as a string instead of a subprocess argument list.
    cmd = f'cmd.exe /d /c call "{vsdevcmd}" -arch=x64 -host_arch=x64 >nul && set'
    log(f"Loading Visual Studio environment from {vsdevcmd}")
    if ctx.dry_run:
        return True

    result = subprocess.run(
        cmd,
        env=ctx.env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        log(result.stderr.strip())
        return False

    for line in result.stdout.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        ctx.env[key] = value
        if key.upper() == "PATH":
            ctx.env["PATH"] = value
    return command_path("cl", ctx.env) is not None


def detect_package_manager(ctx: Context) -> str | None:
    if ctx.system == "Windows":
        if command_path("winget", ctx.env):
            return "winget"
        if command_path("choco", ctx.env):
            return "choco"
        return None

    if ctx.system == "Darwin":
        if command_path("brew", ctx.env):
            return "brew"
        return None

    managers = ("apt-get", "dnf", "pacman", "zypper", "apk")
    for manager in managers:
        if command_path(manager, ctx.env):
            return manager
    if command_path("brew", ctx.env):
        return "brew"
    return None


def package_installed(manager: str, package_name: str) -> bool:
    checks: dict[str, list[str]] = {
        "apt-get": ["dpkg", "-s", package_name],
        "dnf": ["rpm", "-q", package_name],
        "pacman": ["pacman", "-Q", package_name],
        "zypper": ["rpm", "-q", package_name],
        "apk": ["apk", "info", "-e", package_name],
        "brew": ["brew", "list", "--versions", package_name],
        "choco": ["choco", "list", "--local-only", "--exact", package_name],
    }
    cmd = checks.get(manager)
    if not cmd or not command_path(cmd[0]):
        return False
    result = subprocess.run(
        cmd,
        text=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def sudo_prefix() -> list[str]:
    if os.name == "nt":
        return []
    if hasattr(os, "geteuid") and os.geteuid() == 0:
        return []
    return ["sudo"]


WINDOWS_WINGET_PACKAGES = {
    "git": ["install", "--id", "Git.Git", "-e", "--source", "winget"],
    "cmake": ["install", "--id", "Kitware.CMake", "-e", "--source", "winget"],
    "ninja": ["install", "--id", "Ninja-build.Ninja", "-e", "--source", "winget"],
    "msvc": [
        "install",
        "--id",
        "Microsoft.VisualStudio.2022.BuildTools",
        "-e",
        "--source",
        "winget",
        "--override",
        "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended",
    ],
}

WINDOWS_CHOCO_PACKAGES = {
    "git": "git",
    "cmake": "cmake",
    "ninja": "ninja",
    "msvc": "visualstudio2022buildtools",
}

LINUX_PACKAGES = {
    "apt-get": [
        "build-essential",
        "cmake",
        "ninja-build",
        "git",
        "pkg-config",
        "libx11-dev",
        "libxext-dev",
        "libxrandr-dev",
        "libxcursor-dev",
        "libxi-dev",
        "libxfixes-dev",
        "libxinerama-dev",
        "libwayland-dev",
        "wayland-protocols",
        "libxkbcommon-dev",
        "libgl1-mesa-dev",
        "libegl1-mesa-dev",
        "libdbus-1-dev",
        "libudev-dev",
        "libpipewire-0.3-dev",
        "libpulse-dev",
        "libasound2-dev",
    ],
    "dnf": [
        "gcc",
        "gcc-c++",
        "cmake",
        "ninja-build",
        "git",
        "pkgconf-pkg-config",
        "libX11-devel",
        "libXext-devel",
        "libXrandr-devel",
        "libXcursor-devel",
        "libXi-devel",
        "libXfixes-devel",
        "libXinerama-devel",
        "wayland-devel",
        "wayland-protocols-devel",
        "libxkbcommon-devel",
        "mesa-libGL-devel",
        "mesa-libEGL-devel",
        "dbus-devel",
        "systemd-devel",
        "pipewire-devel",
        "pulseaudio-libs-devel",
        "alsa-lib-devel",
    ],
    "pacman": [
        "base-devel",
        "cmake",
        "ninja",
        "git",
        "pkgconf",
        "libx11",
        "libxext",
        "libxrandr",
        "libxcursor",
        "libxi",
        "libxfixes",
        "libxinerama",
        "wayland",
        "wayland-protocols",
        "libxkbcommon",
        "mesa",
        "dbus",
        "libsystemd",
        "pipewire",
        "libpulse",
        "alsa-lib",
    ],
    "zypper": [
        "gcc",
        "gcc-c++",
        "cmake",
        "ninja",
        "git",
        "pkg-config",
        "libX11-devel",
        "libXext-devel",
        "libXrandr-devel",
        "libXcursor-devel",
        "libXi-devel",
        "libXfixes-devel",
        "libXinerama-devel",
        "wayland-devel",
        "wayland-protocols-devel",
        "libxkbcommon-devel",
        "Mesa-libGL-devel",
        "Mesa-libEGL-devel",
        "dbus-1-devel",
        "libudev-devel",
        "pipewire-devel",
        "libpulse-devel",
        "alsa-devel",
    ],
    "apk": [
        "build-base",
        "cmake",
        "ninja",
        "git",
        "pkgconf",
        "linux-headers",
        "libx11-dev",
        "libxext-dev",
        "libxrandr-dev",
        "libxcursor-dev",
        "libxi-dev",
        "libxfixes-dev",
        "libxinerama-dev",
        "wayland-dev",
        "wayland-protocols",
        "libxkbcommon-dev",
        "mesa-dev",
        "dbus-dev",
        "eudev-dev",
        "pipewire-dev",
        "pulseaudio-dev",
        "alsa-lib-dev",
    ],
    "brew": ["cmake", "ninja", "git", "pkg-config"],
}

DARWIN_BREW_PACKAGES = ["cmake", "ninja", "git", "pkg-config"]


def install_windows(ctx: Context, keys: Iterable[str]) -> None:
    manager = ctx.package_manager
    if manager not in ("winget", "choco"):
        raise SystemExit("No Windows package manager found. Install winget or Chocolatey first.")

    for key in keys:
        if key == "msvc":
            if load_msvc_environment(ctx):
                log("MSVC developer environment already detected; skipping install.")
                continue
        elif command_path(key if key != "ninja" else "ninja", ctx.env):
            log(f"{key} already detected; skipping install.")
            continue

        if manager == "winget":
            args = WINDOWS_WINGET_PACKAGES[key]
            cmd = [
                "winget",
                *args,
                "--accept-source-agreements",
                "--accept-package-agreements",
            ]
        else:
            package = WINDOWS_CHOCO_PACKAGES[key]
            cmd = ["choco", "install", package, "-y"]
        run(cmd, ctx=ctx)

    refresh_windows_path(ctx)


def install_linux(ctx: Context) -> None:
    manager = ctx.package_manager
    if not manager or manager not in LINUX_PACKAGES:
        raise SystemExit("No supported Linux package manager found.")

    packages = LINUX_PACKAGES[manager]
    missing = [pkg for pkg in packages if not package_installed(manager, pkg)]
    if not missing:
        log("Linux packages already installed; skipping install.")
        return

    if manager == "apt-get":
        run([*sudo_prefix(), "apt-get", "update"], ctx=ctx)
        run([*sudo_prefix(), "apt-get", "install", "-y", *missing], ctx=ctx)
    elif manager == "dnf":
        run([*sudo_prefix(), "dnf", "install", "-y", *missing], ctx=ctx)
    elif manager == "pacman":
        run([*sudo_prefix(), "pacman", "-S", "--needed", "--noconfirm", *missing], ctx=ctx)
    elif manager == "zypper":
        run([*sudo_prefix(), "zypper", "install", "-y", *missing], ctx=ctx)
    elif manager == "apk":
        run([*sudo_prefix(), "apk", "add", *missing], ctx=ctx)
    elif manager == "brew":
        run(["brew", "install", *missing], ctx=ctx)


def install_darwin(ctx: Context) -> None:
    if not xcode_clt_installed():
        run(["xcode-select", "--install"], ctx=ctx, check=False)
        raise SystemExit(
            "Xcode Command Line Tools installation was requested. Re-run this script after it finishes."
        )

    if ctx.package_manager != "brew":
        raise SystemExit(
            "Homebrew was not found. Install Homebrew first, then re-run with --install."
        )

    missing = [pkg for pkg in DARWIN_BREW_PACKAGES if not package_installed("brew", pkg)]
    if missing:
        run(["brew", "install", *missing], ctx=ctx)
    else:
        log("Homebrew packages already installed; skipping install.")


def xcode_clt_installed() -> bool:
    if platform.system() != "Darwin":
        return True
    result = subprocess.run(
        ["xcode-select", "-p"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def install_missing(ctx: Context, results: list[CheckResult]) -> None:
    missing_required = [item for item in results if item.required and not item.ok]
    missing_keys = sorted({item.install_key for item in missing_required if item.install_key})

    if not missing_required:
        log("No installable missing requirements detected.")
        return

    if not ctx.args.yes and not ctx.dry_run:
        log("Missing installable requirements:")
        for item in missing_required:
            log(f"  - {item.name}: {item.detail}")
        reply = input("Install them now? [y/N] ").strip().lower()
        if reply not in ("y", "yes", "s", "sim"):
            raise SystemExit("Installation cancelled.")

    if ctx.system == "Windows":
        if not missing_keys:
            raise SystemExit("No Windows installer mapping is available for the missing requirements.")
        install_windows(ctx, missing_keys)
    elif ctx.system == "Darwin":
        install_darwin(ctx)
    else:
        install_linux(ctx)


def check_vendor_sources(enable_diligent: bool) -> list[CheckResult]:
    required_paths = [
        ("SDL3 source", ROOT / "core_engine" / "third_party" / "sdl3" / "CMakeLists.txt"),
        ("Assimp source", ROOT / "core_engine" / "third_party" / "Assimp" / "CMakeLists.txt"),
        ("glm source", ROOT / "core_engine" / "third_party" / "glm" / "CMakeLists.txt"),
        ("EnTT source", ROOT / "core_engine" / "third_party" / "entt" / "CMakeLists.txt"),
        ("robin-map source", ROOT / "core_engine" / "third_party" / "robin-map" / "CMakeLists.txt"),
        (
            "Dear ImGui source",
            ROOT / "core_engine" / "third_party" / "DiligentTools" / "ThirdParty" / "imgui" / "imgui.cpp",
        ),
    ]
    if enable_diligent:
        required_paths.extend(
            [
                (
                    "DiligentCore source",
                    ROOT / "core_engine" / "third_party" / "DiligentCore" / "CMakeLists.txt",
                ),
                (
                    "DiligentTools source",
                    ROOT / "core_engine" / "third_party" / "DiligentTools" / "Imgui" / "CMakeLists.txt",
                ),
            ]
        )

    results = []
    for name, path in required_paths:
        results.append(
            CheckResult(
                name=name,
                ok=path.exists(),
                detail=str(path.relative_to(ROOT)) if path.exists() else f"missing: {path.relative_to(ROOT)}",
                install_key=None,
            )
        )
    return results


def update_submodules_if_needed(ctx: Context, results: list[CheckResult]) -> None:
    missing_sources = [item for item in results if item.name.endswith("source") and not item.ok]
    if not missing_sources:
        return
    if not command_path("git", ctx.env):
        raise SystemExit("Some vendor sources are missing, and git is not available to fetch submodules.")
    run(["git", "submodule", "update", "--init", "--recursive"], ctx=ctx)


def check_tools(ctx: Context) -> list[CheckResult]:
    results = []

    cmake_version = command_version(["cmake", "--version"], ctx.env)
    results.append(CheckResult("CMake", cmake_version is not None, cmake_version or "not found", "cmake"))

    git_version = command_version(["git", "--version"], ctx.env)
    results.append(CheckResult("Git", git_version is not None, git_version or "not found", "git"))

    ninja_version = command_version(["ninja", "--version"], ctx.env)
    generator = ctx.args.generator.lower()
    needs_ninja = "ninja" in generator
    results.append(
        CheckResult(
            "Ninja",
            ninja_version is not None or not needs_ninja,
            ninja_version or ("not required by selected generator" if not needs_ninja else "not found"),
            "ninja" if needs_ninja else None,
            required=needs_ninja,
        )
    )

    compiler_ok = False
    compiler_detail = "not found"
    install_key = None
    compiler = ctx.args.compiler

    if ctx.system == "Windows" and compiler in ("auto", "msvc"):
        if load_msvc_environment(ctx):
            compiler_ok = True
            compiler_detail = command_version(["cl"], ctx.env) or "MSVC detected"
        else:
            install_key = "msvc"
            compiler_detail = "MSVC Build Tools not found"

    if not compiler_ok and compiler in ("auto", "clang"):
        clang_version = command_version(["clang++", "--version"], ctx.env)
        if clang_version:
            compiler_ok = True
            compiler_detail = clang_version

    if not compiler_ok and compiler in ("auto", "gcc"):
        gcc_version = command_version(["g++", "--version"], ctx.env)
        if gcc_version:
            compiler_ok = True
            compiler_detail = gcc_version

    if ctx.system != "Windows" and not compiler_ok:
        install_key = "build-tools"
        compiler_detail = "no C++ compiler found; install command will use the platform build package set"

    results.append(CheckResult("C++ compiler", compiler_ok, compiler_detail, install_key))

    if ctx.system == "Darwin":
        results.append(
            CheckResult(
                "Xcode Command Line Tools",
                xcode_clt_installed(),
                "installed" if xcode_clt_installed() else "not installed",
                "xcode-clt",
            )
        )

    if ctx.args.diligent:
        results.extend(check_diligent_requirements(ctx))

    return results


def check_diligent_requirements(ctx: Context) -> list[CheckResult]:
    results: list[CheckResult] = []
    if ctx.system == "Windows":
        sdk_vars = ("WindowsSdkDir", "WindowsSDKVersion", "VCToolsInstallDir", "INCLUDE", "LIB")
        missing = [name for name in sdk_vars if not ctx.env.get(name)]
        results.append(
            CheckResult(
                "MSVC developer environment",
                not missing,
                "ready" if not missing else "missing env vars: " + ", ".join(missing),
                "msvc",
            )
        )
    else:
        vulkan = bool(os.environ.get("VULKAN_SDK")) or command_path("vulkaninfo", ctx.env) is not None
        results.append(
            CheckResult(
                "Vulkan SDK",
                vulkan,
                os.environ.get("VULKAN_SDK", "not found"),
                None,
                required=False,
            )
        )
    return results


def print_report(results: list[CheckResult]) -> None:
    log("")
    log("Requirement check:")
    for item in results:
        status = "OK" if item.ok else ("WARN" if not item.required else "MISSING")
        log(f"  [{status}] {item.name}: {item.detail}")
    log("")


def configure(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    if ctx.args.clean and build_dir.exists():
        shutil.rmtree(build_dir)

    configure_cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
        "-G",
        ctx.args.generator,
        f"-DCORE_ENGINE_ENABLE_DILIGENT={'ON' if ctx.args.diligent else 'OFF'}",
    ]

    if is_single_config_generator(ctx.args.generator):
        configure_cmd.append(f"-DCMAKE_BUILD_TYPE={ctx.args.config}")

    if ctx.args.cmake_arg:
        configure_cmd.extend(ctx.args.cmake_arg)

    run(configure_cmd, ctx=ctx)


def build(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    build_cmd = ["cmake", "--build", str(build_dir), "--config", ctx.args.config]
    if ctx.args.target:
        build_cmd.extend(["--target", ctx.args.target])
    if ctx.args.jobs:
        build_cmd.extend(["--parallel", str(ctx.args.jobs)])
    else:
        build_cmd.append("--parallel")
    run(build_cmd, ctx=ctx)


def is_single_config_generator(generator: str) -> bool:
    lowered = generator.lower()
    return "visual studio" not in lowered and "xcode" not in lowered and "multi-config" not in lowered


def run_sandbox(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    candidates = [
        build_dir / ctx.args.config / executable_name("sandbox"),
        build_dir / "app" / ctx.args.config / executable_name("sandbox"),
        build_dir / "app" / executable_name("sandbox"),
        build_dir / executable_name("sandbox"),
    ]
    for candidate in candidates:
        if candidate.exists():
            run([str(candidate)], ctx=ctx, cwd=candidate.parent)
            return
    raise SystemExit("Built sandbox executable was not found. Build succeeded, but --run could not locate it.")


def executable_name(name: str) -> str:
    return f"{name}.exe" if platform.system() == "Windows" else name


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bootstrap and build CoreEngine.")
    parser.add_argument("--install", action="store_true", help="Install missing requirements before building.")
    parser.add_argument("--yes", "-y", action="store_true", help="Do not prompt before installing missing packages.")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them.")
    parser.add_argument("--check-only", action="store_true", help="Only check requirements; do not configure or build.")
    parser.add_argument("--clean", action="store_true", help="Delete the build directory before configuring.")
    parser.add_argument("--diligent", action="store_true", help="Enable the experimental Diligent renderer backend.")
    parser.add_argument("--run", action="store_true", help="Run the sandbox executable after building.")
    parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR), help="CMake build directory.")
    parser.add_argument("--config", default="Debug", choices=("Debug", "Release", "RelWithDebInfo", "MinSizeRel"))
    parser.add_argument("--generator", default="Ninja", help="CMake generator, e.g. Ninja or Visual Studio 17 2022.")
    parser.add_argument("--target", default="", help="Optional CMake target, e.g. CoreEngine or sandbox.")
    parser.add_argument("--jobs", type=int, default=0, help="Parallel build jobs. Defaults to CMake's choice.")
    parser.add_argument("--compiler", choices=("auto", "msvc", "clang", "gcc"), default="auto")
    parser.add_argument(
        "--cmake-arg",
        action="append",
        default=[],
        help="Extra argument passed to CMake configure. Can be repeated.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])
    ctx = Context(args=args, system=platform.system(), dry_run=args.dry_run)
    refresh_windows_path(ctx)
    ctx.package_manager = detect_package_manager(ctx)

    vendor_results = check_vendor_sources(args.diligent)
    tool_results = check_tools(ctx)
    all_results = tool_results + vendor_results
    print_report(all_results)

    missing_tools = [item for item in tool_results if item.required and not item.ok]
    if missing_tools and args.install:
        install_missing(ctx, tool_results)
        if args.dry_run:
            return 0
        refresh_windows_path(ctx)
        ctx.package_manager = detect_package_manager(ctx)
        tool_results = check_tools(ctx)
        missing_tools = [item for item in tool_results if item.required and not item.ok]

    if missing_tools:
        log("Cannot build yet. Missing required items:")
        for item in missing_tools:
            log(f"  - {item.name}: {item.detail}")
        log("Re-run with --install to install what this script can install.")
        return 2

    update_submodules_if_needed(ctx, vendor_results)
    vendor_results = check_vendor_sources(args.diligent)
    missing_sources = [item for item in vendor_results if item.required and not item.ok]
    if missing_sources:
        log("Cannot build yet. Missing vendor source files:")
        for item in missing_sources:
            log(f"  - {item.name}: {item.detail}")
        log("Run: git submodule update --init --recursive")
        return 2

    if args.check_only:
        return 0

    configure(ctx)
    build(ctx)
    if args.run:
        run_sandbox(ctx)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
