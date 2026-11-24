#include <deque>
#include <map>
#include <unordered_map>

#include "internal.hpp"
#include "tket/OpType/OpTypeInfo.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Predicates/PassLibrary.hpp"
// This is the direct route into TKET. Input operations are appended to its
// circuit graph, and the iterator reads that graph in dependency order. The
// argument and parameter buffers only hold the operation currently being read.
using namespace tket;
using namespace tket_c_api;
namespace {
// The public identifiers have fixed values. Explicit mappings keep them stable
// if the C++ OpType enum changes, and give us a place to reject unsupported
// gates.
OpType op_type(TketGate gate) {
  switch (gate) {
    case TKET_GATE_H:
      return OpType::H;
    case TKET_GATE_CX:
      return OpType::CX;
    case TKET_GATE_CY:
      return OpType::CY;
    case TKET_GATE_CZ:
      return OpType::CZ;
    case TKET_GATE_CRZ:
      return OpType::CRz;
    case TKET_GATE_T:
      return OpType::T;
    case TKET_GATE_TDG:
      return OpType::Tdg;
    case TKET_GATE_S:
      return OpType::S;
    case TKET_GATE_SDG:
      return OpType::Sdg;
    case TKET_GATE_X:
      return OpType::X;
    case TKET_GATE_Y:
      return OpType::Y;
    case TKET_GATE_Z:
      return OpType::Z;
    case TKET_GATE_RX:
      return OpType::Rx;
    case TKET_GATE_RY:
      return OpType::Ry;
    case TKET_GATE_RZ:
      return OpType::Rz;
    case TKET_GATE_CCX:
      return OpType::CCX;
    case TKET_GATE_V:
      return OpType::V;
    case TKET_GATE_VDG:
      return OpType::Vdg;
    case TKET_GATE_PHASEDX:
      return OpType::PhasedX;
    case TKET_GATE_ZZPHASE:
      return OpType::ZZPhase;
    case TKET_GATE_PHASEDXX:
      return OpType::PhasedXX;
    case TKET_GATE_TK1:
      return OpType::TK1;
    case TKET_GATE_SWAP:
      return OpType::SWAP;
    case TKET_GATE_XXPHASE:
      return OpType::XXPhase;
    case TKET_GATE_YYPHASE:
      return OpType::YYPhase;
    case TKET_GATE_ZZMAX:
      return OpType::ZZMax;
    case TKET_GATE_MEASURE:
      return OpType::Measure;
    case TKET_GATE_RESET:
      return OpType::Reset;
    case TKET_GATE_BARRIER:
      return OpType::Barrier;
  }
  throw Failure(TKET_ERROR_UNSUPPORTED_OPERATION, "Unknown gate identifier");
}
TketGate gate_type(OpType op) {
  switch (op) {
    case OpType::H:
      return TKET_GATE_H;
    case OpType::CX:
      return TKET_GATE_CX;
    case OpType::CY:
      return TKET_GATE_CY;
    case OpType::CZ:
      return TKET_GATE_CZ;
    case OpType::CRz:
      return TKET_GATE_CRZ;
    case OpType::T:
      return TKET_GATE_T;
    case OpType::Tdg:
      return TKET_GATE_TDG;
    case OpType::S:
      return TKET_GATE_S;
    case OpType::Sdg:
      return TKET_GATE_SDG;
    case OpType::X:
      return TKET_GATE_X;
    case OpType::Y:
      return TKET_GATE_Y;
    case OpType::Z:
      return TKET_GATE_Z;
    case OpType::Rx:
      return TKET_GATE_RX;
    case OpType::Ry:
      return TKET_GATE_RY;
    case OpType::Rz:
      return TKET_GATE_RZ;
    case OpType::CCX:
      return TKET_GATE_CCX;
    case OpType::V:
      return TKET_GATE_V;
    case OpType::Vdg:
      return TKET_GATE_VDG;
    case OpType::PhasedX:
      return TKET_GATE_PHASEDX;
    case OpType::ZZPhase:
      return TKET_GATE_ZZPHASE;
    case OpType::PhasedXX:
      return TKET_GATE_PHASEDXX;
    case OpType::TK1:
      return TKET_GATE_TK1;
    case OpType::SWAP:
      return TKET_GATE_SWAP;
    case OpType::XXPhase:
      return TKET_GATE_XXPHASE;
    case OpType::YYPhase:
      return TKET_GATE_YYPHASE;
    case OpType::ZZMax:
      return TKET_GATE_ZZMAX;
    case OpType::Measure:
      return TKET_GATE_MEASURE;
    case OpType::Reset:
      return TKET_GATE_RESET;
    case OpType::Barrier:
      return TKET_GATE_BARRIER;
    default:
      throw Failure(
          TKET_ERROR_UNSUPPORTED_OPERATION,
          "Operation has no native C API gate identifier");
  }
}
// JSON can describe named and multidimensional registers. Our numeric wire
// format carries a kind and one index, so check that the register fits before
// returning an index which the caller will interpret as q[i] or c[i].
TketWire wire(const UnitID &unit) {
  require(
      unit.type() == UnitType::Qubit || unit.type() == UnitType::Bit,
      TKET_ERROR_UNSUPPORTED_OPERATION, "Unsupported wire type");
  const auto kind =
      unit.type() == UnitType::Qubit ? TKET_WIRE_QUBIT : TKET_WIRE_BIT;
  require(
      unit.reg_name() == (kind == TKET_WIRE_QUBIT ? "q" : "c") &&
          unit.index().size() == 1,
      TKET_ERROR_UNSUPPORTED_OPERATION, "Expected a default q/c register");
  return {static_cast<TketWireKind>(kind), unit.index()[0]};
}
UnitID unit(const TketWire &wire) {
  if (wire.kind == TKET_WIRE_QUBIT) return Qubit(wire.index);
  if (wire.kind == TKET_WIRE_BIT) return Bit(wire.index);
  throw Failure(TKET_ERROR_INVALID_ARGUMENT, "Unknown wire kind");
}
}  // namespace

