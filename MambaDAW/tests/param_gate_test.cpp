#include "../src/parameter_gate.h"
#include <array>
#include <cassert>
#include <iostream>

int main() {
    // Test with per-param epsilon: [0.05, 0.0, 1.0, 0.02] for [dry_wet, model_type, bypass, ?]
    std::array<double, 4> eps = {0.05, 0.0, 1.0, 0.02};
    ParameterGate gate(eps);

    // Test 1: No changes -> NoChange
    std::vector<ParamSnapshot> empty;
    assert(gate.evaluate(empty) == ParamUpdateAction::NoChange);

    // Test 2: Initial change (uninitialized) -> Flush
    std::vector<ParamSnapshot> init = {{0, 0.5}};
    assert(gate.evaluate(init) == ParamUpdateAction::FlushAndReinit);
    assert(gate.get_last_value(0) == 0.5);

    // Test 3: Small delta (< epsilon) for param0 -> UpdatePromptOnly
    std::vector<ParamSnapshot> small_p0 = {{0, 0.53}}; // delta = 0.03 < 0.05
    assert(gate.evaluate(small_p0) == ParamUpdateAction::UpdatePromptOnly);
    assert(gate.get_last_value(0) == 0.53);

    // Test 4: Large delta (> epsilon) for param0 -> Flush
    std::vector<ParamSnapshot> large_p0 = {{0, 0.6}}; // delta = 0.07 > 0.05
    assert(gate.evaluate(large_p0) == ParamUpdateAction::FlushAndReinit);
    assert(gate.get_last_value(0) == 0.6);

    // Test 5: Param1 (model_type) has epsilon 0.0 -> any change flushes
    std::vector<ParamSnapshot> p1_change = {{1, 1.0}};
    assert(gate.evaluate(p1_change) == ParamUpdateAction::FlushAndReinit);
    assert(gate.get_last_value(1) == 1.0);

    // Test 6: Param2 (bypass) epsilon 1.0 -> very tolerant, only flush if delta > 1.0 (impossible since 0..1)
    // So bypass changes should never flush via param gate (they're handled separately)
    // Initialize bypass first
    gate.evaluate({{2, 0.0}});
    std::vector<ParamSnapshot> bypass_change = {{2, 0.9}};
    assert(gate.evaluate(bypass_change) == ParamUpdateAction::UpdatePromptOnly); // delta 0.9 < 1.0

    // Test 7: Multi-param, largest delta decides
    gate.reset();
    gate.evaluate({{0, 0.0}});
    gate.evaluate({{1, 0.0}});
    gate.evaluate({{2, 0.5}});
    std::vector<ParamSnapshot> multi = {{0, 0.06}, {1, 0.5}, {2, 0.6}}; // p0 delta 0.06 > 0.05 -> flush, p1 delta 0.5 > 0.0 -> flush, p2 delta 0.1 < 1.0 -> no flush alone
    assert(gate.evaluate(multi) == ParamUpdateAction::FlushAndReinit);

    // Test 8: Bypass is ignored for flush decisions (it's in updates but not counted for param flush)
    gate.reset();
    gate.evaluate({{2, 0.0}}); // init bypass=0
    std::vector<ParamSnapshot> only_bypass = {{2, 1.0}};
    assert(gate.evaluate(only_bypass) == ParamUpdateAction::UpdatePromptOnly); // No flush, just update

    std::cout << "All ParameterGate tests passed.\n";
    return 0;
}
