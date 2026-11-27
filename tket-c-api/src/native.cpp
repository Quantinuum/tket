#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <type_traits>
#include <unordered_map>

#include "internal.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/OpType/OpTypeInfo.hpp"
#include "tket/Ops/BarrierOp.hpp"
#include "tket/Ops/ClassicalOps.hpp"
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
    case TKET_GATE_X:
      return OpType::X;
    case TKET_GATE_Y:
      return OpType::Y;
    case TKET_GATE_Z:
      return OpType::Z;
    case TKET_GATE_S:
      return OpType::S;
    case TKET_GATE_SDG:
      return OpType::Sdg;
    case TKET_GATE_T:
      return OpType::T;
    case TKET_GATE_TDG:
      return OpType::Tdg;
    case TKET_GATE_V:
      return OpType::V;
    case TKET_GATE_VDG:
      return OpType::Vdg;
    case TKET_GATE_RX:
      return OpType::Rx;
    case TKET_GATE_RY:
      return OpType::Ry;
    case TKET_GATE_RZ:
      return OpType::Rz;
    case TKET_GATE_PHASEDX:
      return OpType::PhasedX;
    case TKET_GATE_TK1:
      return OpType::TK1;
    case TKET_GATE_TK2:
      return OpType::TK2;
    case TKET_GATE_CX:
      return OpType::CX;
    case TKET_GATE_CY:
      return OpType::CY;
    case TKET_GATE_CZ:
      return OpType::CZ;
    case TKET_GATE_CRZ:
      return OpType::CRz;
    case TKET_GATE_CCX:
      return OpType::CCX;
    case TKET_GATE_SWAP:
      return OpType::SWAP;
    case TKET_GATE_XXPHASE:
      return OpType::XXPhase;
    case TKET_GATE_YYPHASE:
      return OpType::YYPhase;
    case TKET_GATE_ZZPHASE:
      return OpType::ZZPhase;
    case TKET_GATE_ZZMAX:
      return OpType::ZZMax;
    case TKET_GATE_PHASEDXX:
      return OpType::PhasedXX;
    case TKET_GATE_MEASURE:
      return OpType::Measure;
    case TKET_GATE_RESET:
      return OpType::Reset;
    case TKET_GATE_BARRIER:
      return OpType::Barrier;
    case TKET_GATE_PHASE:
      return OpType::Phase;
    case TKET_GATE_NOOP:
      return OpType::noop;
    case TKET_GATE_SET_BITS:
      return OpType::SetBits;
    case TKET_GATE_CIRCBOX:
      return OpType::CircBox;
  }
  throw Failure(TKET_ERROR_UNSUPPORTED_OPERATION, "Unknown gate identifier");
}
TketGate gate_type(OpType operation, bool allow_opaque = false) {
  switch (operation) {
    case OpType::H:
      return TKET_GATE_H;
    case OpType::X:
      return TKET_GATE_X;
    case OpType::Y:
      return TKET_GATE_Y;
    case OpType::Z:
      return TKET_GATE_Z;
    case OpType::S:
      return TKET_GATE_S;
    case OpType::Sdg:
      return TKET_GATE_SDG;
    case OpType::T:
      return TKET_GATE_T;
    case OpType::Tdg:
      return TKET_GATE_TDG;
    case OpType::V:
      return TKET_GATE_V;
    case OpType::Vdg:
      return TKET_GATE_VDG;
    case OpType::Rx:
      return TKET_GATE_RX;
    case OpType::Ry:
      return TKET_GATE_RY;
    case OpType::Rz:
      return TKET_GATE_RZ;
    case OpType::PhasedX:
      return TKET_GATE_PHASEDX;
    case OpType::TK1:
      return TKET_GATE_TK1;
    case OpType::TK2:
      return TKET_GATE_TK2;
    case OpType::CX:
      return TKET_GATE_CX;
    case OpType::CY:
      return TKET_GATE_CY;
    case OpType::CZ:
      return TKET_GATE_CZ;
    case OpType::CRz:
      return TKET_GATE_CRZ;
    case OpType::CCX:
      return TKET_GATE_CCX;
    case OpType::SWAP:
      return TKET_GATE_SWAP;
    case OpType::XXPhase:
      return TKET_GATE_XXPHASE;
    case OpType::YYPhase:
      return TKET_GATE_YYPHASE;
    case OpType::ZZPhase:
      return TKET_GATE_ZZPHASE;
    case OpType::ZZMax:
      return TKET_GATE_ZZMAX;
    case OpType::PhasedXX:
      return TKET_GATE_PHASEDXX;
    case OpType::Measure:
      return TKET_GATE_MEASURE;
    case OpType::Reset:
      return TKET_GATE_RESET;
    case OpType::Barrier:
      return TKET_GATE_BARRIER;
    case OpType::Phase:
      return TKET_GATE_PHASE;
    case OpType::noop:
      return TKET_GATE_NOOP;
    case OpType::SetBits:
      return TKET_GATE_SET_BITS;
    case OpType::CircBox:
      return TKET_GATE_CIRCBOX;
    default:
      if (allow_opaque) return TKET_GATE_OPAQUE;
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
// Reuse wire validation for ordinary gates and barriers. The latter can
// include classical bits, and TKET uses the supplied order for its signature.
unit_vector_t checked_units(
    const Circuit &circuit, const TketWire *args, size_t count,
    const char *signature = nullptr) {
  if (count) nonnull(args);
  unit_vector_t result;
  std::set<UnitID> unique;
  for (size_t i = 0; i < count; ++i) {
    const auto id = unit(args[i]);
    require(
        (signature && signature[i] == 'B') || unique.insert(id).second,
        TKET_ERROR_INVALID_ARGUMENT, "Operation arguments must be distinct");
    require(
        circuit.contains_unit(id), TKET_ERROR_INVALID_ARGUMENT,
        "Wire is outside the circuit");
    result.push_back(id);
  }
  return result;
}

template <class Units>
void require_dense_register(const Units &units) {
  for (size_t i = 0; i < units.size(); ++i)
    require(
        wire(units[i]).index == i, TKET_ERROR_UNSUPPORTED_OPERATION,
        "Expected contiguous wire indices in the default register");
}
// A barrier can read bits without taking ownership of the classical wire.
EdgeType barrier_port(char kind, TketWireKind wire_kind) {
  if (kind == 'Q' && wire_kind == TKET_WIRE_QUBIT) return EdgeType::Quantum;
  if (kind == 'C' && wire_kind == TKET_WIRE_BIT) return EdgeType::Classical;
  if (kind == 'B' && wire_kind == TKET_WIRE_BIT) return EdgeType::Boolean;
  if (kind == 'W' && wire_kind == TKET_WIRE_WASM) return EdgeType::WASM;
  if (kind == 'R' && wire_kind == TKET_WIRE_RNG) return EdgeType::RNG;
  throw Failure(
      TKET_ERROR_INVALID_ARGUMENT, "Barrier port does not match its wire");
}
char barrier_port(EdgeType port) {
  switch (port) {
    case EdgeType::Quantum:
      return 'Q';
    case EdgeType::Classical:
      return 'C';
    case EdgeType::Boolean:
      return 'B';
    case EdgeType::WASM:
      return 'W';
    case EdgeType::RNG:
      return 'R';
    default:
      throw Failure(
          TKET_ERROR_UNSUPPORTED_OPERATION, "Unsupported barrier port");
  }
}
Expr parse_expression(const char *text) {
  nonnull(text);
  // Use the same SymEngine constructor as JSON circuit import.
  return Expr(std::string(text));
}
std::string expression_string(const Expr &expression) {
  const ExprPtr value = expression;
  return value->__str__();
}
char *copy_string(const std::string &text) {
  auto *buffer = static_cast<char *>(std::malloc(text.size() + 1));
  if (!buffer) throw std::bad_alloc();
  std::memcpy(buffer, text.c_str(), text.size() + 1);
  return buffer;
}
Op_ptr make_circbox(const Circuit &inner, const char *id) {
  auto box = std::make_shared<CircBox>(inner);
  if (!id) return box;
  boost::uuids::uuid uuid;
  try {
    uuid = boost::uuids::string_generator{}(id);
  } catch (const std::runtime_error &) {
    throw Failure(TKET_ERROR_INVALID_ARGUMENT, "Invalid circuit box UUID");
  }
  require(
      boost::uuids::to_string(uuid) == id, TKET_ERROR_INVALID_ARGUMENT,
      "Expected a canonical circuit box UUID");
  return set_box_id(*box, uuid);
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
  std::unordered_map<Vertex, std::vector<Vertex>> readers_before_write;
  std::deque<Vertex> ready;
  // The graph's edges tell us which wires arrive at each operation's ports.
  // We compute this mapping once and use it while returning each operation.
  std::map<Edge, UnitID> wires;
  // Returned views borrow these buffers. The next call can reuse them because
  // the public contract requires the caller to finish reading the previous
  // view.
  std::vector<TketWire> args;
  std::vector<double> params;
  std::vector<std::string> expressions;
  std::vector<const char *> expression_views;
  // The payload belongs to the current operation view, just like its wires.
  std::vector<uint8_t> payload;
  std::string barrier_signature;
  Op_ptr current_op;
  // The named view maps all register types to IDs once per iterator.
  bool named;
  unit_vector_t named_wires;
  std::map<UnitID, TketWireId> wire_ids;
  std::optional<std::string> circuit_name;
  std::string wire_name;
  std::vector<uint32_t> wire_indices;
  TketGate current_gate = TKET_GATE_OPAQUE;
  std::vector<uint8_t> opaque_payload;
  bool has_payload = false;
  size_t visited = 0;
  bool failed = false;

  explicit TketCommandIterator(
      const TketCircuit &circuit_handle, bool named_view = false)
      : state(circuit_handle.state),
        revision(state->revision),
        wires(state->circuit.edge_unit_map()),
        named(named_view) {
    const auto &circuit = state->circuit;
    if (named) {
      named_wires = circuit.all_units();
      circuit_name = circuit.get_name();
      for (TketWireId i = 0; i < named_wires.size(); ++i)
        wire_ids.emplace(named_wires[i], i);
    } else {
      for (const auto &qubit : circuit.all_qubits()) (void)wire(qubit);
      for (const auto &bit : circuit.all_bits()) (void)wire(bit);
    }
    // A Boolean edge identifies the version a command reads. The next writer
    // can share the same predecessor as that reader. Emit the reader first
    // so a caller replaying the commands still reads the old value.
    for (const auto &vertex : circuit.all_vertices())
      remaining.emplace(vertex, circuit.n_in_edges(vertex));
    for (const auto &edge :
         boost::make_iterator_range(boost::edges(circuit.dag))) {
      if (circuit.get_edgetype(edge) != EdgeType::Boolean) continue;
      const auto reader = circuit.target(edge);
      const auto writer = circuit.target(circuit.get_linear_edge(edge));
      if (reader != writer) {
        readers_before_write[reader].push_back(writer);
        ++remaining.at(writer);
      }
    }
    // Seed the queue in circuit storage order, including input boundaries.
    for (const auto &vertex : circuit.all_vertices())
      if (remaining.at(vertex) == 0) ready.push_back(vertex);
  }
};

TketError tket_circuit_create(
    uint32_t qubit_count, uint32_t bit_count, TketCircuit **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    // Keep ownership here until construction succeeds. If anything throws,
    // protect() catches it after the local handle has been cleaned up.
    auto circuit_handle = std::make_unique<TketCircuit>();
    circuit_handle->state->circuit = Circuit(qubit_count, bit_count);
    *out = circuit_handle.release();
  });
}
// Both entry points use the same gate and port validation after reading
// their angle arrays.
static void append_gate(
    TketCircuit *circuit_handle, OpType type, const TketWire *args,
    size_t nargs, const std::vector<Expr> &angles) {
  auto &circuit = circuit_handle->state->circuit;
  const auto units = checked_units(circuit, args, nargs);
  require(
      type != OpType::SetBits && type != OpType::CircBox,
      TKET_ERROR_INVALID_ARGUMENT, "Use the operation's dedicated builder");
  if (type == OpType::Barrier) {
    require(
        angles.empty(), TKET_ERROR_INVALID_ARGUMENT,
        "A barrier has no parameters");
    ++circuit_handle->state->revision;
    circuit.add_barrier(units);
    return;
  }
  require(
      optypeinfo().at(type).n_params() == angles.size(),
      TKET_ERROR_INVALID_ARGUMENT, "Wrong number of gate parameters");
  const auto operation = get_op_ptr(type, angles);
  const auto signature = operation->get_signature();
  require(
      signature.size() == nargs, TKET_ERROR_INVALID_ARGUMENT,
      "Wrong number of operation arguments");
  for (size_t i = 0; i < nargs; ++i)
    require(
        signature[i] == (args[i].kind == TKET_WIRE_QUBIT ? EdgeType::Quantum
                                                         : EdgeType::Classical),
        TKET_ERROR_INVALID_ARGUMENT, "Wrong operation argument type");
  ++circuit_handle->state->revision;
  circuit.add_op(operation, units);
}

