#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "tket-c-api.h"

namespace {
void expect(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
void check(TketError error) {
  if (error != TKET_SUCCESS)
    throw std::runtime_error(tket_last_error_message());
}
using Circuit = std::unique_ptr<TketCircuit, decltype(&tket_free_circuit)>;
using Iterator =
    std::unique_ptr<TketCommandIterator, decltype(&tket_free_iterator)>;
using Pass = std::unique_ptr<TketPass, decltype(&tket_free_pass)>;
Circuit circuit(uint32_t nq = 3, uint32_t nb = 1) {
  TketCircuit *raw = nullptr;
  check(tket_circuit_create(nq, nb, &raw));
  return Circuit(raw, tket_free_circuit);
}
Iterator iterator(const Circuit &circuit) {
  TketCommandIterator *raw = nullptr;
  check(tket_circuit_iterator_create(circuit.get(), &raw));
  return Iterator(raw, tket_free_iterator);
}
void add(
    TketCircuit *circuit, TketGate gate, std::vector<TketWire> args,
    std::vector<double> params = {}) {
  check(tket_circuit_add_operation(
      circuit, gate, args.data(), args.size(), params.data(), params.size()));
}
const TketWire q0{TKET_WIRE_QUBIT, 0};
const TketWire q1{TKET_WIRE_QUBIT, 1};
const TketWire q2{TKET_WIRE_QUBIT, 2};
const TketWire c0{TKET_WIRE_BIT, 0};

void all_gates_round_trip() {
  struct Case {
    TketGate gate;
    std::vector<TketWire> args;
    std::vector<double> params;
  };
  // Use different control and target indices, including decreasing indices,
  // so a conversion which sorts arguments would fail these checks. The cases
  // also cover classical measurement arguments and each parameter count.
  const std::vector<Case> cases = {
      {TKET_GATE_H, {q0}, {}},
      {TKET_GATE_CX, {q2, q0}, {}},
      {TKET_GATE_CY, {q2, q0}, {}},
      {TKET_GATE_CZ, {q2, q0}, {}},
      {TKET_GATE_CRZ, {q2, q0}, {0.37}},
      {TKET_GATE_T, {q0}, {}},
      {TKET_GATE_TDG, {q0}, {}},
      {TKET_GATE_S, {q0}, {}},
      {TKET_GATE_SDG, {q0}, {}},
      {TKET_GATE_X, {q0}, {}},
      {TKET_GATE_Y, {q0}, {}},
      {TKET_GATE_Z, {q0}, {}},
      {TKET_GATE_RX, {q0}, {0.37}},
      {TKET_GATE_RY, {q0}, {0.37}},
      {TKET_GATE_RZ, {q0}, {0.37}},
      {TKET_GATE_CCX, {q2, q0, q1}, {}},
      {TKET_GATE_V, {q0}, {}},
      {TKET_GATE_VDG, {q0}, {}},
      {TKET_GATE_PHASEDX, {q0}, {0.37, 0.19}},
      {TKET_GATE_ZZPHASE, {q2, q0}, {0.37}},
      {TKET_GATE_PHASEDXX, {q2, q0}, {0.37, 0.19}},
      {TKET_GATE_TK1, {q0}, {0.37, 0.19, 0.23}},
      {TKET_GATE_TK2, {q2, q0}, {0.37, 0.19, 0.13}},
      {TKET_GATE_SWAP, {q2, q0}, {}},
      {TKET_GATE_XXPHASE, {q2, q0}, {0.37}},
      {TKET_GATE_YYPHASE, {q2, q0}, {0.37}},
      {TKET_GATE_ZZMAX, {q2, q0}, {}},
      {TKET_GATE_MEASURE, {q0, c0}, {}},
      {TKET_GATE_RESET, {q0}, {}},
      {TKET_GATE_BARRIER, {q2, q0, q1}, {}}};
  for (const auto &test : cases) {
    auto c = circuit();
    add(c.get(), test.gate, test.args, test.params);
    auto it = iterator(c);
    TketOperationView op{};
    bool has = false;
    check(tket_iterator_next(it.get(), &op, &has));
    expect(has && op.gate == test.gate, "Wrong gate returned");
    expect(op.args_count == test.args.size(), "Wrong argument count");
    for (size_t i = 0; i < op.args_count; ++i)
      expect(
          op.args[i].kind == test.args[i].kind &&
              op.args[i].index == test.args[i].index,
          "Control/target or classical argument changed");
    expect(op.params_count == test.params.size(), "Wrong parameter count");
    for (size_t i = 0; i < op.params_count; ++i)
      expect(
          std::abs(op.params[i] - test.params[i]) < 1e-12, "Parameter changed");
    for (int repeat = 0; repeat < 2; ++repeat) {
      check(tket_iterator_next(it.get(), &op, &has));
      expect(
          !has && !op.args && !op.params, "Exhaustion should clear the view");
    }
  }
}

void causal_order_and_lifetime() {
  auto c = circuit();
  // Storage order after optimisation isn't an execution order. We should
  // still see the gates on this dependency chain in their causal order.
  add(c.get(), TKET_GATE_H, {q2});
  add(c.get(), TKET_GATE_CX, {q2, q0});
  add(c.get(), TKET_GATE_CCX, {q0, q2, q1});
  auto it = iterator(c);
  // The iterator shares ownership of the graph, so we should still be able
  // to read the complete chain after freeing the public circuit handle.
  c.reset();
  for (const auto gate : {TKET_GATE_H, TKET_GATE_CX, TKET_GATE_CCX}) {
    TketOperationView op{};
    bool has = false;
    check(tket_iterator_next(it.get(), &op, &has));
    expect(has && op.gate == gate, "Causal order changed");
  }

  c = circuit();
  it = iterator(c);
  check(tket_circuit_add_phase(c.get(), 0.125));
  TketOperationView op{};
  bool has = true;
  expect(
      tket_iterator_next(it.get(), &op, &has) ==
              TKET_ERROR_ITERATOR_INVALIDATED &&
          !has,
      "Mutation should invalidate an existing iterator");
}

void passes_and_errors() {
  auto c = circuit();
  add(c.get(), TKET_GATE_CX, {q0, q1});
  add(c.get(), TKET_GATE_CX, {q0, q1});
  TketPass *raw = nullptr;
  check(tket_pass_create_remove_redundancies(&raw));
  Pass remove(raw, tket_free_pass);
  const TketPass *parts[] = {remove.get()};
  check(tket_pass_create_sequence(parts, 1, true, &raw));
  Pass sequence(raw, tket_free_pass);
  remove.reset();
  auto old_iterator = iterator(c);
  check(tket_apply_pass(c.get(), sequence.get()));
  TketOperationView op{};
  bool has = true;
  expect(
      tket_iterator_next(old_iterator.get(), &op, &has) ==
          TKET_ERROR_ITERATOR_INVALIDATED,
      "Pass should invalidate iterator");
  auto it = iterator(c);
  check(tket_iterator_next(it.get(), &op, &has));
  expect(!has, "The two CNOTs should cancel");
  check(tket_circuit_add_phase(c.get(), 0.125));
  double phase = 0.0;
  check(tket_circuit_get_phase(c.get(), &phase));
  expect(std::abs(phase - 0.125) < 1e-12, "Global phase changed");
  bool swaps = true;
  check(tket_circuit_has_implicit_swaps(c.get(), &swaps));
  expect(!swaps, "Unexpected wire permutation");
  uint32_t output = 99;
  check(tket_circuit_output_qubit(c.get(), 2, &output));
  expect(output == 2, "Output wire changed");
  const TketWire duplicate[] = {q0, q0};
  expect(
      tket_circuit_add_operation(
          c.get(), TKET_GATE_CX, duplicate, 2, nullptr, 0) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Duplicate arguments should fail");
  expect(
      tket_circuit_add_phase(
          c.get(), std::numeric_limits<double>::infinity()) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Non-finite angle should fail");
  expect(
      tket_circuit_add_operation(c.get(), 99999, nullptr, 0, nullptr, 0) ==
          TKET_ERROR_UNSUPPORTED_OPERATION,
      "Unknown gate should fail");
  expect(
      tket_circuit_create(0, 0, nullptr) == TKET_ERROR_NULL_POINTER,
      "NULL output should fail");
  auto empty = circuit(0, 0);
  auto empty_it = iterator(empty);
  check(tket_iterator_next(empty_it.get(), &op, &has));
  expect(!has, "An empty circuit should have no operations");

  // The existing JSON entry point must also report an error without letting
  // a C++ exception escape or terminating the caller's process.
  expect(
      tket_circuit_from_json("not json") == nullptr,
      "Invalid JSON should fail");
  expect(
      std::string(tket_last_error_message()).size() > 0,
      "Expected an error message");
}
void squash_to_tk2_and_iterate() {
  auto c = circuit();
  // Squashing two non-cancelling CX gates with a TK2 target should leave a
  // TK2 interaction, which callers need to read back through the direct API.
  add(c.get(), TKET_GATE_CX, {q0, q1});
  add(c.get(), TKET_GATE_H, {q1});
  add(c.get(), TKET_GATE_CX, {q0, q1});
  check(tket_two_qubit_squash(c.get(), TKET_TARGET_TK2, 1.0, false));
  auto it = iterator(c);
  TketOperationView op{};
  bool has = false;
  bool seen_tk2 = false;
  while (true) {
    check(tket_iterator_next(it.get(), &op, &has));
    if (!has) break;
    if (op.gate == TKET_GATE_TK2) {
      expect(op.args_count == 2 && op.params_count == 3, "Invalid TK2 view");
      seen_tk2 = true;
    }
  }
  expect(seen_tk2, "Expected the TK2-targeted squash to return a TK2 gate");
}

void symbolic_operations_and_wire_permutations() {
  // JSON can load operations which the numeric iterator can't represent.
  // Those operations should produce explicit errors when we reach them, so
  // the caller never mistakes a partially returned circuit for a complete one.
  const char *symbolic = R"({"bits":[],"qubits":[["q",[0]]],
    "commands":[{"op":{"type":"Rz","params":["theta"]},"args":[["q",[0]]]}],
    "phase":"0","implicit_permutation":[[["q",[0]],["q",[0]]]]})";
  Circuit c(tket_circuit_from_json(symbolic), tket_free_circuit);
  expect(bool(c), "Could not construct symbolic reference circuit");
  auto it = iterator(c);
  TketOperationView op{};
  bool has = true;
  expect(
      tket_iterator_next(it.get(), &op, &has) ==
              TKET_ERROR_SYMBOLIC_PARAMETER &&
          !has,
      "Symbolic parameters must not be silently omitted");

  const char *unsupported = R"({"bits":[],"qubits":[["q",[0]],["q",[1]]],
    "commands":[{"op":{"type":"CH"},"args":[["q",[0]],["q",[1]]]}],
    "phase":"0","implicit_permutation":
      [[["q",[0]],["q",[0]]],[["q",[1]],["q",[1]]]]})";
  c.reset(tket_circuit_from_json(unsupported));
  expect(bool(c), "Could not construct unsupported reference circuit");
  it = iterator(c);
  expect(
      tket_iterator_next(it.get(), &op, &has) ==
              TKET_ERROR_UNSUPPORTED_OPERATION &&
          !has,
      "Unsupported operations must not be silently omitted");

