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

#include "tket/Utils/UnitID.hpp"

#include <nanobind/operators.h>

#include "UnitRegister.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "py_operators.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {
const std::string bit_reg_name = std::string("BitRegister");
const std::string qubit_reg_name = std::string("QubitRegister");
template <typename T>
void declare_register(nb::module_ &m, const std::string &typestr) {
  nb::class_<UnitRegister<T>>(
      m, typestr.c_str(), "Linear register of UnitID types.")
      .def(
          nb::init<const std::string &, std::size_t>(),
          ("Construct a new " + typestr + "." +
           "\n\n:param name: Name of the register." +
           "\n:param size: Size of register.")
              .c_str(),
          nb::arg("name"), nb::arg("size"))
      .def("__getitem__", &UnitRegister<T>::operator[])
      .def("__lt__", &UnitRegister<T>::operator<)
      .def("__eq__", &py_equals<UnitRegister<T>>)
      .def("__contains__", &UnitRegister<T>::contains)
      .def("__len__", &UnitRegister<T>::size)
      .def("__str__", &UnitRegister<T>::name)
      .def(
          "__repr__",
          [typestr](const UnitRegister<T> &reg) {
            return typestr + "(\"" + reg.name() + "\", " +
                   std::to_string(reg.size()) + ")";
          })
      .def(
          "__iter__",
          [](UnitRegister<T> &reg) {
            reg.set_current(0);
            return reg;
          })
      .def_prop_rw(
          "name", &UnitRegister<T>::name, &UnitRegister<T>::set_name,
          "Name of register.")
      .def_prop_rw(
          "size", &UnitRegister<T>::size, &UnitRegister<T>::set_size,
          "Size of register.")
      .def_prop_rw(
          "_current", &UnitRegister<T>::current, &UnitRegister<T>::set_current,
          "Internal property for iteration.")
      .def("to_list", &UnitRegister<T>::to_vector)
      .def(
          "__hash__",
          [](const UnitRegister<T> &reg) {
            return nb::hash(nb::make_tuple(reg.name(), reg.size()));
          })
      .def(
          "__copy__",
          [](const UnitRegister<T> &reg) { return UnitRegister<T>(reg); })
      .def("__deepcopy__", [](const UnitRegister<T> &reg, nb::dict) {
        return UnitRegister<T>(reg);
      });
}

