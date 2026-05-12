#include "mamba_layer.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static bool almostEqual(float a, float b, float tol = 1e-5f) {
    return std::abs(a - b) < tol;
}

// ============================================================================
// Test 1: NaN/Inf never leaks out
// ============================================================================
static void test_no_nan_inf() {
    std::cout << "[test] no_nan_inf\n";
    MambaLayer layer(1, 16, 4, 2);
    int ok = 0;
    for (int i = 0; i < 1000; ++i) {
        float out = layer.processSample(1.0f);
        if (std::isnan(out) || std::isinf(out)) {
            std::cerr << "FAILED: NaN/Inf at step " << i << std::endl;
            return;
        }
        ++ok;
    }
    std::cout << "  PASSED  (" << ok << " steps, no NaN/Inf)\n";
}

// ============================================================================
// Test 2: state is persistent (same input -> different output after a step)
// ============================================================================
static void test_state_persistence() {
    std::cout << "[test] state_persistence\n";
    MambaLayer layer(1, 16, 4, 2);

    float first  = layer.processSample(1.0f);
    float second = layer.processSample(1.0f);
    if (first == second) {
        std::cerr << "FAILED: state did not change (" << first << " == " << second << ")\n";
        return;
    }
    std::cout << "  PASSED  (" << first << " != " << second << ")\n";
}

// ============================================================================
// Test 3: reset_state brings us back to deterministic initial output
// ============================================================================
static void test_reset_deterministic() {
    std::cout << "[test] reset_deterministic\n";
    MambaLayer layer(1, 16, 4, 2);

    float a = layer.processSample(1.0f);
    layer.reset_state();
    float b = layer.processSample(1.0f);

    if (!almostEqual(a, b)) {
        std::cerr << "FAILED: reset changed initial behaviour (" << a << " vs " << b << ")\n";
        return;
    }
    std::cout << "  PASSED  (" << a << " == " << b << ")\n";
}

// ============================================================================
// Test 4: processBlock == sequential processSample for each step
// ============================================================================
static void test_block_vs_sample() {
    std::cout << "[test] block_vs_sample\n";
    const int N = 256;
    float input[N];
    for (int i = 0; i < N; ++i) input[i] = std::sin(0.1f * i);

    // Sample-at-a-time path
    MambaLayer layer1(1, 16, 4, 2);
    float out_sample[N];
    for (int i = 0; i < N; ++i) out_sample[i] = layer1.processSample(input[i]);

    // Block path
    MambaLayer layer2(1, 16, 4, 2);
    float out_block[N] = {};
    layer2.processBlock(input, out_block, N);

    int mismatches = 0;
    for (int i = 0; i < N; ++i) {
        if (!almostEqual(out_sample[i], out_block[i], 1e-4f)) {
            ++mismatches;
        }
    }
    if (mismatches > 0) {
        std::cerr << "FAILED: " << mismatches << " mismatches between sample/block paths\n";
        return;
    }
    std::cout << "  PASSED  (" << N << " samples match)\n";
}

// ============================================================================
// Test 5: zero input -> zero-ish output after reset
// ============================================================================
static void test_zero_input() {
    std::cout << "[test] zero_input\n";
    MambaLayer layer(1, 16, 4, 2);
    float out = layer.processSample(0.0f);
    if (!almostEqual(out, 0.0f, 1e-3f)) {
        std::cerr << "FAILED: zero input gave " << out << "\n";
        return;
    }
    std::cout << "  PASSED  (zero -> " << out << ")\n";
}

// ============================================================================
// Test 6: clamp extreme inputs (saturation robustness)
// ============================================================================
static void test_extreme_inputs() {
    std::cout << "[test] extreme_inputs\n";
    MambaLayer layer(1, 16, 4, 2);
    float extreme[] = { -1e6f, -1000.0f, -10.0f, 10.0f, 1000.0f, 1e6f };
    for (float x : extreme) {
        float out = layer.processSample(x);
        if (std::isnan(out) || std::isinf(out)) {
            std::cerr << "FAILED: NaN/Inf for input " << x << " -> " << out << "\n";
            return;
        }
    }
    std::cout << "  PASSED\n";
}

// ============================================================================
// Test 7: loadLayerWeights validation
// ============================================================================
static void test_load_weights() {
    std::cout << "[test] loadLayerWeights\n";
    MambaLayer layer(1, 16, 4, 2);

    const int d_inner = 2;
    const int d_state = 16;
    const int d_conv  = 4;

    std::vector<float> in_proj (1 * 2 * d_inner, 0.0f);
    std::vector<float> out_proj(d_inner, 0.0f);
    std::vector<float> conv1d  (d_inner * d_conv, 0.0f);
    std::vector<float> dA      (d_inner * d_state, 0.0f);
    std::vector<float> dB      (d_inner * d_state, 0.0f);
    std::vector<float> C       (d_inner * d_state, 0.0f);
    std::vector<float> D       (d_inner, 0.0f);

    bool ok = layer.loadLayerWeights(
        in_proj.data(),  in_proj.size(),
        out_proj.data(), out_proj.size(),
        conv1d.data(),   conv1d.size(),
        dA.data(),       dA.size(),
        dB.data(),       dB.size(),
        C.data(),        C.size(),
        D.data(),        D.size()
    );
    if (!ok) {
        std::cerr << "FAILED: loadLayerWeights returned false\n";
        return;
    }
    std::cout << "  PASSED\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "=== MambaLayer Unit Tests ===\n";
    test_no_nan_inf();
    test_state_persistence();
    test_reset_deterministic();
    test_block_vs_sample();
    test_zero_input();
    test_extreme_inputs();
    test_load_weights();
    std::cout << "=== All tests passed ===\n";
    return 0;
}
