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

// This must be the first include since it includes Python.h.
// https://docs.python.org/3/c-api/intro.html#include-files
// clang-format off
#include <nanobind/nanobind.h>
// clang-format on

#include <boost/lexical_cast.hpp>

#include "UnitRegister.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "tket/Circuit/Conditional.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

void init_classical(nb::module_& m) {
  nb::class_<Conditional, Op>(
      m, "Conditional",
      "A wrapper for an operation to be applied conditionally on the "
      "value of some classical bits (following the nature of conditional "
      "operations in the OpenQASM specification).")
      .def(
          nb::init<const Op_ptr&, unsigned, unsigned>(),
          "Construct from operation, bit width and (little-endian) value",
          nb::arg("op"), nb::arg("width"), nb::arg("value"))
      .def_prop_ro(
          "op", &Conditional::get_op,
          "The operation to be applied conditionally")
      .def_prop_ro(
          "width", &Conditional::get_width,
          "The number of bits in the condition register")
      .def_prop_ro(
          "value", &Conditional::get_value,
          "The little-endian value the classical register must read "
          "in order to apply the operation (e.g. value 2 (10b) means "
          "bits[0] must be 0 and bits[1] must be 1)");
  nb::class_<ClassicalOp, Op>(m, "ClassicalOp", "Classical operation.")
      .def_prop_ro("n_inputs", &ClassicalOp::get_n_i, "Number of pure inputs.")
      .def_prop_ro(
          "n_input_outputs", &ClassicalOp::get_n_io,
          "Number of pure input/output arguments.")
      .def_prop_ro(
          "n_outputs", &ClassicalOp::get_n_o, "Number of pure outputs.");
  nb::class_<ClassicalEvalOp, ClassicalOp>(
      m, "ClassicalEvalOp", "Evaluatable classical operation.");
  nb::class_<SetBitsOp, ClassicalEvalOp>(
      m, "SetBitsOp",
      "An operation to set the values of Bits to some constants.")
      .def(
          nb::init<const nb::tket_custom::SequenceVec<bool>&>(),
          "Construct from a table of values.", nb::arg("values"))
      .def_prop_ro(
          "values", &SetBitsOp::get_values, "The values to set bits to.");
  nb::class_<CopyBitsOp, ClassicalEvalOp>(
      m, "CopyBitsOp",
      "An operation to copy the values of Bits to other Bits.");
  nb::class_<MultiBitOp, ClassicalEvalOp>(
      m, "MultiBitOp",
      "An operation to apply a classical op multiple times in parallel.")
      .def(
          nb::init<std::shared_ptr<const ClassicalEvalOp>, unsigned>(),
          "Construct from a basic operation and a multiplier.", nb::arg("op"),
          nb::arg("multiplier"))
      .def_prop_ro("basic_op", &MultiBitOp::get_op, "Underlying bitwise op.")
      .def_prop_ro("multiplier", &MultiBitOp::get_n, "Multiplier.");
  nb::class_<RangePredicateOp, ClassicalEvalOp>(
      m, "RangePredicateOp",
      "A predicate defined by a range of values in binary encoding.")
      .def(
          nb::init<unsigned, _tket_uint_t, _tket_uint_t>(),
          "Construct from a bit width, a lower bound and an upper bound.",
          nb::arg("width"), nb::arg("lower"), nb::arg("upper"))
      .def_prop_ro("lower", &RangePredicateOp::lower, "Inclusive lower bound.")
      .def_prop_ro("upper", &RangePredicateOp::upper, "Inclusive upper bound.");

  nb::class_<WASMOp, ClassicalOp>(
      m, "WASMOp",
      "An op holding an external classical call, defined by the external "
      "module id, the name of the function and the arguments. External calls "
      "can only act on entire registers (which will be interpreted as "
      "fixed-width integers).")
      .def(
          nb::init<
              unsigned, unsigned, nb::tket_custom::SequenceVec<unsigned>,
              nb::tket_custom::SequenceVec<unsigned>, const std::string&,
              const std::string&>(),
          "Construct from number of bits, bitwidths of inputs and outputs, "
          "function name and module id.",
          nb::arg("num_bits"), nb::arg("num_w"), nb::arg("n_inputs"),
          nb::arg("n_outputs"), nb::arg("func_name"), nb::arg("wasm_uid"))
      .def_prop_ro("wasm_uid", &WASMOp::get_wasm_file_uid, "Wasm module id.")
      .def_prop_ro("num_w", &WASMOp::get_ww_n, "Number of wasm wire in the op")
      .def_prop_ro("func_name", &WASMOp::get_func_name, "Name of function.")
      .def_prop_ro(
          "num_bits", &WASMOp::get_n, "Number of bits interacted with.")
      .def_prop_ro("n_i32", &WASMOp::get_n_i32, "Number of integers acted on.")
      .def_prop_ro(
          "input_widths", &WASMOp::get_width_i_parameter,
          "Widths of input integers.")
      .def_prop_ro(
          "output_widths", &WASMOp::get_width_o_parameter,
          "Widths of output integers.");
}
}  // namespace tket
