Loom is a proof-of-concept virtual machine for SPMD (single program-multiple data) execution, embedded in C++.
The purpose of this project is to answer question: can we counter control-flow divergence and increase throughput by sorting and regrouping threads?

>Control-flow divergence occurs when parallel threads executing the same program take different execution paths due to data-dependent branches. In SIMD/SIMT architectures, threads following different paths must typically execute them in separate groups or under predication, leaving some processing lanes idle and reducing effective hardware utilization.

In order to experiment freely I had to simplify and restrict conventional SPMD programming model significantly.
The resulting programming model heavily adapts unstructured control flow.
I tried to formalize the model and have drafted an [article](https://groundflyer.github.io/loom/assets/pdf/draft.pdf).
TL;DR: having a thread pool of persistent threads (SPMD kernel instances),
let's sort the threads by program counter and regroup them into more coherent groups.
This scheduling scheme can be more efficient for highly irregular workloads than conventional reconvergence.

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

The VM targets x86 vector extensions from SSE to AVX-512 and implements two main SIMD backends:

* compact - predicated warp execution with dynamic reconvergence
* hybrid - compact SIMD backend integrated within thread pool sorting and regrouping

In path tracing benchmarks the hybrid backend is up to 1.54x faster than the compact one.

There are other execution backends, including CUDA, developed for sole purpose to test things and establish API,
so they are not efficient and do not participate in benchmarking (except the scalar backend that sometimes faster than plain C++).

## Demo

The project comes with an interactive path-tracing demo.



https://github.com/user-attachments/assets/f2524dba-1714-4b08-a1ca-bf351dc6c7f0



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

# Run

To run all benchmarks you need CPU with at least AVX support, but the best results give AVX-512 processors (not only because of SIMD width).

Run all benchmarks:
`src/scripts/benchmark all <dir_with_binaries> <dir_with_benchmark_results>`.

For example, nix users have `<dir_with_binaries>` to be `result/bin/`.

If you don't want to run all benchmarks (it can take quite a while), the other options for benchmarks are `light` and `render`.
`render` benchmarks still take a lot of time.

To inspect results of path tracing benchmarks there is a quick'n'dirty script in `src/scripts/report.py`.

Invoke it with one argument:

`report.py <dir_with_benchmark_results>`

The interactive demo executable is called `loom_pt_demo`. Invoke with `-h` to see available options.

# Programming Model

Unlike conventional SPMD models Loom forbids mutable shared state and doesn't provide thread id.
A kernel is a pure function: all inputs and outputs should be specified explicitly.
The kernel's state is bounded and consists only of virtual registers and program counter.
Registers are defined on the fly.

Here is prettified assembly IR of an exemplary kernel computing Fibonacci sequence:

```asm
%kernel fib n:u64             # one input parameter of type u64 stored into n
    le @u64 cmp n 1           # cmp <- n <= 1
    cmov @u64 c cmp n 1       # c <- cmp ? n : 1
    jmp cmp 0: work 1: HALT   # if cmp = 0 then goto work else terminate
work:
    mov @u64 p 0              # p <- 0
    mov @u64 c 1              # c <- 1
    mov @u64 i 2              # i <- 2
loop:
    add @u64 t p c            # t <- p + c
    mov @u64 p c              # p <- c
    mov @u64 c t              # c <- t
    add @u64 i i 1            # increment i
    le @u64 cmp i n           # cmp <- i <= n
    jmp cmp 0: HALT 1: loop   # if cmp = 0 then terminate else goto loop
%endkernel c:u64              # the result value is retreived from c
```

This how it looks in C++ sources:

```c++
template <std::unsigned_integral T>
TKERNEL1(fib, n)
    TLE(cmp, n, 1),
    TCMOV(c, cmp, n, 1),
    IPJMP2(cmp, work, HALT),
    LABEL(work),
    TMOV(p, 0),
    TMOV(c, 1),
    TMOV(i, 2),
    LABEL(loop),
    TADD(t, p, c),
    TMOV(p, c),
    TMOV(c, t),
    TADD(i, i, 1),
    TLE(cmp, i, n),
    IPJMP2(cmp, HALT, loop)
TKERNEL_END1(c);
```

This is how it looks after preprocessor stages:

```c++
template <std::unsigned_integral T>
using fib = loom::vm::kernel_blank<
    loom::sl2cs<"fib">,
    loom::type_sequence<loom::vm::Variable<T, loom::sl2cs<"n">>>,
    loom::type_sequence<
        loom::vm::Compute<std::less_equal<T>,
                          loom::vm::Variable<loom::uint_of_size_t<sizeof(T)>,
                                             loom::sl2cs<"cmp">>,
                          loom::vm::Variable<T, loom::sl2cs<"n">>,
                          loom::vm::Variable<T, loom::sl2cs<"1">>>,
        loom::vm::Compute<loom::vm::ops::cmov,
                          loom::vm::Variable<T, loom::sl2cs<"c">>,
                          loom::vm::Variable<loom::uint_of_size_t<sizeof(T)>,
                                             loom::sl2cs<"cmp">>,
                          loom::vm::Variable<T, loom::sl2cs<"n">>,
                          loom::vm::Variable<T, loom::sl2cs<"1">>>,
        loom::vm::ParmJump<
            loom::vm::Variable<loom::uint_of_size_t<sizeof(T)>,
                               loom::sl2cs<"cmp">>,
            loom::type_sequence<loom::vm::Label<loom::sl2cs<"work">>,
                                loom::vm::Label<loom::sl2cs<"HALT">>>>,
        loom::vm::Label<loom::sl2cs<"work">>,
        loom::vm::Compute<std::identity,
                          loom::vm::Variable<T, loom::sl2cs<"p">>,
                          loom::vm::Variable<T, loom::sl2cs<"0">>>,
        loom::vm::Compute<std::identity,
                          loom::vm::Variable<T, loom::sl2cs<"c">>,
                          loom::vm::Variable<T, loom::sl2cs<"1">>>,
        loom::vm::Compute<std::identity,
                          loom::vm::Variable<T, loom::sl2cs<"i">>,
                          loom::vm::Variable<T, loom::sl2cs<"2">>>,
        loom::vm::Label<loom::sl2cs<"loop">>,
        loom::vm::Compute<std::plus<T>,
                          loom::vm::Variable<T, loom::sl2cs<"t">>,
                          loom::vm::Variable<T, loom::sl2cs<"p">>,
                          loom::vm::Variable<T, loom::sl2cs<"c">>>,
        loom::vm::Compute<std::identity,
                          loom::vm::Variable<T, loom::sl2cs<"p">>,
                          loom::vm::Variable<T, loom::sl2cs<"c">>>,
        loom::vm::Compute<std::identity,
                          loom::vm::Variable<T, loom::sl2cs<"c">>,
                          loom::vm::Variable<T, loom::sl2cs<"t">>>,
        loom::vm::Compute<std::plus<T>,
                          loom::vm::Variable<T, loom::sl2cs<"i">>,
                          loom::vm::Variable<T, loom::sl2cs<"i">>,
                          loom::vm::Variable<T, loom::sl2cs<"1">>>,
        loom::vm::Compute<std::less_equal<T>,
                          loom::vm::Variable<loom::uint_of_size_t<sizeof(T)>,
                                             loom::sl2cs<"cmp">>,
                          loom::vm::Variable<T, loom::sl2cs<"i">>,
                          loom::vm::Variable<T, loom::sl2cs<"n">>>,
        loom::vm::ParmJump<
            loom::vm::Variable<loom::uint_of_size_t<sizeof(T)>,
                               loom::sl2cs<"cmp">>,
            loom::type_sequence<loom::vm::Label<loom::sl2cs<"HALT">>,
                                loom::vm::Label<loom::sl2cs<"loop">>>>>,
    loom::type_sequence<loom::vm::Variable<T, loom::sl2cs<"c">>>>;
```

This is *roughly* what it could compile to:

```asm
START:
# allocs...
    vmovdqa64   (%rax), %zmm1
    vpcmpleuq   %zmm0, %zmm1, %k1
    vpblendmq   %zmm1, %zmm0, %zmm3 {%k1}
    vpmovm2q    %k1, %zmm4
# spills...
WORK:
# allocs...
# spills...
LOOP:
# allocs...
    knotw       %k1, %k2
    vpmovm2q    %k2, %zmm4
    vporq       %zmm0, %zmm4, %zmm4
    vpcmpleuq   %zmm1, %zmm5, %k2
# spills
```

As you can see there too much data reloading between basic blocks. This is a weak spot of the implementation: basic blocks have to compile into separate non-inlineable functions to allow regrouping. As a result, the overall performance doesn't scale well for more complicated programs.

For instance, root finding benchmark (fp32, 16 simd lanes) is only about 5.5x faster than scalar execution on AMD Zen 4 (dual 256-bit datapath for AVX-512F, so the speedup should be ~8x).

Straightline programs (those without branching) performance scale linearly though, according to running CPU SIMD throughput. At the moment, there are only two straightline benchmarks: fp_mix and threefry.

# FAQ

**Q:** Why 32Gb RAM to build the project?

**A:** Kernel preprocessing, basic-block construction and consistency checks are done at compile time with template metaprogramming.
The template instantiation depth for bigger kernels is so deep, the stack limits need to be unlocked for clang not to crash with stack overflow.
The path-tracing kernel consists of ~1.5K instructions and clang consumes almost 30Gb during its compilation.

# License

Loom: GNU GPL v3

stb_easy_font.h: public domain

static_math: The MIT License (MIT)
