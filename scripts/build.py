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
MSVC_DEVELOPER_ENVIRONMENT_VARS = ("WindowsSdkDir", "WindowsSDKVersion", "VCToolsInstallDir", "INCLUDE", "LIB")
STEAMWORKS_SDK_ENV_VARS = ("CORE_ENGINE_STEAMWORKS_SDK_DIR", "STEAMWORKS_SDK_DIR", "STEAM_SDK_DIR")
STEAMWORKS_REQUIRED_HEADERS = (
    Path("public") / "steam" / "steam_api.h",
    Path("public") / "steam" / "steam_api_common.h",
    Path("public") / "steam" / "steamclientpublic.h",
    Path("public") / "steam" / "steamnetworkingtypes.h",
    Path("public") / "steam" / "steamtypes.h",
    Path("public") / "steam" / "isteamnetworkingsockets.h",
    Path("public") / "steam" / "isteamnetworkingutils.h",
)


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
    msvc_environment_attempted: bool = False


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
    path = env_get(values, "PATH") if os.name == "nt" else values.get("PATH")
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


def env_get(env: dict[str, str], name: str) -> str | None:
    value = env.get(name)
    if value is not None or os.name != "nt":
        return value
    target = name.upper()
    return next((value for key, value in env.items() if key.upper() == target), None)


def env_set_canonical(env: dict[str, str], name: str, value: str) -> None:
    if os.name == "nt":
        target = name.upper()
        for key in list(env):
            if key.upper() == target and key != name:
                env.pop(key, None)
    env[name] = value


def normalize_windows_environment(env: dict[str, str]) -> None:
    if os.name != "nt":
        return

    path_keys = [key for key in env if key.upper() == "PATH"]
    if not path_keys:
        return

    path_value = env.get("PATH") or env.get("Path") or env[path_keys[0]]
    for key in path_keys:
        env.pop(key, None)
    env["PATH"] = path_value


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
        normalize_windows_environment(ctx.env)


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


def infer_vc_tools_install_dir(env: dict[str, str]) -> str | None:
    vc_install_dir = env_get(env, "VCINSTALLDIR")
    vc_tools_version = env_get(env, "VCToolsVersion")
    if vc_install_dir and vc_tools_version:
        candidate = Path(vc_install_dir) / "Tools" / "MSVC" / vc_tools_version
        if candidate.exists():
            return str(candidate) + os.sep

    cl_path = command_path("cl", env)
    if cl_path:
        parents = Path(cl_path).resolve().parents
        if len(parents) > 3:
            candidate = parents[3]
            if (candidate / "include").exists() and (candidate / "lib").exists():
                return str(candidate) + os.sep

    include = env_get(env, "INCLUDE") or ""
    for entry in include.split(os.pathsep):
        path = Path(entry)
        if path.name.lower() == "include" and path.parent.name:
            candidate = path.parent
            if (candidate / "bin").exists() and (candidate / "lib").exists():
                return str(candidate) + os.sep
    return None


def normalize_msvc_developer_environment(env: dict[str, str]) -> None:
    for name in MSVC_DEVELOPER_ENVIRONMENT_VARS:
        value = env_get(env, name)
        if value:
            env_set_canonical(env, name, value)

    if not env_get(env, "VCToolsInstallDir"):
        inferred = infer_vc_tools_install_dir(env)
        if inferred:
            env_set_canonical(env, "VCToolsInstallDir", inferred)


def msvc_developer_environment_ready(env: dict[str, str]) -> bool:
    normalize_msvc_developer_environment(env)
    return all(env_get(env, name) for name in MSVC_DEVELOPER_ENVIRONMENT_VARS)


