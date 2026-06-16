#include "../src/parameter_gate.h"
#include <cassert>
#include <iostream>

int main() {
ParameterGate gate;

// Test 1: No changes -> NoChange
std::vector<ParamSnapshot> empty;
assert(gate.evaluate(empty) == ParamUpdateAction::NoChange);

// Test 2: Initial change (uninitialized) -> Flush
std::vector<ParamSnapshot> init = {{0, 0.5}};
assert(gate.evaluate(init) == ParamUpdateAction::FlushAndReinit);
assert(gate.get_last_value(0) == 0.5);

// Test 3: Small delta (< default epsilon 0.02) -> UpdatePromptOnly
std::vector<ParamSnapshot> small_p0 = {{0, 0.51}}; // delta = 0.01 < 0.02
assert(gate.evaluate(small_p0) == ParamUpdateAction::UpdatePromptOnly);
assert(gate.get_last_value(0) == 0.51);

// Test 4: Large delta (> epsilon 0.02) -> Flush
std::vector<ParamSnapshot> large_p0 = {{0, 0.6}}; // delta = 0.09 > 0.02
assert(gate.evaluate(large_p0) == ParamUpdateAction::FlushAndReinit);
assert(gate.get_last_value(0) == 0.6);

// Test 5: Any uninitialized param -> Flush
gate.reset();
std::vector<ParamSnapshot> p1_change = {{1, 1.0}};
assert(gate.evaluate(p1_change) == ParamUpdateAction::FlushAndReinit);
assert(gate.get_last_value(1) == 1.0);

// Test 6: Multi-param, max delta across any param decides
gate.reset();
gate.evaluate({{0, 0.0}});
gate.evaluate({{1, 0.0}});
gate.evaluate({{2, 0.5}});
std::vector<ParamSnapshot> multi = {{0, 0.06}, {1, 0.5}, {2, 0.6}};
assert(gate.evaluate(multi) == ParamUpdateAction::FlushAndReinit);

// Test 7: Small change on initialized param -> UpdatePromptOnly
gate.reset();
gate.evaluate({{2, 0.0}}); // init param2
std::vector<ParamSnapshot> tiny = {{2, 0.01}}; // delta 0.01 < 0.02
assert(gate.evaluate(tiny) == ParamUpdateAction::UpdatePromptOnly);

std::cout << "All ParameterGate tests passed.\n";
return 0;
}
