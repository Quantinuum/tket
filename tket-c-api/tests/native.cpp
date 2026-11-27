#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
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
// Use the C API's free functions as deleters so an assertion failure also
// releases the handles. C callers make the same calls explicitly.
using Circuit = std::unique_ptr<TketCircuit, decltype(&tket_free_circuit)>;
using Iterator =
    std::unique_ptr<TketCommandIterator, decltype(&tket_free_iterator)>;
using Pass = std::unique_ptr<TketPass, decltype(&tket_free_pass)>;
Circuit make_circuit(uint32_t qubit_count = 3, uint32_t bit_count = 1) {
  TketCircuit *circuit_handle = nullptr;
  check(tket_circuit_create(qubit_count, bit_count, &circuit_handle));
  return Circuit(circuit_handle, tket_free_circuit);
}
Iterator make_iterator(const Circuit &circuit) {
  TketCommandIterator *iterator_handle = nullptr;
  check(tket_circuit_iterator_create(circuit.get(), &iterator_handle));
  return Iterator(iterator_handle, tket_free_iterator);
}
void append_gate(
    TketCircuit *circuit, TketGate gate, std::vector<TketWire> arguments,
    std::vector<double> parameters = {}) {
  check(tket_circuit_add_operation(
      circuit, gate, arguments.data(), arguments.size(), parameters.data(),
      parameters.size()));
}
TketBytes text(std::string_view value) {
  return {reinterpret_cast<const uint8_t *>(value.data()), value.size()};
}
std::string_view read_text(TketBytes value) {
  return value.data
             ? std::string_view(
                   reinterpret_cast<const char *>(value.data), value.size)
             : std::string_view{};
}

// In the default registers, these are q[0], q[1], q[2] and c[0].
const TketWire qubit_0{TKET_WIRE_QUBIT, 0};
const TketWire qubit_1{TKET_WIRE_QUBIT, 1};
const TketWire qubit_2{TKET_WIRE_QUBIT, 2};
const TketWire bit_0{TKET_WIRE_BIT, 0};

void all_gates_round_trip() {
  struct GateCase {
    TketGate gate;
    std::vector<TketWire> arguments;
    std::vector<double> parameters;
  };
  // Use different control and target indices, including decreasing indices,
  // so a conversion which sorts arguments would fail these checks. The cases
  // also cover classical measurement arguments and each parameter count.
  const std::vector<GateCase> cases = {
      {TKET_GATE_H, {qubit_0}, {}},
      {TKET_GATE_CX, {qubit_2, qubit_0}, {}},
      {TKET_GATE_CY, {qubit_2, qubit_0}, {}},
      {TKET_GATE_CZ, {qubit_2, qubit_0}, {}},
      {TKET_GATE_CRZ, {qubit_2, qubit_0}, {0.37}},
      {TKET_GATE_T, {qubit_0}, {}},
      {TKET_GATE_TDG, {qubit_0}, {}},
      {TKET_GATE_S, {qubit_0}, {}},
      {TKET_GATE_SDG, {qubit_0}, {}},
      {TKET_GATE_X, {qubit_0}, {}},
      {TKET_GATE_Y, {qubit_0}, {}},
      {TKET_GATE_Z, {qubit_0}, {}},
      {TKET_GATE_RX, {qubit_0}, {0.37}},
      {TKET_GATE_RY, {qubit_0}, {0.37}},
      {TKET_GATE_RZ, {qubit_0}, {0.37}},
      {TKET_GATE_CCX, {qubit_2, qubit_0, qubit_1}, {}},
      {TKET_GATE_V, {qubit_0}, {}},
      {TKET_GATE_VDG, {qubit_0}, {}},
      {TKET_GATE_PHASEDX, {qubit_0}, {0.37, 0.19}},
      {TKET_GATE_ZZPHASE, {qubit_2, qubit_0}, {0.37}},
      {TKET_GATE_PHASEDXX, {qubit_2, qubit_0}, {0.37, 0.19}},
      {TKET_GATE_TK1, {qubit_0}, {0.37, 0.19, 0.23}},
      {TKET_GATE_TK2, {qubit_2, qubit_0}, {0.37, 0.19, 0.13}},
      {TKET_GATE_SWAP, {qubit_2, qubit_0}, {}},
      {TKET_GATE_XXPHASE, {qubit_2, qubit_0}, {0.37}},
      {TKET_GATE_YYPHASE, {qubit_2, qubit_0}, {0.37}},
      {TKET_GATE_ZZMAX, {qubit_2, qubit_0}, {}},
      {TKET_GATE_MEASURE, {qubit_0, bit_0}, {}},
      {TKET_GATE_RESET, {qubit_0}, {}},
      {TKET_GATE_PHASE, {}, {0.125}},
      {TKET_GATE_NOOP, {qubit_0}, {}},
      {TKET_GATE_BARRIER, {qubit_2, qubit_0, qubit_1}, {}}};
  // Use a fresh circuit per operation. That makes the first iterator result
  // unambiguous even for a gate with no inputs, such as Phase.
  for (const auto &gate_case : cases) {
    auto single_operation_circuit = make_circuit();
    append_gate(
        single_operation_circuit.get(), gate_case.gate, gate_case.arguments,
        gate_case.parameters);
    auto operation_iterator = make_iterator(single_operation_circuit);
    TketOperationView operation{};
    bool has_operation = false;
    check(tket_iterator_next(
        operation_iterator.get(), &operation, &has_operation));
    expect(
        has_operation && operation.gate == gate_case.gate,
        "Wrong gate returned");
    expect(
        operation.args_count == gate_case.arguments.size(),
        "Wrong argument count");
    for (size_t i = 0; i < operation.args_count; ++i)
      expect(
          operation.args[i].kind == gate_case.arguments[i].kind &&
              operation.args[i].index == gate_case.arguments[i].index,
          "Control/target or classical argument changed");
    expect(
        operation.params_count == gate_case.parameters.size(),
        "Wrong parameter count");
    for (size_t i = 0; i < operation.params_count; ++i)
      expect(
          std::abs(operation.params[i] - gate_case.parameters[i]) < 1e-12,
          "Parameter changed");
    // Exhaustion sets has_operation to false and clears the view. The call
    // still succeeds.
    for (int repeat = 0; repeat < 2; ++repeat) {
      check(tket_iterator_next(
          operation_iterator.get(), &operation, &has_operation));
      expect(
          !has_operation && !operation.args && !operation.params,
          "Exhaustion should clear the view");
    }
  }
}