def load_msvc_environment(ctx: Context, *, require_developer_environment: bool = False) -> bool:
    normalize_windows_environment(ctx.env)
    normalize_msvc_developer_environment(ctx.env)
    if command_path("cl", ctx.env) and (
        not require_developer_environment or msvc_developer_environment_ready(ctx.env)
    ):
        return True

    if ctx.msvc_environment_attempted:
        return False

    vsdevcmd = find_vsdevcmd()
    if not vsdevcmd:
        return False

    # cmd.exe treats backslash-escaped quotes literally, so this command is
    # passed as a string instead of a subprocess argument list.
    cmd = f'cmd.exe /d /c call "{vsdevcmd}" -arch=x64 -host_arch=x64 >nul && set'
    log(f"Loading Visual Studio environment from {vsdevcmd}")
    ctx.msvc_environment_attempted = True
    if ctx.dry_run:
        if require_developer_environment:
            for name in MSVC_DEVELOPER_ENVIRONMENT_VARS:
                env_set_canonical(ctx.env, name, "<dry-run>")
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
        if key.upper() == "PATH":
            ctx.env["PATH"] = value
            continue
        ctx.env[key] = value
    normalize_windows_environment(ctx.env)
    normalize_msvc_developer_environment(ctx.env)
    if not command_path("cl", ctx.env):
        return False
    return not require_developer_environment or msvc_developer_environment_ready(ctx.env)


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
            if load_msvc_environment(ctx, require_developer_environment=ctx.args.diligent):
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
    installable_missing = [item for item in missing_required if item.install_key]
    missing_keys = sorted({item.install_key for item in installable_missing if item.install_key})

    if not missing_required:
        log("No installable missing requirements detected.")
        return

    if not installable_missing:
        raise SystemExit("Missing requirements are not installable by this script. Resolve the paths/configuration above.")

    if not ctx.args.yes and not ctx.dry_run:
        log("Missing installable requirements:")
        for item in installable_missing:
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
        if load_msvc_environment(ctx, require_developer_environment=ctx.args.diligent):
            compiler_ok = True
            compiler_detail = command_version(["cl"], ctx.env) or "MSVC detected"
        else:
            install_key = "msvc"
            compiler_detail = "MSVC Build Tools not found"

    allow_non_msvc_compiler = not (ctx.system == "Windows" and ctx.args.diligent and compiler == "auto")

    if not compiler_ok and allow_non_msvc_compiler and compiler in ("auto", "clang"):
        clang_version = command_version(["clang++", "--version"], ctx.env)
        if clang_version:
            compiler_ok = True
            compiler_detail = clang_version

    if not compiler_ok and allow_non_msvc_compiler and compiler in ("auto", "gcc"):
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

    if ctx.args.steam:
        results.extend(check_steam_requirements(ctx))

    return results


def check_diligent_requirements(ctx: Context) -> list[CheckResult]:
    results: list[CheckResult] = []
    if ctx.system == "Windows":
        load_msvc_environment(ctx, require_developer_environment=True)
        missing = [name for name in MSVC_DEVELOPER_ENVIRONMENT_VARS if not env_get(ctx.env, name)]
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


def default_steamworks_sdk_candidates(ctx: Context) -> list[Path]:
    candidates: list[Path] = []
    if ctx.args.steamworks_sdk_dir:
        candidates.append(Path(ctx.args.steamworks_sdk_dir))

    for name in STEAMWORKS_SDK_ENV_VARS:
        value = env_get(ctx.env, name)
        if value:
            candidates.append(Path(value))

    candidates.extend(
        [
            ROOT / "steam_sdk",
            ROOT / "steamworks_sdk",
            ROOT / "third_party" / "steam_sdk",
            ROOT / "third_party" / "steamworks_sdk",
            ROOT / "core_engine" / "third_party" / "steam_sdk",
            ROOT / "core_engine" / "third_party" / "steamworks_sdk",
        ]
    )
    return candidates


def has_steamworks_header(path: Path) -> bool:
    return (path / "public" / "steam" / "steam_api.h").exists()


def missing_steamworks_headers(path: Path) -> list[Path]:
    return [relative for relative in STEAMWORKS_REQUIRED_HEADERS if not (path / relative).exists()]


def resolve_steamworks_sdk_dir(ctx: Context) -> Path | None:
    fallback: Path | None = None
    header_fallback: Path | None = None
    for candidate in default_steamworks_sdk_candidates(ctx):
        if not str(candidate):
            continue
        resolved = candidate.expanduser().resolve()
        if fallback is None:
            fallback = resolved
        if has_steamworks_header(resolved):
            if not missing_steamworks_headers(resolved):
                return resolved
            if header_fallback is None:
                header_fallback = resolved
    return header_fallback or fallback


def steamworks_import_library(ctx: Context, sdk_dir: Path) -> Path | None:
    if ctx.system == "Windows":
        for relative in (
            Path("public") / "steam" / "lib" / "win64" / "steam_api64.lib",
            Path("redistributable_bin") / "win64" / "steam_api64.lib",
        ):
            candidate = sdk_dir / relative
            if candidate.exists():
                return candidate
    elif ctx.system == "Darwin":
        candidate = sdk_dir / "redistributable_bin" / "osx" / "libsteam_api.dylib"
        if candidate.exists():
            return candidate
    else:
        candidate = sdk_dir / "redistributable_bin" / "linux64" / "libsteam_api.so"
        if candidate.exists():
            return candidate
    return None


