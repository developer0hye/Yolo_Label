#!/bin/bash
# Downloads ONNX Runtime pre-built CPU package for the current platform.
# Usage: ./scripts/download_onnxruntime.sh [version] [output_dir]
#
# The downloaded library will be extracted to output_dir (default: ./onnxruntime).
# Then build with: qmake YoloLabel.pro "ONNXRUNTIME_DIR=$PWD/onnxruntime"

set -e

ORT_VERSION="${1:-1.24.1}"
OUTPUT_DIR="${2:-onnxruntime}"

OS_NAME="$(uname -s)"
ARCH="$(uname -m)"

case "${OS_NAME}" in
    Linux)
        if [ "${ARCH}" = "aarch64" ]; then
            PACKAGE="onnxruntime-linux-aarch64-${ORT_VERSION}"
        else
            PACKAGE="onnxruntime-linux-x64-${ORT_VERSION}"
        fi
        EXT="tgz"
        ;;
    Darwin)
        if [ "${ARCH}" = "arm64" ]; then
            PACKAGE="onnxruntime-osx-arm64-${ORT_VERSION}"
        else
            PACKAGE="onnxruntime-osx-x86_64-${ORT_VERSION}"
        fi
        EXT="tgz"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        PACKAGE="onnxruntime-win-x64-${ORT_VERSION}"
        EXT="zip"
        ;;
    *)
        echo "Unsupported platform: ${OS_NAME}"
        exit 1
        ;;
esac

URL="https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/${PACKAGE}.${EXT}"

echo "Downloading ONNX Runtime ${ORT_VERSION} for ${OS_NAME} ${ARCH}..."
echo "  URL: ${URL}"

if [ -d "${OUTPUT_DIR}" ]; then
    echo "  Removing existing ${OUTPUT_DIR}/"
    rm -rf "${OUTPUT_DIR}"
fi

TMPFILE="ort_download.${EXT}"
curl -L -o "${TMPFILE}" "${URL}"

if [ "${EXT}" = "tgz" ]; then
    tar xzf "${TMPFILE}"
elif [ "${EXT}" = "zip" ]; then
    unzip -q "${TMPFILE}"
fi

mv "${PACKAGE}" "${OUTPUT_DIR}"
rm -f "${TMPFILE}"

echo "Done! ONNX Runtime extracted to ${OUTPUT_DIR}/"
echo ""
echo "Build with:"
echo "  qmake YoloLabel.pro \"ONNXRUNTIME_DIR=\$PWD/${OUTPUT_DIR}\""
echo "  make -j\$(nproc)"
