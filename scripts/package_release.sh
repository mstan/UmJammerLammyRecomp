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
for _doc in README.md DISC.md LICENSE launcher_assets/img/BOXART_SOURCE.txt third_party/SDL3-LICENSE.txt; do
  if [[ -f "${ROOT}/${_doc}" ]]; then
    EXTRA+=(--doc "${_doc}")
  fi
done
EXTRA+=(--runtime-dir .github/screenshots --runtime-dir launcher_assets/img)

cd "${ROOT}"
bash "${PACKAGER}" \
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

# Ubuntu 24.04 does not provide SDL3. Bundle the exact shared library used
# by this build and let the extracted executable find it beside itself.
if [[ "${ARTIFACT_TAG}" == linux-* ]]; then
  command -v patchelf >/dev/null
  STAGE="${ROOT}/dist/stage-game-${ARTIFACT_TAG}"
  EXE="${STAGE}/Um_Jammer_Lammy_Recompiled"
  SDL_LIBRARY="$(ldd "${EXE}" | awk '$1 == "libSDL3.so.0" && $2 == "=>" {print $3}')"
  [[ -f "${SDL_LIBRARY}" ]] || { echo "error: cannot resolve this build's SDL3 library" >&2; exit 1; }
  mkdir -p "${STAGE}/lib"
  cp -L "${SDL_LIBRARY}" "${STAGE}/lib/libSDL3.so.0"
  patchelf --set-rpath '$ORIGIN/lib' "${EXE}"
  if ldd "${EXE}" | grep -q 'not found'; then
    echo "error: unresolved Linux release dependencies" >&2
    exit 1
  fi
  VERSION="$(tr -d '[:space:]' < "${ROOT}/VERSION")"
  "${PSX_RELEASE_STAGE_PYTHON:-python3}" "${ROOT}/psxrecomp/tools/create_release_zip.py" \
    --source "${STAGE}" --output "${ROOT}/dist/ujl-${VERSION}-${ARTIFACT_TAG}.zip"
fi
