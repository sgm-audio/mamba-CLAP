#include "clap-plugin/include/clap-plugin.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

int main() {
    std::cout << "Testing STFTProcessor..." << std::endl;
    
    // Test parameters
    int n_fft = 1024;
    int hop_length = 256;
    int n_mels = 80;
    double sample_rate = 44100.0;
    
    // Create STFT processor
    mamba_audio::STFTProcessor stft(n_fft, hop_length, n_mels, sample_rate);
    
    // Test with a simple sine wave
    int signal_length = 4410; // 0.1 seconds at 44.1kHz
    std::vector<float> signal(signal_length);
    float frequency = 440.0; // A4 note
    
    for (int i = 0; i < signal_length; ++i) {
        signal[i] = std::sin(2.0 * M_PI * frequency * i / sample_rate);
    }
    
    // Convert to torch tensor
    torch::Tensor input_tensor = torch::from_blob(signal.data(), {1, signal_length}, torch::kFloat32).clone();
    
    // Forward STFT
    torch::Tensor mel_spec = stft.forward(input_tensor);
    std::cout << "STFT forward: input shape [" << input_tensor.sizes()[0] << ", " << input_tensor.sizes()[1] << "]" << std::endl;
    std::cout << "STFT forward: output shape [" << mel_spec.sizes()[0] << ", " << mel_spec.sizes()[1] << "]" << std::endl;
    
    // Check output dimensions
    assert(mel_spec.dim() == 2);
    assert(mel_spec.size(1) == n_mels); // Should be [time_steps, n_mels]
    
    // Inverse STFT
    torch::Tensor output_tensor = stft.inverse(mel_spec);
    std::cout << "STFT inverse: output shape [" << output_tensor.sizes()[0] << "]" << std::endl;
    
    // Check that we get back approximately the same length (accounting for windowing)
    int expected_length = signal_length;
    int actual_length = output_tensor.size(0);
    std::cout << "Expected length: " << expected_length << ", Actual length: " << actual_length << std::endl;
    
    // Allow some difference due to windowing and overlap
    assert(std::abs(expected_length - actual_length) < hop_length * 2);
    
    // Check that the signal is not all zeros
    auto output_data = output_tensor.accessor<float, 1>();
    float max_val = 0.0f;
    for (int i = 0; i < actual_length; ++i) {
        max_val = std::max(max_val, std::abs(output_data[i]));
    }
    std::cout << "Max output value: " << max_val << std::endl;
    assert(max_val > 0.01f); // Should have some signal
    
    std::cout << "STFTProcessor test PASSED" << std::endl;
    return 0;
}