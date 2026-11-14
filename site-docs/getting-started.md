# Build

## Prerequisites

To build all benchmarks and demo you need:

* GNU/Linux distribution
* 32Gb RAM required ([explanation](#faq))

## Build with nix

The easiest way to build is to use [nix package manager](https://nix.org).

Just `cd` to source directory and run `nix build`.

Apart from `default` build target there are `complete` and `minimal` ones.

## Build in a generic linux distro

Even if you don't run [NixOS](https://nixos.org) or don't have nix installed
you still can build everything with CMake.

### Dependencies

* CMake
* LLVM Clang 19 or newer
* LLVM Libc++ 19 or newer
* libfmt 9 or newer
* glfw3
* range-v3
* fast_float
* hwloc

The last four can be downloaded by cmake.

`./build.sh -b -demo -l` to build benchmarks and demo, add `-f` if you're too lazy to install dependencies
with your distro package manager.
See `./build.sh -h` for more options.

If you're going to build manually with CMake ensure the stack size is unlocked: `ulimit -s unlimited`.
Otherwise, the build of path-tracing benchmarks and demo will fail with stack overflow error.

# Benchmarks

To run all benchmarks you need CPU with at least AVX support, but the best results give AVX-512 processors (not only because of SIMD width).

Run all benchmarks:
`src/scripts/benchmark all <dir_with_binaries> <dir_with_benchmark_results>`.

For example, nix users have `<dir_with_binaries>` to be `result/bin/`.

If you don't want to run all benchmarks (it can take quite a while), the other options for benchmarks are `light` and `render`.
`render` benchmarks still take a lot of time.

To inspect results of path tracing benchmarks there is a quick'n'dirty script in `src/scripts/report.py`.

Invoke it with one argument:

`report.py <dir_with_benchmark_results>`

# Path-tracing demo

The interactive demo executable is called `loom_pt_demo`. Invoke with `-h` to see available options.