  // This circuit swaps its output wires through the boundary mapping. There
  // are no SWAP operations for an iterator to return, so callers need the
  // separate permutation query to recover which output belongs to each input.
  const char *permuted = R"({"bits":[],"qubits":[["q",[0]],["q",[1]]],
    "commands":[],"phase":"0","created_qubits":[["q",[0]]],
    "discarded_qubits":[["q",[1]]],"implicit_permutation":
      [[["q",[0]],["q",[1]]],[["q",[1]],["q",[0]]]]})";
  c.reset(tket_circuit_from_json(permuted));
  expect(bool(c), "Could not construct permuted reference circuit");
  bool swaps = false;
  check(tket_circuit_has_implicit_swaps(c.get(), &swaps));
  expect(swaps, "The implicit permutation should be visible");
  uint32_t output = 99;
  check(tket_circuit_output_qubit(c.get(), 0, &output));
  expect(output == 1, "Wrong direction for implicit permutation");
  check(tket_circuit_output_qubit(c.get(), 1, &output));
  expect(output == 0, "The other qubit should reach output zero");
  expect(
      tket_circuit_output_qubit(c.get(), 2, &output) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "An absent input qubit should fail");
  // The imported boundary flags belong to their respective input and output
  // indices, even when the circuit implicitly swaps those wires.
  for (uint32_t qubit : {0u, 1u}) {
    bool created = false;
    bool discarded = false;
    check(tket_circuit_is_created(c.get(), qubit, &created));
    check(tket_circuit_is_discarded(c.get(), qubit, &discarded));
    expect(created == (qubit == 0), "Wrong creation flag");
    expect(discarded == (qubit == 1), "Wrong discard flag");
  }

  c = circuit();
  check(tket_circuit_mark_created(c.get(), 0));
  check(tket_circuit_mark_discarded(c.get(), 0));
  add(c.get(), TKET_GATE_X, {q0});
  it = iterator(c);
  check(tket_iterator_next(it.get(), &op, &has));
  expect(
      has && op.gate == TKET_GATE_X,
      "Creation and discard should be boundaries rather than gates");
  check(tket_iterator_next(it.get(), &op, &has));
  expect(!has, "Unexpected operation after discard");

  const double angle = 0.25;
  expect(
      tket_circuit_add_operation(c.get(), TKET_GATE_RZ, &q1, 1, nullptr, 0) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Missing angle should fail");
  expect(
      tket_circuit_add_operation(c.get(), TKET_GATE_X, &q1, 1, &angle, 1) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Extra angle should fail");
}
}  // namespace
int main() {
  try {
    all_gates_round_trip();
    causal_order_and_lifetime();
    passes_and_errors();
    squash_to_tk2_and_iterate();
    symbolic_operations_and_wire_permutations();
    std::cout << "Native C API checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
