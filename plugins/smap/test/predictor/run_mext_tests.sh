#!/usr/bin/env bash
# Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
# One-command build & verification runner for SMAP-MEXT Predictive Memory
# Works out of the box on Linux (g++ / clang++) and macOS (clang++).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMAP_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PRED_DIR="${SMAP_DIR}/src/user/predictor"
RECLAIM_DIR="${SMAP_DIR}/src/user/reclaim"
BUILD_DIR="${SCRIPT_DIR}/build_mext_tests"

CXX_BIN="${CXX:-}"
if [[ -z "${CXX_BIN}" ]]; then
    if command -v g++ >/dev/null 2>&1; then
        CXX_BIN="g++"
    elif command -v clang++ >/dev/null 2>&1; then
        CXX_BIN="clang++"
    else
        echo "ERROR: No C++17 compiler (g++ or clang++) found." >&2
        exit 1
    fi
fi

mkdir -p "${BUILD_DIR}"

COMMON_SRCS=(
    "${PRED_DIR}/swap_shadow_map.cpp"
    "${PRED_DIR}/tier1_table.cpp"
    "${PRED_DIR}/tiny_transformer.cpp"
    "${PRED_DIR}/mext_engine.cpp"
)
CXXFLAGS=(-std=c++17 -O3 -Wall -Wextra -I"${SMAP_DIR}/src/user")

echo "[1/4] Building & running core MEXT verification suite (${CXX_BIN})..."
"${CXX_BIN}" "${CXXFLAGS[@]}" \
    "${SCRIPT_DIR}/test_mext_predictor.cpp" "${COMMON_SRCS[@]}" \
    -lpthread -o "${BUILD_DIR}/test_mext_predictor"
"${BUILD_DIR}/test_mext_predictor"

echo ""
echo "[2/4] Building & running NVMe + CPU timing benchmark..."
"${CXX_BIN}" "${CXXFLAGS[@]}" \
    "${SCRIPT_DIR}/bench_mext_perf.cpp" "${COMMON_SRCS[@]}" \
    -lpthread -o "${BUILD_DIR}/bench_mext_perf"
"${BUILD_DIR}/bench_mext_perf"

echo ""
if echo '#include <zlib.h>' | "${CXX_BIN}" -E -x c++ - >/dev/null 2>&1; then
    echo "[3/4] Building & running live two-thread zram compression/offload suite..."
    "${CXX_BIN}" "${CXXFLAGS[@]}" \
        "${SCRIPT_DIR}/test_mext_zram.cpp" "${COMMON_SRCS[@]}" \
        -lpthread -lz -o "${BUILD_DIR}/test_mext_zram"
    "${BUILD_DIR}/test_mext_zram"
else
    echo "[3/4] Skipping test_mext_zram (zlib-devel / zlib1g-dev headers not installed)."
fi

echo ""
echo "[4/4] Building & running Dynamic Proactive Reclaim & burst benchmark suite..."
"${CXX_BIN}" "${CXXFLAGS[@]}" \
    "${SCRIPT_DIR}/test_proactive_reclaim.cpp" \
    "${RECLAIM_DIR}/proactive_reclaim.cpp" \
    "${COMMON_SRCS[@]}" \
    -lpthread -o "${BUILD_DIR}/test_proactive_reclaim"
"${BUILD_DIR}/test_proactive_reclaim"

rm -rf "${BUILD_DIR}"
echo ""
echo "All SMAP-MEXT & Proactive Reclaim suites completed successfully."
