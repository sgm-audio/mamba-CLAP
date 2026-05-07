#include <iostream>
#include "clap-plugin/include/clap-plugin.hpp"

int main() {
    std::cout << "Testing MambaFX CLAP plugin creation..." << std::endl;
    
    // Create a minimal CLAP host (in reality, the host provides this)
    static const clap_host_t dummy_host = {
        // Required fields would be filled by a real host
    };
    
    try {
        // Create plugin instance
        mamba_audio::MambaClapPlugin plugin(&kDescriptor, &dummy_host);
        
        std::cout << "Plugin created successfully" << std::endl;
        std::cout << "Plugin name: " << plugin.get_descriptor()->name << std::endl;
        
        // Test initialization
        if (plugin.init()) {
            std::cout << "Plugin init successful" << std::endl;
        } else {
            std::cout << "Plugin init failed" << std::endl;
        }
        
        // Test activation with standard sample rate
        if (plugin.activate(44100.0, 0.0, 0.0)) {
            std::cout << "Plugin activation successful" << std::endl;
        } else {
            std::cout << "Plugin activation failed" << std::endl;
        }
        
        // Test processing
        plugin.start_processing();
        
        // Create dummy audio buffers
        const int num_samples = 1024;
        float input[num_samples] = {0};
        float output[num_samples] = {0};
        
        // Fill with some test signal (sine wave)
        for (int i = 0; i < num_samples; ++i) {
            input[i] = 0.5f * sin(2.0f * M_PI * 440.0f * i / 44100.0f);
        }
        
        // Create a minimal process structure
        clap_process_t process = {
            .has_output_latency = 0,
            .output_latency = 0,
            .in_place_pair_count = 0,
            .in_place_pairs = nullptr,
            .audio_inputs_count = 1,
            .audio_outputs_count = 1,
            .audio_inputs = {
                {
                    .channel_count = 1,
                    .data32 = input,
                    .data64 = nullptr
                }
            },
            .audio_outputs = {
                {
                    .channel_count = 1,
                    .data32 = output,
                    .data64 = nullptr
                }
            },
            .num_samples = num_samples
        };
        
        plugin.process(&process);
        
        plugin.stop_processing();
        plugin.deactivate();
        
        std::cout << "Test completed successfully" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}