// We use Kahn's algorithm to find an execution order. A vertex becomes ready
// once we've visited every edge leading into it. This lets next() return one
// operation at a time without collecting circuit layers or a command list.
struct TketCommandIterator {
  std::shared_ptr<TketCircuitState> state;
  uint64_t revision;
  // Count edges rather than predecessor vertices because two gates can share
  // several wires. Each of those edges must be accounted for before proceeding.
  std::unordered_map<Vertex, size_t> remaining;
  std::deque<Vertex> ready;
  // The graph's edges tell us which wires arrive at each operation's ports.
  // We compute this mapping once and use it while returning each operation.
  std::map<Edge, UnitID> wires;
  // Returned views borrow these buffers. The next call can reuse them because
  // the public contract requires the caller to finish reading the previous
  // view.
  std::vector<TketWire> args;
  std::vector<double> params;
  size_t visited = 0;
  bool failed = false;

  explicit TketCommandIterator(const TketCircuit &tc)
      : state(tc.state),
        revision(state->revision),
        wires(state->circuit.edge_unit_map()) {
    const auto &circuit = state->circuit;
    for (const auto &q : circuit.all_qubits()) (void)wire(q);
    for (const auto &b : circuit.all_bits()) (void)wire(b);
    // Include input and output boundary vertices in the traversal. Visiting an
    // input makes its first gate ready, and visiting an output lets us account
    // for the complete graph when we check for exhaustion.
    for (const auto &v : circuit.all_vertices()) {
      const auto degree = circuit.n_in_edges(v);
      remaining.emplace(v, degree);
      if (degree == 0) ready.push_back(v);
    }
  }
};

