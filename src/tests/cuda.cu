#include <chrono>
#include <cmath>
#include <iostream>

namespace tinytimer
{
enum FF // Format Flag
{
    DAY,
    HOUR,
    MINUTE,
    SECOND
};

template <typename Clock = std::chrono::steady_clock>
class ElapsedTimer
{
    using ClockDuration =
        std::chrono::duration<typename Clock::rep, typename Clock::period>;
    using TimePoint = typename Clock::time_point;

    TimePoint startpoint = Clock::now();

  public:
    ElapsedTimer() {}

    ElapsedTimer(const ElapsedTimer& rhs) : startpoint(rhs.startpoint) {}

    explicit ElapsedTimer(const TimePoint& rhs) : startpoint(rhs) {}

    bool
    operator==(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint == rhs.startpoint;
    }
    bool
    operator!=(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint != rhs.startpoint;
    }

    // is younger
    bool
    operator<(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint > rhs.startpoint;
    }
    bool
    operator<=(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint >= rhs.startpoint;
    }

    // is older
    bool
    operator>(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint < rhs.startpoint;
    }
    bool
    operator>=(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint <= rhs.startpoint;
    }

    ClockDuration
    operator-(const ElapsedTimer& rhs) const noexcept
    {
        return startpoint - rhs.startpoint;
    }

    ClockDuration
    elapsed() const noexcept
    {
        return Clock::now() - startpoint;
    }

    ClockDuration
    restart() noexcept
    {
        const ClockDuration ret = elapsed();
        startpoint              = Clock::now();
        return ret;
    }

    template <typename Rep, typename Period>
    bool
    has_expired(
        const std::chrono::duration<Rep, Period>& timeout) const noexcept
    {
        return elapsed() > timeout;
    }
};

template <typename F,
          typename Clock = std::chrono::steady_clock,
          typename... Args>
std::chrono::duration<typename Clock::rep, typename Clock::period>
measure(F&& callable, Args&&... args)
{
    ElapsedTimer<Clock> timer;

    callable(std::forward<Args>(args)...);

    return timer.elapsed();
}
} // namespace tinytimer

void
add(std::size_t n, float* x, float* y) noexcept
{
    for(std::size_t i = 0; i < n; i++) y[i] = x[i] + y[i];
}

// CUDA Kernel function to add the elements of two arrays on the GPU
__global__
void cuda_add(std::size_t n, float *x, float *y)
{
  auto index = blockIdx.x * blockDim.x + threadIdx.x;
  auto stride = blockDim.x * gridDim.x;
  for (auto i = index; i < n; i += stride)
      y[i] = x[i] + y[i];
}

using tinytimer::ElapsedTimer;

int main()
{
    int n_devices;
    cudaGetDeviceCount(&n_devices);

    for (int i = 0; i < n_devices; ++i)
    {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        std::cout << "Device #" << i << ' ' << prop.name << std::endl
                  << "Compute Capability: " << prop.major << '.' << prop.minor << std::endl
                  << "\tMemory Clock Rate (KHz): " << prop.memoryClockRate << std::endl
                  << "\tMemory Bus Width (bits): " << prop.memoryBusWidth << std::endl
                  << "\tPeak Memory Bandwidth (GB/s): " << 2.0*prop.memoryClockRate*(prop.memoryBusWidth/8)/1.0e6 << std::endl;
    }

    std::size_t N = 1<<20;

    std::cout << "Starting\n";
    float *host_x = new float[N];
    float *host_y = new float[N];

    // initialize x and y arrays on the host
    for (std::size_t i = 0; i < N; i++)
    {
        host_x[i] = 1.0f;
        host_y[i] = 2.0f;
    }

    float *device_x, *device_y;
    cudaMalloc(&device_x, N*sizeof(float));
    cudaMalloc(&device_y, N*sizeof(float));
    std::cout << "allocated\n";

    cudaMemcpy(device_x, host_x, N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(device_y, host_y, N * sizeof(float), cudaMemcpyHostToDevice);
    std::cout << "copied\n";

    ElapsedTimer<> timer;
    add(N, host_x, host_y);
    std::cout << "CPU time: " << std::chrono::duration_cast<std::chrono::microseconds>(timer.elapsed()).count() << "μs" << std::endl;

    // Check for errors (all values should be 3.0f)
    float maxError = 0.0f;
    for (std::size_t i = 0; i < N; i++)
        maxError = std::fmax(maxError, std::fabs(host_y[i]-3.0f));
    std::cout << "Max error: " << maxError << std::endl;

    std::size_t blockSize = 256;
    std::size_t numBlocks = (N + blockSize - 1) / blockSize;
    timer.restart();
    cuda_add<<<numBlocks, blockSize>>>(N, device_x, device_y);
    cudaDeviceSynchronize();

    std::cout << "CUDA time: " << std::chrono::duration_cast<std::chrono::microseconds>(timer.elapsed()).count() << "μs" << std::endl;

    cudaMemcpy(host_y, device_y, N * sizeof(float), cudaMemcpyDeviceToHost);
    maxError = 0.f;
    for (std::size_t i = 0; i < N; i++)
        maxError = std::fmax(maxError, std::fabs(host_y[i]-3.0f));
    std::cout << "Max error: " << maxError << std::endl;

    cudaFree(device_x);
    cudaFree(device_y);
    cudaDeviceReset();

    delete [] host_x;
    delete [] host_y;

    return 0;
}