void causal_order_and_lifetime() {
  auto dependent_gate_circuit = make_circuit();
  // Each gate depends on its predecessor through a shared qubit. The
  // iterator must report them in that order.
  append_gate(dependent_gate_circuit.get(), TKET_GATE_H, {qubit_2});
  append_gate(dependent_gate_circuit.get(), TKET_GATE_CX, {qubit_2, qubit_0});
  append_gate(
      dependent_gate_circuit.get(), TKET_GATE_CCX, {qubit_0, qubit_2, qubit_1});
  auto dependency_iterator = make_iterator(dependent_gate_circuit);
  // The iterator shares ownership of the graph, so we should still be able
  // to read the complete chain after freeing the public circuit handle.
  dependent_gate_circuit.reset();
  for (const auto gate : {TKET_GATE_H, TKET_GATE_CX, TKET_GATE_CCX}) {
    TketOperationView operation{};
    bool has_operation = false;
    check(tket_iterator_next(
        dependency_iterator.get(), &operation, &has_operation));
    expect(has_operation && operation.gate == gate, "Causal order changed");
  }

  // Take a second iterator and mutate its circuit. After invalidation,
  // create a fresh iterator if you need to read the updated circuit.
  auto mutable_circuit = make_circuit();
  auto invalidated_iterator = make_iterator(mutable_circuit);
  check(tket_circuit_add_phase(mutable_circuit.get(), 0.125));
  TketOperationView operation{};
  bool has_operation = true;
  expect(
      tket_iterator_next(
          invalidated_iterator.get(), &operation, &has_operation) ==
              TKET_ERROR_ITERATOR_INVALIDATED &&
          !has_operation,
      "Mutation should invalidate an existing iterator");
}

