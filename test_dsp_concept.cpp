/**
 * Test file for DSP Processor Concept
 * Compile with: g++ -std=c++20 -c test_dsp_concept.cpp
 */

#include "dsp_processor_concept.hpp"
#include <iostream>
#include <span>
#include <vector>

// Valid DSP processor using float
struct FloatFilter {
    using value_type = float;
    
    void process(std::span<value_type> data) {
        for (auto& sample : data) {
            sample *= 0.5f; // Simple gain
        }
    }
};

// Valid DSP processor using double
struct DoubleFilter {
    using value_type = double;
    
    void process(std::span<value_type> data) {
        for (auto& sample : data) {
            sample *= 0.5; // Simple gain
        }
    }
};

// Invalid: uses int instead of float/double
struct InvalidIntFilter {
    using value_type = int;
    
    void process(std::span<value_type> data) {
        for (auto& sample : data) {
            sample /= 2;
        }
    }
};

// Invalid: wrong process signature (returns int)
struct InvalidReturnFilter {
    using value_type = float;
    
    int process(std::span<value_type> data) {
        return 0;
    }
};

// Invalid: missing value_type
struct NoValueTypeFilter {
    void process(std::span<float> data) {
        // ...
    }
};

// Invalid: wrong parameter type in process
struct WrongParamFilter {
    using value_type = float;
    
    void process(float* data, size_t size) {
        // ...
    }
};

// Function that only accepts valid DSP processors
template<dsp::DSPProcessor P>
void applyProcessing(P& processor, std::span<typename P::value_type> buffer) {
    processor.process(buffer);
}

int main() {
    std::vector<float> floatBuffer = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<double> doubleBuffer = {1.0, 2.0, 3.0, 4.0};
    
    FloatFilter floatFilter;
    DoubleFilter doubleFilter;
    
    // These should compile and work
    applyProcessing(floatFilter, floatBuffer);
    applyProcessing(doubleFilter, doubleBuffer);
    
    std::cout << "Float buffer after processing: ";
    for (const auto& val : floatBuffer) {
        std::cout << val << " ";
    }
    std::cout << "\n";
    
    std::cout << "Double buffer after processing: ";
    for (const auto& val : doubleBuffer) {
        std::cout << val << " ";
    }
    std::cout << "\n";
    
    // Uncomment these to see concept constraint violations:
    // InvalidIntFilter invalidInt;
    // applyProcessing(invalidInt, floatBuffer); // Error: value_type is not float or double
    
    // InvalidReturnFilter invalidReturn;
    // applyProcessing(invalidReturn, floatBuffer); // Error: process doesn't return void
    
    // NoValueTypeFilter noValue;
    // applyProcessing(noValue, floatBuffer); // Error: missing value_type
    
    // WrongParamFilter wrongParam;
    // applyProcessing(wrongParam, floatBuffer); // Error: wrong process signature
    
    std::cout << "All tests passed!\n";
    return 0;
}
