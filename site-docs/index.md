# Loom

**A proof-of-concept virtual machine for SPMD execution, embedded in C++.**

## Overview

Loom is an experimental software framework exploring an alternative approach to data-parallel execution.

It implements an abstract execution machine for Single Program Multiple Data (SPMD) programs,
using dynamic thread regrouping and unordered scheduling to improve execution throughput
on workloads with highly irregular control flow.

!!! example "Single Program Multiple Data"
    [SPMD](https://en.wikipedia.org/wiki/Single_program,_multiple_data) is quite a general term.
    Shaders and GPGPU compute kernels are canonical examples of SPMD programs.
    The project focuses on such kernels.

The project explores a programming and execution model that can target
different parallel processing architectures, including CPU SIMD and GPU SIMT hardware.

## Problem

SIMD/SIMT architectures show exceptional performance in compute-heavy parallel tasks as their main execution mechanism
is to group a few threads into a warp and execute them together, in parallel, within a lockstep.

!!! question "Warps or wavefronts"
    NVidia's term for such group of threads is *warp* and AMD uses *wavefront* in their documentation.

However, irregular control flow in compute kernels can impact performance greatly.

!!! info "Control-flow divergence"
    Control-flow divergence occurs when threads of a warp take different execution paths due to data-dependent branches.
    In this case, scheduler has to execute each path with different warp masks (predicated execution),
    leaving some processing lanes idle and reducing effective hardware utilization.

## Concept

The key concept is embarrassingly simple. It comes from the next observation:

Considering a large enough pool of threads executing the same kernel, there should be a chance that some threads
execute the same control-flow paths: let's group those threads together.

## Benchmarks

There are quite a few benchmarks available. See [Getting Started](getting-started.md) on how to run them and [Benchmark Results](bm-results.md)
to compare the results across different hardware.

## Demo

The project comes with an interactive path-tracing demo showing execution of the same path-tracing kernel with different VM backends.
Executing the  kernel on the hybrid VM backend makes the path tracer inherently wavefront.

<video controls width="100%">
  <source src="assets/video/predicated-vs-hybrid.mp4" type="video/mp4">
  Your browser does not support video playback.
</video>

## Documentation

- [Getting Started](getting-started.md) -- Instructions how to build and benchmark.
- [Architecture Overview](arch/overview.md) -- Abstract machine, execution model and scheduling.
- [API Reference](api.md) -- Generated C++ API documentation.
