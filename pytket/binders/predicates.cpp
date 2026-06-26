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

#include "tket/Predicates/Predicates.hpp"

#include <nanobind/trampoline.h>

#include <vector>

#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Utils/UnitID.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

static std::map<UnitID, UnitID> unit_bimap_to_map(const unit_bimap_t &bimap) {
  std::map<UnitID, UnitID> res;
  for (auto iter = bimap.left.begin(); iter != bimap.left.end(); ++iter) {
    res.insert({iter->first, iter->second});
  }
  return res;
}

NB_MODULE(predicates, m) {
  nb::set_leak_warnings(false);
  /* Predicates */

  class PyPredicate : public Predicate {
   public:
    NB_TRAMPOLINE(Predicate, 4);

    /* Trampolines (need one for each virtual function */
    virtual bool verify(const Circuit &circ) const override {
      NB_OVERRIDE_PURE(verify, circ);
    }
    virtual bool implies(const Predicate &other) const override {
      NB_OVERRIDE_PURE(implies, other);
    }
    virtual PredicatePtr meet(const Predicate &other) const override {
      NB_OVERRIDE_PURE(meet, other);
    }
    virtual std::string to_string() const override {
      NB_OVERRIDE_PURE(to_string);
    }
  };

  nb::class_<Predicate, PyPredicate>(
      m, "Predicate", "A predicate that may be satisfied by a circuit.")
      .def(
          "verify", &Predicate::verify,
          ":return: True if circuit satisfies predicate, else False",
          nb::arg("circuit"))
      .def(
          "implies", &Predicate::implies,
          ":return: True if predicate implies another one, else False",
          nb::arg("other"))
      .def("__str__", &Predicate::to_string)
      .def("__repr__", &Predicate::to_string)
      .def(
          "to_dict",
          [](const PredicatePtr &predicate) {
            return nb::cast<nb::dict>(nb::object(json(predicate)));
          },
          "Return a JSON serializable dict representation of "
          "the Predicate.\n\n"
          ":return: dict representation of the Predicate.")
      .def_static(
          "from_dict",
          [](const nb::dict &predicate_dict) {
            return json(predicate_dict).get<PredicatePtr>();
          },
          "Construct Predicate instance from JSON serializable "
          "dict representation of the Predicate.")
      .def("__getstate__", [](const PredicatePtr &predicate) {
        return nb::make_tuple(nb::cast<nb::dict>(nb::object(json(predicate))));
      });
  nb::class_<GateSetPredicate, Predicate>(
      m, "GateSetPredicate",
      "Predicate asserting that all operations are in the specified set of "
      "types."
      "\n\n"
      "Note that the following are always permitted and do not need to be "
      "included in the specified set:"
      "\n\n"
      "- 'meta' operations (inputs, outputs, barriers);\n"
      "- ``OpType.Phase`` gates (which have no input or output wires)."
      "\n\n"
      "Classically conditioned operations are permitted provided that the "
      "conditioned operation is of a permitted type.")
      .def(
          nb::init<const OpTypeSet &>(), "Construct from a set of gate types.",
          nb::arg("allowed_types"))
      .def_prop_ro("gate_set", &GateSetPredicate::get_allowed_types)
      .def("__setstate__", [](GateSetPredicate &predicate, const nb::tuple &t) {
        const json j = nb::cast<nb::dict>(t[0]);
        PredicatePtr pp = j.get<PredicatePtr>();
        new (&predicate) GateSetPredicate(
            std::dynamic_pointer_cast<GateSetPredicate>(pp)
                ->get_allowed_types());
      });
  nb::class_<NoClassicalControlPredicate, Predicate>(
      m, "NoClassicalControlPredicate",
      "Predicate asserting that a circuit has no classical controls.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoClassicalControlPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoClassicalControlPredicate();
          });
  nb::class_<NoFastFeedforwardPredicate, Predicate>(
      m, "NoFastFeedforwardPredicate",
      "Predicate asserting that a circuit has no fast feedforward.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoFastFeedforwardPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoFastFeedforwardPredicate();
          });
  nb::class_<NoClassicalBitsPredicate, Predicate>(
      m, "NoClassicalBitsPredicate",
      "Predicate asserting that a circuit has no classical wires.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoClassicalBitsPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoClassicalBitsPredicate();
          });
  nb::class_<NoWireSwapsPredicate, Predicate>(
      m, "NoWireSwapsPredicate",
      "Predicate asserting that a circuit has no wire swaps.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoWireSwapsPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoWireSwapsPredicate();
          });
  nb::class_<MaxTwoQubitGatesPredicate, Predicate>(
      m, "MaxTwoQubitGatesPredicate",
      "Predicate asserting that a circuit has no gates with more than "
      "two input wires.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](MaxTwoQubitGatesPredicate &predicate, const nb::tuple &) {
            new (&predicate) MaxTwoQubitGatesPredicate();
          });
  nb::class_<ConnectivityPredicate, Predicate>(
      m, "ConnectivityPredicate",
      "Predicate asserting that a circuit satisfies a given connectivity "
      "graph. The graph is always considered to be undirected.")
      .def(
          nb::init<const Architecture &>(),
          "Construct from an :py:class:`~.Architecture`.",
          nb::arg("architecture"))
      .def(
          "__setstate__",
          [](ConnectivityPredicate &predicate, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            PredicatePtr pp = j.get<PredicatePtr>();
            new (&predicate) ConnectivityPredicate(
                std::dynamic_pointer_cast<ConnectivityPredicate>(pp)
                    ->get_arch());
          });
  nb::class_<DirectednessPredicate, Predicate>(
      m, "DirectednessPredicate",
      "Predicate asserting that a circuit satisfies a given connectivity "
      "graph. The graph is always considered to be directed.")
      .def(
          nb::init<const Architecture &>(),
          "Construct from an :py:class:`~.Architecture`.",
          nb::arg("architecture"))
      .def(
          "__setstate__",
          [](DirectednessPredicate &predicate, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            PredicatePtr pp = j.get<PredicatePtr>();
            new (&predicate) DirectednessPredicate(
                std::dynamic_pointer_cast<DirectednessPredicate>(pp)
                    ->get_arch());
          });
  nb::class_<CliffordCircuitPredicate, Predicate>(
      m, "CliffordCircuitPredicate",
      "Predicate asserting that a circuit has only Clifford gates and "
      "measurements.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](CliffordCircuitPredicate &predicate, const nb::tuple &) {
            new (&predicate) CliffordCircuitPredicate();
          });
  nb::class_<UserDefinedPredicate, Predicate>(
      m, "UserDefinedPredicate", "User-defined predicate.")
      .def(
          nb::init<const std::function<bool(const Circuit &)> &>(),
          "Construct from a user-defined function from "
          ":py:class:`~.Circuit` to `bool`.",
          nb::arg("check_function"));
  nb::class_<DefaultRegisterPredicate, Predicate>(
      m, "DefaultRegisterPredicate",
      "Predicate asserting that a circuit only uses the default quantum "
      "and classical registers.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](DefaultRegisterPredicate &predicate, const nb::tuple &) {
            new (&predicate) DefaultRegisterPredicate();
          });
  nb::class_<MaxNQubitsPredicate, Predicate>(
      m, "MaxNQubitsPredicate",
      "Predicate asserting that a circuit has at most n qubits.")
      .def(nb::init<unsigned>(), "Constructor.", nb::arg("n_qubits"))
      .def(
          "__setstate__",
          [](MaxNQubitsPredicate &predicate, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            PredicatePtr pp = j.get<PredicatePtr>();
            new (&predicate) MaxNQubitsPredicate(
                std::dynamic_pointer_cast<MaxNQubitsPredicate>(pp)
                    ->get_n_qubits());
          });
  nb::class_<MaxNClRegPredicate, Predicate>(
      m, "MaxNClRegPredicate",
      "Predicate asserting that a circuit has at most n classical registers.")
      .def(nb::init<unsigned>(), "Constructor.", nb::arg("n_cl_reg"))
      .def(
          "__setstate__",
          [](MaxNClRegPredicate &predicate, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            PredicatePtr pp = j.get<PredicatePtr>();
            new (&predicate) MaxNClRegPredicate(
                std::dynamic_pointer_cast<MaxNClRegPredicate>(pp)
                    ->get_n_cl_reg());
          });
  ;
  nb::class_<PlacementPredicate, Predicate>(
      m, "PlacementPredicate",
      "Predicate asserting that a circuit has been acted on by some "
      "Placement object.")
      .def(
          nb::init<const Architecture &>(),
          "Construct from an :py:class:`~.Architecture`.",
          nb::arg("architecture"))
      .def(
          nb::init<const node_set_t &>(), "Construct from a set of Node.",
          nb::arg("nodes"))
      .def(
          "__setstate__",
          [](PlacementPredicate &predicate, const nb::tuple &t) {
            const json j = nb::cast<nb::dict>(t[0]);
            PredicatePtr pp = j.get<PredicatePtr>();
            new (&predicate) PlacementPredicate(
                std::dynamic_pointer_cast<PlacementPredicate>(pp)->get_nodes());
          });
  nb::class_<NoBarriersPredicate, Predicate>(
      m, "NoBarriersPredicate",
      "Predicate asserting that a circuit contains no Barrier operations.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoBarriersPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoBarriersPredicate();
          });
  nb::class_<CommutableMeasuresPredicate, Predicate>(
      m, "CommutableMeasuresPredicate",
      "Predicate asserting that all measurements can be delayed to the end of "
      "the circuit.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](CommutableMeasuresPredicate &predicate, const nb::tuple &) {
            new (&predicate) CommutableMeasuresPredicate();
          });
  nb::class_<NoMidMeasurePredicate, Predicate>(
      m, "NoMidMeasurePredicate",
      "Predicate asserting that all measurements occur at the end of the "
      "circuit.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NoMidMeasurePredicate &predicate, const nb::tuple &) {
            new (&predicate) NoMidMeasurePredicate();
          });
  nb::class_<NoSymbolsPredicate, Predicate>(
      m, "NoSymbolsPredicate",
      "Predicate asserting that no gates in the circuit have symbolic "
      "parameters.")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__", [](NoSymbolsPredicate &predicate, const nb::tuple &) {
            new (&predicate) NoSymbolsPredicate();
          });
  nb::class_<NormalisedTK2Predicate, Predicate>(
      m, "NormalisedTK2Predicate",
      "Asserts that all TK2 gates are normalised\n\n"
      "A gate TK2(a, b, c) is considered normalised if\n\n"
      " - If all expressions are non symbolic, then it must hold "
      "`0.5 ≥ a ≥ b ≥ |c|`.\n"
      " - In the ordering (a, b, c), any symbolic expression must appear "
      "before non-symbolic ones. The remaining non-symbolic expressions must "
      "still be ordered in non-increasing order and must be in the interval "
      "[0, 1/2], with the exception of the last one that may be in "
      "[-1/2, 1/2].\n")
      .def(nb::init<>(), "Constructor.")
      .def(
          "__setstate__",
          [](NormalisedTK2Predicate &predicate, const nb::tuple &) {
            new (&predicate) NormalisedTK2Predicate();
          });

  /* Compilation units */

  nb::class_<CompilationUnit>(
      m, "CompilationUnit",
      "This class comprises a circuit and the predicates that the "
      "circuit is required to satisfy, for example to run on a backend.")
      .def(
          nb::init<const Circuit &>(),
          "Construct from a circuit, with no predicates.", nb::arg("circuit"))
      .def(
          nb::init<const Circuit &, const std::vector<PredicatePtr> &>(),
          "Construct from a circuit and some required predicates.",
          nb::arg("circuit"), nb::arg("predicates"))
      .def(
          "check_all_predicates", &CompilationUnit::check_all_predicates,
          ":return: True if all predicates are satisfied, else False")
      .def_prop_ro(
          "circuit",
          [](const CompilationUnit &cu) { return Circuit(cu.get_circ_ref()); },
          "Return a copy of the circuit.")
      .def_prop_ro(
          "initial_map",
          [](const CompilationUnit &cu) {
            return unit_bimap_to_map(cu.get_initial_map_ref());
          },
          "Returns the map from the original qubits to the "
          "corresponding qubits at the start of the current circuit.")
      .def_prop_ro(
          "final_map",
          [](const CompilationUnit &cu) {
            return unit_bimap_to_map(cu.get_final_map_ref());
          },
          "Returns the map from the original qubits to their "
          "corresponding qubits at the end of the current circuit.")
      .def(
          "__str__",
          [](const CompilationUnit &) { return "<tket::CompilationUnit>"; })
      .def("__repr__", &CompilationUnit::to_string);
}

}  // namespace tket
