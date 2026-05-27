#!/usr/bin/env bash
set -euo pipefail

HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

IMAGE="odin-builder:trixie"
WORKDIR="/work"
SYSROOT_HOST="${HOME}/odin/rpi-sysroot"
SYSROOT_CONT="/sysroot"
BUILD_DIR="${WORKDIR}/build"

CCACHE_HOST="${HOME}/.ccache/odin"
CCACHE_CONT="/ccache"

BUILD_GUI_AARCH64="${BUILD_GUI_AARCH64:-ON}"
BUILD_ARM_AARCH64="${BUILD_ARM_AARCH64:-ON}"

PKG_ARM_NAME="${PKG_ARM_NAME:-RoboticArm-arm}"
PKG_GUI_NAME="${PKG_GUI_NAME:-RoboticArm-gui}"

if [[ ! -d "${SYSROOT_HOST}" ]]; then
  echo "ERROR: Sysroot not found at: ${SYSROOT_HOST}"
  exit 1
fi

if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
  echo "ERROR: Docker image not found: ${IMAGE}"
  echo "Build it first: docker build -t ${IMAGE} -f docker/Dockerfile ."
  exit 1
fi

mkdir -p "${CCACHE_HOST}"

CMAKE_ARGS=(
  "-G" "Ninja"
  "-DCMAKE_MAKE_PROGRAM=/usr/bin/ninja"
  "-DCMAKE_TOOLCHAIN_FILE=${WORKDIR}/toolchains/rpi-aarch64-trixie.cmake"
  "-DQt6_DIR=${SYSROOT_CONT}/usr/local/qt6/lib/cmake/Qt6"
  "-DCMAKE_PREFIX_PATH=${SYSROOT_CONT}/usr/local/qt6"
  "-DQt6HostInfo_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6HostInfo"
  "-DQt6CoreTools_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6CoreTools"
  "-DCMAKE_IGNORE_PATH=${SYSROOT_CONT}/usr/lib/qt6;${SYSROOT_CONT}/lib/qt6"
  "-DCMAKE_EXE_LINKER_FLAGS_INIT=--sysroot=${SYSROOT_CONT} -Wl,-rpath-link,${SYSROOT_CONT}/usr/lib/aarch64-linux-gnu -Wl,-rpath-link,${SYSROOT_CONT}/lib/aarch64-linux-gnu -Wl,-rpath-link,${SYSROOT_CONT}/usr/lib/aarch64-linux-gnu/openblas-pthread"
  "-DBUILD_GUI_AARCH64=${BUILD_GUI_AARCH64}"
  "-DBUILD_ARM_AARCH64=${BUILD_ARM_AARCH64}"
)

echo "== Build in Docker =="

rm -rf ./build

docker run --rm -it \
  --user "${HOST_UID}:${HOST_GID}" \
  --group-add "${HOST_GID}" \
  -v "$PWD":"${WORKDIR}" \
  -v "${SYSROOT_HOST}":"${SYSROOT_CONT}":ro \
  -v "${CCACHE_HOST}":"${CCACHE_CONT}":rw \
  -e CCACHE_DIR="${CCACHE_CONT}" \
  -e CCACHE_MAXSIZE="20G" \
  "${IMAGE}" \
  cmake -S "${WORKDIR}" -B "${BUILD_DIR}" "${CMAKE_ARGS[@]}"

docker run --rm -it \
  --user "${HOST_UID}:${HOST_GID}" \
  --group-add "${HOST_GID}" \
  -v "$PWD":"${WORKDIR}" \
  -v "${SYSROOT_HOST}":"${SYSROOT_CONT}":ro \
  -v "${CCACHE_HOST}":"${CCACHE_CONT}":rw \
  -e CCACHE_DIR="${CCACHE_CONT}" \
  -e CCACHE_MAXSIZE="20G" \
  "${IMAGE}" \
  ninja -C "${BUILD_DIR}"

echo
echo "== Package: RoboticArm =="
docker run --rm -it \
  --user "${HOST_UID}:${HOST_GID}" \
  --group-add "${HOST_GID}" \
  -v "$PWD":"${WORKDIR}" \
  "${IMAGE}" \
  bash -lc "cd '${BUILD_DIR}' && cpack -G TGZ -D CPACK_COMPONENTS_ALL=RoboticArm -D CPACK_PACKAGE_FILE_NAME='${PKG_ARM_NAME}'"

echo
echo "== Package: RoboticGui =="
docker run --rm -it \
  --user "${HOST_UID}:${HOST_GID}" \
  --group-add "${HOST_GID}" \
  -v "$PWD":"${WORKDIR}" \
  "${IMAGE}" \
  bash -lc "cd '${BUILD_DIR}' && cpack -G TGZ -D CPACK_COMPONENTS_ALL=RoboticGui -D CPACK_PACKAGE_FILE_NAME='${PKG_GUI_NAME}'"

echo
echo "== Done =="
ls -lh ./build/*.tar.gz 2>/dev/null || true
