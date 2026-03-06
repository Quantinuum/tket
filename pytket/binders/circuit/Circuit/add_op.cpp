// Copyright Quantinuum
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <nanobind/nanobind.h>

#include <memory>
#include <optional>
#include <vector>

#include "UnitRegister.hpp"
#include "add_gate.hpp"
#include "circuit_registers.hpp"
#include "nanobind-stl.hpp"
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/ConjugationBox.hpp"
#include "tket/Circuit/DiagonalBox.hpp"
#include "tket/Circuit/DummyBox.hpp"
#include "tket/Circuit/Multiplexor.hpp"
#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/Circuit/StatePreparation.hpp"
#include "tket/Circuit/ToffoliBox.hpp"
#include "tket/Converters/PhasePoly.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/Ops/ClExpr.hpp"
#include "tket/Utils/UnitID.hpp"
#include "typecast.hpp"
namespace nb = nanobind;

namespace tket {

typedef nb::tket_custom::SequenceVec<UnitID> py_unit_vector_t;
typedef nb::tket_custom::SequenceVec<Bit> py_bit_vector_t;
typedef nb::tket_custom::SequenceVec<Qubit> py_qubit_vector_t;
typedef nb::tket_custom::SequenceVec<QubitRegister> py_qreg_vector_t;
typedef nb::tket_custom::SequenceVec<BitRegister> py_creg_vector_t;

const bit_vector_t no_bits;

Circuit *add_gate_method_any(
    Circuit *circ, const Op_ptr &op,
    const std::variant<std::vector<unsigned>, std::vector<UnitID>> &args,
    const nb::kwargs &kwargs) {
  if (op->get_desc().is_meta()) {
    throw CircuitInvalidity("Cannot add metaop to a circuit.");
  }
  if (op->get_desc().is_barrier()) {
    throw CircuitInvalidity(
        "Please use `add_barrier` to add a barrier to a circuit.");
  }
  static const std::set<std::string> allowed_kwargs = {
      "opgroup", "condition", "condition_bits", "condition_value"};
  for (const auto kwarg : kwargs) {
    const std::string kwargstr = nb::cast<std::string>(kwarg.first);
    if (!allowed_kwargs.contains(kwargstr)) {
      std::stringstream msg;
      msg << "Unsupported keyword argument '" << kwargstr << "'";
      throw CircuitInvalidity(msg.str());
    }
  }
  std::optional<std::string> opgroup;
  if (kwargs.contains("opgroup")) {
    opgroup = nb::cast<std::string>(kwargs["opgroup"]);
  }
  bool condition_given = kwargs.contains("condition");
  bool condition_bits_given = kwargs.contains("condition_bits");
  bool condition_value_given = kwargs.contains("condition_value");
  if (condition_given && condition_bits_given) {
    throw CircuitInvalidity("Both `condition` and `condition_bits` specified");
  }
  if (condition_value_given && !condition_bits_given) {
    throw CircuitInvalidity(
        "`condition_value` specified without `condition_bits`");
  }
  Op_ptr new_op = op;
  unit_vector_t new_args;
  if (condition_given) {
    nb::module_ condition =
        nb::module_::import_("pytket.circuit.add_condition");
    nb::object add_condition = condition.attr("_add_condition");
    auto conditions = nb::cast<std::pair<std::variant<unsigned, UnitID>, bool>>(
        add_condition(circ, kwargs["condition"]));
    if (std::holds_alternative<UnitID>(conditions.first)) {
      new_args.push_back(std::get<UnitID>(conditions.first));
    } else {
      new_args.push_back(Bit(std::get<unsigned>(conditions.first)));
    }
    new_op = std::make_shared<Conditional>(op, 1, int(conditions.second));
  } else if (condition_bits_given) {
    auto condition_ids =
        nb::cast<std::variant<std::vector<unsigned>, std::vector<UnitID>>>(
            kwargs["condition_bits"]);
    if (std::holds_alternative<std::vector<UnitID>>(condition_ids)) {
      new_args = std::get<std::vector<UnitID>>(condition_ids);
    } else {
      for (unsigned id : std::get<std::vector<unsigned>>(condition_ids)) {
        new_args.push_back(Bit(id));
      }
    }
    unsigned condition_width = new_args.size();
    unsigned value = condition_value_given
                         ? nb::cast<unsigned>(kwargs["condition_value"])
                         : (1u << condition_width) - 1;
    new_op = std::make_shared<Conditional>(op, condition_width, value);
  }
  if (std::holds_alternative<std::vector<UnitID>>(args)) {
    std::vector<UnitID> unit_args = std::get<std::vector<UnitID>>(args);
    new_args.insert(new_args.end(), unit_args.begin(), unit_args.end());
  } else {
    op_signature_t sig = op->get_signature();
    std::vector<unsigned> u_args = std::get<std::vector<unsigned>>(args);
    for (unsigned i = 0; i < u_args.size(); ++i) {
      unsigned uid = u_args.at(i);
      switch (sig.at(i)) {
        case EdgeType::Quantum: {
          new_args.push_back(Qubit(uid));
          break;
        }
        case EdgeType::WASM: {
          new_args.push_back(WasmState(uid));
          break;
        }
        case EdgeType::RNG: {
          new_args.push_back(RngState(uid));
          break;
        }
        case EdgeType::Classical:
        case EdgeType::Boolean: {
          new_args.push_back(Bit(uid));
          break;
        }
        default: {
          TKET_ASSERT(!"add_gate_method found invalid edge type in signature");
        }
      }
    }
  }
  circ->add_op(new_op, new_args, opgroup);
  return circ;
}

typedef std::variant<
    nb::tket_custom::SequenceVec<unsigned>,
    nb::tket_custom::SequenceVec<UnitID>>
    var_seq_ids_t;
typedef std::variant<
    nb::tket_custom::SequenceVec<unsigned>, nb::tket_custom::SequenceVec<Qubit>>
    var_seq_qbs_t;

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_sequence_to_var_vecs(const var_seq_ids_t &var_seq) {
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(var_seq)) {
    return std::get<nb::tket_custom::SequenceVec<unsigned>>(var_seq);
  } else {
    return std::get<nb::tket_custom::SequenceVec<UnitID>>(var_seq);
  }
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_sequence_to_var_vecs(const var_seq_qbs_t &var_seq) {
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(var_seq)) {
    return std::get<nb::tket_custom::SequenceVec<unsigned>>(var_seq);
  } else {
    auto qb_vec = std::get<nb::tket_custom::SequenceVec<Qubit>>(var_seq);
    return std::vector<UnitID>{qb_vec.begin(), qb_vec.end()};
  }
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_qq_to_var_vec(
    const std::variant<unsigned, Qubit> &qb0,
    const std::variant<unsigned, Qubit> &qb1, const std::string &method_name) {
  if (std::holds_alternative<unsigned>(qb0) !=
      std::holds_alternative<unsigned>(qb1)) {
    throw CircuitInvalidity(
        "Qubits passed to `" + method_name +
        "` must either both be `int` or both `Qubit`.");
  }
  if (std::holds_alternative<unsigned>(qb0)) {
    unsigned uq0 = std::get<unsigned>(qb0);
    unsigned uq1 = std::get<unsigned>(qb1);
    return std::vector<unsigned>{uq0, uq1};
  } else {
    Qubit qq0 = std::get<Qubit>(qb0);
    Qubit qq1 = std::get<Qubit>(qb1);
    return std::vector<UnitID>{qq0, qq1};
  }
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_qqq_to_var_vec(
    const std::variant<unsigned, Qubit> &qb0,
    const std::variant<unsigned, Qubit> &qb1,
    const std::variant<unsigned, Qubit> &qb2, const std::string &method_name) {
  bool is_unsigned = std::holds_alternative<unsigned>(qb0);
  if (std::holds_alternative<unsigned>(qb1) != is_unsigned ||
      std::holds_alternative<unsigned>(qb2) != is_unsigned) {
    throw CircuitInvalidity(
        "Qubits passed to `" + method_name +
        "` must either all be `int` or all `Qubit`.");
  }
  if (is_unsigned) {
    unsigned uq0 = std::get<unsigned>(qb0);
    unsigned uq1 = std::get<unsigned>(qb1);
    unsigned uq2 = std::get<unsigned>(qb2);
    return std::vector<unsigned>{uq0, uq1, uq2};
  } else {
    Qubit qq0 = std::get<Qubit>(qb0);
    Qubit qq1 = std::get<Qubit>(qb1);
    Qubit qq2 = std::get<Qubit>(qb2);
    return std::vector<UnitID>{qq0, qq1, qq2};
  }
}

static unsigned var_seq_len(const var_seq_ids_t &var_seq) {
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(var_seq)) {
    return std::get<nb::tket_custom::SequenceVec<unsigned>>(var_seq).size();
  } else {
    return std::get<nb::tket_custom::SequenceVec<UnitID>>(var_seq).size();
  }
}

static Circuit *add_gate_method_sequence_args_any(
    Circuit *circ, const Op_ptr &op, const var_seq_ids_t &args,
    const nb::kwargs &kwargs) {
  return add_gate_method_any(circ, op, var_sequence_to_var_vecs(args), kwargs);
}

static Circuit *add_gate_method_noparams_any(
    Circuit *circ, OpType type, const var_seq_ids_t &args,
    const nb::kwargs &kwargs) {
  return add_gate_method_any(
      circ, get_op_ptr(type, std::vector<Expr>{}, var_seq_len(args)),
      var_sequence_to_var_vecs(args), kwargs);
}

static Circuit *add_gate_method_oneparam_any(
    Circuit *circ, OpType type, const Expr &p, const var_seq_ids_t &args,
    const nb::kwargs &kwargs) {
  return add_gate_method_any(
      circ, get_op_ptr(type, p, var_seq_len(args)),
      var_sequence_to_var_vecs(args), kwargs);
}

static Circuit *add_gate_method_manyparams_any(
    Circuit *circ, OpType type, const nb::tket_custom::SequenceVec<Expr> &ps,
    const var_seq_ids_t &args, const nb::kwargs &kwargs) {
  return add_gate_method_any(
      circ, get_op_ptr(type, ps, var_seq_len(args)),
      var_sequence_to_var_vecs(args), kwargs);
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_q_to_var_vecs(const std::variant<unsigned, Qubit> &qb) {
  if (std::holds_alternative<unsigned>(qb))
    return std::vector<unsigned>{std::get<unsigned>(qb)};
  else
    return std::vector<UnitID>{std::get<Qubit>(qb)};
}

static std::function<Circuit *(
    Circuit *, const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_q(OpType ot) {
  return [ot](
             Circuit *circ, const std::variant<unsigned, Qubit> &qb,
             const nb::kwargs &kwargs) {
    return add_gate_method_any(
        circ, get_op_ptr(ot, std::vector<Expr>{}, 1), var_q_to_var_vecs(qb),
        kwargs);
  };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const std::variant<unsigned, Qubit> &,
    const nb::kwargs &)>
add_gate_method_a_q(OpType ot) {
  return
      [ot](
          Circuit *circ, const Expr &angle,
          const std::variant<unsigned, Qubit> &qb, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle}, 1), var_q_to_var_vecs(qb), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const Expr &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_aa_q(OpType ot) {
  return
      [ot](
          Circuit *circ, const Expr &angle0, const Expr &angle1,
          const std::variant<unsigned, Qubit> &qb, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle0, angle1}, 1), var_q_to_var_vecs(qb),
            kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const Expr &, const Expr &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_aaa_q(OpType ot) {
  return [ot](
             Circuit *circ, const Expr &angle0, const Expr &angle1,
             const Expr &angle2, const std::variant<unsigned, Qubit> &qb,
             const nb::kwargs &kwargs) {
    return add_gate_method_any(
        circ, get_op_ptr(ot, {angle0, angle1, angle2}, 1),
        var_q_to_var_vecs(qb), kwargs);
  };
}

static std::function<Circuit *(
    Circuit *, const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_qq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, std::vector<Expr>{}, 2),
            var_qq_to_var_vec(qb0, qb1, method_name), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_a_qq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const Expr &angle,
          const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle}, 2),
            var_qq_to_var_vec(qb0, qb1, method_name), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const Expr &,
    const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_aa_qq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const Expr &angle0, const Expr &angle1,
          const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle0, angle1}, 2),
            var_qq_to_var_vec(qb0, qb1, method_name), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const Expr &, const Expr &,
    const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_aaa_qq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const Expr &angle0, const Expr &angle1,
          const Expr &angle2, const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle0, angle1, angle2}, 2),
            var_qq_to_var_vec(qb0, qb1, method_name), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_qqq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1,
          const std::variant<unsigned, Qubit> &qb2, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, std::vector<Expr>{}, 3),
            var_qqq_to_var_vec(qb0, qb1, qb2, method_name), kwargs);
      };
}

