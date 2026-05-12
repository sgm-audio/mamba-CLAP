#include "mamba_model.h"
#include "mamba_layer.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cmath>

// ============================================================================
// Timer helper
// ============================================================================
using Clock = std::chrono::high_resolution_clock;

static double benchmarkSampleBySample(MambaModel& model, int samples) {
    auto start = Clock::now();
    for (int i = 0; i < samples; ++i) {
        model.processSample(0.5f);
    }
    auto end = Clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count() / samples;
}

static double benchmarkBlock(MambaModel& model, int samples) {
    std::vector<float> input(samples, 0.5f);
    std::vector<float> output(samples);
    auto start = Clock::now();
    model.processBlock(input.data(), output.data(), samples);
    auto end = Clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count() / samples;
}

// ============================================================================
// Baseline: unrolled simple arithmetic (for overhead measurement)
// ============================================================================
static double benchmarkBaseline(int samples) {
    volatile float acc = 0.0f;
    auto start = Clock::now();
    for (int i = 0; i < samples; ++i) {
        acc += 0.5f * 0.1f;
    }
    (void)acc;
    auto end = Clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count() / samples;
}

// ============================================================================
// Print results
// ============================================================================
static void printResult(const char* name, double us_per_sample) {
    double sr = 1'000'000.0 / us_per_sample;
    std::cout << "  " << std::left << std::setw(24) << name
              << std::right << std::setw(8) << std::fixed << std::setprecision(3) << us_per_sample << " us"
              << "    (max SR " << std::setw(10) << std::setprecision(0) << sr << " Hz)\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    constexpr int SAMPLES = 44100;  // 1 second at 44.1 kHz
    constexpr double TARGET_US = 23.0; // < 23 us per sample = real-time at 44.1 kHz

    std::cout << "=== Mamba Performance Benchmark ===\n";
    std::cout << "Target: < " << TARGET_US << " us / sample  (44.1 kHz real-time)\n\n";

    // --- baseline (minimal overhead) ---
    double baseline = benchmarkBaseline(SAMPLES);
    printResult("Baseline (arithmetic)", baseline);

    // --- small model ---
    {
        MambaModel model(1, 2, 16, 4, 2);
        double t = benchmarkSampleBySample(model, SAMPLES);
        printResult("Mamba(2L,16S,4C)", t);
        std::cout << "  " << (t < TARGET_US ? "  [PASS]" : "  [FAIL]") << "\n";
    }

    // --- larger model ---
    {
        MambaModel model(1, 4, 32, 4, 2);
        double t = benchmarkSampleBySample(model, SAMPLES);
        printResult("Mamba(4L,32S,4C)", t);
        std::cout << "  " << (t < TARGET_US ? "  [PASS]" : "  [FAIL]") << "\n";
    }

    // --- block vs sample path ---
    {
        MambaModel model(1, 2, 16, 4, 2);
        double t_sample = benchmarkSampleBySample(model, SAMPLES);
        double t_block  = benchmarkBlock(model, SAMPLES);
        printResult("Block path (2L,16S)", t_block);
        std::cout << "  Overhead vs sample path: " << std::fixed << std::setprecision(2)
                  << (t_block - t_sample) / t_sample * 100.0 << "%\n";
    }

    // --- latency distribution (worst-case estimate via 10 runs) ---
    {
        MambaModel model(1, 2, 16, 4, 2);
        double max_us = 0.0;
        for (int run = 0; run < 10; ++run) {
            double t = benchmarkSampleBySample(model, SAMPLES / 10);
            max_us = std::max(max_us, t);
        }
        std::cout << "\n  Worst-case latency (10 runs): " << max_us << " us\n";
    }

    std::cout << "\n=== Benchmark complete ===\n";
    return 0;
}
