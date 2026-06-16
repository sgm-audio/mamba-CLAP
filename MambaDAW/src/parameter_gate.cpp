#include "parameter_gate.h"
#include <algorithm>
#include <cmath>

ParameterGate::ParameterGate() {
    per_param_epsilon_.fill(0.02);
    reset();
}

ParamUpdateAction ParameterGate::evaluate(const std::vector<ParamSnapshot>& updates) {
 if (updates.empty()) {
  return ParamUpdateAction::NoChange;
 }
 return evaluate(updates.data(), static_cast<uint32_t>(updates.size()));
}

ParamUpdateAction ParameterGate::evaluate(const ParamSnapshot* updates, uint32_t count) {
 if (!updates || count == 0) {
  return ParamUpdateAction::NoChange;
 }

 for (uint32_t i = 0; i < count; ++i) {
  if (updates[i].param_id < MAX_PARAMS && !initialized_[updates[i].param_id]) {
   for (uint32_t j = 0; j < count; ++j) {
    if (updates[j].param_id < MAX_PARAMS) {
     last_values_[updates[j].param_id] = updates[j].value;
     initialized_[updates[j].param_id] = true;
    }
   }
   return ParamUpdateAction::FlushAndReinit;
  }
 }

 double max_delta = 0.0;
 for (uint32_t i = 0; i < count; ++i) {
  if (updates[i].param_id >= MAX_PARAMS) continue;
  double delta = std::abs(updates[i].value - last_values_[updates[i].param_id]);
  max_delta = std::max(max_delta, delta);
 }

 for (uint32_t i = 0; i < count; ++i) {
  if (updates[i].param_id < MAX_PARAMS) {
   last_values_[updates[i].param_id] = updates[i].value;
   initialized_[updates[i].param_id] = true;
  }
 }

 for (uint32_t i = 0; i < count; ++i) {
  if (updates[i].param_id >= MAX_PARAMS) continue;
  if (max_delta > per_param_epsilon_[updates[i].param_id]) {
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
