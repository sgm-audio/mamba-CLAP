#include "parameter_gate.h"
#include <algorithm>
#include <cmath>

ParameterGate::ParameterGate() : per_param_epsilon_{0.02, 0.02, 0.02, 0.02} {
    reset();
}

ParameterGate::ParameterGate(std::array<double, 4> per_param_epsilon)
    : per_param_epsilon_(per_param_epsilon) {
    reset();
}

ParamUpdateAction ParameterGate::evaluate(const std::vector<ParamSnapshot>& updates) {
    if (updates.empty()) {
        return ParamUpdateAction::NoChange;
    }

    // Check for any uninitialized parameters (first-time set) - treat as significant change
    for (const auto& upd : updates) {
        if (upd.param_id < MAX_PARAMS && !initialized_[upd.param_id]) {
            // Commit all values
            for (const auto& u2 : updates) {
                if (u2.param_id < MAX_PARAMS) {
                    last_values_[u2.param_id] = u2.value;
                    initialized_[u2.param_id] = true;
                }
            }
            return ParamUpdateAction::FlushAndReinit;
        }
    }

    // All parameters are initialized, compute max delta
    double max_delta = 0.0;
    for (const auto& upd : updates) {
        if (upd.param_id >= MAX_PARAMS)
            continue;
        double delta = std::abs(upd.value - last_values_[upd.param_id]);
        max_delta = std::max(max_delta, delta);
    }

    // Commit all values
    for (const auto& upd : updates) {
        if (upd.param_id < MAX_PARAMS) {
            last_values_[upd.param_id] = upd.value;
            initialized_[upd.param_id] = true;
        }
    }

    // Check if max delta exceeds ANY parameter's epsilon
    for (const auto& upd : updates) {
        if (upd.param_id >= MAX_PARAMS)
            continue;
        if (max_delta > per_param_epsilon_[upd.param_id]) {
            return ParamUpdateAction::FlushAndReinit;
        }
    }

    return ParamUpdateAction::UpdatePromptOnly;
}

void ParameterGate::reset() {
    for (size_t i = 0; i < MAX_PARAMS; ++i) {
        initialized_[i] = false;
        last_values_[i] = 0.0;
    }
}

double ParameterGate::get_last_value(uint32_t param_id) const {
    if (param_id >= MAX_PARAMS) return 0.0;
    return last_values_[param_id];
}
