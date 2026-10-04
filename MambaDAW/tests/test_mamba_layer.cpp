#include "mamba_layer.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {

bool almostEqual(float a, float b, float tolerance = 1e-5f) {
    return std::abs(a - b) <= tolerance;
}

bool loadDeterministicWeights(MambaLayer& layer) {
    if (layer.dModel() != 1) {
        std::cerr << "  FAILED: deterministic test fixture expects d_model=1\n";
        return false;
    }

    const auto d_inner = static_cast<std::size_t>(layer.dInner());
    const auto d_state = static_cast<std::size_t>(layer.dState());
    const auto d_conv = static_cast<std::size_t>(layer.dConv());

    std::vector<float> in_proj(2 * d_inner);
    for (std::size_t i = 0; i < d_inner; ++i) {
        in_proj[i] = 0.25f + 0.05f * static_cast<float>(i);
        in_proj[d_inner + i] = 0.75f + 0.02f * static_cast<float>(i);
    }

    std::vector<float> out_proj(d_inner, 0.5f);
    std::vector<float> conv1d(d_inner * d_conv, 0.5f);
    std::vector<float> dA(d_inner * d_state, 0.8f);
    std::vector<float> dB(d_inner * d_state, 0.5f);
    std::vector<float> C(d_inner * d_state, 0.7f);
    std::vector<float> D(d_inner, 0.1f);

    return layer.loadLayerWeights(
        in_proj.data(), in_proj.size(),
        out_proj.data(), out_proj.size(),
        conv1d.data(), conv1d.size(),
        dA.data(), dA.size(),
        dB.data(), dB.size(),
        C.data(), C.size(),
        D.data(), D.size());
}

bool test_no_nan_inf() {
    std::cout << "[test] no_nan_inf\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) return false;

    for (int i = 0; i < 1000; ++i) {
        const float out = layer.processSample(1.0f);
        if (!std::isfinite(out)) {
            std::cerr << "  FAILED: non-finite output at step " << i << '\n';
            return false;
        }
    }
    std::cout << "  PASSED (1000 finite outputs)\n";
    return true;
}

bool test_state_persistence() {
    std::cout << "[test] state_persistence\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) return false;

    const float first = layer.processSample(1.0f);
    const float second = layer.processSample(1.0f);
    if (almostEqual(first, second, 1e-8f)) {
        std::cerr << "  FAILED: repeated input did not change output ("
                  << first << ", " << second << ")\n";
        return false;
    }
    std::cout << "  PASSED (" << first << " != " << second << ")\n";
    return true;
}

bool test_reset_deterministic() {
    std::cout << "[test] reset_deterministic\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) return false;

    const float before_reset = layer.processSample(1.0f);
    layer.reset_state();
    const float after_reset = layer.processSample(1.0f);
    if (!almostEqual(before_reset, after_reset)) {
        std::cerr << "  FAILED: output after reset differs ("
                  << before_reset << " vs " << after_reset << ")\n";
        return false;
    }
    std::cout << "  PASSED (" << before_reset << " == " << after_reset << ")\n";
    return true;
}

bool test_block_vs_sample() {
    std::cout << "[test] block_vs_sample\n";
    constexpr int sample_count = 256;
    float input[sample_count];
    for (int i = 0; i < sample_count; ++i)
        input[i] = std::sin(0.1f * static_cast<float>(i));

    MambaLayer sample_layer(1, 16, 4, 2);
    MambaLayer block_layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(sample_layer) ||
        !loadDeterministicWeights(block_layer))
        return false;

    float sample_output[sample_count];
    float block_output[sample_count]{};
    for (int i = 0; i < sample_count; ++i)
        sample_output[i] = sample_layer.processSample(input[i]);
    block_layer.processBlock(input, block_output, sample_count);

    for (int i = 0; i < sample_count; ++i) {
        if (!almostEqual(sample_output[i], block_output[i], 1e-6f)) {
            std::cerr << "  FAILED: mismatch at sample " << i << " ("
                      << sample_output[i] << " vs " << block_output[i] << ")\n";
            return false;
        }
    }
    std::cout << "  PASSED (" << sample_count << " samples match with identical weights)\n";
    return true;
}

bool test_zero_input() {
    std::cout << "[test] zero_input\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) return false;

    const float out = layer.processSample(0.0f);
    if (!almostEqual(out, 0.0f, 1e-6f)) {
        std::cerr << "  FAILED: zero input produced " << out << '\n';
        return false;
    }
    std::cout << "  PASSED (zero input -> " << out << ")\n";
    return true;
}

bool test_extreme_inputs() {
    std::cout << "[test] extreme_inputs\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) return false;

    const float extreme_inputs[] = {
        -1e6f, -1000.0f, -10.0f, 10.0f, 1000.0f, 1e6f,
    };
    for (const float input : extreme_inputs) {
        const float output = layer.processSample(input);
        if (!std::isfinite(output)) {
            std::cerr << "  FAILED: non-finite output for input " << input << '\n';
            return false;
        }
    }
    std::cout << "  PASSED (all outputs finite)\n";
    return true;
}

bool test_load_weights_validation() {
    std::cout << "[test] load_weights_validation\n";
    MambaLayer layer(1, 16, 4, 2);
    if (!loadDeterministicWeights(layer)) {
        std::cerr << "  FAILED: valid deterministic weights were rejected\n";
        return false;
    }

    std::vector<float> in_proj(3, 0.0f); // Expected length is 4.
    std::vector<float> out_proj(2, 0.0f);
    std::vector<float> conv1d(8, 0.0f);
    std::vector<float> dA(32, 0.0f);
    std::vector<float> dB(32, 0.0f);
    std::vector<float> C(32, 0.0f);
    std::vector<float> D(2, 0.0f);

    const bool malformed_weights_accepted = layer.loadLayerWeights(
        in_proj.data(), in_proj.size(),
        out_proj.data(), out_proj.size(),
        conv1d.data(), conv1d.size(),
        dA.data(), dA.size(),
        dB.data(), dB.size(),
        C.data(), C.size(),
        D.data(), D.size());
    if (malformed_weights_accepted) {
        std::cerr << "  FAILED: invalid input-projection length was accepted\n";
        return false;
    }

    std::cout << "  PASSED (valid weights accepted; invalid size rejected)\n";
    return true;
}

} // namespace

int main() {
    using Test = bool (*)();
    const Test tests[] = {
        test_no_nan_inf,
        test_state_persistence,
        test_reset_deterministic,
        test_block_vs_sample,
        test_zero_input,
        test_extreme_inputs,
        test_load_weights_validation,
    };

    int failures = 0;
    for (const Test test : tests) {
        if (!test()) ++failures;
    }

    if (failures != 0) {
        std::cerr << "=== " << failures << " test(s) failed ===\n";
        return 1;
    }

    std::cout << "=== All " << (sizeof(tests) / sizeof(tests[0]))
              << " tests passed ===\n";
    return 0;
}
