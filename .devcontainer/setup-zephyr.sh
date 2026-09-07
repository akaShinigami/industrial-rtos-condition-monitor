#!/usr/bin/env bash

set -euo pipefail

WORKSPACE_DIR=".zephyr-workspace"
ZEPHYR_VERSION="v4.4.0"

echo "Preparing Zephyr ${ZEPHYR_VERSION} development workspace..."

if [ ! -d "${WORKSPACE_DIR}/.west" ]; then
    echo "Initializing Zephyr workspace..."

    west init \
        -m https://github.com/zephyrproject-rtos/zephyr \
        --mr "${ZEPHYR_VERSION}" \
        "${WORKSPACE_DIR}"
else
    echo "Existing Zephyr workspace found."
fi

cd "${WORKSPACE_DIR}"

echo "Updating Zephyr modules..."
west update

echo "Exporting Zephyr CMake package..."
west zephyr-export

echo
echo "Zephyr development environment ready."
west topdir
