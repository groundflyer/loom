#!/usr/bin/env bash

showhelp() {
    echo "Usage: $0 [-h] [-t] [-b] [-d] [-e] [-a] [-c] [-cuda] [-D] [-g] [-l] [-f] [-demo] [-prof]"
    echo -e "\t-t\tbuild tests"
    echo -e "\t-b\tbuild benchmarks"
    echo -e "\t-d\tbuild documentation"
    echo -e "\t-e\tuse temporary directory"
    echo -e "\t-a\tbuild all"
    echo -e "\t-c\trun cmake only, don't build"
    echo -e "\t-cuda\tuse cuda"
    echo -e "\t-D\tbuild RelWithDebInfo"
    echo -e "\t-g\tbuild debugger"
    echo -e "\t-h\tshow this help message and exit"
    echo -e "\t-l\tset libc++ build flags"
    echo -e "\t-f\tfetch dependecies instead of using system's ones"
    echo -e "\t-demo\tbuild pt demo"
    echo -e "\t-prof\tbuild binaries for profiling"
}

SRC_DIR=`realpath $(dirname "${BASH_SOURCE[0]}")`
BUILD_DIR=${SRC_DIR}/build
CMAKE_ARGS=(
    -DCMAKE_VERBOSE_MAKEFILE=ON
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)
TEST=false
BENCHMARK=false
[ -z "${JOBS:-}" ] && JOBS=16
DO_BUILD=true

[ -z "${BM_RUNS:-}" ] && BM_RUNS=4

enable_tests() {
    CMAKE_ARGS+=(-DBUILD_TESTS=ON)
    TEST=true
}

enable_benchmarks() {
    CMAKE_ARGS+=(-DBUILD_BENCHMARKS=ON -DBUILD_HEAVY_BENCHMARKS=ON)
}

enable_debugger() {
    CMAKE_ARGS+=(-DBUILD_DEBUGGERS=ON)
}

enable_docs() {
    CMAKE_ARGS+=(-DBUILD_DOCS=ON)
}

enable_demo() {
    CMAKE_ARGS+=(-DBUILD_DEMO=ON)
}

enable_prof() {
    CMAKE_ARGS+=(-DBUILD_PROF=ON)
}

enable_debug() {
    CMAKE_ARGS+=(-DCMAKE_BUILD_TYPE=RelWithDebInfo)
    BUILD_DIR+=_dbg
}

use_cuda() {
    CMAKE_ARGS+=(-DUSE_CUDA=ON)
    CMAKE_ARGS+=(-DCMAKE_CUDA_ARCHITECTURES=21)
    BUILD_DIR+="_cuda"
}

set_libcxx() {
    CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-stdlib=libc++"
                 -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -lc++abi"
                 -DCMAKE_CUDA_FLAGS="-stdlib=libc++")
}

fetch_deps() {
    CMAKE_ARGS+=(-DUSE_SYSTEM_RANGEV3=OFF -DUSE_SYSTEM_FMT=OFF -DUSE_SYSTEM_GLFW=OFF -DUSE_SYSTEM_FAST_FLOAT=OFF)
}

run_tests() {
    if [ $TEST = true ]; then
        ctest .
    fi
}

run_build() {
    mkdir -p $1
    pushd $1
    cmake "${CMAKE_ARGS[@]}" ${SRC_DIR}
    if [ $DO_BUILD = true ]; then
        make -j${JOBS} light_targets
        make heavy_targets
    fi
    popd
}

for key in $@; do
    case $key in
        -t)
            enable_tests
            ;;
        -b)
            enable_benchmarks
            ;;
        -d)
            enable_docs
            ;;
        -e)
            BUILD_DIR=`mktemp -d`
            ;;
        -a)
            enable_tests
            enable_benchmarks
            enable_docs
            ;;
        -c)
            DO_BUILD=false
            ;;
        -cuda)
            use_cuda
            ;;
        -D)
            enable_debug
            ;;
        -g)
            enable_debugger
            ;;
        -l)
            set_libcxx
            ;;
        -f)
            fetch_deps
            ;;
        -demo)
            enable_demo
            ;;
        -prof)
            enable_prof
            ;;
        -h)
            showhelp
            exit 0
            ;;
        *)
            echo "Unknown option $key"
            showhelp
            exit 1
            ;;
    esac
done

ulimit -s unlimited

run_build ${BUILD_DIR}
