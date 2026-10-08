#!/usr/bin/env bash
# Thin wrapper around the shared psxrecomp bundled-release packager.
# Autofilled by tools/new_project_layout/setup_project.{sh,ps1}.
#
# Ships the compiled game built from locally regenerated game C:
# executable, runtime data, bundled OpenBIOS, mod catalog, overlay toolchain.
# No sources, emitters, CLI, generated C, or BIOS dumps.
#
# Usage:
#   scripts/package_release.sh <build-dir> <artifact-tag> [recompiler-build-dir]
#
# Writes: dist/ujl-<VERSION>-<artifact-tag>.zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-}"
ARTIFACT_TAG="${2:-}"
RECOMPILER_BUILD="${3:-build-recompiler}"

if [[ -z "${BUILD_DIR}" || -z "${ARTIFACT_TAG}" ]]; then
  echo "usage: $0 <build-dir> <artifact-tag> [recompiler-build-dir]" >&2
  exit 2
fi

PACKAGER="${ROOT}/psxrecomp/tools/package_game_release.sh"
if [[ ! -f "${PACKAGER}" ]]; then
  echo "error: missing ${PACKAGER} (psxrecomp submodule predates bundled releases -- bump it)" >&2
  exit 1
fi
chmod +x "${PACKAGER}" 2>/dev/null || true

EXTRA=()
# A developer packaging locally with an overlay cache can ship it instead:
#   PSX_OVERLAY_CACHE_ROOT=/path/to/cache scripts/package_release.sh ...
if [[ -n "${PSX_OVERLAY_CACHE_ROOT:-}" ]]; then
  EXTRA+=(--overlay-cache-root "${PSX_OVERLAY_CACHE_ROOT}")
else
  EXTRA+=(--ship-without-overlay-cache-because \
    "the boot executable is recompiled locally; uncached overlay gaps use the bundled toolchain or interpreter")
fi
# Require the title's artwork and attribution before producing a release.
for _asset in launcher_assets/img/boxart.tga launcher_assets/img/BOXART_SOURCE.txt; do
  if [[ ! -f "${ROOT}/${_asset}" ]]; then
    echo "error: missing release artwork: ${_asset}" >&2
    exit 1
  fi
done
# Extra docs shipped at the zip root (DISC.md tells players which dump works).
for _doc in README.md DISC.md LICENSE launcher_assets/img/BOXART_SOURCE.txt; do
  if [[ -f "${ROOT}/${_doc}" ]]; then
    EXTRA+=(--doc "${_doc}")
  fi
done
EXTRA+=(--runtime-dir .github/screenshots --runtime-dir launcher_assets/img)

cd "${ROOT}"
exec bash "${PACKAGER}" \
  --root "${ROOT}" \
  --build-dir "${BUILD_DIR}" \
  --artifact "${ARTIFACT_TAG}" \
  --zip-prefix ujl \
  --exe-name Um_Jammer_Lammy_Recompiled \
  --display-name "Um Jammer Lammy Recompiled" \
  --recompiler-build "${RECOMPILER_BUILD}" \
  --version-env RELEASE_VERSION \
  --disc-hint "your legally owned Um Jammer Lammy disc" \
  "${EXTRA[@]}"