TketError tket_circuit_add_operation(
    TketCircuit *circuit_handle, TketGate gate, const TketWire *args,
    size_t nargs, const double *params, size_t nparams) {
  return protect([&] {
    nonnull(circuit_handle);
    if (nparams) nonnull(params);
    const auto type = op_type(gate);
    std::vector<Expr> angles;
    for (size_t i = 0; i < nparams; ++i) {
      require_finite(params[i]);
      angles.emplace_back(params[i]);
    }
    append_gate(circuit_handle, type, args, nargs, angles);
  });
}
TketError tket_circuit_add_operation_with_expressions(
    TketCircuit *circuit_handle, TketGate gate, const TketWire *args,
    size_t nargs, const char *const *expressions, size_t count) {
  return protect([&] {
    nonnull(circuit_handle);
    if (count) nonnull(expressions);
    const auto type = op_type(gate);
    std::vector<Expr> angles;
    for (size_t i = 0; i < count; ++i)
      angles.push_back(parse_expression(expressions[i]));
    append_gate(circuit_handle, type, args, nargs, angles);
  });
}
TketError tket_circuit_add_barrier(
    TketCircuit *circuit_handle, const TketWire *args, size_t count,
    const char *signature, const char *data, size_t size) {
  return protect([&] {
    nonnull(circuit_handle);
    if (size) nonnull(data);
    auto &circuit = circuit_handle->state->circuit;
    const auto units = checked_units(circuit, args, count, signature);
    op_signature_t ports;
    for (size_t i = 0; i < count; ++i) {
      const auto kind = args[i].kind;
      ports.push_back(barrier_port(
          signature ? signature[i] : (kind == TKET_WIRE_QUBIT ? 'Q' : 'C'),
          kind));
    }
    const auto operation = std::make_shared<BarrierOp>(
        ports, size ? std::string(data, size) : std::string{});
    ++circuit_handle->state->revision;
    circuit.add_op(operation, units);
  });
}