def steamworks_runtime_library(ctx: Context, sdk_dir: Path) -> Path:
    if ctx.system == "Windows":
        return sdk_dir / "redistributable_bin" / "win64" / "steam_api64.dll"
    if ctx.system == "Darwin":
        return sdk_dir / "redistributable_bin" / "osx" / "libsteam_api.dylib"
    return sdk_dir / "redistributable_bin" / "linux64" / "libsteam_api.so"


def steam_client_running(ctx: Context) -> bool | None:
    if ctx.system != "Windows" or ctx.dry_run:
        return None
    try:
        result = subprocess.run(
            ["tasklist", "/FI", "IMAGENAME eq steam.exe"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except OSError:
        return None
    return "steam.exe" in (result.stdout or "").lower()


def check_steam_requirements(ctx: Context) -> list[CheckResult]:
    results: list[CheckResult] = [
        CheckResult("Steamworks integration", True, "enabled by default; pass --no-steam for explicit offline builds")
    ]

    if not str(ctx.args.steam_app_id).isdigit() or int(ctx.args.steam_app_id) <= 0:
        results.append(CheckResult("Steam AppID", False, f"invalid AppID: {ctx.args.steam_app_id}"))
    else:
        results.append(CheckResult("Steam AppID", True, str(ctx.args.steam_app_id)))

    sdk_dir = resolve_steamworks_sdk_dir(ctx)
    sdk_hint = "set --steamworks-sdk-dir or CORE_ENGINE_STEAMWORKS_SDK_DIR"
    if sdk_dir is None or not has_steamworks_header(sdk_dir):
        detail = f"not found; {sdk_hint}"
        if sdk_dir is not None:
            detail = f"missing public/steam/steam_api.h under {sdk_dir}; {sdk_hint}"
        results.append(CheckResult("Steamworks SDK", False, detail))
        return results

    results.append(CheckResult("Steamworks SDK", True, str(sdk_dir)))

    missing_headers = missing_steamworks_headers(sdk_dir)
    results.append(
        CheckResult(
            "Steamworks headers",
            not missing_headers,
            "complete"
            if not missing_headers
            else "missing "
            + ", ".join(str(path).replace("\\", "/") for path in missing_headers)
            + "; copy the full SDK public/steam directory",
        )
    )

    import_library = steamworks_import_library(ctx, sdk_dir)
    results.append(
        CheckResult(
            "Steamworks import library",
            import_library is not None,
            str(import_library) if import_library is not None else f"not found under {sdk_dir}",
        )
    )

    runtime_library = steamworks_runtime_library(ctx, sdk_dir)
    results.append(
        CheckResult(
            "Steamworks runtime library",
            runtime_library.exists(),
            str(runtime_library) if runtime_library.exists() else f"not found: {runtime_library}",
        )
    )

    running = steam_client_running(ctx)
    if running is not None:
        results.append(
            CheckResult(
                "Steam client",
                running,
                "running" if running else "not running; overlay requires the Steam client",
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
        f"-DCORE_ENGINE_ENABLE_STEAM={'ON' if ctx.args.steam else 'OFF'}",
        f"-DCORE_ENGINE_STEAM_APP_ID={ctx.args.steam_app_id}",
    ]

    if ctx.args.steam:
        steamworks_sdk_dir = resolve_steamworks_sdk_dir(ctx)
        if steamworks_sdk_dir is not None:
            configure_cmd.append(f"-DCORE_ENGINE_STEAMWORKS_SDK_DIR={steamworks_sdk_dir}")

    if is_single_config_generator(ctx.args.generator):
        configure_cmd.append(f"-DCMAKE_BUILD_TYPE={ctx.args.config}")

    if ctx.args.cmake_arg:
        configure_cmd.extend(ctx.args.cmake_arg)

    run(configure_cmd, ctx=ctx)


def build(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    build_cmd = ["cmake", "--build", str(build_dir), "--config", ctx.args.config]
    target_name = build_target_name(ctx)
    if target_name:
        build_cmd.extend(["--target", target_name])
    if ctx.args.jobs:
        build_cmd.extend(["--parallel", str(ctx.args.jobs)])
    else:
        build_cmd.append("--parallel")
    run(build_cmd, ctx=ctx)


def is_single_config_generator(generator: str) -> bool:
    lowered = generator.lower()
    return "visual studio" not in lowered and "xcode" not in lowered and "multi-config" not in lowered


def run_executable(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    target_name = run_target_name(ctx)
    candidates = executable_candidates(build_dir, ctx.args.config, target_name)
    for candidate in candidates:
        if candidate.exists():
            run([str(candidate), *ctx.args.run_arg], ctx=ctx, cwd=candidate.parent)
            return
    raise SystemExit(
        f"Built executable '{target_name}' was not found. Build succeeded, but --run could not locate it. "
        "Use --run-target <name-or-path> to select the executable."
    )


def package_executable(ctx: Context) -> None:
    build_dir = Path(ctx.args.build_dir).resolve()
    target_name = run_target_name(ctx)
    source_executable = next((path for path in executable_candidates(build_dir, ctx.args.config, target_name) if path.exists()), None)
    if source_executable is None:
        raise SystemExit(
            f"Built executable '{target_name}' was not found. Build succeeded, but --package could not locate it."
        )

    package_name = package_directory_name(source_executable, ctx)
    package_root = Path(ctx.args.package_dir).resolve()
    package_dir = package_root / package_name
    if package_dir.exists():
        package_dir.relative_to(package_root)
        shutil.rmtree(package_dir)
    copy_runtime_bundle(source_executable.parent, package_dir, ctx.args.config)

    log(f"Packaged {source_executable.name} into {package_dir}")
    if ctx.system == "Windows" and ctx.args.config == "Debug":
        log("Note: Debug packages copy MSVC Debug CRT DLLs for developer-machine handoff builds. Use --config RelWithDebInfo or Release for portable sharing.")


def package_directory_name(executable: Path, ctx: Context) -> str:
    system_name = ctx.system.lower()
    machine_name = platform.machine().lower() or "unknown"
    return f"{executable.stem}-{ctx.args.config}-{system_name}-{machine_name}"


def copy_runtime_bundle(source_dir: Path, destination_dir: Path, config: str) -> None:
    destination_dir.mkdir(parents=True, exist_ok=True)
    for item in source_dir.iterdir():
        if should_skip_package_item(item, config):
            continue

        target = destination_dir / item.name
        if item.is_dir():
            shutil.copytree(item, target, dirs_exist_ok=True, ignore=lambda directory, names: package_ignore(directory, names, config))
        else:
            shutil.copy2(item, target)


def package_ignore(directory: str, names: list[str], config: str) -> set[str]:
    ignored: set[str] = set()
    for name in names:
        path = Path(directory) / name
        if should_skip_package_item(path, config):
            ignored.add(name)
    return ignored


def should_skip_package_item(path: Path, config: str | None = None) -> bool:
    if path.name in {"CMakeFiles", "cmake_install.cmake"}:
        return True
    lowered = path.name.lower()
    if lowered.endswith((".ninja", ".ninja_deps", ".ninja_log", ".cmake")):
        return True
    if config:
        return should_skip_config_specific_runtime(lowered, config)
    return False


def should_skip_config_specific_runtime(lowered_name: str, config: str) -> bool:
    is_debug = config.lower() == "debug"
    if lowered_name.endswith(".dll"):
        if is_debug:
            return (
                is_release_msvc_runtime(lowered_name)
                or is_release_windows_ucrt_runtime(lowered_name)
                or is_release_diligent_runtime(lowered_name)
            )
        return is_debug_msvc_runtime(lowered_name) or is_debug_windows_ucrt_runtime(lowered_name) or is_debug_diligent_runtime(lowered_name)
    return False


def is_debug_msvc_runtime(lowered_name: str) -> bool:
    debug_runtime_names = {
        "concrt140d.dll",
        "msvcp140d.dll",
        "msvcp140d_atomic_wait.dll",
        "msvcp140d_codecvt_ids.dll",
        "msvcp140_1d.dll",
        "msvcp140_2d.dll",
        "vccorlib140d.dll",
        "vcruntime140d.dll",
        "vcruntime140_1d.dll",
        "vcruntime140_threadsd.dll",
    }
    return lowered_name in debug_runtime_names


def is_release_msvc_runtime(lowered_name: str) -> bool:
    release_runtime_names = {
        "concrt140.dll",
        "msvcp140.dll",
        "msvcp140_atomic_wait.dll",
        "msvcp140_codecvt_ids.dll",
        "msvcp140_1.dll",
        "msvcp140_2.dll",
        "vccorlib140.dll",
        "vcruntime140.dll",
        "vcruntime140_1.dll",
        "vcruntime140_threads.dll",
    }
    return lowered_name in release_runtime_names


def is_debug_windows_ucrt_runtime(lowered_name: str) -> bool:
    return lowered_name == "ucrtbased.dll"


def is_release_windows_ucrt_runtime(lowered_name: str) -> bool:
    return (
        lowered_name == "ucrtbase.dll"
        or lowered_name.startswith("api-ms-win-crt-")
        or lowered_name.startswith("api-ms-win-core-")
    )


def is_debug_diligent_runtime(lowered_name: str) -> bool:
    return lowered_name.startswith("graphicsengine") and lowered_name.endswith("_64d.dll")


def is_release_diligent_runtime(lowered_name: str) -> bool:
    return lowered_name.startswith("graphicsengine") and lowered_name.endswith("_64r.dll")


def run_target_name(ctx: Context) -> str:
    return ctx.args.run_target or ctx.args.target or "sandbox"


def build_target_name(ctx: Context) -> str:
    if ctx.args.target:
        return ctx.args.target
    if not ctx.args.run and not ctx.args.package:
        return ""

    target_name = ctx.args.run_target or "sandbox"
    target_path = Path(target_name)
    if target_path.is_absolute() or target_path.parent != Path("."):
        return ""
    if platform.system() == "Windows" and target_path.suffix.lower() == ".exe":
        return target_path.stem
    return target_name


def executable_candidates(build_dir: Path, config: str, target_name: str) -> list[Path]:
    target_path = Path(target_name)
    executable = executable_name(target_path.name)
    candidates: list[Path] = []

    if target_path.is_absolute():
        candidates.append(target_path)
    elif target_path.parent != Path("."):
        candidates.extend([ROOT / target_path, build_dir / target_path])

    candidates.extend(
        [
            build_dir / "app" / config / executable,
            build_dir / config / executable,
            build_dir / "app" / executable,
            build_dir / executable,
        ]
    )

    if build_dir.exists():
        candidates.extend(
            path for path in build_dir.rglob(executable) if "CMakeFiles" not in path.parts
        )

    unique_candidates: list[Path] = []
    seen: set[Path] = set()
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        unique_candidates.append(candidate)
    return unique_candidates


def executable_name(name: str) -> str:
    if platform.system() == "Windows" and Path(name).suffix.lower() != ".exe":
        return f"{name}.exe"
    return name


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Bootstrap and build CoreEngine.")
    parser.add_argument("--install", action="store_true", help="Install missing requirements before building.")
    parser.add_argument("--yes", "-y", action="store_true", help="Do not prompt before installing missing packages.")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without executing them.")
    parser.add_argument("--check-only", action="store_true", help="Only check requirements; do not configure or build.")
    parser.add_argument("--clean", action="store_true", help="Delete the build directory before configuring.")
    parser.add_argument("--diligent", action="store_true", help="Enable the experimental Diligent renderer backend.")
    parser.add_argument("--steam", dest="steam", action="store_true", default=True, help="Enable Steamworks integration. Enabled by default.")
    parser.add_argument("--no-steam", dest="steam", action="store_false", help="Disable Steamworks integration for explicit offline builds.")
    parser.add_argument(
        "--steamworks-sdk-dir",
        default="",
        help="Path to the Steamworks SDK root. Defaults to CORE_ENGINE_STEAMWORKS_SDK_DIR, STEAMWORKS_SDK_DIR, or STEAM_SDK_DIR.",
    )
    parser.add_argument("--steam-app-id", default="480", help="Steam AppID used by development builds.")
    parser.add_argument("--run", action="store_true", help="Run an executable after building.")
    parser.add_argument("--package", action="store_true", help="Copy the built executable bundle to a dist directory after building.")
    parser.add_argument("--package-dir", default=str(ROOT / "dist"), help="Directory that receives --package bundles.")
    parser.add_argument(
        "--run-target",
        default="",
        help="Executable target/name/path to run. Defaults to --target, or sandbox when --target is empty.",
    )
    parser.add_argument(
        "--run-arg",
        action="append",
        default=[],
        help="Argument passed to the executable when using --run. Can be repeated.",
    )
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
    normalize_windows_environment(ctx.env)
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
        if any(item.install_key for item in missing_tools):
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
    if args.package:
        package_executable(ctx)
    if args.run:
        run_executable(ctx)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
