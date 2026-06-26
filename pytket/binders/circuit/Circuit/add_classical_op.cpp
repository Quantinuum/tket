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

#define STR(x) #x

// This must be the first include since it includes Python.h.
// https://docs.python.org/3/c-api/intro.html#include-files
// clang-format off
#include <nanobind/nanobind.h>
// clang-format on

#include <bitset>
#include <string>
#include <utility>
#include <vector>

#include "UnitRegister.hpp"
#include "add_gate.hpp"
#include "nanobind-stl.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "typecast.hpp"

namespace nb = nanobind;

namespace tket {

typedef std::variant<
    nb::tket_custom::SequenceVec<unsigned>, nb::tket_custom::SequenceVec<Bit>>
    var_seq_bs_t;

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_sequence_to_var_vecs(const var_seq_bs_t &var_seq) {
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(var_seq)) {
    return std::get<nb::tket_custom::SequenceVec<unsigned>>(var_seq);
  } else {
    auto b_vec = std::get<nb::tket_custom::SequenceVec<Bit>>(var_seq);
    return std::vector<UnitID>{b_vec.begin(), b_vec.end()};
  }
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_bs_b_to_var_vec(
    const var_seq_bs_t &reg, const std::variant<unsigned, Bit> &b,
    const std::string &method_name) {
  bool is_unsigned =
      std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(reg);
  if (std::holds_alternative<unsigned>(b) != is_unsigned) {
    throw CircuitInvalidity(
        "Bits passed to `" + method_name +
        "` must either all be `int` or all `Bit`.");
  }
  if (is_unsigned) {
    std::vector<unsigned> bs =
        std::get<nb::tket_custom::SequenceVec<unsigned>>(reg);
    bs.push_back(std::get<unsigned>(b));
    return bs;
  } else {
    std::vector<UnitID> out;
    for (const Bit &b : std::get<nb::tket_custom::SequenceVec<Bit>>(reg))
      out.push_back(b);
    out.push_back(std::get<Bit>(b));
    return out;
  }
}

static std::variant<std::vector<unsigned>, std::vector<UnitID>>
var_bs_bs_to_var_vec(
    const var_seq_bs_t &reg0, const var_seq_bs_t &reg1,
    const std::string &method_name) {
  bool is_unsigned =
      std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(reg0);
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(reg1) !=
      is_unsigned) {
    throw CircuitInvalidity(
        "Bits passed to `" + method_name +
        "` must either all be `int` or all `Bit`.");
  }
  if (is_unsigned) {
    std::vector<unsigned> bs0 =
        std::get<nb::tket_custom::SequenceVec<unsigned>>(reg0);
    std::vector<unsigned> bs1 =
        std::get<nb::tket_custom::SequenceVec<unsigned>>(reg1);
    bs0.insert(bs0.end(), bs1.begin(), bs1.end());
    return bs0;
  } else {
    std::vector<UnitID> out;
    for (const Bit &b : std::get<nb::tket_custom::SequenceVec<Bit>>(reg0))
      out.push_back(b);
    for (const Bit &b : std::get<nb::tket_custom::SequenceVec<Bit>>(reg1))
      out.push_back(b);
    return out;
  }
}

static unsigned var_seq_len(const var_seq_bs_t &var_seq) {
  if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(var_seq)) {
    return std::get<nb::tket_custom::SequenceVec<unsigned>>(var_seq).size();
  } else {
    return std::get<nb::tket_custom::SequenceVec<Bit>>(var_seq).size();
  }
}

static void apply_classical_op_to_registers(
    Circuit &circ, const std::shared_ptr<const ClassicalEvalOp> &op,
    const std::vector<BitRegister> &registers, const nb::kwargs &kwargs) {
  unsigned n_op_args = registers.size();
  const unsigned n_bits = std::min_element(
                              registers.begin(), registers.end(),
                              [](const BitRegister &i, const BitRegister &j) {
                                return i.size() < j.size();
                              })
                              ->size();
  std::vector<UnitID> args(n_bits * n_op_args);
  for (unsigned i = 0; i < n_bits; i++) {
    for (unsigned j = 0; j < n_op_args; j++) {
      args[n_op_args * i + j] = registers[j][i];
    }
  }
  std::shared_ptr<MultiBitOp> mbop = std::make_shared<MultiBitOp>(op, n_bits);
  add_gate_method_any(&circ, mbop, args, kwargs);
}

