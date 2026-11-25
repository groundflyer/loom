# Benchmarks Results

This is placeholder page with detailed results of two benchmarks
where hybrid SIMD execution compared to predicated SIMD execution.

There are just two path-tracing kernels but they run with different configurations:
blur, adaptive sampling threshold and bounce depth vary.

## Legend

TE - threaded execution

BRP - block-local register projection

UTR - utilization threshold ratio


=== "AVX-512"

    # Configuration

    Repo hash: 1ff5287

    Compiler: clang-21.1.7

    Fash-math: False

    CPU: AuthenticAMD fam25 mod116 step1

    SIMD: avx512

    ## Hybrid to predicated ratio statistics

    === "Path-Tracer v1"

        Average speedup: 1.22x

        | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
        |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
        |  best  |    1.54x |  1   |  0.0001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
        | worst  |    0.82x |  1   |  0.0005   |   5   |   hybrid_stream   |     288     | True | True |  6% |

        ??? Detailed

            | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
            |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
            |   1    |    1.54x |  1   |  0.0001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   2    |    1.52x |  5   |  0.0001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   3    |    1.44x |  1   |  0.0005   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   4    |    1.39x |  5   |   0.001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   5    |    1.37x |  20  |  0.0001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   6    |    1.36x |  1   |   0.001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   7    |    1.34x |  20  |  0.0005   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   8    |    1.29x |  20  |   0.001   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   9    |    1.25x |  1   |   0.001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   10   |    1.24x |  5   |   0.001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   11   |    1.22x |  1   |   0.001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   12   |    1.21x |  5   |  0.0005   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   13   |    1.21x |  1   |  0.0001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   14   |    1.20x |  5   |   0.001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   15   |    1.20x |  20  |   0.001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   16   |    1.19x |  5   |  0.0001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   17   |    1.19x |  20  |  0.0005   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   18   |    1.18x |  5   |  0.0005   |   2   |   hybrid_stream   |     288     | True | True |  6% |
            |   19   |    1.18x |  5   |  0.0005   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   20   |    1.17x |  1   |  0.0001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   21   |    1.16x |  20  |  0.0001   |   5   |   hybrid_stream   |     288     | True | True |  6% |
            |   22   |    1.16x |  20  |   0.001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   23   |    1.14x |  20  |  0.0005   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   24   |    1.12x |  20  |  0.0001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   25   |    1.04x |  5   |  0.0001   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   26   |    0.91x |  1   |  0.0005   |  20   |   hybrid_stream   |     288     | True | True |  6% |
            |   27   |    0.82x |  1   |  0.0005   |   5   |   hybrid_stream   |     288     | True | True |  6% |


    === "Path-Tracer v2"

        Average speedup: 1.26x

        | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
        |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
        |  best  |    1.49x |  1   |   0.001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
        | worst  |    1.16x |  20  |  0.0005   |  20   |   hybrid_stream   |     352     | True | True |  6% |

        ??? Detailed

            | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
            |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
            |   1    |    1.49x |  1   |   0.001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   2    |    1.38x |  5   |  0.0001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   3    |    1.38x |  1   |  0.0005   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   4    |    1.38x |  1   |  0.0001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   5    |    1.30x |  5   |  0.0005   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   6    |    1.30x |  20  |  0.0005   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   7    |    1.30x |  5   |  0.0001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   8    |    1.30x |  1   |   0.001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   9    |    1.29x |  1   |  0.0005   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   10   |    1.28x |  1   |  0.0005   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   11   |    1.28x |  1   |  0.0001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   12   |    1.27x |  1   |  0.0001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   13   |    1.26x |  1   |   0.001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   14   |    1.25x |  5   |   0.001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   15   |    1.25x |  5   |   0.001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   16   |    1.24x |  5   |  0.0005   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   17   |    1.24x |  5   |  0.0005   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   18   |    1.23x |  5   |  0.0001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   19   |    1.22x |  5   |   0.001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   20   |    1.18x |  20  |   0.001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   21   |    1.18x |  20  |  0.0001   |   2   |   hybrid_stream   |     352     | True | True |  6% |
            |   22   |    1.17x |  20  |  0.0001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   23   |    1.17x |  20  |   0.001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   24   |    1.17x |  20  |   0.001   |  20   |   hybrid_stream   |     352     | True | True |  6% |
            |   25   |    1.16x |  20  |  0.0001   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   26   |    1.16x |  20  |  0.0005   |   5   |   hybrid_stream   |     352     | True | True |  6% |
            |   27   |    1.16x |  20  |  0.0005   |  20   |   hybrid_stream   |     352     | True | True |  6% |



