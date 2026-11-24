#include <cstdlib>
#include <cstring>

#include "internal.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
// JSON loading, pass application and the transform helpers all use the same
// circuit storage as the direct interface. This lets callers combine the two
// interfaces without converting a circuit between separate representations.
using namespace tket;
using namespace tket_c_api;
using json = nlohmann::json;

namespace {
OpType target_type(TketTargetGate target) {
  switch (target) {
    case TKET_TARGET_CX:
      return OpType::CX;
    case TKET_TARGET_TK2:
      return OpType::TK2;
  }
  throw Failure(TKET_ERROR_INVALID_ARGUMENT, "Invalid target gate");
}
}  // namespace

TketCircuit *tket_circuit_from_json(const char *text) {
  TketCircuit *result = nullptr;
  protect([&] {
    nonnull(text);
    // Only hand ownership to the caller once parsing and circuit construction
    // have both succeeded. On failure the local handle is freed during
    // unwinding and the caller receives NULL with the error message set by
    // protect().
    auto circuit = std::make_unique<TketCircuit>();
    circuit->state->circuit = json::parse(text).get<Circuit>();
    result = circuit.release();
  });
  return result;
}
TketError tket_circuit_to_json(const TketCircuit *tc, char **text) {
  if (text) *text = nullptr;
  return protect([&] {
    nonnull(tc);
    nonnull(text);
    const std::string s = json(tc->state->circuit).dump();
    // The returned string outlives the local std::string. Allocate it with the
    // matching allocator used by tket_free_string, including space for the NUL.
    auto *buffer = static_cast<char *>(std::malloc(s.size() + 1));
    if (!buffer) throw std::bad_alloc();
    std::memcpy(buffer, s.c_str(), s.size() + 1);
    *text = buffer;
  });
}
TketPass *tket_pass_from_json(const char *text) {
  TketPass *result = nullptr;
  protect([&] {
    nonnull(text);
    auto pass = std::make_unique<TketPass>();
    pass->pass = deserialise(json::parse(text));
    result = pass.release();
  });
  return result;
}
TketError tket_apply_pass(TketCircuit *tc, const TketPass *tp) {
  return protect([&] {
    nonnull(tc);
    nonnull(tp);
    // Invalidate iterators before any work which might change the graph. We
    // keep this rule even when a pass fails, so callers can safely handle
    // errors without continuing an iterator whose graph may have changed.
    ++tc->state->revision;
    // A sequence runs all its passes inside this compilation unit, so the
    // circuit only needs to be copied in and out once for the whole pipeline.
    CompilationUnit cu(tc->state->circuit);
    tp->pass->apply(cu);
    tc->state->circuit = cu.get_circ_ref();
  });
}
void tket_free_circuit(TketCircuit *tc) { delete tc; }
void tket_free_pass(TketPass *tp) { delete tp; }
void tket_free_string(char *text) { std::free(text); }
// These transform helpers edit the shared circuit directly. As with pass
// application, advance its revision before the transform can change any edges.
TketError tket_two_qubit_squash(
    TketCircuit *tc, TketTargetGate target, double fidelity, bool swaps) {
  return protect([&] {
    nonnull(tc);
    require_finite(fidelity);
    const auto gate = target_type(target);
    ++tc->state->revision;
    Transforms::two_qubit_squash(gate, fidelity, swaps)
        .apply(tc->state->circuit);
  });
}
TketError tket_clifford_simp(
    TketCircuit *tc, TketTargetGate target, bool swaps) {
  return protect([&] {
    nonnull(tc);
    const auto gate = target_type(target);
    ++tc->state->revision;
    Transforms::clifford_simp(swaps, gate).apply(tc->state->circuit);
  });
}
TketError tket_squash_phasedx_rz(TketCircuit *tc) {
  return protect([&] {
    nonnull(tc);
    ++tc->state->revision;
    Transforms::squash_1qb_to_Rz_PhasedX().apply(tc->state->circuit);
  });
}
const char *tket_last_error_message(void) { return last_error; }
const char *tket_error_string(TketError code) {
  switch (code) {
    case TKET_SUCCESS:
      return "Success";
    case TKET_ERROR_NULL_POINTER:
      return "Unexpected NULL pointer";
    case TKET_ERROR_CIRCUIT_INVALID:
      return "Invalid circuit";
    case TKET_ERROR_INVALID_ARGUMENT:
      return "Invalid argument";
    case TKET_ERROR_UNSUPPORTED_OPERATION:
      return "Unsupported operation";
    case TKET_ERROR_SYMBOLIC_PARAMETER:
      return "Symbolic parameter";
    case TKET_ERROR_OUT_OF_MEMORY:
      return "Out of memory";
    case TKET_ERROR_INTERNAL:
      return "Internal error";
    case TKET_ERROR_ITERATOR_INVALIDATED:
      return "Iterator invalidated";
  }
  return "Unknown error";
}