void apply_a_pass_sequence() {
  // Two identical CNOTs cancel. Wrap the redundancy pass in a sequence to
  // demonstrate how the sequence keeps its component handles alive.
  auto cancellation_circuit = make_circuit();
  append_gate(cancellation_circuit.get(), TKET_GATE_CX, {qubit_0, qubit_1});
  append_gate(cancellation_circuit.get(), TKET_GATE_CX, {qubit_0, qubit_1});
  TketPass *pass_handle = nullptr;
  check(tket_pass_create_remove_redundancies(&pass_handle));
  Pass remove_redundancies(pass_handle, tket_free_pass);
  const TketPass *component_passes[] = {remove_redundancies.get()};
  check(tket_pass_create_sequence(component_passes, 1, true, &pass_handle));
  Pass sequence(pass_handle, tket_free_pass);
  remove_redundancies.reset();
  auto original_iterator = make_iterator(cancellation_circuit);
  check(tket_apply_pass(cancellation_circuit.get(), sequence.get()));
  TketOperationView operation{};
  bool has_operation = true;
  expect(
      tket_iterator_next(original_iterator.get(), &operation, &has_operation) ==
          TKET_ERROR_ITERATOR_INVALIDATED,
      "Pass should invalidate iterator");
  // Applying a pass invalidates the earlier iterator. Inspect the result
  // with a new one and check that the expected cancellation took place.
  auto result_iterator = make_iterator(cancellation_circuit);
  check(tket_iterator_next(result_iterator.get(), &operation, &has_operation));
  expect(!has_operation, "The two CNOTs should cancel");

  // A circuit's global phase and output routing have separate queries.
  // 0.125 half-turns adds a phase of pi/8 radians.
  check(tket_circuit_add_phase(cancellation_circuit.get(), 0.125));
  double phase = 0.0;
  check(tket_circuit_get_phase(cancellation_circuit.get(), &phase));
  expect(std::abs(phase - 0.125) < 1e-12, "Global phase changed");
  bool has_implicit_swaps = true;
  check(tket_circuit_has_implicit_swaps(
      cancellation_circuit.get(), &has_implicit_swaps));
  expect(!has_implicit_swaps, "Unexpected wire permutation");
  uint32_t output_qubit = 99;
  check(
      tket_circuit_output_qubit(cancellation_circuit.get(), 2, &output_qubit));
  expect(output_qubit == 2, "Output wire changed");
}

