#pragma once

#include <array>
#include <cstdint>
#include <vector>

/**
 * @brief Gating mechanism for parameter changes to control SSM state flushes.
 *
 * Intended call domains per ADR-002, not a hard-real-time audit:
 *   evaluate(vector) — [Main] (caller owns an allocated vector)
 *   evaluate(ptr,count) — proposed [Audio] path; inspect actual operations and caller
 *   reset(), get_last_value() — intended cross-domain; synchronization is unverified
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

    ParamUpdateAction evaluate(const ParamSnapshot* updates, uint32_t count); // [Audio] — intended allocation-free path; not audited

    void reset();                                  // [Audio|Main]
    double get_last_value(uint32_t param_id) const; // [Audio|Main]

private:
    static constexpr uint32_t MAX_PARAMS = 13;
    std::array<double, MAX_PARAMS> per_param_epsilon_{};
    std::array<double, MAX_PARAMS> last_values_{};
    std::array<bool, MAX_PARAMS> initialized_{};
};