TketError tket_circuit_add_set_bits(
    TketCircuit *circuit_handle, const uint32_t *bits, const uint8_t *values,
    size_t count) {
  return protect([&] {
    nonnull(circuit_handle);
    if (count) {
      nonnull(bits);
      nonnull(values);
    }
    std::vector<TketWire> args;
    std::vector<bool> assigned;
    for (size_t i = 0; i < count; ++i) {
      require(
          values[i] <= 1, TKET_ERROR_INVALID_ARGUMENT,
          "SetBits values must be 0 or 1");
      args.push_back({TKET_WIRE_BIT, bits[i]});
      assigned.push_back(values[i] != 0);
    }
    auto &circuit = circuit_handle->state->circuit;
    const auto units = checked_units(circuit, args.data(), args.size());
    ++circuit_handle->state->revision;
    circuit.add_op(std::make_shared<SetBitsOp>(assigned), units);
  });
}

TketError tket_circuit_add_circbox(
    TketCircuit *circuit_handle, const TketCircuit *contents,
    const TketWire *args, size_t count, const char *id) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(contents);
    const auto &inner = contents->state->circuit;
    const auto qubit_count = inner.n_qubits();
    require(
        inner.n_units() == qubit_count + inner.n_bits(),
        TKET_ERROR_UNSUPPORTED_OPERATION,
        "Circuit boxes with additional wire types are unsupported");
    require(
        count == qubit_count + inner.n_bits(), TKET_ERROR_INVALID_ARGUMENT,
        "Wrong number of circuit box arguments");
    const auto units =
        checked_units(circuit_handle->state->circuit, args, count);
    for (size_t i = 0; i < count; ++i)
      require(
          args[i].kind == (i < qubit_count ? TKET_WIRE_QUBIT : TKET_WIRE_BIT),
          TKET_ERROR_INVALID_ARGUMENT, "Wrong circuit box argument type");
    const auto operation = make_circbox(inner, id);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.add_op(operation, units);
  });
}