NB_MODULE(unit_id, m) {
  nb::set_leak_warnings(false);
  m.attr("_TEMP_REG_SIZE") = _TKET_REG_WIDTH;
  m.attr("_TEMP_BIT_NAME") = "tk_SCRATCH_BIT";
  m.attr("_TEMP_BIT_REG_BASE") = "tk_SCRATCH_BITREG";
  m.attr("_DEBUG_ONE_REG_PREFIX") = nb::str(c_debug_one_prefix().c_str());
  m.attr("_DEBUG_ZERO_REG_PREFIX") = nb::str(c_debug_zero_prefix().c_str());

  nb::enum_<UnitType>(
      m, "UnitType",
      "Enum for data types of units in circuits (e.g. Qubits vs Bits).")
      .value("qubit", UnitType::Qubit, "A single Qubit")
      .value("wasmstate", UnitType::WasmState, "A single WasmState")
      .value("bit", UnitType::Bit, "A single classical Bit")
      .value("rngstate", UnitType::RngState, "A single RngState");

  nb::class_<UnitID>(
      m, "UnitID", "A handle to a computational unit (e.g. qubit, bit)")
      .def(nb::init<>())
      .def("__eq__", &py_equals<UnitID>)
      .def("__lt__", &UnitID::operator<)
      .def("__repr__", &UnitID::repr)
      .def("__hash__", [](const UnitID &id) { return hash_value(id); })
      .def("__copy__", [](const UnitID &id) { return UnitID(id); })
      .def(
          "__deepcopy__",
          [](const UnitID &id, const nb::dict &) { return UnitID(id); })
      .def_prop_ro("reg_name", &UnitID::reg_name, "Readable name of register")
      .def_prop_ro(
          "index", &UnitID::index,
          "Index vector describing position in the register. The "
          "length of this vector is the dimension of the register")
      .def_prop_ro(
          "type", &UnitID::type,
          "Type of unit, either ``UnitType.qubit`` or "
          "``UnitType.bit`` or ``UnitType.wasmstate``");

  nb::class_<Qubit, UnitID>(m, "Qubit", "A handle to a qubit")
      .def("__copy__", [](const Qubit &id) { return Qubit(id); })
      .def(
          "__deepcopy__",
          [](const Qubit &id, const nb::dict &) { return Qubit(id); })
      .def(
          nb::init<unsigned>(),
          "Constructs an id for some index in the default qubit "
          "register\n\n:param index: The index in the register",
          nb::arg("index"))
      .def(
          nb::init<const std::string &>(),
          "Constructs a named id (i.e. corresponding to a singleton "
          "register)\n\n:param name: The readable name for the id",
          nb::arg("name"))
      .def(
          nb::init<const std::string &, unsigned>(),
          "Constructs an indexed id (i.e. corresponding to an element "
          "in a linear register)\n\n:param name: The readable name for "
          "the register\n:param index: The numerical index",
          nb::arg("name"), nb::arg("index"))
      .def(
          nb::init<const std::string &, unsigned, unsigned>(),
          "Constructs a doubly-indexed id (i.e. corresponding to an "
          "element in a grid register)\n\n:param name: The readable "
          "name for the register\n:param row: The row index\n:param "
          "col: The column index",
          nb::arg("name"), nb::arg("row"), nb::arg("col"))
      .def(
          nb::init<
              const std::string &, nb::tket_custom::SequenceVec<unsigned> &>(),
          "Constructs an id with an arbitrary-dimensional "
          "index\n\n:param name: The readable name for the "
          "register\n:param index: The index vector",
          nb::arg("name"), nb::arg("index"))
      .def(
          "__getstate__",
          [](const Qubit &q) {
            return nb::make_tuple(q.reg_name(), q.index());
          })
      .def(
          "__setstate__",
          [](Qubit &q, const nb::tuple &t) {
            if (t.size() != 2) {
              throw std::runtime_error(
                  "Invalid state: tuple size: " + std::to_string(t.size()));
            }
            new (&q) Qubit(
                nb::cast<std::string>(t[0]),
                nb::cast<std::vector<unsigned>>(t[1]));
          })
      .def(
          "to_list",
          [](const Qubit &q) {
            return nb::cast<nb::list>(nb::object(json(q)));
          },
          ":return: a JSON serializable list representation of "
          "the Qubit")
      .def_static(
          "from_list",
          [](const nb::list &py_list) { return json(py_list).get<Qubit>(); },
          "Construct Qubit instance from JSON serializable "
          "list representation of the Qubit.");

  nb::class_<Bit, UnitID>(m, "Bit", "A handle to a bit")
      .def("__copy__", [](const Bit &id) { return Bit(id); })
      .def(
          "__deepcopy__",
          [](const Bit &id, const nb::dict &) { return Bit(id); })
      .def(
          nb::init<unsigned>(),
          "Constructs an id for some index in the default classical "
          "register\n\n:param index: The index in the register",
          nb::arg("index"))
      .def(
          nb::init<const std::string &>(),
          "Constructs a named id (i.e. corresponding to a singleton "
          "register)\n\n:param name: The readable name for the id",
          nb::arg("name"))
      .def(
          nb::init<const std::string &, unsigned>(),
          "Constructs an indexed id (i.e. corresponding to an element "
          "in a linear register)\n\n:param name: The readable name for "
          "the register\n:param index: The numerical index",
          nb::arg("name"), nb::arg("index"))
      .def(
          nb::init<const std::string &, unsigned, unsigned>(),
          "Constructs a doubly-indexed id (i.e. corresponding to an "
          "element in a grid register)\n\n:param name: The readable "
          "name for the register\n:param row: The row index\n:param "
          "col: The column index",
          nb::arg("name"), nb::arg("row"), nb::arg("col"))
      .def(
          nb::init<
              const std::string &, nb::tket_custom::SequenceVec<unsigned> &>(),
          "Constructs an id with an arbitrary-dimensional "
          "index\n\n:param name: The readable name for the "
          "register\n:param index: The index vector",
          nb::arg("name"), nb::arg("index"))
      .def("__eq__", &py_equals<Bit>)
      .def("__hash__", [](const Bit &b) { return hash_value(b); })
      .def(
          "__getstate__",
          [](const Bit &b) { return nb::make_tuple(b.reg_name(), b.index()); })
      .def(
          "__setstate__",
          [](Bit &b, const nb::tuple &t) {
            if (t.size() != 2)
              throw std::runtime_error(
                  "Invalid state: tuple size: " + std::to_string(t.size()));
            new (&b)
                Bit(nb::cast<std::string>(t[0]),
                    nb::cast<std::vector<unsigned>>(t[1]));
          })
      .def(
          "to_list",
          [](const Bit &b) { return nb::cast<nb::list>(nb::object(json(b))); },
          "Return a JSON serializable list representation of "
          "the Bit."
          "\n\n:return: list containing register name and index")
      .def_static(
          "from_list",
          [](const nb::list &py_list) { return json(py_list).get<Bit>(); },
          "Construct Bit instance from JSON serializable "
          "list representation of the Bit.");

  nb::class_<WasmState, UnitID>(m, "WasmState", "A handle to a wasmstate")
      .def("__copy__", [](const WasmState &id) { return WasmState(id); })
      .def(
          "__deepcopy__",
          [](const WasmState &id, const nb::dict &) { return WasmState(id); })
      .def(
          nb::init<unsigned>(),
          "Constructs an id for some index in the default wasm "
          "register\n\n:param index: The index in the register",
          nb::arg("index"))
      .def("__eq__", &py_equals<WasmState>)
      .def("__hash__", [](const WasmState &b) { return hash_value(b); })
      .def(
          "__getstate__",
          [](const WasmState &b) {
            return nb::make_tuple(b.reg_name(), b.index());
          })
      .def(
          "__setstate__",
          [](WasmState &b, const nb::tuple &t) {
            if (t.size() != 2)
              throw std::runtime_error(
                  "Invalid state: tuple size: " + std::to_string(t.size()));
            new (&b) WasmState(
                nb::cast<std::string>(t[0]),
                nb::cast<std::vector<unsigned>>(t[1]));
          })
      .def(
          "to_list",
          [](const WasmState &b) {
            return nb::cast<nb::list>(nb::object(json(b)));
          },
          "Return a JSON serializable list representation of "
          "the WasmState."
          "\n\n:return: list containing register name and index")
      .def_static(
          "from_list",
          [](const nb::list &py_list) {
            return json(py_list).get<WasmState>();
          },
          "Construct WasmState instance from JSON serializable "
          "list representation of the WasmState.");

  nb::class_<RngState, UnitID>(m, "RngState", "A handle to an rngstate")
      .def("__copy__", [](const RngState &id) { return RngState(id); })
      .def(
          "__deepcopy__",
          [](const RngState &id, const nb::dict &) { return RngState(id); })
      .def(
          nb::init<unsigned>(),
          "Constructs an id for some index in the default RNG "
          "register\n\n:param index: The index in the register",
          nb::arg("index"))
      .def("__eq__", &py_equals<RngState>)
      .def("__hash__", [](const RngState &b) { return hash_value(b); })
      .def(
          "__getstate__",
          [](const RngState &b) {
            return nb::make_tuple(b.reg_name(), b.index());
          })
      .def(
          "__setstate__",
          [](RngState &b, const nb::tuple &t) {
            if (t.size() != 2)
              throw std::runtime_error(
                  "Invalid state: tuple size: " + std::to_string(t.size()));
            new (&b) RngState(
                nb::cast<std::string>(t[0]),
                nb::cast<std::vector<unsigned>>(t[1]));
          })
      .def(
          "to_list",
          [](const RngState &b) {
            return nb::cast<nb::list>(nb::object(json(b)));
          },
          "Return a JSON serializable list representation of "
          "the RngState."
          "\n\n:return: list containing register name and index")
      .def_static(
          "from_list",
          [](const nb::list &py_list) { return json(py_list).get<RngState>(); },
          "Construct RngState instance from JSON serializable "
          "list representation of the RngState.");

  nb::class_<Node, Qubit>(m, "Node", "A handle to a device node")
      .def("__copy__", [](const Node &id) { return Node(id); })
      .def(
          "__deepcopy__",
          [](const Node &id, const nb::dict &) { return Node(id); })
      .def(
          nb::init<unsigned>(),
          "Constructs an id for some index in the default physical "
          "register\n\n:param index: The index in the register",
          nb::arg("index"))
      .def(
          nb::init<const std::string &, unsigned>(),
          "Constructs an indexed id (i.e. corresponding to an element "
          "in a linear register)\n\n:param name: The readable name for "
          "the register\n:param index: The numerical index",
          nb::arg("name"), nb::arg("index"))
      .def(
          nb::init<const std::string &, unsigned, unsigned>(),
          "Constructs a doubly-indexed id (i.e. corresponding to an "
          "element in a grid register)\n\n:param name: The readable "
          "name for the register\n:param row: The row index\n:param "
          "col: The column index",
          nb::arg("name"), nb::arg("row"), nb::arg("col"))
      .def(
          nb::init<const std::string &, unsigned, unsigned, unsigned>(),
          "Constructs a triply-indexed id (i.e. corresponding to an "
          "element in a 3D grid register)\n\n:param name: The readable "
          "name for the register\n:param row: The row index\n:param "
          "col: The column index\n:param layer: The layer index",
          nb::arg("name"), nb::arg("row"), nb::arg("col"), nb::arg("layer"))
      .def(
          nb::init<
              const std::string &, nb::tket_custom::SequenceVec<unsigned> &>(),
          "Constructs an id with an arbitrary-dimensional "
          "index\n\n:param name: The readable name for the "
          "register\n:param index: The index vector",
          nb::arg("name"), nb::arg("index"))
      .def(
          "__getstate__",
          [](const Node &n) { return nb::make_tuple(n.reg_name(), n.index()); })
      .def(
          "__setstate__",
          [](Node &n, const nb::tuple &t) {
            if (t.size() != 2)
              throw std::runtime_error(
                  "Invalid state: tuple size: " + std::to_string(t.size()));
            new (&n)
                Bit(nb::cast<std::string>(t[0]),
                    nb::cast<std::vector<unsigned>>(t[1]));
          })
      .def(
          "to_list",
          [](const Node &n) { return nb::cast<nb::list>(nb::object(json(n))); },
          ":return: a JSON serializable list representation of "
          "the Node")
      .def_static(
          "from_list",
          [](const nb::list &py_list) { return json(py_list).get<Node>(); },
          "Construct Node instance from JSON serializable "
          "list representation of the Node.");
  declare_register<Bit>(m, bit_reg_name);
  declare_register<Qubit>(m, qubit_reg_name);
}
}  // namespace tket
