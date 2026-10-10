Loom
====

Loom is a proof-of-concept virtual machine for SPMD execution embedded in C++
with the goal of testing one stupid idea on how to counter divergent control flow.

# TL;DR

Having a thread pool of persistent threads associated with a SPMD kernel instances,
let's sort the threads by program counter and regroup them into coherent groups
after each execution pass.

# Build

## Prerequisites

* GNU/Linux distribution
* x86 CPU with AVX (AVX-512F recommended) support to run all the benchmarks and the demo
* OpenGL3-compatible GPU to run demo
* at least 32 Gb of RAM to build all the benchmarks and the demo

## Nix

If you're on Nix:
`cd` to source directory and run `nix build`.

## Generic

Even if you don't run [NixOS](nixos.org) or don't have [nix package manager](nix.org) installed
you still can build everything with CMake.

### Dependencies

* CMake
* LLVM clang version 19 or newer
* LLVM libc++ version 19 or newer
* libfmt version 9 or newer
* glfw3
* range-v3
* fast_float
* hwloc

The last four can be downloaded by cmake.

`./build.sh -b -demo -l` to build benchmarks and demo, add `-f` if you're too lazy to install dependencies
with your distro package manager.
See `./build.sh -h` for more options.

If you're going to build manually with CMake ensure the stack size is unlocked: `ulimit -s unlimited`.
Otherwise, the build of path-tracing benchmarks and demo will fail with stack overflow.

# Running benchmarks

To run all benchmarks:
`src/scripts/benchmark all <dir_with_binaries> <dir_to_bm_results>`.

For example, nix users have `<dir_with_binaries>` to be `result/bin/`.

Other options for benchmarks are `light` and `render`.