TketError tket_circuit_permute_outputs(
    TketCircuit *circuit_handle, const uint32_t *outputs, size_t count) {
  return protect([&] {
    nonnull(circuit_handle);
    if (count) nonnull(outputs);
    auto &circuit = circuit_handle->state->circuit;
    require_dense_register(circuit.all_qubits());
    require(
        count == circuit.n_qubits(), TKET_ERROR_INVALID_ARGUMENT,
        "Expected an output index for every qubit");
    std::vector<bool> seen(count);
    qubit_map_t mapping;
    for (size_t i = 0; i < count; ++i) {
      require(
          outputs[i] < count && !seen[outputs[i]], TKET_ERROR_INVALID_ARGUMENT,
          "Output indices must form a permutation");
      seen[outputs[i]] = true;
      mapping.emplace(Qubit(i), Qubit(outputs[i]));
    }
    // TKET relabels the current output boundary. Apply this after appending
    // commands so those commands use the original wire labels.
    ++circuit_handle->state->revision;
    circuit.permute_boundary_output(mapping);
  });
}

TketError tket_circuit_register_sizes(
    const TketCircuit *circuit_handle, uint32_t *qubits, uint32_t *bits) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(qubits);
    nonnull(bits);
    const auto &circuit = circuit_handle->state->circuit;
    require(
        circuit.n_units() == circuit.n_qubits() + circuit.n_bits(),
        TKET_ERROR_UNSUPPORTED_OPERATION, "Unsupported circuit registers");
    require_dense_register(circuit.all_qubits());
    require_dense_register(circuit.all_bits());
    *qubits = circuit.n_qubits();
    *bits = circuit.n_bits();
  });
}

TketError tket_circuit_has_opgroups(
    const TketCircuit *circuit_handle, bool *has_groups) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(has_groups);
    *has_groups = !circuit_handle->state->circuit.get_opgroups().empty();
  });
}

TketError tket_circuit_set_name(TketCircuit *circuit_handle, const char *name) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(name);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.set_name(name);
  });
}

TketError tket_circuit_get_name(const TketCircuit *circuit_handle, char **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out);
    const auto name = circuit_handle->state->circuit.get_name();
    if (!name) return;
    require(
        name->find('\0') == std::string::npos, TKET_ERROR_UNSUPPORTED_OPERATION,
        "Name contains a NUL byte");
    *out = copy_string(*name);
  });
}

