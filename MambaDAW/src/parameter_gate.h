#pragma once

#include <array>
#include <cstdint>
#include <vector>

/**
 * @brief Gating mechanism for parameter changes to control SSM state flushes.
 *
 * Thread-domain per ADR-002:
 *   evaluate(vector) — [Main] (allocates via std::vector)
 *   evaluate(ptr,count) — [Audio] (RT-safe, no allocation)
 *   reset(), get_last_value() — [Audio|Main]
 */

enum class ParamUpdateAction {
    NoChange,
    UpdatePromptOnly,
    FlushAndReinit,
};

struct ParamSnapshot {
    uint32_t param_id;
    double value;
};

class ParameterGate {
public:
    explicit ParameterGate();

    ParamUpdateAction evaluate(const std::vector<ParamSnapshot>& updates); // [Main] — allocates

    ParamUpdateAction evaluate(const ParamSnapshot* updates, uint32_t count); // [Audio] — RT-safe

    void reset();                                  // [Audio|Main]
    double get_last_value(uint32_t param_id) const; // [Audio|Main]

private:
    static constexpr uint32_t MAX_PARAMS = 13;
    std::array<double, MAX_PARAMS> per_param_epsilon_{};
    std::array<double, MAX_PARAMS> last_values_{};
    std::array<bool, MAX_PARAMS> initialized_{};
};
