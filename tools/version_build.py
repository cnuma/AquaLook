Import("env")

import subprocess


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


version = "5.8.0"
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