TketError tket_circuit_add_phase(TketCircuit *circuit_handle, double phase) {
  return protect([&] {
    nonnull(circuit_handle);
    require_finite(phase);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.add_phase(phase);
  });
}
TketError tket_circuit_add_phase_expression(
    TketCircuit *circuit_handle, const char *text) {
  return protect([&] {
    nonnull(circuit_handle);
    const auto phase = parse_expression(text);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.add_phase(phase);
  });
}
TketError tket_circuit_get_phase_expression(
    const TketCircuit *circuit_handle, char **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out);
    *out = copy_string(
        expression_string(circuit_handle->state->circuit.get_phase()));
  });
}
TketError tket_circuit_get_phase(
    const TketCircuit *circuit_handle, double *phase) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(phase);
    *phase = numeric(circuit_handle->state->circuit.get_phase());
  });
}
TketError tket_circuit_mark_created(
    TketCircuit *circuit_handle, uint32_t qubit_index) {
  return protect([&] {
    nonnull(circuit_handle);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.qubit_create(Qubit(qubit_index));
  });
}
TketError tket_circuit_mark_discarded(
    TketCircuit *circuit_handle, uint32_t qubit_index) {
  return protect([&] {
    nonnull(circuit_handle);
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.qubit_discard(Qubit(qubit_index));
  });
}
// Create and Discard are boundary vertices, which the operation iterator skips.
TketError tket_circuit_is_created(
    const TketCircuit *circuit_handle, uint32_t qubit, bool *out_created) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out_created);
    const auto &circuit = circuit_handle->state->circuit;
    const Qubit wire(qubit);
    require(
        circuit.contains_unit(wire), TKET_ERROR_INVALID_ARGUMENT,
        "Input qubit is outside the circuit");
    *out_created = circuit.is_created(wire);
  });
}
TketError tket_circuit_is_discarded(
    const TketCircuit *circuit_handle, uint32_t qubit, bool *out_discarded) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out_discarded);
    const auto &circuit = circuit_handle->state->circuit;
    const Qubit wire(qubit);
    require(
        circuit.contains_unit(wire), TKET_ERROR_INVALID_ARGUMENT,
        "Output qubit is outside the circuit");
    *out_discarded = circuit.is_discarded(wire);
  });
}
TketError tket_circuit_output_qubit(
    const TketCircuit *circuit_handle, uint32_t input, uint32_t *output) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(output);
    const auto &circuit = circuit_handle->state->circuit;
    const Qubit qubit(input);
    require(
        circuit.contains_unit(qubit), TKET_ERROR_INVALID_ARGUMENT,
        "Input qubit is outside the circuit");
    // Follow this wire only, so querying every output walks each path once.
    const auto path = circuit.unit_path(qubit);
    *output = wire(circuit.get_id_from_out(path.back().first)).index;
  });
}
TketError tket_circuit_has_implicit_swaps(
    const TketCircuit *circuit_handle, bool *swaps) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(swaps);
    *swaps = false;
    for (const auto &[in, out] :
         circuit_handle->state->circuit.implicit_qubit_permutation())
      if (in != out) *swaps = true;
  });
}
// Named builders borrow the caller's descriptors while inserting directly
// into the graph. The only long-lived lookup table is the original wire
// declarations, which map compact IDs to TKET's UnitIDs.
static std::optional<std::string> optional_text(TketBytes text) {
  if (!text.data) {
    require(!text.size, TKET_ERROR_INVALID_ARGUMENT, "Missing text bytes");
    return std::nullopt;
  }
  return std::string(reinterpret_cast<const char *>(text.data), text.size);
}
static TketBytes text_view(const std::string &text) {
  return {reinterpret_cast<const uint8_t *>(text.data()), text.size()};
}
static TketWireKind wire_kind(UnitType type) {
  switch (type) {
    case UnitType::Qubit:
      return TKET_WIRE_QUBIT;
    case UnitType::Bit:
      return TKET_WIRE_BIT;
    case UnitType::WasmState:
      return TKET_WIRE_WASM;
    case UnitType::RngState:
      return TKET_WIRE_RNG;
  }
  throw Failure(TKET_ERROR_INTERNAL, "Unknown circuit wire type");
}
static bool port_matches(EdgeType port, UnitType type) {
  switch (port) {
    case EdgeType::Quantum:
      return type == UnitType::Qubit;
    case EdgeType::Boolean:
    case EdgeType::Classical:
      return type == UnitType::Bit;
    case EdgeType::WASM:
      return type == UnitType::WasmState;
    case EdgeType::RNG:
      return type == UnitType::RngState;
  }
  return false;
}
static const UnitID &declared_wire(
    const TketCircuit *circuit_handle, TketWireId id) {
  require(
      circuit_handle->named && id < circuit_handle->declared_wires.size(),
      TKET_ERROR_INVALID_ARGUMENT, "Unknown named wire ID");
  const auto &wire = circuit_handle->declared_wires[id];
  require(
      circuit_handle->state->circuit.contains_unit(wire),
      TKET_ERROR_INVALID_ARGUMENT, "Declared wire is no longer in the circuit");
  return wire;
}
static const UnitID &named_port(
    const TketCircuit *circuit_handle, const TketCommandPorts *ports,
    size_t port_index) {
  nonnull(ports->wires);
  return declared_wire(circuit_handle, ports->wires[port_index]);
}
static void append_named_operation(
    TketCircuit *circuit_handle, const TketCommandPorts *ports,
    const Op_ptr &operation) {
  const auto signature = operation->get_signature();
  require(
      signature.size() == ports->count, TKET_ERROR_INVALID_ARGUMENT,
      "Wrong number of operation arguments");
  unit_vector_t wires;
  wires.reserve(ports->count);
  for (size_t i = 0; i < ports->count; ++i) {
    const auto &wire = named_port(circuit_handle, ports, i);
    require(
        port_matches(signature[i], wire.type()), TKET_ERROR_INVALID_ARGUMENT,
        "Operation port does not match the wire kind");
    wires.push_back(wire);
  }
  auto group = optional_text(ports->group);
  ++circuit_handle->state->revision;
  circuit_handle->state->circuit.add_op(operation, wires, std::move(group));
}