void init_circuit_add_classical_op(nb::class_<Circuit> &c) {
  c.def(
       "add_c_transform",
       [](Circuit &circ, const nb::tket_custom::SequenceVec<_tket_uint_t> &values,
          const var_seq_bs_t &args, const std::string &name,
          const nb::kwargs &kwargs) {
         unsigned n_args = var_seq_len(args);
         std::shared_ptr<ClassicalTransformOp> op =
             std::make_shared<ClassicalTransformOp>(n_args, values, name);
         return add_gate_method_any(&circ, op, var_sequence_to_var_vecs(args), kwargs);
       },
       "Appends a purely classical transformation, defined by a table of "
       "values, to "
       "the end of the circuit."
       "\n\n:param values: table of values: bit :math:`j` (in little-endian "
       "order) of the "
       "term indexed by :math:`sum_i a_i 2^i` is output :math:`j` of the "
       "transform "
       "applied to inputs :math:`(a_i)`."
       "\n:param args: bits to which the transform is applied"
       "\n:param name: operation name"
       "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
       " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
       " `condition_value`"
       "\n:return: the new :py:class:`~.Circuit`",
       nb::arg("values"), nb::arg("args"),
       nb::arg("name") = "ClassicalTransform", nb::arg("kwargs"))
      .def(
          "_add_wasm",
          [](Circuit &circ, const std::string &funcname,
             const std::string &wasm_uid,
             const nb::tket_custom::SequenceVec<unsigned> &width_i_parameter,
             const nb::tket_custom::SequenceVec<unsigned> &width_o_parameter,
             const var_seq_bs_t &args,
             const nb::tket_custom::SequenceVec<unsigned> &wasm_wire_args,
             const nb::kwargs &kwargs) -> Circuit * {
            std::vector<UnitID> new_args;
            if (std::holds_alternative<nb::tket_custom::SequenceVec<unsigned>>(args)) {
              for (unsigned i : std::get<nb::tket_custom::SequenceVec<unsigned>>(args)) {
                new_args.push_back(Bit(i));
              }
            } else {
              for (const Bit &b : std::get<nb::tket_custom::SequenceVec<Bit>>(args)) {
                new_args.push_back(b);
              }
            }
            unsigned n_args = new_args.size();
            for (auto i : wasm_wire_args) {
              new_args.push_back(WasmState(i));
            }
            unsigned ww_n = wasm_wire_args.size();
            std::shared_ptr<WASMOp> op = std::make_shared<WASMOp>(
                n_args, ww_n, width_i_parameter, width_o_parameter, funcname, wasm_uid);
            return add_gate_method_any(&circ, op, new_args, kwargs);
          },
          "Add a classical function call from a wasm file to the circuit. "
          "\n\n:param funcname: name of the function that is called"
          "\n:param wasm_uid: unit id to identify the wasm file"
          "\n:param width_i_parameter: list of the number of bits in the input "
          "variables"
          "\n:param width_o_parameter: list of the number of bits in the output "
          "variables"
          "\n:param args: vector of circuit bits the wasm op should be added to"
          "\n:param wasm_wire_args: vector of circuit wasmwires the wasm op should be added to"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("funcname"), nb::arg("wasm_uid"), nb::arg("width_i_parameter"),
          nb::arg("width_o_parameter"), nb::arg("args"), nb::arg("wasm_wire_args"), nb::arg("kwargs"))
      .def(
          "_add_wasm",
          [](Circuit &circ, const std::string &funcname,
             const std::string &wasm_uid,
             const nb::tket_custom::SequenceVec<BitRegister> &list_reg_in,
             const nb::tket_custom::SequenceVec<BitRegister> &list_reg_out,
             const nb::tket_custom::SequenceVec<unsigned> &wasm_wire_args,
             const nb::kwargs &kwargs) -> Circuit * {
            unsigned n_args = 0;

            unsigned ww_n = wasm_wire_args.size();

            for (const auto& r : list_reg_in) {
              n_args += r.size();
            }

            for (const auto& r : list_reg_out) {
              n_args += r.size();
            }

            std::vector<Bit> args(n_args);
            std::vector<unsigned> width_i_parameter(list_reg_in.size());
            std::vector<unsigned> width_o_parameter(list_reg_out.size());

            unsigned i = 0;
            unsigned j = 0;
            for (const auto& r : list_reg_in) {
              width_i_parameter[i] = r.size();
              for (unsigned k = 0; k < r.size(); ++k) {
                args[j] = r[k];
                ++j;
              }
              ++i;
            }

            i = 0;
            for (const auto& r : list_reg_out) {
              width_o_parameter[i] = r.size();
              for (unsigned k = 0; k < r.size(); ++k) {
                args[j] = r[k];
                ++j;
              }
              ++i;
            }

            std::shared_ptr<WASMOp> op = std::make_shared<WASMOp>(
                n_args, ww_n, width_i_parameter, width_o_parameter, funcname, wasm_uid);

            std::vector<UnitID> new_args;

            new_args.reserve(args.size());
            for (const auto& b : args) {
              new_args.push_back(b);
            }

            for (auto ii : wasm_wire_args) {
              new_args.push_back(WasmState(ii));
            }

            return add_gate_method_any(&circ, op, new_args, kwargs);
          },
          "Add a classical function call from a wasm file to the circuit. "
          "\n\n:param funcname: name of the function that is called"
          "\n:param wasm_uid: unit id to identify the wasm file"
          "\n:param list_reg_in: list of the classical registers in the "
          "circuit used as inputs"
          "\n:param list_reg_out: list of the classical registers in the "
          "circuit used as outputs"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("funcname"), nb::arg("wasm_uid"), nb::arg("list_reg_in"),
          nb::arg("list_reg_out"), nb::arg("wasm_wire_args"), nb::arg("kwargs"))
      .def("_set_rng_seed",
           [](Circuit &circ, BitRegister &creg, unsigned rng_wire,
            const nb::kwargs &kwargs) {
            std::shared_ptr<OpaqueClassicalOp> op = std::make_shared<OpaqueClassicalOp>(OpType::RNGSeed);
            std::vector<UnitID> args;
            for (const Bit& arg : creg.to_vector()) {
              args.push_back(arg);
            }
            args.push_back(RngState(rng_wire));
            return add_gate_method_any(&circ, op, args, kwargs);
           },
           "Seed an RNG from the contents of a classical register."
           "\n\nThe classical register must be exactly 64 bits."
           "\n\n:param creg: register of classical bits"
           "\n:param rng_wire: index of RNG wire"
           "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` "
           "(allowed parameters are `opgroup`,  `condition`, `condition_bits` "
           "and `condition_value`)"
           "\n:return: the new :py:class:`~.Circuit`",
           nb::arg("creg"), nb::arg("rng_wire"), nb::arg("kwargs"))
      .def("_set_rng_bound",
           [](Circuit &circ, BitRegister &creg, unsigned rng_wire,
            const nb::kwargs &kwargs) {
            std::shared_ptr<OpaqueClassicalOp> op = std::make_shared<OpaqueClassicalOp>(OpType::RNGBound);
            std::vector<UnitID> args;
            for (const Bit& arg : creg.to_vector()) {
              args.push_back(arg);
            }
            args.push_back(RngState(rng_wire));
            return add_gate_method_any(&circ, op, args, kwargs);
           },
           "Set an RNG upper bound from the contents of a classical register."
           "\n\nThe classical register must be exactly 32 bits. It encodes the "
           "upper bound in little-endian binary (least significant bit first). "
           "The bound is inclusive."
           "\n\n:param creg: register of classical bits"
           "\n:param rng_wire: index of RNG wire"
           "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` "
           "(allowed parameters are `opgroup`,  `condition`, `condition_bits` "
           "and `condition_value`)"
           "\n:return: the new :py:class:`~.Circuit`",
           nb::arg("creg"), nb::arg("rng_wire"), nb::arg("kwargs"))
      .def("_set_rng_index",
           [](Circuit &circ, BitRegister &creg, unsigned rng_wire,
            const nb::kwargs &kwargs) {
            std::shared_ptr<OpaqueClassicalOp> op = std::make_shared<OpaqueClassicalOp>(OpType::RNGIndex);
            std::vector<UnitID> args;
            for (const Bit& arg : creg.to_vector()) {
              args.push_back(arg);
            }
            args.push_back(RngState(rng_wire));
            return add_gate_method_any(&circ, op, args, kwargs);
           },
           "Set an RNG stream index from the contents of a classical register."
           "\n\nThe classical register must be exactly 32 bits. It encodes the "
           "index in little-endian binary (least significant bit first)."
           "\n\n:param creg: register of classical bits"
           "\n:param rng_wire: index of RNG wire"
           "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` "
           "(allowed parameters are `opgroup`,  `condition`, `condition_bits` "
           "and `condition_value`)"
           "\n:return: the new :py:class:`~.Circuit`",
           nb::arg("creg"), nb::arg("rng_wire"), nb::arg("kwargs"))
      .def("_get_rng_num",
           [](Circuit &circ, BitRegister &creg, unsigned rng_wire,
            const nb::kwargs &kwargs) {
            std::shared_ptr<OpaqueClassicalOp> op = std::make_shared<OpaqueClassicalOp>(OpType::RNGNum);
            std::vector<UnitID> args;
            for (const Bit& arg : creg.to_vector()) {
              args.push_back(arg);
            }
            args.push_back(RngState(rng_wire));
            return add_gate_method_any(&circ, op, args, kwargs);
           },
           "Get RNG output into a classical register."
           "\n\nThe classical register must be exactly 32 bits. After the "
           "operation it encodes the output number in little-endian binary "
           "(least significant bit first)."
           "\n\n:param creg: register of classical bits"
           "\n:param rng_wire: index of RNG wire"
           "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` "
           "(allowed parameters are `opgroup`,  `condition`, `condition_bits` "
           "and `condition_value`)"
           "\n:return: the new :py:class:`~.Circuit`",
           nb::arg("creg"), nb::arg("rng_wire"), nb::arg("kwargs"))
      .def("_get_job_shot_num",
          [](Circuit &circ, BitRegister &creg, const nb::kwargs &kwargs) {
            std::shared_ptr<OpaqueClassicalOp> op = std::make_shared<OpaqueClassicalOp>(OpType::JobShotNum);
            std::vector<UnitID> args;
            for (const Bit& arg : creg.to_vector()) {
              args.push_back(arg);
            }
            return add_gate_method_any(&circ, op, args, kwargs);
           },
           "Get shot number into a classical register."
           "\n\nThe classical register must be exactly 32 bits. After the "
           "operation it encodes the shot number in little-endian binary "
           "(least significant bit first)."
           "\n\n:param creg: register of classical bits"
           "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` "
           "(allowed parameters are `opgroup`,  `condition`, `condition_bits` "
           "and `condition_value`)"
           "\n:return: the new :py:class:`~.Circuit`",
           nb::arg("creg"), nb::arg("kwargs"))
      .def(
          "add_c_setbits",
          [](Circuit &circ, const nb::tket_custom::SequenceVec<bool> &values,
             const var_seq_bs_t& args, const nb::kwargs &kwargs) {
            std::shared_ptr<SetBitsOp> op = std::make_shared<SetBitsOp>(values);
            return add_gate_method_any(&circ, op, var_sequence_to_var_vecs(args), kwargs);
          },
          "Appends an operation to set some bit values."
          "\n\n:param values: values to set"
          "\n:param args: bits to set"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("values"), nb::arg("args"), nb::arg("kwargs"))
      .def(
          "add_c_setreg",
          [](Circuit &circ, const _tket_uint_t value, const BitRegister &reg,
             const nb::kwargs &kwargs) {
            if (reg.size() < _TKET_REG_WIDTH && value >> reg.size() != 0) {
              throw std::runtime_error("Value " + std::to_string(value) + " cannot be held on a " + std::to_string(reg.size()) + "-bit register.");
            }
            auto bs = std::bitset<_TKET_REG_WIDTH>(value);
            std::vector<UnitID> args(reg.size());
            std::vector<bool> vals(reg.size());
            for (unsigned i = 0; i < reg.size(); i++) {
              args[i] = reg[i];
              vals[i] = (i < _TKET_REG_WIDTH) && bs[i];
            }

            std::shared_ptr<SetBitsOp> op = std::make_shared<SetBitsOp>(vals);
            return add_gate_method_any(&circ, op, args, kwargs);
          },
          "Set a classical register to an unsigned integer value. The "
          "little-endian bitwise representation of the integer is truncated to "
          "the register size, up to " STR(_TKET_REG_WIDTH) " bit width. It is "
          "zero-padded if the width of the register is greater than "
          STR(_TKET_REG_WIDTH) ".",
          nb::arg("value"), nb::arg("arg"), nb::arg("kwargs"))
      .def(
          "add_c_copybits",
          [](Circuit &circ, const var_seq_bs_t &args_in,
             const var_seq_bs_t &args_out, const nb::kwargs &kwargs) {
            unsigned n_args_in = var_seq_len(args_in);
            std::shared_ptr<CopyBitsOp> op =
                std::make_shared<CopyBitsOp>(n_args_in);
            return add_gate_method_any(&circ, op, var_bs_bs_to_var_vec(args_in, args_out, "add_c_copybits"), kwargs);
          },
          "Appends a classical copy operation"
          "\n\n:param args_in: source bits"
          "\n:param args_out: destination bits"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("args_in"), nb::arg("args_out"), nb::arg("kwargs"))
      .def(
          "add_c_copyreg",
          [](Circuit &circ, const BitRegister &input_reg,
             const BitRegister &output_reg, const nb::kwargs &kwargs) {
            const unsigned width =
                std::min(input_reg.size(), output_reg.size());

            std::shared_ptr<CopyBitsOp> op =
                std::make_shared<CopyBitsOp>(width);
            std::vector<UnitID> args(width * 2);
            for (unsigned i = 0; i < width; i++) {
              args[i] = input_reg[i];
              args[i + width] = output_reg[i];
            }
            return add_gate_method_any(&circ, op, args, kwargs);
          },
          "Copy a classical register to another. Copying is truncated to the "
          "size of the smaller of the two registers.",
          nb::arg("input_reg"), nb::arg("output_reg"), nb::arg("kwargs"))
      .def(
          "add_c_predicate",
          [](Circuit &circ, const nb::tket_custom::SequenceVec<bool> &values,
             const var_seq_bs_t &args_in, const std::variant<unsigned, Bit> &arg_out,
             const std::string &name, const nb::kwargs &kwargs) {
            unsigned n_args_in = var_seq_len(args_in);
            std::shared_ptr<ExplicitPredicateOp> op =
                std::make_shared<ExplicitPredicateOp>(n_args_in, values, name);
            return add_gate_method_any(&circ, op, var_bs_b_to_var_vec(args_in, arg_out, "add_c_predicate"), kwargs);
          },
          "Appends a classical predicate, defined by a truth table, to the end "
          "of the "
          "circuit."
          "\n\n:param values: table of values: the term indexed by "
          ":math:`sum_i a_i 2^i` "
          "is the value of the predicate for inputs :math:`(a_i)`."
          "\n:param args_in: input bits for the predicate"
          "\n:param arg_out: output bit, distinct from all inputs",
          "\n:param name: operation name"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("values"), nb::arg("args_in"), nb::arg("arg_out"),
          nb::arg("name") = "ExplicitPredicate", nb::arg("kwargs"))
      .def(
          "add_c_modifier",
          [](Circuit &circ, const nb::tket_custom::SequenceVec<bool> &values,
             const var_seq_bs_t &args_in, const std::variant<unsigned, Bit> &arg_inout,
             const std::string &name, const nb::kwargs &kwargs) {
            unsigned n_args_in = var_seq_len(args_in);
            std::shared_ptr<ExplicitModifierOp> op =
                std::make_shared<ExplicitModifierOp>(n_args_in, values, name);
            return add_gate_method_any(&circ, op, var_bs_b_to_var_vec(args_in, arg_inout, "add_c_modifier"), kwargs);
          },
          "Appends a classical modifying operation, defined by a truth table, "
          "to the "
          "end of the circuit."
          "\n\n:param values: table of values: the term indexed by "
          ":math:`sum_i a_i 2^i` "
          "is the value of the predicate for inputs :math:`(a_i)`, where the "
          "modified "
          "bit is the last of the :math:`a_i`."
          "\n:param args_in: input bits, excluding the modified bit"
          "\n:param arg_out: modified bit",
          "\n:param name: operation name"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("values"), nb::arg("args_in"), nb::arg("arg_inout"),
          nb::arg("name") = "ExplicitModifier", nb::arg("kwargs"))
      .def(
          "add_c_and",
          [](Circuit &circ, const std::variant<unsigned, Bit> &arg0_in, const std::variant<unsigned, Bit> &arg1_in,
             const std::variant<unsigned, Bit> &arg_out, const nb::kwargs &kwargs) {
            bool is_unsigned = std::holds_alternative<unsigned>(arg0_in);
            if (std::holds_alternative<unsigned>(arg1_in) != is_unsigned || std::holds_alternative<unsigned>(arg_out) != is_unsigned) {
              throw CircuitInvalidity("Bits passed to `add_c_and` must either all be `int` or all `Bit`.");
            }
            if (is_unsigned) {
              unsigned uin0 = std::get<unsigned>(arg0_in);
              unsigned uin1 = std::get<unsigned>(arg1_in);
              unsigned uout = std::get<unsigned>(arg_out);
              if (uin0 == uout) return add_gate_method_any(&circ, AndWithOp(), std::vector<unsigned>{uin1, uout}, kwargs);
              else if (uin1 == uout) return add_gate_method_any(&circ, AndWithOp(), std::vector<unsigned>{uin0, uout}, kwargs);
              else return add_gate_method_any(&circ, AndOp(), std::vector<unsigned>{uin0, uin1, uout}, kwargs);
            }
            else {
              Bit bin0 = std::get<Bit>(arg0_in);
              Bit bin1 = std::get<Bit>(arg1_in);
              Bit bout = std::get<Bit>(arg_out);
              if (bin0 == bout) return add_gate_method_any(&circ, AndWithOp(), std::vector<UnitID>{bin1, bout}, kwargs);
              else if (bin1 == bout) return add_gate_method_any(&circ, AndWithOp(), std::vector<UnitID>{bin0, bout}, kwargs);
              else return add_gate_method_any(&circ, AndOp(), std::vector<UnitID>{bin0, bin1, bout}, kwargs);
            }
          },
          "Appends a binary AND operation to the end of the circuit."
          "\n\n:param arg0_in: first input bit"
          "\n:param arg1_in: second input bit"
          "\n:param arg_out: output bit"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("arg0_in"), nb::arg("arg1_in"), nb::arg("arg_out"), nb::arg("kwargs"))
          .def(
            "add_c_or",
            [](Circuit &circ, const std::variant<unsigned, Bit> &arg0_in, const std::variant<unsigned, Bit> &arg1_in,
               const std::variant<unsigned, Bit> &arg_out, const nb::kwargs &kwargs) {
              bool is_unsigned = std::holds_alternative<unsigned>(arg0_in);
              if (std::holds_alternative<unsigned>(arg1_in) != is_unsigned || std::holds_alternative<unsigned>(arg_out) != is_unsigned) {
                throw CircuitInvalidity("Bits passed to `add_c_or` must either all be `int` or all `Bit`.");
              }
              if (is_unsigned) {
                unsigned uin0 = std::get<unsigned>(arg0_in);
                unsigned uin1 = std::get<unsigned>(arg1_in);
                unsigned uout = std::get<unsigned>(arg_out);
                if (uin0 == uout) return add_gate_method_any(&circ, OrWithOp(), std::vector<unsigned>{uin1, uout}, kwargs);
                else if (uin1 == uout) return add_gate_method_any(&circ, OrWithOp(), std::vector<unsigned>{uin0, uout}, kwargs);
                else return add_gate_method_any(&circ, OrOp(), std::vector<unsigned>{uin0, uin1, uout}, kwargs);
              }
              else {
                Bit bin0 = std::get<Bit>(arg0_in);
                Bit bin1 = std::get<Bit>(arg1_in);
                Bit bout = std::get<Bit>(arg_out);
                if (bin0 == bout) return add_gate_method_any(&circ, OrWithOp(), std::vector<UnitID>{bin1, bout}, kwargs);
                else if (bin1 == bout) return add_gate_method_any(&circ, OrWithOp(), std::vector<UnitID>{bin0, bout}, kwargs);
                else return add_gate_method_any(&circ, OrOp(), std::vector<UnitID>{bin0, bin1, bout}, kwargs);
              }
            },
          "Appends a binary OR operation to the end of the circuit."
          "\n\n:param arg0_in: first input bit"
          "\n:param arg1_in: second input bit"
          "\n:param arg_out: output bit"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("arg0_in"), nb::arg("arg1_in"), nb::arg("arg_out"), nb::arg("kwargs"))
      .def(
          "add_c_xor",
          [](Circuit &circ, const std::variant<unsigned, Bit> &arg0_in, const std::variant<unsigned, Bit> &arg1_in,
             const std::variant<unsigned, Bit> &arg_out, const nb::kwargs &kwargs) {
            bool is_unsigned = std::holds_alternative<unsigned>(arg0_in);
            if (std::holds_alternative<unsigned>(arg1_in) != is_unsigned || std::holds_alternative<unsigned>(arg_out) != is_unsigned) {
              throw CircuitInvalidity("Bits passed to `add_c_xor` must either all be `int` or all `Bit`.");
            }
            if (is_unsigned) {
              unsigned uin0 = std::get<unsigned>(arg0_in);
              unsigned uin1 = std::get<unsigned>(arg1_in);
              unsigned uout = std::get<unsigned>(arg_out);
              if (uin0 == uout) return add_gate_method_any(&circ, XorWithOp(), std::vector<unsigned>{uin1, uout}, kwargs);
              else if (uin1 == uout) return add_gate_method_any(&circ, XorWithOp(), std::vector<unsigned>{uin0, uout}, kwargs);
              else return add_gate_method_any(&circ, XorOp(), std::vector<unsigned>{uin0, uin1, uout}, kwargs);
            }
            else {
              Bit bin0 = std::get<Bit>(arg0_in);
              Bit bin1 = std::get<Bit>(arg1_in);
              Bit bout = std::get<Bit>(arg_out);
              if (bin0 == bout) return add_gate_method_any(&circ, XorWithOp(), std::vector<UnitID>{bin1, bout}, kwargs);
              else if (bin1 == bout) return add_gate_method_any(&circ, XorWithOp(), std::vector<UnitID>{bin0, bout}, kwargs);
              else return add_gate_method_any(&circ, XorOp(), std::vector<UnitID>{bin0, bin1, bout}, kwargs);
            }
          },
          "Appends a binary XOR operation to the end of the circuit."
          "\n\n:param arg0_in: first input bit"
          "\n:param arg1_in: second input bit"
          "\n:param arg_out: output bit"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("arg0_in"), nb::arg("arg1_in"), nb::arg("arg_out"), nb::arg("kwargs"))
      .def(
          "add_c_not",
          [](Circuit &circ, const std::variant<unsigned, Bit> &arg_in, const std::variant<unsigned, Bit> &arg_out,
             const nb::kwargs &kwargs) {
            bool is_unsigned = std::holds_alternative<unsigned>(arg_in);
            if (std::holds_alternative<unsigned>(arg_out) != is_unsigned) {
              throw CircuitInvalidity("Bits passed to `add_c_not` must either all be `int` or all `Bit`.");
            }
            if (is_unsigned) {
              unsigned uin = std::get<unsigned>(arg_in);
              unsigned uout = std::get<unsigned>(arg_out);
              return add_gate_method_any(&circ, NotOp(), std::vector<unsigned>{uin, uout}, kwargs);
            }
            else {
              Bit bin = std::get<Bit>(arg_in);
              Bit bout = std::get<Bit>(arg_out);
              return add_gate_method_any(&circ, NotOp(), std::vector<UnitID>{bin, bout}, kwargs);
            }
          },
          "Appends a NOT operation to the end of the circuit."
          "\n\n:param arg_in: input bit"
          "\n:param arg_out: output bit"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("arg_in"), nb::arg("arg_out"), nb::arg("kwargs"))
      .def(
          "add_c_range_predicate",
          [](Circuit &circ, _tket_uint_t a, _tket_uint_t b,
             const var_seq_bs_t &args_in, const std::variant<unsigned, Bit> &arg_out,
             const nb::kwargs &kwargs) {
            unsigned n_args_in = var_seq_len(args_in);
            std::shared_ptr<RangePredicateOp> op =
                std::make_shared<RangePredicateOp>(n_args_in, a, b);
            return add_gate_method_any(&circ, op, var_bs_b_to_var_vec(args_in, arg_out, "add_c_range_predicate"), kwargs);
          },
          "Appends a range-predicate operation to the end of the circuit."
          "\n\n:param minval: lower bound of input in little-endian encoding"
          "\n:param maxval: upper bound of input in little-endian encoding"
          "\n:param args_in: input bits"
          "\n:param arg_out: output bit (distinct from input bits)"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("minval"), nb::arg("maxval"), nb::arg("args_in"),
          nb::arg("arg_out"), nb::arg("kwargs"))
      .def(
          "add_c_and_to_registers",
          [](Circuit &circ, const BitRegister &reg0_in,
             const BitRegister &reg1_in, const BitRegister &reg_out,
             const nb::kwargs &kwargs) {
            if (reg0_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, AndWithOp(), {reg1_in, reg_out}, kwargs);
            } else if (reg1_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, AndWithOp(), {reg0_in, reg_out}, kwargs);
            } else {
              apply_classical_op_to_registers(
                  circ, AndOp(), {reg0_in, reg1_in, reg_out}, kwargs);
            }
            return circ;
          },
          "Applies bitwise AND to linear registers."
          "\n\nThe operation is applied to the bits with indices 0, 1, 2, ... "
          "in "
          "each register, up to the size of the smallest register."
          "\n\n:param reg0_in: first input register"
          "\n:param reg1_in: second input register"
          "\n:param reg_out: output register"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("reg0_in"), nb::arg("reg1_in"), nb::arg("reg_out"), nb::arg("kwargs"))
      .def(
          "add_c_or_to_registers",
          [](Circuit &circ, const BitRegister &reg0_in,
             const BitRegister &reg1_in, const BitRegister &reg_out,
             const nb::kwargs &kwargs) {
            if (reg0_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, OrWithOp(), {reg1_in, reg_out}, kwargs);
            } else if (reg1_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, OrWithOp(), {reg0_in, reg_out}, kwargs);
            } else {
              apply_classical_op_to_registers(
                  circ, OrOp(), {reg0_in, reg1_in, reg_out}, kwargs);
            }
            return circ;
          },
          "Applies bitwise OR to linear registers."
          "\n\nThe operation is applied to the bits with indices 0, 1, 2, ... "
          "in "
          "each register, up to the size of the smallest register."
          "\n\n:param reg0_in: first input register"
          "\n:param reg1_in: second input register"
          "\n:param reg_out: output register"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("reg0_in"), nb::arg("reg1_in"), nb::arg("reg_out"), nb::arg("kwargs"))
      .def(
          "add_c_xor_to_registers",
          [](Circuit &circ, const BitRegister &reg0_in,
             const BitRegister &reg1_in, const BitRegister &reg_out,
             const nb::kwargs &kwargs) {
            if (reg0_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, XorWithOp(), {reg1_in, reg_out}, kwargs);
            } else if (reg1_in == reg_out) {
              apply_classical_op_to_registers(
                  circ, XorWithOp(), {reg0_in, reg_out}, kwargs);
            } else {
              apply_classical_op_to_registers(
                  circ, XorOp(), {reg0_in, reg1_in, reg_out}, kwargs);
            }
            return circ;
          },
          "Applies bitwise XOR to linear registers."
          "\n\nThe operation is applied to the bits with indices 0, 1, 2, ... "
          "in "
          "each register, up to the size of the smallest register."
          "\n\n:param reg0_in: first input register"
          "\n:param reg1_in: second input register"
          "\n:param reg_out: output register"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("reg0_in"), nb::arg("reg1_in"), nb::arg("reg_out"), nb::arg("kwargs"))
      .def(
          "add_c_not_to_registers",
          [](Circuit &circ, const BitRegister &reg_in,
             const BitRegister &reg_out, const nb::kwargs &kwargs) {
            apply_classical_op_to_registers(
                circ, NotOp(), {reg_in, reg_out}, kwargs);
            return circ;
          },
          "Applies bitwise NOT to linear registers."
          "\n\nThe operation is applied to the bits with indices 0, 1, 2, ... "
          "in "
          "each register, up to the size of the smallest register."
          "\n\n:param reg_in: input register"
          "\n:param reg_out: name of output register"
          "\n:param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` ."
          " Allowed parameters are `opgroup`,  `condition`, `condition_bits`,"
          " `condition_value`"
          "\n:return: the new :py:class:`~.Circuit`",
          nb::arg("reg_in"), nb::arg("reg_out"), nb::arg("kwargs"));
}

}  // namespace tket