static std::function<Circuit *(
    Circuit *, const Expr &, const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &,
    const std::variant<unsigned, Qubit> &, const nb::kwargs &)>
add_gate_method_a_qqq(OpType ot, const std::string &method_name) {
  return
      [ot, method_name](
          Circuit *circ, const Expr &angle,
          const std::variant<unsigned, Qubit> &qb0,
          const std::variant<unsigned, Qubit> &qb1,
          const std::variant<unsigned, Qubit> &qb2, const nb::kwargs &kwargs) {
        return add_gate_method_any(
            circ, get_op_ptr(ot, {angle}, 3),
            var_qqq_to_var_vec(qb0, qb1, qb2, method_name), kwargs);
      };
}

void init_circuit_add_op(nb::class_<Circuit> &c) {
  c.def(
       "add_gate", &add_gate_method_sequence_args_any,
       "Appends a single operation to the end of the circuit on some "
       "particular qubits/bits. The number of qubits/bits specified "
       "must match the arity of the gate.",
       nb::arg("Op"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_gate", &add_gate_method_noparams_any,
          "Appends a single (non-parameterised) gate to the end of the "
          "circuit on some particular qubits from the default register "
          "('q'). The number of qubits specified must match the arity "
          "of the gate. For `OpType.Measure` operations the bit from "
          "the default register should follow the qubit."
          "\n\n>>> c.add_gate(OpType.H, [0]) # equivalent to "
          "c.H(0)\n>>> c.add_gate(OpType.CX, [0,1]) # equivalent to "
          "c.CX(0,1)"
          "\n\n:param type: The type of operation to add"
          "\n:param args: The qubits/bits to apply the gate to"
          "\n:param kwargs: Additional properties for classical "
          "conditions"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("type"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_gate", &add_gate_method_oneparam_any,
          "Appends a single gate, parameterised by an expression, to "
          "the end of circuit on some particular qubits from the "
          "default register ('q')."
          "\n\n:param type: The type of gate to add"
          "\n:param angle: The parameter for the gate in halfturns"
          "\n:param args: The qubits/bits to apply the gate to"
          "\n:param kwargs: Additional properties for classical "
          "conditions"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("type"), nb::arg("angle"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_gate", &add_gate_method_manyparams_any,
          "Appends a single gate to the end of the circuit"
          "\n\n:param type: The type of gate to add"
          "\n:param params: The parameters for the gate in halfturns"
          "\n:param args: The qubits/bits to apply the gate to"
          "\n:param kwargs: Additional properties for classical "
          "conditions"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("type"), nb::arg("angles"), nb::arg("args"),
          nb::arg("kwargs"))
      .def(
          "add_barrier",
          [](Circuit *circ,
             const nb::tket_custom::SequenceVec<unsigned> &qubits,
             const nb::tket_custom::SequenceVec<unsigned> &bits,
             const std::string &data) {
            circ->add_barrier(qubits, bits, data);
            return circ;
          },
          "Append a Barrier on the given units"
          "\n\n:param data: additional data stored in the barrier"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubits"), nb::arg("bits") = no_bits, nb::arg("data") = "")
      .def(
          "add_conditional_barrier",
          [](Circuit *circ,
             const nb::tket_custom::SequenceVec<unsigned> &barrier_qubits,
             const nb::tket_custom::SequenceVec<unsigned> &barrier_bits,
             const nb::tket_custom::SequenceVec<unsigned> &condition_bits,
             unsigned value, const std::string &_data) {
            circ->add_conditional_barrier(
                barrier_qubits, barrier_bits, condition_bits, value, _data);
            return circ;
          },
          "Append a Conditional Barrier on the given barrier qubits and "
          "barrier bits, conditioned on the given condition bits."
          "\n\n:param barrier_qubits: Qubit in Barrier operation."
          "\n:param barrier_bits: Bit in Barrier operation."
          "\n:param condition_bits: Bit covering classical control condition "
          "of barrier operation."
          "\n:param value: Value that classical condition must have to "
          "hold (little-endian)."
          "\n:param data: Additional data stored in Barrier operation."
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("barrier_qubits"), nb::arg("barrier_bits"),
          nb::arg("condition_bits"), nb::arg("value"), nb::arg("data") = "")
      .def(
          "add_clexpr",
          [](Circuit *circ, const WiredClExpr &expr,
             const nb::tket_custom::SequenceVec<Bit> &args,
             const nb::kwargs &kwargs) {
            Op_ptr op = std::make_shared<ClExprOp>(expr);
            std::vector<UnitID> uid_args;
            for (const Bit &b : args) uid_args.push_back(b);
            return add_gate_method_any(circ, op, uid_args, kwargs);
          },
          "Append a :py:class:`~.WiredClExpr` to the circuit.\n\n"
          ":param expr: The expression to append\n"
          ":param args: The bits to apply the expression to\n"
          ":return: the new :py:class:`~.Circuit`",
          nb::arg("expr"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_barrier",
          [](Circuit *circ, const py_unit_vector_t &units,
             const std::string &data) {
            circ->add_barrier(units, data);
            return circ;
          },
          "Append a Barrier on the given units"
          "\n\n:param data: additional data stored in the barrier"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("units"), nb::arg("data") = "")
      .def(
          "add_conditional_barrier",
          [](Circuit *circ, const py_unit_vector_t &barrier_args,
             const py_bit_vector_t &condition_bits, unsigned value,
             const std::string &_data) {
            circ->add_conditional_barrier(
                barrier_args, condition_bits, value, _data);
            return circ;
          },
          "Append a Conditional Barrier on the given barrier qubits and "
          "barrier bits, conditioned on the given condition bits."
          "\n\n:param barrier_args: Qubit and Bit in Barrier operation."
          "\n:param condition_bits: Bit covering classical control "
          " condition of barrier operation."
          "\n:param value: Value that classical condition must have to "
          "hold (little-endian)."
          "\n:param data: Additional data stored in Barrier operation."
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("barrier_args"), nb::arg("condition_bits"), nb::arg("value"),
          nb::arg("data") = "")
      .def(
          "add_circbox",
          [](Circuit *circ, const CircBox &box, const var_seq_ids_t &args,
             const nb::kwargs &kwargs) {
            return add_gate_method_sequence_args_any(
                circ, std::make_shared<CircBox>(box), args, kwargs);
          },
          "Append a :py:class:`~.CircBox` to the circuit."
          "\n\nThe qubits and bits of the :py:class:`~.CircBox` are wired into "
          "the circuit in lexicographic order. Bits follow qubits in the order "
          "of arguments."
          "\n\n:param circbox: The box to append"
          "\n:param args: The qubits/bits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("circbox"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_circbox_regwise",
          [](Circuit *circ, const CircBox &box, const py_qreg_vector_t &qregs,
             const py_creg_vector_t &cregs, const nb::kwargs &kwargs) {
            std::vector<UnitID> args;

            for (const QubitRegister &qreg : qregs) {
              const std::string &name = qreg.name();
              for (std::size_t i = 0; i < qreg.size(); i++) {
                args.push_back(Qubit(name, i));
              }
            }
            for (const BitRegister &creg : cregs) {
              const std::string &name = creg.name();
              for (std::size_t i = 0; i < creg.size(); i++) {
                args.push_back(Bit(name, i));
              }
            }
            return add_gate_method_any(
                circ, std::make_shared<CircBox>(box), args, kwargs);
          },
          "Append a :py:class:`~.CircBox` to the circuit, wiring whole "
          "registers "
          "together."
          "\n\n:param circbox: The box to append"
          "\n:param qregs: Sequence of :py:class:`~.QubitRegister` from the "
          "outer :py:class:`~.Circuit`, the order corresponding to the "
          "lexicographic order of corresponding registers in the "
          ":py:class:`~.CircBox`"
          "\n:param cregs: Sequence of :py:class:`~.BitRegister` from the "
          "outer :py:class:`~.Circuit`, the order corresponding to the "
          "lexicographic order of corresponding registers in the "
          ":py:class:`~.CircBox`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("circbox"), nb::arg("qregs"), nb::arg("cregs"),
          nb::arg("kwargs"))
      .def(
          "add_circbox_with_regmap",
          [](Circuit *circ, const CircBox &box,
             const std::map<std::string, std::string> &qregmap,
             const std::map<std::string, std::string> &cregmap,
             const nb::kwargs &kwargs) {
            // Get registers of circuit:
            std::vector<QubitRegister> circ_qregs =
                get_unit_registers<QubitRegister>(*circ);
            std::vector<BitRegister> circ_cregs =
                get_unit_registers<BitRegister>(*circ);

            // Map name -> size for circuit registers:
            std::map<std::string, std::size_t> circ_qreg_sizes;
            for (const QubitRegister &qreg : circ_qregs) {
              circ_qreg_sizes[qreg.name()] = qreg.size();
            }
            std::map<std::string, unsigned> circ_creg_sizes;
            for (const BitRegister &creg : circ_cregs) {
              circ_creg_sizes[creg.name()] = creg.size();
            }

            // Get box circuit:
            std::shared_ptr<Circuit> box_circ = box.to_circuit();

            // Get units of box (in lexicographic order):
            const qubit_vector_t box_qubits = box_circ->all_qubits();
            const bit_vector_t box_bits = box_circ->all_bits();

            // Get registers of box:
            std::vector<QubitRegister> box_qregs =
                get_unit_registers<QubitRegister>(*box_circ);
            std::vector<BitRegister> box_cregs =
                get_unit_registers<BitRegister>(*box_circ);

            // Map name -> size for box registers
            std::map<std::string, std::size_t> box_qreg_sizes;
            for (const QubitRegister &qreg : box_qregs) {
              box_qreg_sizes[qreg.name()] = qreg.size();
            }
            std::map<std::string, unsigned> box_creg_sizes;
            for (const BitRegister &creg : box_cregs) {
              box_creg_sizes[creg.name()] = creg.size();
            }

            // Check that all units in the box are in a register:
            for (const Qubit &qb : box_qubits) {
              if (box_qreg_sizes.find(qb.reg_name()) == box_qreg_sizes.end()) {
                throw CircuitInvalidity("Box contains non-register qubits.");
              }
            }
            for (const Bit &cb : box_bits) {
              if (box_creg_sizes.find(cb.reg_name()) == box_creg_sizes.end()) {
                throw CircuitInvalidity("Box contains non-register bits.");
              }
            }

            // Check that the sizes of the registers match up:
            for (const auto &pair : box_qreg_sizes) {
              if (circ_qreg_sizes.at(qregmap.at(pair.first)) != pair.second) {
                throw CircuitInvalidity("Size mismatch in qubit registers");
              }
            }
            for (const auto &pair : box_creg_sizes) {
              if (circ_creg_sizes.at(cregmap.at(pair.first)) != pair.second) {
                throw CircuitInvalidity("Size mismatch in bit registers");
              }
            }

            // Populate the arguments (qubits then bits). Note that they are in
            // lexicographic order; when the box is inserted into the circuit
            // (in Circuit::substitute_box_vertex()) the units are also
            // connected in lexicographic order.
            std::vector<UnitID> args;
            for (const Qubit &qb : box_qubits) {
              args.push_back(Qubit(qregmap.at(qb.reg_name()), qb.index()[0]));
            }
            for (const Bit &cb : box_bits) {
              args.push_back(Bit(cregmap.at(cb.reg_name()), cb.index()[0]));
            }

            // Add the box:
            return add_gate_method_any(
                circ, std::make_shared<CircBox>(box), args, kwargs);
          },
          "Append a :py:class:`~.CircBox` to the circuit, wiring whole "
          "registers "
          "together."
          "\n\nThis method expects two maps (one for qubit registers and one "
          "for bit registers), which must have keys corresponding to all "
          "register names in the box. The box may not contain any qubits or "
          "bits that do not belong to a register, i.e. all must be single-"
          "indexed contiguously from zero."
          "\n\n:param circbox: The box to append"
          "\n:param qregmap: Map specifying which qubit register in the "
          ":py:class:`~.CircBox` (the map's keys) matches which register in "
          "the "
          "outer circuit (the map's values)"
          "\n:param cregmap: Map specifying which bit register in the "
          ":py:class:`~.CircBox` (the map's keys) matches which register in "
          "the "
          "outer circuit (the map's values)"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("circbox"), nb::arg("qregmap"), nb::arg("cregmap"),
          nb::arg("kwargs"))
      .def(
          "add_unitary1qbox",
          [](Circuit *circ, const Unitary1qBox &box,
             const std::variant<unsigned, Qubit> &q0,
             const nb::kwargs &kwargs) {
            if (std::holds_alternative<unsigned>(q0)) {
              unsigned uq0 = std::get<unsigned>(q0);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary1qBox>(box),
                  std::vector<unsigned>{uq0}, kwargs);
            } else {
              Qubit qq0 = std::get<Qubit>(q0);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary1qBox>(box),
                  std::vector<UnitID>{qq0}, kwargs);
            }
          },
          "Append a :py:class:`~.Unitary1qBox` to the "
          "circuit.\n\n:param unitarybox: The box to append\n:param "
          "qubit_0: The qubit to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("unitarybox"), nb::arg("qubit_0"), nb::arg("kwargs"))
      .def(
          "add_unitary2qbox",
          [](Circuit *circ, const Unitary2qBox &box,
             const std::variant<unsigned, Qubit> &q0,
             const std::variant<unsigned, Qubit> &q1,
             const nb::kwargs &kwargs) {
            if (std::holds_alternative<unsigned>(q0) !=
                std::holds_alternative<unsigned>(q1)) {
              throw CircuitInvalidity(
                  "Qubits passed to `add_unitary2qbox` must either both be "
                  "`int` or both `Qubit`.");
            }
            if (std::holds_alternative<unsigned>(q0)) {
              unsigned uq0 = std::get<unsigned>(q0);
              unsigned uq1 = std::get<unsigned>(q1);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary2qBox>(box),
                  std::vector<unsigned>{uq0, uq1}, kwargs);
            } else {
              Qubit qq0 = std::get<Qubit>(q0);
              Qubit qq1 = std::get<Qubit>(q1);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary2qBox>(box),
                  std::vector<UnitID>{qq0, qq1}, kwargs);
            }
          },
          "Append a :py:class:`~.Unitary2qBox` to the circuit.\n\nThe "
          "matrix representation is ILO-BE.\n\n:param unitarybox: "
          "The box to append\n:param qubit_0: The first target "
          "qubit\n:param qubit_1: The second target qubit"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("unitarybox"), nb::arg("qubit_0"), nb::arg("qubit_1"),
          nb::arg("kwargs"))
      .def(
          "add_unitary3qbox",
          [](Circuit *circ, const Unitary3qBox &box,
             const std::variant<unsigned, Qubit> &q0,
             const std::variant<unsigned, Qubit> &q1,
             const std::variant<unsigned, Qubit> &q2,
             const nb::kwargs &kwargs) {
            bool is_unsigned = std::holds_alternative<unsigned>(q0);
            if (std::holds_alternative<unsigned>(q1) != is_unsigned ||
                std::holds_alternative<unsigned>(q2) != is_unsigned) {
              throw CircuitInvalidity(
                  "Qubits passed to `add_unitary3qbox` must either all be "
                  "`int` or all `Qubit`.");
            }
            if (is_unsigned) {
              unsigned uq0 = std::get<unsigned>(q0);
              unsigned uq1 = std::get<unsigned>(q1);
              unsigned uq2 = std::get<unsigned>(q2);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary3qBox>(box),
                  std::vector<unsigned>{uq0, uq1, uq2}, kwargs);
            } else {
              Qubit qq0 = std::get<Qubit>(q0);
              Qubit qq1 = std::get<Qubit>(q1);
              Qubit qq2 = std::get<Qubit>(q2);
              return add_gate_method_any(
                  circ, std::make_shared<Unitary3qBox>(box),
                  std::vector<UnitID>{qq0, qq1, qq2}, kwargs);
            }
          },
          "Append a :py:class:`~.Unitary3qBox` to the circuit."
          "\n\n:param unitarybox: box to append"
          "\n:param qubit_0: index of target qubit 0"
          "\n:param qubit_1: index of target qubit 1"
          "\n:param qubit_2: index of target qubit 2"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("unitarybox"), nb::arg("qubit_0"), nb::arg("qubit_1"),
          nb::arg("qubit_2"), nb::arg("kwargs"))
      .def(
          "add_expbox",
          [](Circuit *circ, const ExpBox &box,
             const std::variant<unsigned, Qubit> &q0,
             const std::variant<unsigned, Qubit> &q1,
             const nb::kwargs &kwargs) {
            if (std::holds_alternative<unsigned>(q0) !=
                std::holds_alternative<unsigned>(q1))
              throw CircuitInvalidity(
                  "Qubits passed to `add_expbox` must either both be `int` or "
                  "both `Qubit`.");
            if (std::holds_alternative<unsigned>(q0)) {
              unsigned uq0 = std::get<unsigned>(q0);
              unsigned uq1 = std::get<unsigned>(q1);
              return add_gate_method_any(
                  circ, std::make_shared<ExpBox>(box),
                  std::vector<unsigned>{uq0, uq1}, kwargs);
            } else {
              Qubit qq0 = std::get<Qubit>(q0);
              Qubit qq1 = std::get<Qubit>(q1);
              return add_gate_method_any(
                  circ, std::make_shared<ExpBox>(box),
                  std::vector<UnitID>{qq0, qq1}, kwargs);
            }
          },
          "Append an :py:class:`~.ExpBox` to the circuit.\n\nThe "
          "matrix representation is ILO-BE.\n\n:param expbox: The "
          "box to append\n:param qubit_0: The first target "
          "qubit\n:param qubit_1: The second target qubit"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("expbox"), nb::arg("qubit_0"), nb::arg("qubit_1"),
          nb::arg("kwargs"))
      .def(
          "add_pauliexpbox",
          [](Circuit *circ, const PauliExpBox &box, const var_seq_qbs_t &qubits,
             const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<PauliExpBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.PauliExpBox` to the "
          "circuit.\n\n:param pauliexpbox: The box to append\n:param "
          "qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("pauliexpbox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_pauliexppairbox",
          [](Circuit *circ, const PauliExpPairBox &box,
             const var_seq_qbs_t &qubits, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<PauliExpPairBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.PauliExpPairBox` to the "
          "circuit.\n\n:param pauliexppairbox: The box to append\n:param "
          "qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("pauliexppairbox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_pauliexpcommutingsetbox",
          [](Circuit *circ, const PauliExpCommutingSetBox &box,
             const var_seq_qbs_t &qubits, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<PauliExpCommutingSetBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.PauliExpCommutingSetBox` to the "
          "circuit.\n\n:param pauliexpcommutingsetbox: The box to "
          "append\n:param "
          "qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("pauliexpcommutingsetbox"), nb::arg("qubits"),
          nb::arg("kwargs"))
      .def(
          "add_termsequencebox",
          [](Circuit *circ, const TermSequenceBox &box,
             const var_seq_qbs_t &qubits, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<TermSequenceBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.TermSequenceBox` to the "
          "circuit.\n\n:param termsequencebox: The box to "
          "append\n:param "
          "qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("termsequencebox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_toffolibox",
          [](Circuit *circ, const ToffoliBox &box, const var_seq_qbs_t &qubits,
             const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<ToffoliBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.ToffoliBox` to the "
          "circuit.\n\n:param toffolibox: The box to append\n:param "
          "qubits: Indices of the qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("toffolibox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_dummybox",
          [](Circuit *circ, const DummyBox &box, const var_seq_qbs_t &qubits,
             const std::variant<
                 nb::tket_custom::SequenceVec<unsigned>,
                 nb::tket_custom::SequenceVec<Bit>> &bits,
             const nb::kwargs &kwargs) {
            if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(
                    qubits)) {
              auto qbs =
                  std::get<nb::tket_custom::SequenceVec<unsigned>>(qubits);
              if (std::holds_alternative<
                      nb::tket_custom::SequenceVec<unsigned>>(bits)) {
                auto bs =
                    std::get<nb::tket_custom::SequenceVec<unsigned>>(bits);
                qbs.insert(qbs.end(), bs.begin(), bs.end());
                return add_gate_method_any(
                    circ, std::make_shared<DummyBox>(box), qbs, kwargs);
              } else {
                auto bs = std::get<nb::tket_custom::SequenceVec<Bit>>(bits);
                std::vector<UnitID> args;
                for (unsigned q : qbs) args.push_back(Qubit(q));
                args.insert(args.end(), bs.begin(), bs.end());
                return add_gate_method_any(
                    circ, std::make_shared<DummyBox>(box), args, kwargs);
              }
            } else {
              auto qbs = std::get<nb::tket_custom::SequenceVec<Qubit>>(qubits);
              std::vector<UnitID> args;
              args.insert(args.end(), qbs.begin(), qbs.end());
              if (std::holds_alternative<
                      nb::tket_custom::SequenceVec<unsigned>>(bits)) {
                auto bs =
                    std::get<nb::tket_custom::SequenceVec<unsigned>>(bits);
                for (unsigned b : bs) args.push_back(Bit(b));
              } else {
                auto bs = std::get<nb::tket_custom::SequenceVec<Bit>>(bits);
                args.insert(args.end(), bs.begin(), bs.end());
              }
              return add_gate_method_any(
                  circ, std::make_shared<DummyBox>(box), args, kwargs);
            }
          },
          "Append a :py:class:`~.DummyBox` to the circuit."
          "\n\n:param dummybox: The box to append"
          "\n:param qubits: Qubits to append the box to"
          "\n:param bits: Bits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("dummybox"), nb::arg("qubits"), nb::arg("bits"),
          nb::arg("kwargs"))
      .def(
          "add_qcontrolbox",
          [](Circuit *circ, const QControlBox &box, const var_seq_qbs_t &qubits,
             const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<QControlBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.QControlBox` to the circuit.\n\n"
          ":param qcontrolbox: The box to append\n"
          ":param qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qcontrolbox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_phasepolybox",
          [](Circuit *circ, const PhasePolyBox &box,
             const var_seq_qbs_t &qubits, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<PhasePolyBox>(box),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append a :py:class:`~.PhasePolyBox` to the "
          "circuit.\n\n:param phasepolybox: The box to append\n:param "
          "qubits: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("phasepolybox"), nb::arg("qubits"), nb::arg("kwargs"))
      .def(
          "add_custom_gate",
          [](Circuit *circ, const composite_def_ptr_t &definition,
             const nb::tket_custom::SequenceVec<Expr> &params,
             const var_seq_qbs_t &qubits, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<CustomGate>(definition, params),
                var_sequence_to_var_vecs(qubits), kwargs);
          },
          "Append an instance of a :py:class:`~.CustomGateDef` to the "
          "circuit.\n\n:param def: The custom gate "
          "definition\n:param params: List of parameters to "
          "instantiate the gate with, in halfturns\n:param qubits: "
          "The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("definition"), nb::arg("params"), nb::arg("qubits"),
          nb::arg("kwargs"))
      .def(
          "add_assertion",
          [](Circuit *circ, const ProjectorAssertionBox &box,
             const var_seq_qbs_t &qubits,
             const std::optional<std::variant<unsigned, Qubit>> &ancilla,
             const std::optional<std::string> &name) -> Circuit * {
            std::vector<Qubit> qubits_;
            if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(
                    qubits)) {
              for (unsigned q :
                   std::get<nb::tket_custom::SequenceVec<unsigned>>(qubits))
                qubits_.push_back(Qubit(q));
            } else {
              qubits_ = std::get<nb::tket_custom::SequenceVec<Qubit>>(qubits);
            }
            std::optional<Qubit> ancilla_ = std::nullopt;
            if (ancilla) {
              if (std::holds_alternative<unsigned>(*ancilla)) {
                ancilla_ = Qubit(std::get<unsigned>(*ancilla));
              } else {
                ancilla_ = std::get<Qubit>(*ancilla);
              }
            }
            circ->add_assertion(box, qubits_, ancilla_, name);
            return circ;
          },
          "Append a :py:class:`~.ProjectorAssertionBox` to the circuit."
          "\n\n:param box: ProjectorAssertionBox to append"
          "\n:param qubits: target qubits"
          "\n:param ancilla: ancilla qubit"
          "\n:param name: name used to identify this assertion"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("qubits"), nb::arg("ancilla") = std::nullopt,
          nb::arg("name") = std::nullopt)
      .def(
          "add_assertion",
          [](Circuit *circ, const StabiliserAssertionBox &box,
             const var_seq_qbs_t &qubits,
             const std::variant<unsigned, Qubit> &ancilla,
             const std::optional<std::string> &name) -> Circuit * {
            std::vector<Qubit> qubits_;
            if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(
                    qubits)) {
              for (unsigned q :
                   std::get<nb::tket_custom::SequenceVec<unsigned>>(qubits))
                qubits_.push_back(Qubit(q));
            } else {
              qubits_ = std::get<nb::tket_custom::SequenceVec<Qubit>>(qubits);
            }
            Qubit ancilla_;
            if (std::holds_alternative<unsigned>(ancilla)) {
              ancilla_ = Qubit(std::get<unsigned>(ancilla));
            } else {
              ancilla_ = std::get<Qubit>(ancilla);
            }
            circ->add_assertion(box, qubits_, ancilla_, name);
            return circ;
          },
          "Append a :py:class:`~.StabiliserAssertionBox` to the circuit."
          "\n\n:param box: StabiliserAssertionBox to append"
          "\n:param qubits: target qubits"
          "\n:param ancilla: ancilla qubit"
          "\n:param name: name used to identify this assertion"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("qubits"), nb::arg("ancilla"),
          nb::arg("name") = std::nullopt)
      .def(
          "add_multiplexor",
          [](Circuit *circ, const MultiplexorBox &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<MultiplexorBox>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.MultiplexorBox` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_multiplexedrotation",
          [](Circuit *circ, const MultiplexedRotationBox &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<MultiplexedRotationBox>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.MultiplexedRotationBox` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_multiplexedu2",
          [](Circuit *circ, const MultiplexedU2Box &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<MultiplexedU2Box>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.MultiplexedU2Box` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_multiplexed_tensored_u2",
          [](Circuit *circ, const MultiplexedTensoredU2Box &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<MultiplexedTensoredU2Box>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.MultiplexedTensoredU2Box` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_state_preparation_box",
          [](Circuit *circ, const StatePreparationBox &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<StatePreparationBox>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.StatePreparationBox` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_diagonal_box",
          [](Circuit *circ, const DiagonalBox &box, const var_seq_qbs_t &args,
             const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<DiagonalBox>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.DiagonalBox` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_conjugation_box",
          [](Circuit *circ, const ConjugationBox &box,
             const var_seq_qbs_t &args, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, std::make_shared<ConjugationBox>(box),
                var_sequence_to_var_vecs(args), kwargs);
          },
          "Append a :py:class:`~.ConjugationBox` to the circuit.\n\n"
          ":param box: The box to append\n"
          ":param args: The qubits to append the box to"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("box"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "H", add_gate_method_q(OpType::H),
          "Appends a Hadamard gate."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "X", add_gate_method_q(OpType::X), "Appends an X gate.",
          "\n\n:return: the new :py:class:`~.Circuit`", nb::arg("qubit"),
          nb::arg("kwargs"))
      .def(
          "Y", add_gate_method_q(OpType::Y), "Appends a Y gate.",
          "\n\n:return: the new :py:class:`~.Circuit`", nb::arg("qubit"),
          nb::arg("kwargs"))
      .def(
          "Z", add_gate_method_q(OpType::Z), "Appends a Z gate.",
          "\n\n:return: the new :py:class:`~.Circuit`", nb::arg("qubit"),
          nb::arg("kwargs"))
      .def(
          "T", add_gate_method_q(OpType::T),
          "Appends a T gate (equivalent to U1(0.25,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Tdg", add_gate_method_q(OpType::Tdg),
          "Appends a T-dagger gate (equivalent to U1(-0.25,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "S", add_gate_method_q(OpType::S),
          "Appends an S gate (equivalent to U1(0.5,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Sdg", add_gate_method_q(OpType::Sdg),
          "Appends an S-dagger gate (equivalent to U1(-0.5,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "V", add_gate_method_q(OpType::V),
          "Appends a V gate (equivalent to Rx(0.5,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Vdg", add_gate_method_q(OpType::Vdg),
          "Appends a V-dagger gate (equivalent to Rx(-0.5,-))."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "SX", add_gate_method_q(OpType::SX),
          "Appends a SX gate (equivalent to Rx(0.5,-)"
          " up to a 0.25 global phase)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "SXdg", add_gate_method_q(OpType::SXdg),
          "Appends a SXdg gate (equivalent to Rx(-0.5,-)"
          " up to a -0.25 global phase)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Measure",
          [](Circuit *circ, const std::variant<unsigned, Qubit> &qb,
             std::variant<unsigned, Bit> &b, const nb::kwargs &kwargs) {
            Qubit qubit = std::holds_alternative<unsigned>(qb)
                              ? Qubit(std::get<unsigned>(qb))
                              : std::get<Qubit>(qb);
            Bit bit = std::holds_alternative<unsigned>(b)
                          ? Bit(std::get<unsigned>(b))
                          : std::get<Bit>(b);
            return add_gate_method_noparams_any(
                circ, OpType::Measure,
                nb::tket_custom::SequenceVec<UnitID>{qubit, bit}, kwargs);
          },
          "Appends a single-qubit measurement in the computational "
          "(Z) basis."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("bit"), nb::arg("kwargs"))
      .def(
          "Reset", add_gate_method_q(OpType::Reset),
          "Appends a Reset operation. Sets a qubit to the Z-basis 0 state. "
          "Non-unitary operation."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Rz", add_gate_method_a_q(OpType::Rz),
          "Appends an Rz gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Rx", add_gate_method_a_q(OpType::Rx),
          "Appends an Rx gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "Ry", add_gate_method_a_q(OpType::Ry),
          "Appends an Ry gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "U1", add_gate_method_a_q(OpType::U1),
          "Appends a U1 gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "U2", add_gate_method_aa_q(OpType::U2),
          "Appends a U2 gate with possibly symbolic angles "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit"),
          nb::arg("kwargs"))
      .def(
          "U3", add_gate_method_aaa_q(OpType::U3),
          "Appends a U3 gate with possibly symbolic angles "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("angle2"),
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "GPI", add_gate_method_a_q(OpType::GPI),
          "Appends a GPI gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "GPI2", add_gate_method_a_q(OpType::GPI2),
          "Appends a GPI2 gate with a possibly symbolic angle "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "AAMS", add_gate_method_aaa_qq(OpType::AAMS, "AAMS"),
          "Appends an AAMS gate with possibly symbolic angles "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("angle2"),
          nb::arg("qubit0"), nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "TK1", add_gate_method_aaa_q(OpType::TK1),
          "Appends a TK1 gate with possibly symbolic angles "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("angle2"),
          nb::arg("qubit"), nb::arg("kwargs"))
      .def(
          "TK2", add_gate_method_aaa_qq(OpType::TK2, "TK2"),
          "Appends a TK2 gate with possibly symbolic angles "
          "(specified in half-turns)."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("angle2"),
          nb::arg("qubit0"), nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "CX", add_gate_method_qq(OpType::CX, "CX"),
          "Appends a CX gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CY", add_gate_method_qq(OpType::CY, "CY"),
          "Appends a CY gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CZ", add_gate_method_qq(OpType::CZ, "CZ"),
          "Appends a CZ gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CH", add_gate_method_qq(OpType::CH, "CH"),
          "Appends a CH gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CV", add_gate_method_qq(OpType::CV, "CV"),
          "Appends a CV gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CVdg", add_gate_method_qq(OpType::CVdg, "CVdg"),
          "Appends a CVdg gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CSX", add_gate_method_qq(OpType::CSX, "CSX"),
          "Appends a CSX gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CSXdg", add_gate_method_qq(OpType::CSXdg, "CSXdg"),
          "Appends a CSXdg gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CS", add_gate_method_qq(OpType::CS, "CS"),
          "Appends a CS gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CSdg", add_gate_method_qq(OpType::CSdg, "CSdg"),
          "Appends a CSdg gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "CRz", add_gate_method_a_qq(OpType::CRz, "CRz"),
          "Appends a CRz gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified control and "
          "target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("control_qubit"), nb::arg("target_qubit"),
          nb::arg("kwargs"))
      .def(
          "CRx", add_gate_method_a_qq(OpType::CRx, "CRx"),
          "Appends a CRx gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified control and "
          "target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("control_qubit"), nb::arg("target_qubit"),
          nb::arg("kwargs"))
      .def(
          "CRy", add_gate_method_a_qq(OpType::CRy, "CRy"),
          "Appends a CRy gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified control and "
          "target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("control_qubit"), nb::arg("target_qubit"),
          nb::arg("kwargs"))
      .def(
          "CU1", add_gate_method_a_qq(OpType::CU1, "CU1"),
          "Appends a CU1 gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified control and "
          "target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("control_qubit"), nb::arg("target_qubit"),
          nb::arg("kwargs"))
      .def(
          "CU3", add_gate_method_aaa_qq(OpType::CU3, "CU3"),
          "Appends a CU3 gate with possibly symbolic angles (specified in "
          "half-turns) on the wires for the specified control and "
          "target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("angle2"),
          nb::arg("control_qubit"), nb::arg("target_qubit"), nb::arg("kwargs"))
      .def(
          "ZZPhase", add_gate_method_a_qq(OpType::ZZPhase, "ZZPhase"),
          "Appends a ZZ gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified two qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("kwargs"))
      .def(
          "ZZMax", add_gate_method_qq(OpType::ZZMax, "ZZMax"),
          "Appends a ZZMax gate on the wires for the specified two qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit0"), nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "ESWAP", add_gate_method_a_qq(OpType::ESWAP, "ESWAP"),
          "Appends an ESWAP gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified two qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("kwargs"))
      .def(
          "FSim", add_gate_method_aa_qq(OpType::FSim, "FSim"),
          "Appends an FSim gate with possibly symbolic angles (specified in "
          "half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit0"),
          nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "Sycamore", add_gate_method_qq(OpType::Sycamore, "Sycamore"),
          "Appends a Sycamore gate on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit0"), nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "XXPhase", add_gate_method_a_qq(OpType::XXPhase, "XXPhase"),
          "Appends a XX gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified two qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("kwargs"))
      .def(
          "YYPhase", add_gate_method_a_qq(OpType::YYPhase, "YYPhase"),
          "Appends a YY gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified two qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("kwargs"))
      .def(
          "XXPhase3", add_gate_method_a_qqq(OpType::XXPhase3, "XXPhase3"),
          "Appends a 3-qubit XX gate with a possibly symbolic angle (specified "
          "in "
          "half-turns) on the wires for the specified three qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("qubit2"), nb::arg("kwargs"))
      .def(
          "PhasedX", add_gate_method_aa_q(OpType::PhasedX),
          "Appends a PhasedX gate with possibly symbolic angles (specified in "
          "half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit"),
          nb::arg("kwargs"))
      .def(
          "CCX", add_gate_method_qqq(OpType::CCX, "CCX"),
          "Appends a CCX gate on the wires for the specified control "
          "and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control_0"), nb::arg("control_1"), nb::arg("target"),
          nb::arg("kwargs"))
      .def(
          "ECR", add_gate_method_qq(OpType::ECR, "ECR"),
          "Appends an ECR gate on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit_0"), nb::arg("qubit_1"), nb::arg("kwargs"))
      .def(
          "SWAP", add_gate_method_qq(OpType::SWAP, "SWAP"),
          "Appends a SWAP gate on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit_0"), nb::arg("qubit_1"), nb::arg("kwargs"))
      .def(
          "CSWAP", add_gate_method_qqq(OpType::CSWAP, "CSWAP"),
          "Appends a CSWAP gate on the wires for the specified "
          "control and target qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("control"), nb::arg("target_0"), nb::arg("target_1"),
          nb::arg("kwargs"))
      .def(
          "ISWAP", add_gate_method_a_qq(OpType::ISWAP, "ISWAP"),
          "Appends an ISWAP gate with a possibly symbolic angle (specified in "
          "half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle"), nb::arg("qubit0"), nb::arg("qubit1"),
          nb::arg("kwargs"))
      .def(
          "ISWAPMax", add_gate_method_qq(OpType::ISWAPMax, "ISWAPMax"),
          "Appends an ISWAPMax gate on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("qubit0"), nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "PhasedISWAP",
          add_gate_method_aa_qq(OpType::PhasedISWAP, "PhasedISWAP"),
          "Appends a PhasedISWAP gate with possibly symbolic angles (specified "
          "in "
          "half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit0"),
          nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "TwinPhasedX",
          add_gate_method_aa_qq(OpType::TwinPhasedX, "TwinPhasedX"),
          "Appends a TwinPhasedX gate with possibly symbolic angles "
          "(specified in half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit0"),
          nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "PhasedXX", add_gate_method_aa_qq(OpType::PhasedXX, "PhasedXX"),
          "Appends a PhasedXX gate with possibly symbolic angles (specified in "
          "half-turns) on the wires for the specified qubits."
          "\n\n:return: the new :py:class:`~.Circuit`",
          nb::arg("angle0"), nb::arg("angle1"), nb::arg("qubit0"),
          nb::arg("qubit1"), nb::arg("kwargs"))
      .def(
          "measure_all",
          [](Circuit *circ) {
            opt_reg_info_t creg_info = circ->get_reg_info("c");
            register_info_t default_info = {UnitType::Bit, 1};
            if (creg_info && creg_info.value() != default_info)
              throw CircuitInvalidity(
                  "Cannot measure all; default classical "
                  "register name is already in use");
            qubit_vector_t qbs = circ->all_qubits();
            for (unsigned i = 0; i < qbs.size(); i++) {
              Bit id(i);
              circ->add_bit(id, false);
              circ->add_measure(qbs[i], id);
            }
            return circ;
          },
          "Appends a measure gate to all qubits, storing the results "
          "in the default classical register. Bits are added to the "
          "circuit if they do not already exist."
          "\n\n:return: the new :py:class:`~.Circuit`")
      .def(
          "measure_register",
          [](Circuit *circ, const QubitRegister &qreg,
             const std::string &creg_name) {
            if (!circ->get_reg_info(qreg.name())) {
              throw CircuitInvalidity(
                  "The given QubitRegister is not in use, please use "
                  "add_q_register to add it to the circuit first.");
            }
            opt_reg_info_t creg_info = circ->get_reg_info(creg_name);
            if (creg_info == std::nullopt) {
              circ->add_c_register(creg_name, qreg.size());
            } else if (circ->get_reg(creg_name).size() != qreg.size()) {
              throw CircuitInvalidity(
                  "The given classical register already exists, "
                  "but its size doesn't match the given QubitRegister.");
            }
            for (unsigned i = 0; i < qreg.size(); i++) {
              circ->add_measure(qreg[i], Bit(creg_name, i));
            }
            return circ;
          },
          "Appends a measure gate to all qubits in the given register, storing "
          "the results in the given classical register with matching indices."
          "The classical register will be created if it doesn't exist."
          "\n\n:param qreg: the QubitRegister to be measured"
          "\n:param creg_name: the name of the BitRegister to store the results"
          "\n:return: the new :py:class:`~.Circuit`")
      .def(
          "Phase",
          [](Circuit *circ, const Expr &angle, const nb::kwargs &kwargs) {
            return add_gate_method_any(
                circ, get_op_ptr(OpType::Phase, angle, 0),
                std::vector<UnitID>{}, kwargs);
          });
}

}  // namespace tket
