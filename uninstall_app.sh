#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)"
exec "$SCRIPT_DIR/tools/desktop_manager.sh" uninstall "$@"
