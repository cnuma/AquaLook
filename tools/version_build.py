Import("env")

from pathlib import Path
import re
import subprocess


VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")


def git_value(args, fallback):
    try:
        return subprocess.check_output(
            ["git", *args],
            cwd=env.subst("$PROJECT_DIR"),
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:
        return fallback


def read_version(project_dir):
    version_path = Path(project_dir) / "VERSION"
    version = version_path.read_text(encoding="utf-8").strip()
    if not VERSION_PATTERN.fullmatch(version):
        raise ValueError(f"Invalid AquaLook VERSION value: {version!r}")
    return version


def config_manager_middleware(source_env, node):
    source_path = Path(node.get_path())
    if source_path.name != "ConfigManager.cpp":
        return source_env

    scoped_env = source_env.Clone()
    recovery_header = Path(project_dir) / "src" / "AquaLookPreferencesRecovery.h"
    scoped_env.Append(
        CXXFLAGS=["-include", str(recovery_header)]
    )
    return scoped_env


project_dir = env.subst("$PROJECT_DIR")
version = read_version(project_dir)
build_number = git_value(["rev-list", "--count", "HEAD"], "local")
git_sha = git_value(["rev-parse", "--short=7", "HEAD"], "unknown")
git_branch = git_value(["rev-parse", "--abbrev-ref", "HEAD"], "unknown")
pio_environment = env.subst("$PIOENV")

if pio_environment == "ProgrammeArrosage_v4":
    ota_target = "v4"
elif pio_environment in ("ProgrammeArrosage", "ProgrammeArrosage_legacy"):
    ota_target = "legacy"
else:
    ota_target = "unsupported"

env.AddBuildMiddleware(config_manager_middleware, "*/ConfigManager.cpp")

env.Append(
    CPPDEFINES=[
        ("AQUALOOK_VERSION", f'\\"{version}\\"'),
        ("AQUALOOK_BUILD_NUMBER", f'\\"{build_number}\\"'),
        ("AQUALOOK_GIT_SHA", f'\\"{git_sha}\\"'),
        ("AQUALOOK_GIT_BRANCH", f'\\"{git_branch}\\"'),
        ("AQUALOOK_OTA_TARGET", f'\\"{ota_target}\\"'),
        ("AQUALOOK_PIO_ENV", f'\\"{pio_environment}\\"'),
    ]
)

print(
    f"[BuildInfo] AquaLook {version} build {build_number} "
    f"sha {git_sha} branch {git_branch} env {pio_environment} "
    f"ota-target {ota_target}"
)
