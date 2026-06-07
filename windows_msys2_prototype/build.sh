#!/usr/bin/env bash
set -euo pipefail
mkdir -p build data
CXX=${CXX:-g++}
CXXFLAGS=${CXXFLAGS:-"-std=c++17 -O2 -Wall -Wextra"}
SRC_DIR="src"
$CXX $CXXFLAGS -I"$SRC_DIR" \
  "$SRC_DIR/main.cpp" \
  "$SRC_DIR/experiments_a1a2_boundary_sweep.cpp" \
  "$SRC_DIR/experiments_a1a2_retry_budget_sweep.cpp" \
  "$SRC_DIR/experiments_c1.cpp" \
  "$SRC_DIR/experiments_d1.cpp" \
  "$SRC_DIR/experiment_d5.cpp" \
  "$SRC_DIR/experiment_review_supp.cpp" \
  "$SRC_DIR/common/"*.cpp \
  "$SRC_DIR/lattice/"*.cpp \
  "$SRC_DIR/preauth/"*.cpp \
  "$SRC_DIR/zk/"*.cpp \
  "$SRC_DIR/offload/"*.cpp \
  -o build/repair_windows_prototype.exe

echo "Built: build/repair_windows_prototype.exe"
