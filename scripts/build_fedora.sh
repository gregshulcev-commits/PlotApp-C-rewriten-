#!/usr/bin/env bash
set -euo pipefail
sudo dnf install -y gcc-c++ cmake ninja-build qt6-qtbase-devel minizip-ng-compat-devel
cmake -S . -B build -GNinja
cmake --build build
ctest --test-dir build --output-on-failure