TketError tket_circuit_create_named(
    const TketNamedWire *wires, size_t count, uint32_t wasm_wire_count,
    uint32_t rng_wire_count, TketBytes name, TketCircuit **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(out);
    if (count) nonnull(wires);
    require(
        count <= UINT32_MAX &&
            uint64_t(count) + wasm_wire_count + rng_wire_count <= UINT32_MAX,
        TKET_ERROR_INVALID_ARGUMENT, "Too many wire IDs");
    auto circuit_handle = std::make_unique<TketCircuit>();
    circuit_handle->named = true;
    auto &circuit = circuit_handle->state->circuit;
    if (const auto text = optional_text(name)) circuit.set_name(*text);
    for (size_t i = 0; i < count; ++i) {
      const auto &wire = wires[i];
      const auto reg = optional_text(wire.name);
      require(
          reg.has_value(), TKET_ERROR_INVALID_ARGUMENT,
          "Missing register name");
      std::vector<unsigned> index;
      if (wire.index_count) {
        nonnull(wire.indices);
        index.assign(wire.indices, wire.indices + wire.index_count);
      }
      if (wire.kind == TKET_WIRE_QUBIT) {
        const Qubit qubit(*reg, index);
        circuit.add_qubit(qubit);
        circuit_handle->declared_wires.push_back(qubit);
      } else if (wire.kind == TKET_WIRE_BIT) {
        const Bit bit(*reg, index);
        circuit.add_bit(bit);
        circuit_handle->declared_wires.push_back(bit);
      } else {
        throw Failure(
            TKET_ERROR_INVALID_ARGUMENT, "Expected a qubit or bit declaration");
      }
    }
    circuit.add_wasm_register(wasm_wire_count);
    circuit.add_rng_register(rng_wire_count);
    for (uint32_t i = 0; i < wasm_wire_count; ++i)
      circuit_handle->declared_wires.emplace_back(WasmState(i));
    for (uint32_t i = 0; i < rng_wire_count; ++i)
      circuit_handle->declared_wires.emplace_back(RngState(i));
    *out = circuit_handle.release();
  });
}
TketError tket_circuit_mark_named_boundary(
    TketCircuit *circuit_handle, TketWireId id, bool output) {
  return protect([&] {
    nonnull(circuit_handle);
    const Qubit qubit(declared_wire(circuit_handle, id));
    ++circuit_handle->state->revision;
    if (output)
      circuit_handle->state->circuit.qubit_discard(qubit);
    else
      circuit_handle->state->circuit.qubit_create(qubit);
  });
}
TketError tket_circuit_permute_named_outputs(
    TketCircuit *circuit_handle, const TketWireId *from, const TketWireId *to,
    size_t count) {
  return protect([&] {
    nonnull(circuit_handle);
    if (count) {
      nonnull(from);
      nonnull(to);
    }
    qubit_map_t mapping;
    std::set<Qubit> inputs, outputs;
    for (size_t i = 0; i < count; ++i) {
      const Qubit input(declared_wire(circuit_handle, from[i]));
      const Qubit output(declared_wire(circuit_handle, to[i]));
      require(
          inputs.insert(input).second && outputs.insert(output).second,
          TKET_ERROR_INVALID_ARGUMENT, "Repeated output permutation label");
      mapping.emplace(input, output);
    }
    require(
        inputs == outputs, TKET_ERROR_INVALID_ARGUMENT,
        "Expected a permutation of the supplied labels");
    ++circuit_handle->state->revision;
    circuit_handle->state->circuit.permute_boundary_output(mapping);
  });
}
TketError tket_circuit_append_named_gate(
    TketCircuit *circuit_handle, const TketCommandPorts *ports, TketGate gate,
    const char *const *expressions, size_t count) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(ports);
    if (count) nonnull(expressions);
    const auto type = op_type(gate);
    require(
        type != OpType::Barrier && type != OpType::SetBits &&
            type != OpType::CircBox,
        TKET_ERROR_INVALID_ARGUMENT, "Use the operation's dedicated builder");
    require(
        optypeinfo().at(type).n_params() == count, TKET_ERROR_INVALID_ARGUMENT,
        "Wrong number of gate angles");
    std::vector<Expr> params;
    for (size_t i = 0; i < count; ++i)
      params.push_back(parse_expression(expressions[i]));
    append_named_operation(circuit_handle, ports, get_op_ptr(type, params));
  });
}
TketError tket_circuit_append_named_barrier(
    TketCircuit *circuit_handle, const TketCommandPorts *ports,
    const char *signature, TketBytes data) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(ports);
    op_signature_t sig;
    for (size_t i = 0; i < ports->count; ++i) {
      const auto kind = wire_kind(named_port(circuit_handle, ports, i).type());
      constexpr char linear_ports[] = {'Q', 'C', 'W', 'R'};
      const auto port = signature ? signature[i] : linear_ports[kind];
      sig.push_back(barrier_port(port, kind));
    }
    append_named_operation(
        circuit_handle, ports,
        std::make_shared<BarrierOp>(
            sig, optional_text(data).value_or(std::string{})));
  });
}
TketError tket_circuit_append_named_set_bits(
    TketCircuit *circuit_handle, const TketCommandPorts *ports,
    const uint8_t *values) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(ports);
    if (ports->count) nonnull(values);
    std::vector<bool> assigned;
    for (size_t i = 0; i < ports->count; ++i) {
      require(
          values[i] <= 1, TKET_ERROR_INVALID_ARGUMENT,
          "SetBits values must be 0 or 1");
      assigned.push_back(values[i] != 0);
    }
    append_named_operation(
        circuit_handle, ports, std::make_shared<SetBitsOp>(assigned));
  });
}
TketError tket_circuit_append_named_circbox(
    TketCircuit *circuit_handle, const TketCommandPorts *ports,
    const TketCircuit *contents, const char *id) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(ports);
    nonnull(contents);
    append_named_operation(
        circuit_handle, ports, make_circbox(contents->state->circuit, id));
  });
}
TketError tket_circuit_append_opaque_operation(
    TketCircuit *circuit_handle, const TketCommandPorts *ports,
    TketBytes operation) {
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(ports);
    nonnull(operation.data);
    const auto payload = nlohmann::json::from_cbor(
        operation.data, operation.data + operation.size);
    append_named_operation(circuit_handle, ports, payload.get<Op_ptr>());
  });
}

