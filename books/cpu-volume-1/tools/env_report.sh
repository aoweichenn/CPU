#!/usr/bin/env bash
set -euo pipefail

echo "== date =="
date -Is

echo
echo "== uname =="
uname -a

echo
echo "== virtualization hint =="
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "WSL or Microsoft-provided kernel detected."
else
    echo "No Microsoft kernel marker detected in /proc/version."
fi

echo
echo "== lscpu =="
if command -v lscpu >/dev/null 2>&1; then
    lscpu
else
    echo "lscpu not found."
fi

echo
echo "== cpu flags =="
if [ -r /proc/cpuinfo ]; then
    awk -F: '/flags/{print $2; exit}' /proc/cpuinfo
else
    echo "/proc/cpuinfo not readable."
fi

echo
echo "== compiler versions =="
if command -v g++ >/dev/null 2>&1; then
    g++ --version | head -n 1
else
    echo "g++ not found."
fi

if command -v clang++ >/dev/null 2>&1; then
    clang++ --version | head -n 1
else
    echo "clang++ not found."
fi

if command -v cmake >/dev/null 2>&1; then
    cmake --version | head -n 1
else
    echo "cmake not found."
fi

echo
echo "== perf availability =="
if command -v perf >/dev/null 2>&1; then
    perf --version
else
    echo "perf not found."
fi

echo
echo "== perf_event_paranoid =="
if [ -r /proc/sys/kernel/perf_event_paranoid ]; then
    cat /proc/sys/kernel/perf_event_paranoid
else
    echo "/proc/sys/kernel/perf_event_paranoid not readable."
fi

echo
echo "== git =="
if command -v git >/dev/null 2>&1 && git rev-parse --show-toplevel >/dev/null 2>&1; then
    git rev-parse --show-toplevel
    git rev-parse --short HEAD
    git status --short
else
    echo "not a git checkout or git not found."
fi
