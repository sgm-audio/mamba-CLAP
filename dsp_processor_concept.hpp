#pragma once

#include <concepts>
#include <span>
#include <type_traits>

namespace dsp {

/**
 * @brief Concept that checks if T is either float or double
 */
template<typename T>
concept FloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

/**
 * @brief Concept that enforces a DSP processor interface
 * 
 * Requirements:
 * 1. Must have a nested type alias 'value_type' that is float or double
 * 2. Must have a process method that accepts std::span<value_type>
 * 3. The process method must return void
 * 
 * Example usage:
 * @code
 * struct MyFilter {
 *     using value_type = float;
 *     
 *     void process(std::span<value_type> data) {
 *         // DSP processing logic here
 *     }
 * };
 * 
 * template<DSPProcessor P>
 * void applyProcessing(P& processor, std::span<typename P::value_type> buffer) {
 *     processor.process(buffer);
 * }
 * @endcode
 */
template<typename T>
concept DSPProcessor = requires(T t, std::span<typename T::value_type> span) {
    // Require value_type nested type
    typename T::value_type;
    
    // Require value_type to be float or double
    requires FloatingPoint<typename T::value_type>;
    
    // Require process method with correct signature
    { t.process(span) } -> std::same_as<void>;
};

} // namespace dsp
