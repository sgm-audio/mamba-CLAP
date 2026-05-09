#include <iostream>
#include <vector>
#include <chrono>
#include "../src/mamba_layer.h"
#include "../src/mamba_model.h"

int main() {
    // Typical parameters
    int64_t d_model = 1;
    int64_t n_layers = 4;
    int64_t d_state = 16;
    int64_t d_conv = 4;
    int64_t expand = 2;

    MambaModel model(d_model, n_layers, d_state, d_conv, expand);

    int block_size = 256;
    std::vector<float> input(block_size, 0.5f);
    std::vector<float> output(block_size, 0.0f);

    // Warmup
    for (int i = 0; i < 100; ++i) {
        model.processBlock(input.data(), output.data(), block_size);
    }

    // Benchmark
    int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        model.processBlock(input.data(), output.data(), block_size);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_us = static_cast<double>(duration_us) / iterations;

    std::cout << "Average time for " << block_size << " samples: " << avg_us << " us" << std::endl;
    std::cout << "Time per sample: " << (avg_us / block_size * 1000.0) << " ns" << std::endl;

    return 0;
}