TketError tket_circuit_iterator_create(
    const TketCircuit *circuit_handle, TketCommandIterator **out) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out);
    *out = new TketCommandIterator(*circuit_handle);
  });
}
// Advancing is entirely a C++ operation. Once this returns, callers can process
// the view in their own language with no callback or C++ traversal frame
// active.
template <class View>
static TketError advance(TketCommandIterator *iterator, View *out, bool *has) {
  // Leave the same empty result on exhaustion and failure, so a caller
  // can't accidentally reuse an earlier view after either outcome.
  if (out) *out = {};
  if (has) *has = false;
  const auto error = protect([&] {
    nonnull(iterator);
    nonnull(out);
    nonnull(has);
    constexpr bool named = std::is_same_v<View, TketNamedOperationView>;
    require(
        iterator->named == named, TKET_ERROR_INVALID_ARGUMENT,
        "Iterator and command view use different wire identifiers");
    require(
        !iterator->failed, TKET_ERROR_ITERATOR_INVALIDATED,
        "Iterator failed on an earlier call");
    // Check this before reading the frontier because mutations can delete
    // the vertices and edges it refers to, even while the shared state
    // stays alive.
    require(
        iterator->revision == iterator->state->revision,
        TKET_ERROR_ITERATOR_INVALIDATED, "Circuit changed during iteration");
    iterator->current_op.reset();
    iterator->has_payload = false;
    iterator->payload.clear();
    auto &circuit = iterator->state->circuit;
    while (!iterator->ready.empty()) {
      const auto vertex = iterator->ready.front();
      iterator->ready.pop_front();
      ++iterator->visited;
      // Release this vertex's outgoing dependencies before skipping
      // boundaries or returning a gate. Its successors are considered on
      // later iterations.
      for (const auto &edge :
           boost::make_iterator_range(boost::out_edges(vertex, circuit.dag))) {
        const auto next = circuit.target(edge);
        if (--iterator->remaining.at(next) == 0)
          iterator->ready.push_back(next);
      }
      if (const auto readers = iterator->readers_before_write.find(vertex);
          readers != iterator->readers_before_write.end())
        for (const auto writer : readers->second)
          if (--iterator->remaining.at(writer) == 0)
            iterator->ready.push_back(writer);
      const auto operation = circuit.get_Op_ptr_from_Vertex(vertex);
      if (is_boundary_type(operation->get_type())) continue;
      const auto gate = gate_type(operation->get_type(), named);
      iterator->args.clear();
      iterator->params.clear();
      iterator->expressions.clear();
      iterator->expression_views.clear();
      // get_in_edges() returns port order. Preserve it because controls and
      // targets can have any wire indices, and sorting them would change a
      // gate.
      for (const auto &edge : circuit.get_in_edges(vertex)) {
        const auto &unit = iterator->wires.at(edge);
        if constexpr (named)
          iterator->args.push_back(
              {wire_kind(unit.type()), iterator->wire_ids.at(unit)});
        else
          iterator->args.push_back(wire(unit));
      }
      if (gate == TKET_GATE_BARRIER) {
        const auto data = static_cast<const BarrierOp &>(*operation).get_data();
        iterator->payload.assign(data.begin(), data.end());
        iterator->barrier_signature.clear();
        for (const auto port : operation->get_signature())
          iterator->barrier_signature.push_back(barrier_port(port));
        iterator->has_payload = true;
      } else if (gate == TKET_GATE_SET_BITS) {
        const auto values =
            static_cast<const SetBitsOp &>(*operation).get_values();
        iterator->payload.assign(values.begin(), values.end());
        iterator->has_payload = true;
      } else if (gate != TKET_GATE_CIRCBOX && gate != TKET_GATE_OPAQUE) {
        if constexpr (std::is_same_v<View, TketOperationView>) {
          for (const auto &parameter : operation->get_params())
            iterator->params.push_back(numeric(parameter));
        } else {
          for (const auto &parameter : operation->get_params_reduced())
            iterator->expressions.push_back(expression_string(parameter));
          for (const auto &text : iterator->expressions)
            iterator->expression_views.push_back(text.c_str());
        }
      }
      if constexpr (std::is_same_v<View, TketOperationView>) {
        *out = {
            gate, iterator->args.data(), iterator->args.size(),
            iterator->params.data(), iterator->params.size()};
      } else {
        const TketExpressionOperationView view{
            gate, iterator->args.data(), iterator->args.size(),
            iterator->expression_views.data(),
            iterator->expression_views.size()};
        if constexpr (named) {
          const auto &group = circuit.get_opgroup_from_Vertex(vertex);
          *out = {view, group ? text_view(*group) : TketBytes{}};
        } else {
          *out = view;
        }
      }
      iterator->current_op = operation;
      iterator->current_gate = gate;
      *has = true;
      return;
    }
    // A valid circuit reaches every vertex. If the frontier emptied
    // earlier, some dependencies form a cycle and we must report an
    // incomplete traversal.
    require(
        iterator->visited == iterator->remaining.size(),
        TKET_ERROR_CIRCUIT_INVALID, "Circuit contains a cycle");
  });
  // We may already have advanced the frontier when conversion fails.
  // Require a fresh iterator after an error so retrying can't silently skip
  // that gate.
  if (error != TKET_SUCCESS && iterator) iterator->failed = true;
  return error;
}
TketError tket_iterator_next(
    TketCommandIterator *iterator, TketOperationView *out, bool *has) {
  return advance(iterator, out, has);
}
TketError tket_iterator_next_with_expressions(
    TketCommandIterator *iterator, TketExpressionOperationView *out,
    bool *has) {
  return advance(iterator, out, has);
}
TketError tket_named_iterator_next(
    TketCommandIterator *iterator, TketNamedOperationView *out, bool *has) {
  return advance(iterator, out, has);
}
// A named iterator owns the metadata for the pass result. Builder IDs
// describe the input circuit and may refer to wires that a pass removed.
static void check_named_iterator(const TketCommandIterator *iterator) {
  nonnull(iterator);
  require(
      iterator->named, TKET_ERROR_INVALID_ARGUMENT,
      "Expected a named iterator");
  require(
      !iterator->failed && iterator->revision == iterator->state->revision,
      TKET_ERROR_ITERATOR_INVALIDATED, "Iterator is no longer valid");
}
TketError tket_circuit_named_iterator_create(
    const TketCircuit *circuit_handle, TketCommandIterator **out,
    size_t *count) {
  if (out) *out = nullptr;
  return protect([&] {
    nonnull(circuit_handle);
    nonnull(out);
    nonnull(count);
    auto iterator =
        std::make_unique<TketCommandIterator>(*circuit_handle, true);
    *count = iterator->named_wires.size();
    *out = iterator.release();
  });
}
TketError tket_named_iterator_get_name(
    const TketCommandIterator *iterator, TketBytes *out) {
  return protect([&] {
    check_named_iterator(iterator);
    nonnull(out);
    *out = iterator->circuit_name ? text_view(*iterator->circuit_name)
                                  : TketBytes{};
  });
}
TketError tket_named_iterator_get_wire(
    TketCommandIterator *iterator, TketWireId id, TketNamedWire *out,
    bool *created, bool *discarded, TketWireId *output) {
  return protect([&] {
    check_named_iterator(iterator);
    nonnull(out);
    nonnull(created);
    nonnull(discarded);
    nonnull(output);
    require(
        id < iterator->named_wires.size(), TKET_ERROR_INVALID_ARGUMENT,
        "Unknown iterator wire ID");
    const auto &unit = iterator->named_wires[id];
    iterator->wire_name = unit.reg_name();
    iterator->wire_indices = unit.index();
    *out = {
        wire_kind(unit.type()), text_view(iterator->wire_name),
        iterator->wire_indices.data(), iterator->wire_indices.size()};
    *created = *discarded = false;
    *output = id;
    if (unit.type() == UnitType::Qubit) {
      const auto &circuit = iterator->state->circuit;
      const Qubit qubit(unit);
      *created = circuit.is_created(qubit);
      *discarded = circuit.is_discarded(qubit);
      const auto path = circuit.unit_path(qubit);
      *output =
          iterator->wire_ids.at(circuit.get_id_from_out(path.back().first));
    }
  });
}
TketError tket_named_iterator_get_opaque_operation(
    TketCommandIterator *iterator, TketBytes *out) {
  return protect([&] {
    check_named_iterator(iterator);
    nonnull(out);
    require(
        iterator->current_op && iterator->current_gate == TKET_GATE_OPAQUE,
        TKET_ERROR_INVALID_ARGUMENT,
        "Iterator is not positioned on an opaque operation");
    iterator->opaque_payload =
        nlohmann::json::to_cbor(iterator->current_op->serialize());
    *out = {iterator->opaque_payload.data(), iterator->opaque_payload.size()};
  });
}

