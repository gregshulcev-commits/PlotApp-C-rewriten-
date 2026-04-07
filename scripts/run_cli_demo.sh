#!/usr/bin/env bash
set -euo pipefail
./build/plotapp-cli <<'CMDS'
help
import ../examples/sample_points.csv 0 1 raw
list-layers
exit
CMDS
