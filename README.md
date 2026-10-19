Loom is a proof-of-concept virtual machine for SPMD (single program-multiple data) execution, embedded in C++.
The purpose of this project is to answer question: can we counter control-flow divergence and increase throughput by sorting and regrouping threads?

>Control-flow divergence occurs when parallel threads executing the same program take different execution paths due to data-dependent branches. In SIMD/SIMT architectures, threads following different paths must typically execute them in separate groups or under predication, leaving some processing lanes idle and reducing effective hardware utilization.

Example of Cornell Box rendering process with different VM backends:

In order to experiment freely I had to simplify and restrict conventional SPMD programming model significantly.
The resulting programming model heavily adapts unstructured control flow.
I tried to formalize the model and have drafted an [article](https://groundflyer.github.io/loom/assets/pdf/draft.pdf).

## TL;DR

Having a thread pool of persistent threads (SPMD kernel instances),
let's sort the threads by program counter and regroup them into more coherent groups.

<!-- The flowchart of VM's execution algorithm: -->

<!-- ```mermaid -->
<!-- flowchart TD -->
<!--     Start(["START"]) -\-> Populate["Populate"] -->
<!--     Populate -\-> Group["Group"] -->
<!--     Group -\-> Execute["Execute"] -->
<!--     Execute -\-> Sort["Sort"] -->
<!--     Sort -\-> Tasks{"Tasks available?"} -->
<!--     Tasks -\->|Yes| Exchange["Exchange"] -->
<!--     Exchange -\-> Group -->
<!--     Tasks -\->|No| Complete{"All instances complete?"} -->
<!--     Complete -\->|No| Group -->
<!--     Complete -\->|Yes| Retrieve["Retrieve"] -->
<!--     Retrieve -\-> Finish(["END"]) -->

<!--     classDef terminal fill:#f0fdf4,stroke:#4ade80 -->
<!--     classDef process fill:#eef2ff,stroke:#818cf8 -->
<!--     classDef decision fill:#fff7ed,stroke:#fb923c -->
<!--     class Start,Finish terminal -->
<!--     class Populate,Group,Execute,Sort,Exchange,Retrieve process -->
<!--     class Tasks,Complete decision -->
<!-- ``` -->

<!-- To illustrate the process, let's consider a kernel with unstructured control flow. -->

<!-- ```mermaid -->
<!-- --- -->
<!-- config: -->
<!--   layout: elk -->
<!-- --- -->
<!-- flowchart TB -->
<!--     START["START"] -\-> A["A"] & HALT(("HALT")) & B["B"] -->
<!--     A -\-> HALT -->
<!--     B -\-> HALT -->

<!--     START@{ shape: rounded} -->
<!--     A@{ shape: rounded} -->
<!--     B@{ shape: rounded} -->
<!--      START:::startNode -->
<!--      A:::nodeA -->
<!--      HALT:::haltNode -->
<!--      B:::nodeB -->
<!--     classDef startNode stroke:#3b82f6,fill:#dbeafe -->
<!--     classDef nodeA stroke:#facc15,fill:#fefce8 -->
<!--     classDef nodeB stroke:#4ade80,fill:#f0fdf4 -->
<!--     classDef haltNode stroke:#000000,fill:#f3f4f6 -->
<!--     style START stroke:#000000,fill:#2962FF -->
<!--     style A fill:#FFD600,stroke:#000000 -->
<!--     style HALT fill:#000000,color:#ffffff -->
<!--     style B stroke:#000000,fill:#00C853 -->
<!-- ``` -->

<!-- Thread pool evolution during execution is illustrated with this graph: ![thread-pool](https://groundflyer.github.io/loom/assets/svg/states.svg) -->

## Implementation

The VM targets x86 vector extension from SSE to AVX-512 and implements two main SIMD backends:

* compact - predicated warp execution with dynamic reconvergence
* hybrid - compact SIMD backend integrated within thread pool sorting and regrouping

In path tracing benchmarks the hybrid backend is up to 1.54x faster than the compact one.

There are other backends developed for validation and testing, but most of interest could be BSE (Basic Scalar Evaluator).
Its performance is roughly the same as plain C++.

## Demo

The project comes with an interactive path-tracing demo.

| predicated  | hybrid (size=1024)  | hybrid (size=4000)  |
|:-----------:|:----------------:|:----------------:|
| [predicated.webm](https://github.com/user-attachments/assets/bf28c091-3434-4e4d-a634-4e5f4e252c00) | [hybrid_s1024.webm](https://github.com/user-attachments/assets/0175f4ff-e2ae-4fc8-a603-b0d473a7e647) | [hybrid_s4000.webm](https://github.com/user-attachments/assets/c38c9f2e-a91c-4f46-99db-325d48c0802e) |


# Build

## Prerequisites

To build all benchmarks and demo you need:

* GNU/Linux distribution
* 32Gb RAM required ([explanation](#faq))

## Build with nix

The easiest way to build is to use [nix package manager](https://nix.org).

Just `cd` to source directory and run `nix build`.

Apart from `default` build target there are `complete` and `minimal` ones.

## Generic linux distro

Even if you don't run [NixOS](https://nixos.org) or don't have nix installed
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
Otherwise, the build of path-tracing benchmarks and demo will fail with stack overflow error.

# Run

To run all benchmarks you need CPU with at least AVX support, but the best results give AVX-512 processors (not only because of SIMD width).

Run all benchmarks:
`src/scripts/benchmark all <dir_with_binaries> <dir_with_benchmark_results>`.

For example, nix users have `<dir_with_binaries>` to be `result/bin/`.

If you don't want to run all benchmarks (it can take quite a while), the other options for benchmarks are `light` and `render`.
`render` benchmarks still take a lot of time.

The interactive demo executable is called loom_pt_demo. Invoke with `-h` to see available options.

# FAQ

**Q:** Why 32Gb RAM to build the project?

**A:** Kernel preprocessing, basic-block construction and consistency checks are done at compile time with template metaprogramming.
The template instantiation depth for bigger kernels is so deep, the stack limits need to be unlocked for clang not to crash with stack overflow.
The path-tracing kernel consists of ~1.5K instructions and clang consumes almost 30Gb during its compilation.

# License

Loom: GNU GPL v3

stb_easy_font.h: public domain

static_math: The MIT License (MIT)