TketError tket_circuit_create(uint32_t nq, uint32_t nb, TketCircuit **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    // Keep ownership here until construction succeeds. If anything throws,
    // protect() catches it after the local handle has been cleaned up.
    auto tc = std::make_unique<TketCircuit>();
    tc->state->circuit = Circuit(nq, nb);
    *out = tc.release();
  });
}
TketError tket_circuit_add_operation(
    TketCircuit *tc, TketGate gate, const TketWire *args, size_t nargs,
    const double *params, size_t nparams) {
  return protect([&] {
    nonnull(tc);
    if (nargs) nonnull(args);
    if (nparams) nonnull(params);
    const auto type = op_type(gate);
    auto &circuit = tc->state->circuit;
    // Validate the borrowed inputs before changing the circuit. In particular,
    // a repeated wire would connect two ports of the same gate to one wire.
    unit_vector_t units;
    std::set<UnitID> unique;
    for (size_t i = 0; i < nargs; ++i) {
      auto u = unit(args[i]);
      require(
          unique.insert(u).second, TKET_ERROR_INVALID_ARGUMENT,
          "Operation arguments must be distinct");
      require(
          circuit.boundary.get<TagID>().find(u) !=
              circuit.boundary.get<TagID>().end(),
          TKET_ERROR_INVALID_ARGUMENT, "Wire is outside the circuit");
      units.push_back(u);
    }
    std::vector<Expr> angles;
    for (size_t i = 0; i < nparams; ++i) {
      require_finite(params[i]);
      angles.emplace_back(params[i]);
    }
    // Barriers can cover any number of wires, so Circuit has a separate builder
    // for them. Ordinary operations get their fixed signature from TKET below.
    if (type == OpType::Barrier) {
      require(
          nparams == 0, TKET_ERROR_INVALID_ARGUMENT,
          "A barrier has no parameters");
      for (size_t i = 0; i < nargs; ++i)
        require(
            args[i].kind == TKET_WIRE_QUBIT, TKET_ERROR_INVALID_ARGUMENT,
            "Expected qubit arguments for barrier");
      ++tc->state->revision;
      circuit.add_barrier(units);
      return;
    }
    require(
        optypeinfo().at(type).n_params() == nparams,
        TKET_ERROR_INVALID_ARGUMENT, "Wrong number of gate parameters");
    const auto op = get_op_ptr(type, angles);
    const auto signature = op->get_signature();
    require(
        signature.size() == nargs, TKET_ERROR_INVALID_ARGUMENT,
        "Wrong number of operation arguments");
    for (size_t i = 0; i < nargs; ++i)
      require(
          signature[i] == (args[i].kind == TKET_WIRE_QUBIT
                               ? EdgeType::Quantum
                               : EdgeType::Classical),
          TKET_ERROR_INVALID_ARGUMENT, "Wrong operation argument type");
    ++tc->state->revision;
    circuit.add_op(op, units);
  });
}
TketError tket_circuit_add_phase(TketCircuit *tc, double phase) {
  return protect([&] {
    nonnull(tc);
    require_finite(phase);
    ++tc->state->revision;
    tc->state->circuit.add_phase(phase);
  });
}
TketError tket_circuit_get_phase(const TketCircuit *tc, double *phase) {
  return protect([&] {
    nonnull(tc);
    nonnull(phase);
    *phase = numeric(tc->state->circuit.get_phase());
  });
}
TketError tket_circuit_mark_created(TketCircuit *tc, uint32_t q) {
  return protect([&] {
    nonnull(tc);
    ++tc->state->revision;
    tc->state->circuit.qubit_create(Qubit(q));
  });
}
TketError tket_circuit_mark_discarded(TketCircuit *tc, uint32_t q) {
  return protect([&] {
    nonnull(tc);
    ++tc->state->revision;
    tc->state->circuit.qubit_discard(Qubit(q));
  });
}
TketError tket_circuit_output_qubit(
    const TketCircuit *tc, uint32_t input, uint32_t *output) {
  return protect([&] {
    nonnull(tc);
    nonnull(output);
    const auto permutation = tc->state->circuit.implicit_qubit_permutation();
    const auto it = permutation.find(Qubit(input));
    require(
        it != permutation.end(), TKET_ERROR_INVALID_ARGUMENT,
        "Input qubit is outside the circuit");
    *output = wire(it->second).index;
  });
}
TketError tket_circuit_has_implicit_swaps(const TketCircuit *tc, bool *swaps) {
  return protect([&] {
    nonnull(tc);
    nonnull(swaps);
    *swaps = false;
    for (const auto &[in, out] :
         tc->state->circuit.implicit_qubit_permutation())
      if (in != out) *swaps = true;
  });
}
TketError tket_circuit_iterator_create(
    const TketCircuit *tc, TketCommandIterator **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(tc);
    nonnull(out);
    *out = new TketCommandIterator(*tc);
  });
}
// Advancing is entirely a C++ operation. Once this returns, callers can process
// the view in their own language with no callback or C++ traversal frame
// active.
TketError tket_iterator_next(
    TketCommandIterator *it, TketOperationView *out, bool *has) {
  // Leave the same empty result on exhaustion and failure, so a caller can't
  // accidentally reuse an earlier view after either outcome.
  if (out) *out = {};
  if (has) *has = false;
  const auto error = protect([&] {
    nonnull(it);
    nonnull(out);
    nonnull(has);
    require(
        !it->failed, TKET_ERROR_ITERATOR_INVALIDATED,
        "Iterator failed on an earlier call");
    // Check this before reading the frontier because mutations can delete the
    // vertices and edges it refers to, even while the shared state stays alive.
    require(
        it->revision == it->state->revision, TKET_ERROR_ITERATOR_INVALIDATED,
        "Circuit changed during iteration");
    auto &circuit = it->state->circuit;
    while (!it->ready.empty()) {
      const auto v = it->ready.front();
      it->ready.pop_front();
      ++it->visited;
      // Release this vertex's outgoing dependencies before skipping boundaries
      // or returning a gate. Its successors are considered on later iterations.
      for (const auto &edge :
           boost::make_iterator_range(boost::out_edges(v, circuit.dag))) {
        const auto next = circuit.target(edge);
        if (--it->remaining.at(next) == 0) it->ready.push_back(next);
      }
      const auto op = circuit.get_Op_ptr_from_Vertex(v);
      if (is_boundary_type(op->get_type())) continue;
      const auto gate = gate_type(op->get_type());
      it->args.clear();
      it->params.clear();
      // get_in_edges() returns port order. Preserve it because controls and
      // targets can have any wire indices, and sorting them would change a
      // gate.
      for (const auto &edge : circuit.get_in_edges(v))
        it->args.push_back(wire(it->wires.at(edge)));
      // Barrier parameters aren't exposed through Op::get_params(). Its empty
      // buffer already describes the operation completely.
      if (op->get_type() != OpType::Barrier)
        for (const auto &p : op->get_params()) it->params.push_back(numeric(p));
      *out = {
          gate, it->args.data(), it->args.size(), it->params.data(),
          it->params.size()};
      *has = true;
      return;
    }
    // A valid circuit reaches every vertex. If the frontier emptied earlier,
    // some dependencies form a cycle and we must report an incomplete
    // traversal.
    require(
        it->visited == it->remaining.size(), TKET_ERROR_CIRCUIT_INVALID,
        "Circuit contains a cycle");
  });
  // We may already have advanced the frontier when conversion fails. Require
  // a fresh iterator after an error so retrying can't silently skip that gate.
  if (error != TKET_SUCCESS && it) it->failed = true;
  return error;
}
void tket_free_iterator(TketCommandIterator *it) { delete it; }
TketError tket_pass_create_auto_rebase(
    const TketGate *gates, size_t count, bool swaps, TketPass **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    if (count) nonnull(gates);
    OpTypeSet basis;
    for (size_t i = 0; i < count; ++i) basis.insert(op_type(gates[i]));
    auto pass = std::make_unique<TketPass>();
    pass->pass = gen_auto_rebase_pass(basis, swaps);
    *out = pass.release();
  });
}
TketError tket_pass_create_remove_redundancies(TketPass **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    auto pass = std::make_unique<TketPass>();
    pass->pass = RemoveRedundancies();
    *out = pass.release();
  });
}
TketError tket_pass_create_commute_through_multis(TketPass **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    auto pass = std::make_unique<TketPass>();
    pass->pass = CommuteThroughMultis();
    *out = pass.release();
  });
}
TketError tket_pass_create_sequence(
    const TketPass *const *passes, size_t count, bool strict, TketPass **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    if (count) nonnull(passes);
    // Copy the shared pointers into the sequence. The caller can then free the
    // original handles while the sequence continues to own and reuse its
    // passes.
    std::vector<PassPtr> items;
    for (size_t i = 0; i < count; ++i) {
      nonnull(passes[i]);
      items.push_back(passes[i]->pass);
    }
    auto pass = std::make_unique<TketPass>();
    pass->pass = std::make_shared<SequencePass>(items, strict);
    *out = pass.release();
  });
}
