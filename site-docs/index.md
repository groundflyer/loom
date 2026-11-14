# Loom

**A proof-of-concept virtual machine for unordered, streaming execution of data-parallel SPMD programs.**

## Overview

Loom is an experimental software framework exploring an alternative approach to data-parallel execution.

It implements an abstract execution machine for Single Program Multiple Data (SPMD) programs,
using dynamic instance regrouping and unordered scheduling to improve execution efficiency on workloads with irregular control flow and divergent execution paths.

The project explores a programming and execution model that can target different parallel processing architectures, including CPU SIMD and GPU SIMT hardware.

## Key Features

- **Unordered execution:** Program instances are scheduled dynamically rather than following a globally synchronized execution order.
- **Dynamic regrouping:** Instances executing the same basic block are grouped together to improve execution coherence.
- **Streaming execution:** Work progresses through the execution system without requiring global ordering of independent instances.
- **Persistent execution state:** Virtual registers preserve per-instance state across scheduling cycles.
- **Multiple backends:** The abstract machine can be implemented on different hardware architectures.
- **Inspectable execution:** Development tools provide facilities for inspecting and debugging execution behavior.

## Demo

The project comes with an interactive path-tracing demo showing execution of the same path-tracing kernel with different VM backends.

<video controls width="100%">
  <source src="assets/video/predicated-vs-hybrid.mp4" type="video/mp4">
  Your browser does not support video playback.
</video>

## Documentation

- [Getting Started](getting-started.md) -- Instructions how to build and benchmark.
- [Architecture Overview](arch/overview.md) -- Abstract machine, execution model and scheduling.
- [API Reference](api.md) -- Generated C++ API documentation.