void invalid_inputs_report_status_codes() {
  auto empty_circuit = make_circuit();
  // Each invalid call is checked at the boundary where it occurs. Reuse the
  // valid handle afterwards to confirm these failures leave it usable.
  const TketWire duplicate_wires[] = {qubit_0, qubit_0};
  expect(
      tket_circuit_add_operation(
          empty_circuit.get(), TKET_GATE_CX, duplicate_wires, 2, nullptr, 0) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Duplicate arguments should fail");
  expect(
      tket_circuit_add_phase(
          empty_circuit.get(), std::numeric_limits<double>::infinity()) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Non-finite angle should fail");
  expect(
      tket_circuit_add_operation(
          empty_circuit.get(), 99999, nullptr, 0, nullptr, 0) ==
          TKET_ERROR_UNSUPPORTED_OPERATION,
      "Unknown gate should fail");
  expect(
      tket_circuit_create(0, 0, nullptr) == TKET_ERROR_NULL_POINTER,
      "NULL output should fail");
  const double unexpected_angle = 0.25;
  expect(
      tket_circuit_add_operation(
          empty_circuit.get(), TKET_GATE_RZ, &qubit_1, 1, nullptr, 0) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Missing angle should fail");
  expect(
      tket_circuit_add_operation(
          empty_circuit.get(), TKET_GATE_X, &qubit_1, 1, &unexpected_angle,
          1) == TKET_ERROR_INVALID_ARGUMENT,
      "Extra angle should fail");
  auto empty_circuit_iterator = make_iterator(empty_circuit);
  TketOperationView operation{};
  bool has_operation = true;
  check(tket_iterator_next(
      empty_circuit_iterator.get(), &operation, &has_operation));
  expect(!has_operation, "Rejected operations should leave the circuit empty");

  // The existing JSON entry point must also report an error without letting
  // a C++ exception escape or terminating the caller's process.
  expect(
      tket_circuit_from_json("not json") == nullptr,
      "Invalid JSON should fail");
  expect(
      std::string(tket_last_error_message()).size() > 0,
      "Expected an error message");
}
void rebase_to_a_typed_basis() {
  // Supply a basis with a general one-qubit gate and an entangling gate.
  // The input uses H and CZ, so both parts of the basis are exercised.
  auto circuit_to_rebase = make_circuit(2, 0);
  append_gate(circuit_to_rebase.get(), TKET_GATE_H, {qubit_0});
  append_gate(circuit_to_rebase.get(), TKET_GATE_CZ, {qubit_0, qubit_1});
  const TketGate basis[] = {TKET_GATE_TK1, TKET_GATE_CX};
  TketPass *pass_handle = nullptr;
  check(tket_pass_create_auto_rebase(basis, 2, false, &pass_handle));
  const Pass rebase_pass(pass_handle, tket_free_pass);
  check(tket_apply_pass(circuit_to_rebase.get(), rebase_pass.get()));

  // Read the transformed circuit through a fresh iterator. Its quantum
  // operations should all belong to the requested basis.
  auto rebased_iterator = make_iterator(circuit_to_rebase);
  TketOperationView operation{};
  bool has_operation = false;
  bool saw_tk1 = false, saw_cx = false;
  while (true) {
    check(
        tket_iterator_next(rebased_iterator.get(), &operation, &has_operation));
    if (!has_operation) break;
    expect(
        operation.gate == TKET_GATE_TK1 || operation.gate == TKET_GATE_CX ||
            operation.gate == TKET_GATE_PHASE,
        "Rebased circuit contains a gate outside the requested basis");
    saw_tk1 |= operation.gate == TKET_GATE_TK1;
    saw_cx |= operation.gate == TKET_GATE_CX;
  }
  expect(saw_tk1 && saw_cx, "Expected the rebase to use both basis gates");

  // CX alone cannot express arbitrary single-qubit gates. The pass builder
  // reports the unsupported basis and clears the output handle.
  expect(
      tket_pass_create_auto_rebase(basis + 1, 1, false, &pass_handle) ==
              TKET_ERROR_UNSUPPORTED_OPERATION &&
          pass_handle == nullptr,
      "Expected an unsupported basis to leave no pass handle");
}

void squash_to_tk2_and_iterate() {
  auto circuit_to_squash = make_circuit();
  // Squashing two non-cancelling CX gates with a TK2 target should leave a
  // TK2 interaction, which callers need to read back through the direct API.
  append_gate(circuit_to_squash.get(), TKET_GATE_CX, {qubit_0, qubit_1});
  append_gate(circuit_to_squash.get(), TKET_GATE_H, {qubit_1});
  append_gate(circuit_to_squash.get(), TKET_GATE_CX, {qubit_0, qubit_1});
  check(tket_two_qubit_squash(
      circuit_to_squash.get(), TKET_TARGET_TK2, 1.0, false));
  auto squashed_iterator = make_iterator(circuit_to_squash);
  TketOperationView operation{};
  bool has_operation = false;
  bool seen_tk2 = false;
  while (true) {
    check(tket_iterator_next(
        squashed_iterator.get(), &operation, &has_operation));
    if (!has_operation) break;
    if (operation.gate == TKET_GATE_TK2) {
      expect(
          operation.args_count == 2 && operation.params_count == 3,
          "Invalid TK2 view");
      seen_tk2 = true;
    }
  }
  expect(seen_tk2, "Expected the TK2-targeted squash to return a TK2 gate");
}

void symbolic_angles() {
  // Build a symbolic rotation directly. We pass one expression in half-turns
  // for the gate's one angle and a separate array for its wire arguments.
  auto symbolic_circuit = make_circuit(1, 0);
  const char *angles[] = {"theta"};
  check(tket_circuit_add_operation_with_expressions(
      symbolic_circuit.get(), TKET_GATE_RZ, &qubit_0, 1, angles, 1));
  auto numeric_iterator = make_iterator(symbolic_circuit);
  TketOperationView numeric_operation{};
  bool has_operation = true;
  expect(
      tket_iterator_next(
          numeric_iterator.get(), &numeric_operation, &has_operation) ==
              TKET_ERROR_SYMBOLIC_PARAMETER &&
          !has_operation,
      "A numeric step should report a symbolic angle");

  // A failed step invalidates its iterator. Start again with the expression
  // step, which can return the angle in its original symbolic form.
  numeric_iterator.reset();
  auto expression_iterator = make_iterator(symbolic_circuit);
  TketExpressionOperationView operation{};
  check(tket_iterator_next_with_expressions(
      expression_iterator.get(), &operation, &has_operation));
  expect(
      has_operation && operation.gate == TKET_GATE_RZ &&
          operation.expression_count == 1,
      "Expected the symbolic rotation");
  expect(
      std::string_view(operation.expressions[0]) == "theta",
      "Symbolic angle changed");
  check(tket_iterator_next_with_expressions(
      expression_iterator.get(), &operation, &has_operation));
  expect(!has_operation, "Expected exactly one rotation");
}

void boundary_annotations_and_permutations() {
  // This circuit swaps its output wires through the boundary mapping. There
  // are no SWAP operations for an iterator to return, so callers need the
  // separate permutation query to recover which output belongs to each input.
  const char *permuted_circuit_json =
      R"({"bits":[],"qubits":[["q",[0]],["q",[1]]],
    "commands":[],"phase":"0","created_qubits":[["q",[0]]],
    "discarded_qubits":[["q",[1]]],"implicit_permutation":
      [[["q",[0]],["q",[1]]],[["q",[1]],["q",[0]]]]})";
  Circuit permuted_circuit(
      tket_circuit_from_json(permuted_circuit_json), tket_free_circuit);
  expect(
      bool(permuted_circuit), "Could not construct permuted reference circuit");
  bool has_implicit_swaps = false;
  check(tket_circuit_has_implicit_swaps(
      permuted_circuit.get(), &has_implicit_swaps));
  expect(has_implicit_swaps, "The implicit permutation should be visible");
  uint32_t output_qubit = 99;
  check(tket_circuit_output_qubit(permuted_circuit.get(), 0, &output_qubit));
  expect(output_qubit == 1, "Wrong direction for implicit permutation");
  check(tket_circuit_output_qubit(permuted_circuit.get(), 1, &output_qubit));
  expect(output_qubit == 0, "The other qubit should reach output zero");
  expect(
      tket_circuit_output_qubit(permuted_circuit.get(), 2, &output_qubit) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "An absent input qubit should fail");
  // The imported boundary flags belong to their respective input and output
  // indices, even when the circuit implicitly swaps those wires.
  for (uint32_t qubit : {0u, 1u}) {
    bool created = false;
    bool discarded = false;
    check(tket_circuit_is_created(permuted_circuit.get(), qubit, &created));
    check(tket_circuit_is_discarded(permuted_circuit.get(), qubit, &discarded));
    expect(created == (qubit == 0), "Wrong creation flag");
    expect(discarded == (qubit == 1), "Wrong discard flag");
  }

  // The same boundary annotations can be set on a circuit built directly.
  // They are still read separately from the actual operation stream.
  auto annotated_circuit = make_circuit();
  check(tket_circuit_mark_created(annotated_circuit.get(), 0));
  check(tket_circuit_mark_discarded(annotated_circuit.get(), 0));
  append_gate(annotated_circuit.get(), TKET_GATE_X, {qubit_0});
  auto annotated_iterator = make_iterator(annotated_circuit);
  TketOperationView operation{};
  bool has_operation = false;
  check(
      tket_iterator_next(annotated_iterator.get(), &operation, &has_operation));
  expect(
      has_operation && operation.gate == TKET_GATE_X,
      "Expected X as the only operation");
  check(
      tket_iterator_next(annotated_iterator.get(), &operation, &has_operation));
  expect(!has_operation, "Unexpected operation after discard");
}
void classical_payloads_and_boxes() {
  // The circuit-name query returns an owned string. Release it with the
  // API's string deleter.
  auto outer_circuit = make_circuit(3, 2);
  check(tket_circuit_set_name(outer_circuit.get(), "payloads"));
  char *name = nullptr;
  check(tket_circuit_get_name(outer_circuit.get(), &name));
  const std::unique_ptr<char, decltype(&tket_free_string)> owned_name(
      name, tket_free_string);
  expect(std::string(name) == "payloads", "Circuit name changed");

  // SetBits pairs each bit index with a byte containing 0 or 1. Here the
  // barrier will read c[0] from this assignment and a later write will reset
  // it.
  const uint32_t bit_indices[] = {0, 1};
  const uint8_t initial_values[] = {1, 0};
  const uint8_t reset_values[] = {0};
  check(tket_circuit_add_set_bits(
      outer_circuit.get(), bit_indices, initial_values, 2));
  // Delay the barrier on a quantum wire. A simple graph sort would then
  // emit the second SetBits before the barrier has read the old value.
  append_gate(outer_circuit.get(), TKET_GATE_H, {qubit_2});
  append_gate(outer_circuit.get(), TKET_GATE_H, {qubit_2});
  append_gate(outer_circuit.get(), TKET_GATE_H, {qubit_2});
  // Signature characters follow argument order. B reads c[0], Q carries
  // q[2] and C carries c[1] as a read/write bit. The payload can contain NUL.
  const TketWire barrier_wires[] = {bit_0, qubit_2, {TKET_WIRE_BIT, 1}};
  const char barrier_data[] = {'a', '\0', 'b'};
  check(tket_circuit_add_barrier(
      outer_circuit.get(), barrier_wires, 3, "BQC", barrier_data,
      sizeof(barrier_data)));
  check(tket_circuit_add_set_bits(
      outer_circuit.get(), bit_indices, reset_values, 1));

  // Add a box mapping its internal q[0] onto the outer q[2]. The builder
  // copies the contents, so it is safe to free the inner handle immediately.
  auto box_contents = make_circuit(1, 0);
  append_gate(box_contents.get(), TKET_GATE_X, {qubit_0});
  const char box_uuid[] = "01234567-89ab-cdef-0123-456789abcdef";
  check(tket_circuit_add_circbox(
      outer_circuit.get(), box_contents.get(), &qubit_2, 1, box_uuid));
  box_contents.reset();

  // Relabel the outputs with a three-cycle. This distinguishes following
  // the wire from accidentally applying the inverse permutation.
  const uint32_t outputs[] = {2, 0, 1};
  check(tket_circuit_permute_outputs(outer_circuit.get(), outputs, 3));
  const uint32_t repeated_outputs[] = {0, 0, 1};
  expect(
      tket_circuit_permute_outputs(outer_circuit.get(), repeated_outputs, 3) ==
          TKET_ERROR_INVALID_ARGUMENT,
      "Repeated output indices should fail");
  for (uint32_t input = 0; input < 3; ++input) {
    uint32_t output = 99;
    check(tket_circuit_output_qubit(outer_circuit.get(), input, &output));
    expect(output == outputs[input], "Wrong output permutation");
  }
  uint32_t qubit_count = 0, bit_count = 0;
  check(tket_circuit_register_sizes(
      outer_circuit.get(), &qubit_count, &bit_count));
  expect(qubit_count == 3 && bit_count == 2, "Wrong register sizes");

  // Inspect each operation's payload while positioned on its command.
  // The next iterator call may reuse the same storage.
  auto outer_iterator = make_iterator(outer_circuit);
  TketOperationView operation{};
  bool has_operation = false;
  bool read_barrier = false, read_box = false;
  unsigned assignment_count = 0;
  while (true) {
    check(tket_iterator_next(outer_iterator.get(), &operation, &has_operation));
    if (!has_operation) break;
    if (operation.gate == TKET_GATE_SET_BITS) {
      const uint8_t *values = nullptr;
      size_t size = 0;
      check(tket_iterator_get_payload(outer_iterator.get(), &values, &size));
      if (++assignment_count == 1)
        expect(
            size == 2 && values[0] == 1 && values[1] == 0,
            "Wrong SetBits values");
      else
        expect(
            read_barrier && size == 1 && values[0] == 0,
            "A write overtook a barrier read");
    }
    if (operation.gate == TKET_GATE_BARRIER) {
      const char *signature = nullptr;
      const uint8_t *payload = nullptr;
      size_t size = 0;
      check(tket_iterator_get_barrier_signature(
          outer_iterator.get(), &signature));
      check(tket_iterator_get_payload(outer_iterator.get(), &payload, &size));
      expect(
          operation.args_count == 3 && std::string(signature, 3) == "BQC",
          "Barrier signature changed");
      expect(
          size == sizeof(barrier_data) &&
              reinterpret_cast<const char *>(payload)[2] == 'b' &&
              payload[1] == 0,
          "Barrier payload changed");
      read_barrier = true;
    }
    if (operation.gate == TKET_GATE_CIRCBOX) {
      TketCircuit *box_contents_handle = nullptr;
      char returned_box_uuid[37];
      check(tket_iterator_get_circbox(
          outer_iterator.get(), &box_contents_handle, returned_box_uuid));
      // This copy has its own handle and can be kept after the iterator
      // advances. Release it with the usual circuit deleter.
      const Circuit copied_box_contents(box_contents_handle, tket_free_circuit);
      check(tket_circuit_register_sizes(
          copied_box_contents.get(), &qubit_count, &bit_count));
      expect(
          qubit_count == 1 && bit_count == 0 &&
              std::string(returned_box_uuid) == box_uuid,
          "Circuit box changed");
      auto box_iterator = make_iterator(copied_box_contents);
      TketOperationView box_operation{};
      bool has_box_operation = false;
      check(tket_iterator_next(
          box_iterator.get(), &box_operation, &has_box_operation));
      expect(
          has_box_operation && box_operation.gate == TKET_GATE_X,
          "Box contents changed");
      check(tket_iterator_next(
          box_iterator.get(), &box_operation, &has_box_operation));
      expect(!has_box_operation, "Expected one gate inside the box");
      read_box = true;
    }
  }
  expect(
      read_barrier && read_box && assignment_count == 2,
      "Expected a barrier, a box and both assignments");
}

void named_wires_and_groups() {
  // Declare the qubits in the order the input happens to provide them.
  // The builder uses their positions as IDs. These are coordinate pairs in
  // one register, so neither position is the qubit's register index.
  const uint32_t right_coordinates[] = {1, 8};
  const uint32_t left_coordinates[] = {1, 3};
  const TketNamedWire declarations[] = {
      {TKET_WIRE_QUBIT, text("grid"), right_coordinates, 2},
      {TKET_WIRE_QUBIT, text("grid"), left_coordinates, 2}};
  TketCircuit *circuit_handle = nullptr;
  check(tket_circuit_create_named(
      declarations, 2, 0, 0, text("grid circuit"), &circuit_handle));
  Circuit grid_circuit(circuit_handle, tket_free_circuit);

  // Append Rz to the wire declared at position 0 and give that command a
  // group. The expression is in half-turns and remains symbolic in TKET.
  const TketWireId right_wire_id = 0;
  const TketWireId left_wire_id = 1;
  const TketCommandPorts ports{&right_wire_id, 1, text("rotations")};
  const char *angles[] = {"theta"};
  check(tket_circuit_append_named_gate(
      grid_circuit.get(), &ports, TKET_GATE_RZ, angles, 1));

  // Create the right input and swap the two output labels. Mark discard
  // after permuting so the flag refers to the output's final label.
  check(tket_circuit_mark_named_boundary(
      grid_circuit.get(), right_wire_id, false));
  const TketWireId original_outputs[] = {right_wire_id, left_wire_id};
  const TketWireId relabeled_outputs[] = {left_wire_id, right_wire_id};
  check(tket_circuit_permute_named_outputs(
      grid_circuit.get(), original_outputs, relabeled_outputs, 2));
  check(
      tket_circuit_mark_named_boundary(grid_circuit.get(), left_wire_id, true));

  // A fresh iterator assigns IDs to the circuit it reads. Resolve them by
  // looking up each wire's register and coordinates before using gate views.
  TketCommandIterator *iterator_handle = nullptr;
  size_t wire_count = 0;
  check(tket_circuit_named_iterator_create(
      grid_circuit.get(), &iterator_handle, &wire_count));
  const Iterator grid_iterator(iterator_handle, tket_free_iterator);
  expect(wire_count == 2, "Expected both declared wires");
  TketBytes name{};
  check(tket_named_iterator_get_name(grid_iterator.get(), &name));
  expect(read_text(name) == "grid circuit", "Circuit name changed");
  struct WireDetails {
    uint32_t column;
    bool created;
    bool discarded;
    TketWireId output_wire_id;
  };
  std::vector<WireDetails> wires;
  for (TketWireId wire_id = 0; wire_id < wire_count; ++wire_id) {
    TketNamedWire view{};
    bool created = false, discarded = false;
    TketWireId output_wire_id = 0;
    check(tket_named_iterator_get_wire(
        grid_iterator.get(), wire_id, &view, &created, &discarded,
        &output_wire_id));
    expect(
        view.kind == TKET_WIRE_QUBIT && read_text(view.name) == "grid" &&
            view.index_count == 2 && view.indices[0] == 1,
        "Expected a qubit in row one of grid");
    // Copy the column now. The next lookup can reuse the coordinate buffer.
    wires.push_back({view.indices[1], created, discarded, output_wire_id});
  }

  TketNamedOperationView command{};
  bool has_operation = false;
  check(
      tket_named_iterator_next(grid_iterator.get(), &command, &has_operation));
  expect(
      has_operation && command.operation.gate == TKET_GATE_RZ,
      "Expected the rotation");
  expect(read_text(command.group) == "rotations", "Operation group changed");
  expect(
      command.operation.expression_count == 1 &&
          std::string_view(command.operation.expressions[0]) == "theta",
      "Symbolic angle changed");
  expect(command.operation.args_count == 1, "Expected one rotation wire");
  const auto &rotation_input = wires.at(command.operation.args[0].index);
  const auto &rotation_output = wires.at(rotation_input.output_wire_id);
  expect(
      rotation_input.column == 8 && rotation_input.created &&
          !rotation_input.discarded,
      "The rotation should use the created input at grid[1,8]");
  expect(
      rotation_output.column == 3 && !rotation_output.created &&
          rotation_output.discarded,
      "The created input should reach the discarded output at grid[1,3]");

  check(
      tket_named_iterator_next(grid_iterator.get(), &command, &has_operation));
  expect(
      !has_operation, "Wire annotations should appear only in the wire table");
}

void opaque_operations() {
  // This is the CBOR encoding of the single-operation descriptor
  // {"type": "CH"}. Wire arguments are supplied separately in port order.
  const uint8_t controlled_hadamard_cbor[] = {0xa1, 0x64, 't', 'y', 'p',
                                              'e',  0x62, 'C', 'H'};
  const TketNamedWire declarations[] = {
      {TKET_WIRE_QUBIT, text("q"), &qubit_0.index, 1},
      {TKET_WIRE_QUBIT, text("q"), &qubit_1.index, 1}};
  TketCircuit *circuit_handle = nullptr;
  check(tket_circuit_create_named(declarations, 2, 0, 0, {}, &circuit_handle));
  Circuit opaque_circuit(circuit_handle, tket_free_circuit);
  const TketWireId wires[] = {0, 1};
  const TketCommandPorts ports{wires, 2, {}};
  check(tket_circuit_append_opaque_operation(
      opaque_circuit.get(), &ports,
      {controlled_hadamard_cbor, sizeof(controlled_hadamard_cbor)}));

  // A default-register iterator can open the circuit. It returns an explicit
  // error when it reaches CH, which has no typed TKET_GATE_* identifier.
  auto numeric_iterator = make_iterator(opaque_circuit);
  TketOperationView gate{};
  bool has_operation = true;
  expect(
      tket_iterator_next(numeric_iterator.get(), &gate, &has_operation) ==
              TKET_ERROR_UNSUPPORTED_OPERATION &&
          !has_operation,
      "The numeric iterator should report an operation it cannot represent");

  // The named iterator yields the operation's ports and lets the caller
  // retrieve its payload. Read it before advancing or freeing the iterator.
  TketCommandIterator *iterator_handle = nullptr;
  size_t wire_count = 0;
  check(tket_circuit_named_iterator_create(
      opaque_circuit.get(), &iterator_handle, &wire_count));
  const Iterator named_iterator(iterator_handle, tket_free_iterator);
  TketNamedOperationView command{};
  check(
      tket_named_iterator_next(named_iterator.get(), &command, &has_operation));
  expect(
      has_operation && command.operation.gate == TKET_GATE_OPAQUE,
      "Expected an opaque command");
  expect(
      wire_count == 2 && command.operation.args_count == 2,
      "Expected both CH ports");
  // Resolve the compact IDs to actual wire coordinates before checking port
  // order. CH uses q[0] as its control and q[1] as its target.
  for (uint32_t port = 0; port < 2; ++port) {
    TketNamedWire view{};
    bool created = false, discarded = false;
    TketWireId output = 0;
    check(tket_named_iterator_get_wire(
        named_iterator.get(), command.operation.args[port].index, &view,
        &created, &discarded, &output));
    expect(
        read_text(view.name) == "q" && view.index_count == 1 &&
            view.indices[0] == port,
        "CH control and target are out of order");
  }
  TketBytes payload{};
  check(
      tket_named_iterator_get_opaque_operation(named_iterator.get(), &payload));
  expect(
      payload.size == sizeof(controlled_hadamard_cbor) &&
          std::equal(
              payload.data, payload.data + payload.size,
              controlled_hadamard_cbor),
      "The opaque operation's descriptor changed");
  check(
      tket_named_iterator_next(named_iterator.get(), &command, &has_operation));
  expect(!has_operation, "Expected exactly one opaque command");
}

}  // namespace
int main() {
  try {
    all_gates_round_trip();
    causal_order_and_lifetime();
    apply_a_pass_sequence();
    invalid_inputs_report_status_codes();
    rebase_to_a_typed_basis();
    squash_to_tk2_and_iterate();
    symbolic_angles();
    boundary_annotations_and_permutations();
    classical_payloads_and_boxes();
    named_wires_and_groups();
    opaque_operations();
    std::cout << "Native C API checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
