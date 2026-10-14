Import("env")

from pathlib import Path
import re
import subprocess


VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?$")

# Cible OTA et carte de chaque environnement (P1.4, P3.1). La release GitHub
# ne publie que la cible "s3" (tools/generate_ota_manifest.py, memes valeurs) :
# un module ne prend que l'entree de SA cible, et verifie ensuite la carte et
# l'environnement. La carte historique (ProgrammeArrosage) n'est plus publiee :
# "unsupported", le module repond alors "target-missing" sans rien telecharger.
S3_BOARD = "jc4827w543c_i"
S3_ENVIRONMENTS = (
    "ProgrammeArrosage_s3",
    "ProgrammeArrosage_s3_banc",
    "ProgrammeArrosage_s3_n16r8",
)


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


project_dir = env.subst("$PROJECT_DIR")
version = read_version(project_dir)
build_number = git_value(["rev-list", "--count", "HEAD"], "local")
git_sha = git_value(["rev-parse", "--short=7", "HEAD"], "unknown")
git_branch = git_value(["rev-parse", "--abbrev-ref", "HEAD"], "unknown")
pio_environment = env.subst("$PIOENV")

if pio_environment in S3_ENVIRONMENTS:
    ota_target = "s3"
    board = S3_BOARD
elif pio_environment == "ProgrammeArrosage":
    ota_target = "unsupported"
    board = "esp32-2432S028"
else:
    # Bancs de test : jamais publies, carte non revendiquee.
    ota_target = "unsupported"
    board = "unknown"

env.Append(
    CPPDEFINES=[
        ("AQUALOOK_VERSION", f'\\"{version}\\"'),
        ("AQUALOOK_BUILD_NUMBER", f'\\"{build_number}\\"'),
        ("AQUALOOK_GIT_SHA", f'\\"{git_sha}\\"'),
        ("AQUALOOK_GIT_BRANCH", f'\\"{git_branch}\\"'),
        ("AQUALOOK_OTA_TARGET", f'\\"{ota_target}\\"'),
        ("AQUALOOK_BOARD", f'\\"{board}\\"'),
        ("AQUALOOK_PIO_ENV", f'\\"{pio_environment}\\"'),
    ]
)

print(
    f"[BuildInfo] AquaLook {version} build {build_number} "
    f"sha {git_sha} branch {git_branch} env {pio_environment} "
    f"ota-target {ota_target} board {board}"
)
