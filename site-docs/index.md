# Loom

**A proof-of-concept virtual machine for SPMD execution, embedded in C++.**

## Overview

Loom is an experimental software framework exploring an alternative execution model
for data-parallel SPMD programs.
It uses dynamic thread regrouping and unordered scheduling to improve performance
on workloads with highly irregular control flow.

!!! example "Single Program Multiple Data"
    [SPMD](https://en.wikipedia.org/wiki/Single_program,_multiple_data) is quite a general term.
    Shaders and GPGPU compute kernels are canonical examples of SPMD programs.
    The project focuses on such kernels.

The main idea is simple: instead of keeping threads in their original order and relying on conventional SIMD/SIMT reconvergence,
we can sacrifice that ordering and dynamically regroup threads based on where they are in the program.
This allows threads following the same execution path to work together, potentially reducing the impact of control-flow divergence.

The project builds a virtual machine to explore this idea, develop the underlying execution and programming models,
and experiment with different scheduling strategies.

The main goal is to find out whether this approach can actually be efficient on contemporary hardware,
and whether it can offer an alternative to conventional ways of handling control-flow divergence.

## The problem of control-flow divergence

SIMD/SIMT architectures are particularly effective at compute-heavy, data-parallel workloads.
Their execution model groups multiple threads into a *warp* and executes the same instruction across all threads in the group.

!!! question "Warps or wavefronts"
    NVIDIA uses the term *warp*, while AMD uses *wavefront* to describe these groups of threads.

This approach works well when threads follow similar execution paths.
However, real-world workloads are not always that predictable.
Data-dependent branches can send threads in the same warp down different execution paths, significantly reducing execution efficiency.

!!! info "Control-flow divergence"
    Control-flow divergence occurs when threads within a warp take different execution paths due to data-dependent branches.
    The hardware handles this by executing each path separately, using different thread masks to enable only the threads that follow that path.
    Threads that are not active during a given execution step leave their processing lanes idle, reducing effective hardware utilization.

The impact becomes especially noticeable in workloads with highly irregular control flow,
where threads frequently diverge and follow different paths.
Conventional reconvergence mechanisms help manage this problem, but they cannot always prevent substantial lane underutilization.

This raises an interesting question:
**what if we stopped trying to keep threads in their original groups and instead regrouped them dynamically based on their current execution paths?**

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
