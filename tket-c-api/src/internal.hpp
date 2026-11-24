#pragma once
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include "tket-c-api.h"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Predicates/CompilerPass.hpp"

// Both circuit interfaces use these handle definitions. An iterator shares
// ownership of the circuit state, so freeing the public circuit handle leaves
// the graph alive until the last iterator has also been freed.
struct TketCircuitState {
  tket::Circuit circuit;
  // Iterators remember the revision they started with. Every mutation advances
  // it before changing the graph, so the next iterator call can report an error
  // before touching vertices or edges which may have been removed.
  uint64_t revision = 0;
};
struct TketCircuit {
  std::shared_ptr<TketCircuitState> state =
      std::make_shared<TketCircuitState>();
};
// TKET already shares ownership of passes through PassPtr. Keeping that here
// also lets a sequence outlive the handles used to construct its component
// passes.
struct TketPass {
  tket::PassPtr pass;
};
namespace tket_c_api {
// Reporting an error must still work if an allocation failed. Fixed storage
// lets us copy the message without allocating, and each thread gets its own
// buffer so another thread's API calls won't overwrite it.
inline thread_local char last_error[1024] = {};
// Validation failures carry the public error code we want the caller to see.
// protect() handles these along with exceptions raised by TKET itself.
struct Failure : std::runtime_error {
  TketError code;
  Failure(TketError c, const char *message)
      : std::runtime_error(message), code(c) {}
};
inline void require(bool condition, TketError code, const char *message) {
  if (!condition) throw Failure(code, message);
}
// Each fallible entry point runs its implementation inside this wrapper.
// We let C++ unwind its local objects here, then return a status to the caller.
// This keeps exceptions inside the library when the caller uses a C ABI.
template <class F>
TketError protect(F &&body) noexcept {
  last_error[0] = '\0';
  try {
    body();
    return TKET_SUCCESS;
  } catch (const Failure &e) {
    std::snprintf(last_error, sizeof(last_error), "%s", e.what());
    return e.code;
  } catch (const std::bad_alloc &) {
    std::snprintf(last_error, sizeof(last_error), "Out of memory");
    return TKET_ERROR_OUT_OF_MEMORY;
  } catch (const std::exception &e) {
    std::snprintf(last_error, sizeof(last_error), "%s", e.what());
    return TKET_ERROR_INTERNAL;
  } catch (...) {
    std::snprintf(last_error, sizeof(last_error), "Unknown C++ exception");
    return TKET_ERROR_INTERNAL;
  }
}
inline void nonnull(const void *p) {
  require(p, TKET_ERROR_NULL_POINTER, "Unexpected NULL pointer");
}
inline void require_finite(double x) {
  require(
      std::isfinite(x), TKET_ERROR_INVALID_ARGUMENT, "Angle must be finite");
}
// A circuit loaded from JSON may contain symbolic expressions. The direct
// interface promises finite doubles, so we report an error if evaluating an
// expression can't produce one. The caller can still use the JSON interface
// to work with circuits containing those expressions.
inline double numeric(const tket::Expr &expr) {
  auto value = tket::eval_expr(expr);
  require(
      value.has_value(), TKET_ERROR_SYMBOLIC_PARAMETER, "Symbolic parameter");
  require_finite(*value);
  return *value;
}
}  // namespace tket_c_api
