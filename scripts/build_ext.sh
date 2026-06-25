#!/usr/bin/env bash
# Fallback: build the _bourse extension in-place, no pip.
# Run from the project root in the MSYS2 UCRT64 shell (conda deactivated).
set -euo pipefail

EXT=$(python -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")
OUT="python/bourse/_bourse${EXT}"

g++ -O3 -Wall -shared -std=c++20 \
    -Iinclude $(python -m pybind11 --includes) \
    src/order_book.cpp bindings/pybind_module.cpp \
    -o "${OUT}" \
    $(python-config --ldflags --embed)

echo "Built ${OUT}"
echo "Run with:  PYTHONPATH=python python scripts/demo.py"