#pragma once

#include <array>
#include <cstdint>
#include <vector>

/**
 * @brief Gating mechanism for parameter changes to control SSM state flushes.
 *
 * Runs on the main (non-RT) thread. Evaluates a batch of parameter updates
 * and determines whether the changes are significant enough to warrant a
 * full SSM state reset (flush) or if only the parameter values need updating.
 *
 * Uses per-parameter epsilon thresholds.
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
    explicit ParameterGate(std::array<double, 4> per_param_epsilon);

    /**
     * @brief Feed a batch of parameter updates.
     *
     * @param updates Vector of param_id/value changes.
     * @return Action indicating whether to flush SSM state.
     */
    ParamUpdateAction evaluate(const std::vector<ParamSnapshot>& updates);

    /**
     * @brief Reset gate state (e.g., after SSM flush or plugin init).
     */
    void reset();

    /**
     * @brief Get last committed value for a parameter (for testing/debug).
     */
    double get_last_value(uint32_t param_id) const;

private:
    static constexpr uint32_t MAX_PARAMS = 4;
    std::array<double, MAX_PARAMS> per_param_epsilon_;
    std::array<double, MAX_PARAMS> last_values_;
    std::array<bool, MAX_PARAMS> initialized_;
};