TketError tket_iterator_get_payload(
    const TketCommandIterator *iterator, const uint8_t **data, size_t *size) {
  return protect([&] {
    nonnull(iterator);
    nonnull(data);
    nonnull(size);
    require(
        !iterator->failed && iterator->revision == iterator->state->revision,
        TKET_ERROR_ITERATOR_INVALIDATED, "Iterator is no longer valid");
    require(
        iterator->has_payload, TKET_ERROR_INVALID_ARGUMENT,
        "The iterator has no current operation with a payload");
    *data = iterator->payload.data();
    *size = iterator->payload.size();
  });
}

// The accessors share the same lifetime check as the operation view.
static const Op &current_operation(
    const TketCommandIterator *iterator, OpType type) {
  nonnull(iterator);
  require(
      !iterator->failed && iterator->revision == iterator->state->revision,
      TKET_ERROR_ITERATOR_INVALIDATED, "Iterator is no longer valid");
  require(
      iterator->current_op && iterator->current_op->get_type() == type,
      TKET_ERROR_INVALID_ARGUMENT,
      "Iterator is not positioned on the requested operation");
  return *iterator->current_op;
}

TketError tket_iterator_get_barrier_signature(
    const TketCommandIterator *iterator, const char **signature) {
  return protect([&] {
    nonnull(signature);
    current_operation(iterator, OpType::Barrier);
    *signature = iterator->barrier_signature.data();
  });
}

TketError tket_iterator_get_circbox(
    const TketCommandIterator *iterator, TketCircuit **contents,
    char out_id[37]) {
  if (contents) *contents = nullptr;
  return protect([&] {
    nonnull(contents);
    nonnull(out_id);
    const auto &operation = current_operation(iterator, OpType::CircBox);
    const auto &box = static_cast<const CircBox &>(operation);
    const auto id = boost::uuids::to_string(box.get_id());
    auto copy = std::make_unique<TketCircuit>();
    copy->state->circuit = *box.to_circuit();
    std::memcpy(out_id, id.c_str(), 37);
    *contents = copy.release();
  });
}

void tket_free_iterator(TketCommandIterator *iterator) { delete iterator; }
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