=== "AVX"

    # Configuration

    Repo hash: 1ff5287

    Compiler: clang-21.1.7

    Fash-math: False

    CPU: GenuineIntel fam6 mod42 step7

    SIMD: avx

    ## Hybrid to predicated ratio statistics

    === "Path-Tracer v1"

        Average speedup: 1.10x

        | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
        |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
        |  best  |    1.24x |  5   |  0.0001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
        | worst  |    0.99x |  5   |  0.0001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |

        ??? Detailed

            | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
            |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
            |   1    |    1.24x |  5   |  0.0001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   2    |    1.22x |  1   |  0.0001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   3    |    1.19x |  5   |  0.0005   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   4    |    1.17x |  20  |  0.0001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   5    |    1.16x |  5   |   0.001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   6    |    1.16x |  1   |  0.0005   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   7    |    1.15x |  20  |  0.0005   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   8    |    1.14x |  5   |   0.001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   9    |    1.14x |  1   |   0.001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   10   |    1.13x |  5   |  0.0005   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   11   |    1.13x |  20  |   0.001   |   2   |   hybrid_stream   |     144     | True | True | 12% |
            |   12   |    1.09x |  5   |   0.001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   13   |    1.09x |  1   |   0.001   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   14   |    1.08x |  20  |  0.0005   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   15   |    1.07x |  5   |  0.0005   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   16   |    1.07x |  20  |   0.001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   17   |    1.07x |  1   |   0.001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   18   |    1.07x |  1   |  0.0005   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   19   |    1.06x |  1   |  0.0001   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   20   |    1.06x |  20  |   0.001   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   21   |    1.05x |  1   |  0.0005   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   22   |    1.05x |  1   |  0.0001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   23   |    1.05x |  20  |  0.0005   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   24   |    1.04x |  5   |  0.0001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   25   |    1.04x |  20  |  0.0001   |   5   |   hybrid_stream   |     144     | True | True | 12% |
            |   26   |    1.02x |  20  |  0.0001   |  20   |   hybrid_stream   |     144     | True | True | 12% |
            |   27   |    0.99x |  5   |  0.0001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |


    === "Path-Tracer v2"

        Average speedup: 1.11x

        | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
        |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
        |  best  |    1.20x |  1   |  0.0001   |   2   |   hybrid_stream   |     176     | True | True | 12% |
        | worst  |    1.06x |  1   |   0.001   |  20   |   hybrid_stream   |     176     | True | True | 12% |

        ??? Detailed

            | Case   | Speedup  | Blur | Threshold | Depth | Method            | Stream Size | TE   | BRP  | UTR |
            |:------:|---------:|:----:|:---------:|:-----:|:-----------------:|:-----------:|:----:|:----:|:---:|
            |   1    |    1.20x |  1   |  0.0001   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   2    |    1.17x |  20  |  0.0005   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   3    |    1.16x |  20  |  0.0001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   4    |    1.16x |  20  |   0.001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   5    |    1.15x |  20  |  0.0001   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   6    |    1.15x |  1   |  0.0005   |  20   |   hybrid_stream   |     176     | True | True | 12% |
            |   7    |    1.14x |  20  |  0.0005   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   8    |    1.14x |  1   |  0.0005   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   9    |    1.14x |  20  |  0.0005   |   2   | hybrid_full_range |     N/A     | True | True | 12% |
            |   10   |    1.14x |  20  |   0.001   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   11   |    1.13x |  20  |   0.001   |   2   | hybrid_full_range |     N/A     | True | True | 12% |
            |   12   |    1.11x |  20  |  0.0001   |   2   | hybrid_full_range |     N/A     | True | True | 12% |
            |   13   |    1.11x |  1   |   0.001   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   14   |    1.11x |  5   |  0.0001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   15   |    1.11x |  5   |   0.001   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   16   |    1.10x |  5   |  0.0005   |  20   | hybrid_full_range |     N/A     | True | True | 12% |
            |   17   |    1.10x |  1   |  0.0001   |  20   |   hybrid_stream   |     176     | True | True | 12% |
            |   18   |    1.09x |  1   |  0.0001   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   19   |    1.09x |  5   |  0.0005   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   20   |    1.09x |  5   |   0.001   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   21   |    1.08x |  5   |   0.001   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   22   |    1.08x |  1   |   0.001   |   5   |   hybrid_stream   |     176     | True | True | 12% |
            |   23   |    1.08x |  1   |  0.0005   |   5   |   hybrid_stream   |     176     | True | True | 12% |
            |   24   |    1.08x |  5   |  0.0001   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   25   |    1.07x |  5   |  0.0005   |   5   | hybrid_full_range |     N/A     | True | True | 12% |
            |   26   |    1.07x |  5   |  0.0001   |   2   |   hybrid_stream   |     176     | True | True | 12% |
            |   27   |    1.06x |  1   |   0.001   |  20   |   hybrid_stream   |     176     | True | True | 12% |
