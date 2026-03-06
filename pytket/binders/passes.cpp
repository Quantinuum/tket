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
#include <nanobind/trampoline.h>

#include <optional>
#include <string>
#include <tklog/TketLog.hpp>

#include "binder_utils.hpp"
#include "nanobind-stl.hpp"
#include "nanobind_json/nanobind_json.hpp"
#include "tket/Mapping/LexiLabelling.hpp"
#include "tket/Mapping/LexiRouteRoutingMethod.hpp"
#include "tket/Mapping/RoutingMethod.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassGenerators.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Transformations/ContextualReduction.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"
#include "tket/Transformations/Transform.hpp"
#include "typecast.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

// using nb::object and converting internally to json creates better stubs,
// hence this wrapper
typedef std::function<void(const CompilationUnit &, const nb::object &)>
    PyPassCallback;
PassCallback from_py_pass_callback(const PyPassCallback &py_pass_callback) {
  return [py_pass_callback](
             const CompilationUnit &compilationUnit, const json &j) {
    return py_pass_callback(compilationUnit, nb::object(j));
  };
}

// given keyword arguments for DecomposeTK2, return a TwoQbFidelities struct
Transforms::TwoQbFidelities get_fidelities(const nb::kwargs &kwargs) {
  Transforms::TwoQbFidelities fid;
  for (const auto &kwarg : kwargs) {
    const std::string kwargstr = nb::cast<std::string>(kwarg.first);
    using Func = std::function<double(double)>;
    if (kwargstr == "CX_fidelity") {
      fid.CX_fidelity = nb::cast<double>(kwarg.second);
    } else if (kwargstr == "ZZMax_fidelity") {
      fid.ZZMax_fidelity = nb::cast<double>(kwarg.second);
    } else if (kwargstr == "ZZPhase_fidelity") {
      fid.ZZPhase_fidelity = nb::cast<std::variant<double, Func>>(kwarg.second);
    } else {
      std::string msg = "got an unexpected keyword argument '" + kwargstr + "'";
      throw nb::type_error(msg.c_str());
    }
  }
  return fid;
}

static PassPtr gen_cx_mapping_pass_kwargs(
    const Architecture &arc, const Placement::Ptr &placer, nb::kwargs kwargs) {
  std::vector<RoutingMethodPtr> config = {
      std::make_shared<LexiLabellingMethod>(),
      std::make_shared<LexiRouteRoutingMethod>()};
  if (kwargs.contains("config")) {
    config = nb::cast<std::vector<RoutingMethodPtr>>(kwargs["config"]);
  }
  bool directed_cx = false;
  if (kwargs.contains("directed_cx")) {
    directed_cx = nb::cast<bool>(kwargs["directed_cx"]);
  }
  bool delay_measures = true;
  if (kwargs.contains("delay_measures")) {
    delay_measures = nb::cast<bool>(kwargs["delay_measures"]);
  }
  return gen_cx_mapping_pass(arc, placer, config, directed_cx, delay_measures);
}

static PassPtr gen_default_routing_pass(const Architecture &arc) {
  return gen_routing_pass(
      arc, {std::make_shared<LexiLabellingMethod>(),
            std::make_shared<LexiRouteRoutingMethod>()});
}

static PassPtr gen_default_aas_routing_pass(
    const Architecture &arc, nb::kwargs kwargs) {
  unsigned lookahead = 1;
  aas::CNotSynthType cnotsynthtype = aas::CNotSynthType::Rec;

  if (kwargs.contains("lookahead"))
    lookahead = nb::cast<unsigned>(kwargs["lookahead"]);

  if (kwargs.contains("cnotsynthtype"))
    cnotsynthtype = nb::cast<aas::CNotSynthType>(kwargs["cnotsynthtype"]);

  if (lookahead == 0) {
    throw std::invalid_argument(
        "[AAS]: invalid input, the lookahead must be > 0");
  }

  return gen_full_mapping_pass_phase_poly(arc, lookahead, cnotsynthtype);
}

const PassPtr &DecomposeClassicalExp() {
  // a special box decomposer for Circuits containing ClExprOp
  static const PassPtr pp([]() {
    Transform t = Transform([](Circuit &circ) {
      nb::module_ decomposer =
          nb::module_::import_("pytket.circuit.decompose_classical");
      const nb::tuple result =
          nb::cast<nb::tuple>(decomposer.attr("_decompose_expressions")(circ));
      const bool success = nb::cast<bool>(result[1]);
      if (success) {
        circ = nb::cast<Circuit>(result[0]);
      }
      return success;
    });
    PredicatePtrMap s_ps;
    /**
     * Preserves Max2QubitGatesPredicate since any box with >2 qubits is
     * already invalid.
     * Preserves ConnectivityPredicate (and DirectednessPredicate) since the
     * verification looks inside CircBoxes and any other boxes with >2
     * qubits are already invalid.
     * Most others are preserved since the predicates look within CircBoxes.
     *
     * Invalidates GateSetPredicate because it adds Classical OpTypes
     */
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear},
    };
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    json j;
    j["name"] = "DecomposeClassicalExp";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

std::optional<OpTypeSet> get_gate_set(const BasePass &base_pass) {
  std::optional<OpTypeSet> allowed_ops;
  for (const std::pair<const std::type_index, std::shared_ptr<tket::Predicate>>
           &p : base_pass.get_conditions().first) {
    std::shared_ptr<GateSetPredicate> gsp_ptr =
        std::dynamic_pointer_cast<GateSetPredicate>(p.second);
    if (!gsp_ptr) {
      continue;
    }
    OpTypeSet candidate_allowed_ops = gsp_ptr->get_allowed_types();
    if (!allowed_ops) {
      allowed_ops = candidate_allowed_ops;
    } else {
      OpTypeSet intersection;
      std::set_intersection(
          candidate_allowed_ops.begin(), candidate_allowed_ops.end(),
          allowed_ops->begin(), allowed_ops->end(),
          std::inserter(intersection, intersection.begin()));
      allowed_ops = intersection;
    }
  }
  return allowed_ops;
}

NB_MODULE(passes, m) {
  nb::set_leak_warnings(false);
  nb::module_::import_("pytket._tket.predicates");
  nb::enum_<SafetyMode>(m, "SafetyMode")
      .value(
          "Audit", SafetyMode::Audit,
          "Checks which predicates a circuit satisfies after the "
          "application of each base pass")
      .value(
          "Default", SafetyMode::Default,
          "Only check that a circuit satisfies the preconditions of "
          "the overall pass at the start and the postconditions at "
          "the end")
      // .value("Off", SafetyMode::Off) // not currently supported
      .export_values();

  nb::enum_<aas::CNotSynthType>(m, "CNotSynthType")
      .value(
          "SWAP", aas::CNotSynthType::SWAP,
          "swap-based algorithm for CNOT synthesis")
      .value(
          "HamPath", aas::CNotSynthType::HamPath,
          "Hamilton-path-based method for CNOT synthesis; this method will "
          "fail if there is no Hamilton path in the given architecture")
      .value(
          "Rec", aas::CNotSynthType::Rec,
          "recursive Steiner--Gauss method for CNOT synthesis")
      .export_values();

  /* Compiler passes */

  class PyBasePass : public BasePass {
   public:
    NB_TRAMPOLINE(BasePass, 3);

    /* Trampolines (need one for each virtual function */
    virtual bool apply(
        CompilationUnit &c_unit, SafetyMode safe_mode = SafetyMode::Default,
        const PassCallback &before_apply = trivial_callback,
        const PassCallback &after_apply = trivial_callback) const override {
      NB_OVERRIDE_PURE(apply, c_unit, safe_mode, before_apply, after_apply);
    }
    virtual std::string to_string() const override {
      NB_OVERRIDE_PURE(to_string);
    }
    virtual json get_config() const override { NB_OVERRIDE_PURE(get_config); }
  };
  nb::class_<BasePass, PyBasePass>(m, "BasePass", "Base class for passes.")
      .def(
          "apply",
          [](const BasePass &pass, CompilationUnit &cu,
             SafetyMode safety_mode) { return pass.apply(cu, safety_mode); },
          "Apply to a :py:class:`~.CompilationUnit`.\n\n"
          ":return: True if the pass modified the circuit. Note that in some "
          "cases the method may return True even when the circuit is "
          "unmodified (but a return value of False definitely implies no "
          "modification).",
          nb::arg("compilation_unit"),
          nb::arg("safety_mode") = SafetyMode::Default)
      .def(
          "apply",
          [](const BasePass &pass, Circuit &circ) {
            CompilationUnit cu(circ);
            bool applied = pass.apply(cu);
            circ = cu.get_circ_ref();
            return applied;
          },
          "Apply to a :py:class:`~.Circuit` in-place.\n\n"
          ":return: True if pass modified the circuit, else False",
          nb::arg("circuit"))
      .def(
          "apply",
          [](const BasePass &pass, Circuit &circ,
             const PyPassCallback &before_apply,
             const PyPassCallback &after_apply) {
            CompilationUnit cu(circ);
            bool applied = pass.apply(
                cu, SafetyMode::Default, from_py_pass_callback(before_apply),
                from_py_pass_callback(after_apply));
            circ = cu.get_circ_ref();
            return applied;
          },
          "Apply to a :py:class:`~.Circuit` in-place and invoke "
          "callbacks "
          "for all nested passes.\n\n"
          "\n:param before_apply: Invoked before a pass is applied. "
          "The CompilationUnit and a summary of the pass "
          "configuration are passed into the callback."
          "\n:param after_apply: Invoked after a pass is applied. "
          "The CompilationUnit and a summary of the pass "
          "configuration are passed into the callback."
          "\n:return: True if pass modified the circuit, else False",
          nb::arg("circuit"), nb::arg("before_apply"), nb::arg("after_apply"))
      .def("__str__", [](const BasePass &) { return "<tket::BasePass>"; })
      .def("__repr__", &BasePass::to_string)
      .def(
          "to_dict",
          [](const BasePass &base_pass) { return serialise(base_pass); },
          ":return: A JSON serializable dictionary representation of the Pass.")
      .def(
          "get_preconditions",
          [](const BasePass &base_pass) {
            std::vector<PredicatePtr> pre_conditions;
            for (const std::pair<
                     const std::type_index, std::shared_ptr<tket::Predicate>>
                     &p : base_pass.get_conditions().first) {
              pre_conditions.push_back(p.second);
            }
            return pre_conditions;
          },
          "Returns the precondition Predicates for the given pass."
          "\n:return: A list of Predicate")
      .def(
          "get_postconditions",
          [](const BasePass &base_pass) {
            std::vector<PredicatePtr> post_conditions;
            for (const std::pair<
                     const std::type_index, std::shared_ptr<tket::Predicate>> &
                     p : base_pass.get_conditions().second.specific_postcons_) {
              post_conditions.push_back(p.second);
            }
            return post_conditions;
          },
          "Returns the postcondition Predicates for the given pass."
          "\n\n:return: A list of :py:class:`~.Predicate`")
      .def(
          "get_gate_set", &get_gate_set,
          "Returns the intersection of all set of OpType for all "
          "GateSetPredicate in the `BasePass` preconditions, or `None` "
          "if there are no gate-set predicates.",
          "\n\n:return: A set of allowed OpType")
      .def_static(
          "from_dict",
          [](const nb::dict &base_pass_dict,
             const std::map<
                 std::string, std::function<Circuit(const Circuit &)>>
                 &custom_deserialisation,
             const std::map<
                 std::string,
                 std::function<
                     std::pair<Circuit, std::pair<unit_map_t, unit_map_t>>(
                         const Circuit &)>> &custom_map_deserialisation) {
            return deserialise(
                base_pass_dict, custom_deserialisation,
                custom_map_deserialisation);
          },
          "Construct a new Pass instance from a JSON serializable dictionary "
          "representation. `custom_deserialisation` is a map between "
          "`CustomPass` "
          "label attributes and a Circuit to Circuit function matching the "
          "`CustomPass` `transform` argument. This allows the construction of "
          "some `CustomPass` from JSON. `CustomPass` without a matching entry "
          "in "
          "`custom_deserialisation` will be rejected.",
          nb::arg("base_pass_dict"),
          nb::arg("custom_deserialisation") =
              std::map<std::string, std::function<Circuit(const Circuit &)>>{},
          nb::arg("custom_map_deserialisation") = std::map<
              std::string, std::function<std::pair<
                               Circuit, std::pair<unit_map_t, unit_map_t>>(
                               const Circuit &)>>{})
      .def(
          "__getstate__",
          [](const BasePass &pass) {
            return nb::make_tuple(nb::cast(serialise(pass)));
          })
      .def("__setstate__", [](PassPtr &pass, const nb::tuple &t) {
        const json j = nb::cast<json>(t[0]);
        PassPtr pp = deserialise(j);
        new (&pass) PassPtr(pp);
      });
  nb::class_<SequencePass, BasePass>(
      m, "SequencePass", "A sequence of compilation passes.")
      .def(
          nb::init<const nb::tket_custom::SequenceVec<PassPtr> &, bool>(),
          "Construct from a list of compilation passes arranged in order of "
          "application."
          "\n\n:param pass_list: sequence of passes"
          "\n:param strict: if True (the default), check that all "
          "postconditions and preconditions of the passes in the sequence are "
          "compatible and raise an exception if not."
          "\n:return: a pass that applies the sequence",
          nb::arg("pass_list"), nb::arg("strict") = true)
      .def("__str__", [](const BasePass &) { return "<tket::SequencePass>"; })
      .def(
          "to_dict",
          [](const SequencePass &seq_pass) {
            return serialise(std::make_shared<SequencePass>(seq_pass));
          },
          ":return: A JSON serializable dictionary representation of the "
          "SequencePass.")
      .def(
          "get_sequence", &SequencePass::get_sequence,
          ":return: The underlying sequence of passes.");

  nb::class_<RepeatPass, BasePass>(
      m, "RepeatPass",
      "Repeat a pass until its `apply()` method returns False, or if "
      "`strict_check` is True until it stops modifying the circuit.")
      .def(
          nb::init<const PassPtr &, bool>(),
          "Construct from a compilation pass.", nb::arg("compilation_pass"),
          nb::arg("strict_check") = false)
      .def("__str__", [](const RepeatPass &) { return "<tket::BasePass>"; })
      .def(
          "get_pass", &RepeatPass::get_pass,
          ":return: The underlying compilation pass.");
  nb::class_<RepeatWithMetricPass, BasePass>(
      m, "RepeatWithMetricPass",
      "Repeat a compilation pass until the given metric stops "
      "decreasing.")
      .def(
          nb::init<const PassPtr &, const Transform::Metric &>(),
          "Construct from a compilation pass and a metric function.",
          nb::arg("compilation_pass"), nb::arg("metric"))
      .def(
          "__str__",
          [](const BasePass &) { return "<tket::RepeatWithMetricPass>"; })
      .def(
          "get_pass", &RepeatWithMetricPass::get_pass,
          ":return: The underlying compilation pass.")
      .def(
          "get_metric", &RepeatWithMetricPass::get_metric,
          ":return: The underlying metric.");
  nb::class_<RepeatUntilSatisfiedPass, BasePass>(
      m, "RepeatUntilSatisfiedPass",
      "Repeat a compilation pass until a predicate on the circuit is "
      "satisfied.")
      .def(
          nb::init<const PassPtr &, const PredicatePtr &>(),
          "Construct from a compilation pass and a predicate.",
          nb::arg("compilation_pass"), nb::arg("predicate"))
      .def(
          nb::init<
              const PassPtr &, const std::function<bool(const Circuit &)> &>(),
          "Construct from a compilation pass and a user-defined "
          "function from :py:class:`~.Circuit` to `bool`.",
          nb::arg("compilation_pass"), nb::arg("check_function"))
      .def(
          "__str__",
          [](const BasePass &) { return "<tket::RepeatUntilSatisfiedPass>"; })
      .def(
          "get_pass", &RepeatUntilSatisfiedPass::get_pass,
          ":return: The underlying compilation pass.")
      .def(
          "get_predicate", &RepeatUntilSatisfiedPass::get_predicate,
          ":return: The underlying predicate.");

  /* Pass library */
  m.def(
      "KAKDecomposition", &KAKDecomposition,
      "Squash sequences of two-qubit operations into minimal form.\n\n"
      "Pass to squash together sequences of single- and two-qubit gates "
      "into minimal form. Can decompose to TK2 or CX gates.\n\n"
      "Two-qubit operations can always be expressed in a minimal form of "
      "maximum three CXs, or as a single TK2 gate (a result also known "
      "as the KAK or Cartan decomposition).\n\n"
      "It is in general recommended to squash to TK2 gates, and to then use"
      " the `DecomposeTK2` pass for noise-aware decompositions to other "
      "gatesets. For backward compatibility, decompositions to CX are also "
      "supported. In this case, `cx_fidelity` can be provided to perform "
      "approximate decompositions to CX gates.\n\n"
      "When decomposing to TK2 gates, any sequence of two or more two-qubit"
      " gates on the same set of qubits are replaced by a single TK2 gate. "
      "When decomposing to CX, the substitution is only performed if it "
      "results in a reduction of the number of CX gates, or if at least "
      "one of the two-qubit gates is not a CX.\n\n"
      "Using the `allow_swaps=True` (default) option, qubits will be "
      "swapped when convenient to further reduce the two-qubit gate count "
      "(only applicable when decomposing to CX gates).\n\n"
      "Note that gates containing symbolic parameters are not squashed.\n\n"
      ":param target_2qb_gate: OpType to decompose to. Either TK2 or CX.\n"
      ":param cx_fidelity: Estimated CX gate fidelity, used when "
      "target_2qb_gate=CX.\n"
      ":param allow_swaps: Whether to allow implicit wire swaps.",
      nb::arg("target_2qb_gate") = OpType::CX, nb::arg("cx_fidelity") = 1.,
      nb::arg("allow_swaps") = true);
  m.def(
      "KAKDecomposition",
      [](double cx_fidelity) {
        return KAKDecomposition(OpType::CX, cx_fidelity);
      },
      nb::arg("cx_fidelity"));
  m.def(
      "DecomposeTK2",
      [](bool allow_swaps, const nb::kwargs &kwargs) {
        return DecomposeTK2(get_fidelities(kwargs), allow_swaps);
      },
      "Decompose each TK2 gate into two-qubit gates."
      "\n\nGate fidelities can be passed as keyword arguments to perform "
      "noise-aware decompositions. If the fidelities of several gate types "
      "are provided, the best will be chosen.\n\n"
      "We currently support `CX_fidelity`, `ZZMax_fidelity` and "
      "`ZZPhase_fidelity`. If provided, the `CX` and `ZZMax` fidelities "
      "must be given by a single floating point fidelity. The `ZZPhase` "
      "fidelity is given as a lambda float -> float, mapping a ZZPhase "
      "angle parameter to its fidelity, or by a single float. These parameters "
      "will be used to return the optimal decomposition of each TK2 gate, "
      "taking noise into consideration.\n\n"
      "If no fidelities are provided, the TK2 gates will be decomposed "
      "exactly using CX gates. For equal fidelities, ZZPhase will be preferred "
      "over ZZMax and CX if the decomposition results in fewer two-qubit "
      "gates.\n\n"
      "All TK2 gate parameters must be normalised, i.e. they must satisfy "
      "`NormalisedTK2Predicate`. (This can be achieved by applying the "
      ":py:meth:`NormaliseTK2` pass beforehand.)\n\n"
      "Using the `allow_swaps=True` (default) option, qubits will be swapped "
      "when convenient to reduce the two-qubit gate count of the decomposed "
      "TK2.\n\n"
      "If the TK2 angles are symbolic values, the decomposition will "
      "be exact (i.e. not noise-aware). It is not possible in general "
      "to obtain optimal decompositions for arbitrary symbolic parameters, "
      "so consider substituting for concrete values if possible."
      "\n\n:param allow_swaps: Whether to allow implicit wire swaps.",
      nb::arg("allow_swaps") = true, nb::arg("kwargs"));
  m.def(
      "NormaliseTK2", &NormaliseTK2,
      "Normalises all TK2 gates.\n\n"
      "TK2 gates have three angles in the interval [0, 4], but these can always"
      " be normalised to be within the so-called Weyl chamber by adding "
      "single-qubit gates.\n\n"
      "More precisely, the three angles a, b, c of TK2(a, b, c) are normalised "
      "exactly when the two following conditions are met:\n"
      " - numerical values must be in the Weyl chamber, "
      "ie `1/2 >= a >= b >= |c|`,\n"
      " - symbolic values must come before any numerical value in the array."
      "\n\nAfter this pass, all TK2 angles will be normalised and the circuit "
      "will satisfy `NormalisedTK2Predicate`.");
  m.def(
      "ThreeQubitSquash", &ThreeQubitSquash,
      "Squash three-qubit subcircuits into subcircuits having fewer CX gates, "
      "when possible, and apply Clifford simplification."
      "\n\nThe circuit to which this is applied must consist of single-qubit, "
      "pure-classical and CX gates, and Measure, Collapse, Reset, Phase and "
      "conditional gates."
      "\n\n:param allow_swaps: whether to allow implicit wire swaps",
      nb::arg("allow_swaps") = true);
  m.def(
      "CommuteThroughMultis", &CommuteThroughMultis,
      "Moves single-qubit operations past multi-qubit operations that they "
      "commute with, towards the front of the circuit.");
  m.def(
      "DecomposeArbitrarilyControlledGates",
      &DecomposeArbitrarilyControlledGates,
      "Decomposes CCX, CnX, CnY, CnZ, CnRy, CnRz and CnRx gates into "
      "CX and single-qubit gates.");
  m.def(
      "DecomposeBoxes", &DecomposeBoxes,
      "Recursively replaces all boxes by their decomposition into circuits. "
      "\n\nArguments specify ways to filter which boxes are decomposed. A box "
      "must satisfy ALL filters in order to be decomposed (i.e. be in the "
      "inclusive sets and not in the exclusive sets)."
      "\n\n:param excluded_types: box " CLSOBJS(~.OpType) " excluded from "
      "decomposition"
      "\n:param excluded_opgroups: opgroups excluded from decomposition"
      "\n:param included_types: optional, only decompose these box "
      CLSOBJS(~.OpType)
      "\n:param included_opgroups: optional, only decompose these opgroups",
      nb::arg("excluded_types") = std::unordered_set<OpType>(),
      nb::arg("excluded_opgroups") = std::unordered_set<std::string>(),
      nb::arg("included_types") = std::nullopt,
      nb::arg("included_opgroups") = std::nullopt);
  m.def(
      "DecomposeClassicalExp", &DecomposeClassicalExp,
      "Replaces each `ClExprOp` by a sequence of classical gates.");
  m.def(
      "DecomposeMultiQubitsCX", &DecomposeMultiQubitsCX,
      "Converts all multi-qubit gates into CX and single-qubit gates.");
  m.def(
      "DecomposeSingleQubitsTK1", &DecomposeSingleQubitsTK1,
      "Converts all single-qubit gates into TK1 gates.");
  m.def(
      "PeepholeOptimise2Q", &PeepholeOptimise2Q,
      "Performs peephole optimisation including resynthesis of 2-qubit "
      "gate sequences, and converts to a circuit containing only CX and TK1 "
      "gates."
      "\n\n:param allow_swaps: whether to allow implicit wire swaps",
      nb::arg("allow_swaps") = true);
  m.def(
      "FullPeepholeOptimise", &FullPeepholeOptimise,
      "Performs peephole optimisation including resynthesis of 2- and 3-qubit "
      "gate sequences, and converts to a circuit containing only the given "
      "2-qubit gate (which may be CX or TK2) and TK1 gates."
      "\n\n:param allow_swaps: whether to allow implicit wire swaps",
      nb::arg("allow_swaps") = true, nb::arg("target_2qb_gate") = OpType::CX);
  m.def(
      "RebaseTket", &RebaseTket,
      "Converts all gates to CX, TK1 and Phase. "
      "(Any Measure and Reset operations are left untouched; "
      "Conditional gates are also allowed.)");
  m.def(
      "RxFromSX", &RxFromSX,
      "Replaces all SX in the circuit with Rx(1/2) and all SXdg with "
      "Rx(-1/2).");
  m.def(
      "RemoveRedundancies", &RemoveRedundancies,
      "Removes gate-inverse pairs, merges rotations, removes identity "
      "rotations, and removes redundant gates before measurement. Does not "
      "add any new gate types.\n\n"
      "When merging rotations with the same op group name, the merged "
      "operation keeps the same name.");
  m.def(
      "SynthesiseTK", &SynthesiseTK,
      "Optimises and converts all gates to TK2, TK1 and Phase gates.");
  m.def(
      "SynthesiseTket", &SynthesiseTket,
      "Optimises and converts all gates to CX, TK1 and Phase gates.");
  m.def(
      "SquashTK1", &SquashTK1,
      "Squash sequences of single-qubit gates to TK1 gates.");
  m.def(
      "SquashRzPhasedX", &SquashRzPhasedX,
      "Squash single qubit gates into PhasedX and Rz gates.");
  m.def(
      "FlattenRegisters", &FlattenRegisters,
      "Merges all quantum and classical registers into their "
      "respective "
      "default registers with contiguous indexing.");
  m.def(
      "SquashCustom", &gen_squash_pass,
      "Squash sequences of single qubit gates from the target gate set "
      "into an optimal form given by `tk1_replacement`."
      "\n\n:param singleqs: The types of single qubit gates in the target "
      "gate set. This pass will only affect sequences of gates that are "
      "already in this set."
      "\n:param tk1_replacement: A function which, given the parameters of "
      "an Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the "
      "desired basis."
      "\n:param always_squash_symbols: If true, always squash symbolic gates "
      "regardless of the blow-up in complexity. Default is false, meaning that "
      "symbolic gates are only squashed if doing so reduces the overall "
      "symbolic complexity.",
      nb::arg("singleqs"), nb::arg("tk1_replacement"),
      nb::arg("always_squash_symbols") = false);
  m.def(
      "AutoSquash", &gen_auto_squash_pass,
      "Attempt to generate a squash pass automatically for the given target "
      "single qubit gateset.\n"
      "Raises an error if no known TK1 decomposition can be found based on the "
      "given gateset, in which case try using :py:meth:`~.SquashCustom` with "
      "your own decomposition."
      "\n\n:param singleqs: The types of single qubit gates in the target "
      "gate set. This pass will only affect sequences of gates that are "
      "already in this set.",
      nb::arg("singleqs"));
  m.def(
      "DelayMeasures", &DelayMeasures,
      "Commutes Measure operations to the end of the circuit. Throws an "
      "exception when this is not possible because of gates following the "
      "measure which are dependent on either the resulting quantum state "
      "or classical values."
      "\n\n:param allow_partial: Whether to allow measurements that cannot be "
      "commuted to "
      "the end, and delay them as much as possible instead. If false, the pass "
      "includes a :py:class:`~.CommutableMeasuresPredicate` precondition.",
      nb::arg("allow_partial") = true);
  m.def(
      "RemoveDiscarded", &RemoveDiscarded,
      "A pass to remove all operations that have no ``OpType.Output`` or "
      "``OpType.ClOutput`` in their causal future (in other words, all "
      "operations whose causal future is discarded).");
  m.def(
      "SimplifyMeasured", &SimplifyMeasured,
      "A pass to replace all 'classical maps' followed by measure "
      "operations whose quantum output is discarded with classical "
      "operations following the measure. (A 'classical map' is a quantum "
      "operation that acts as a permutation of the computational basis "
      "states followed by a diagonal operation.)");
  m.def(
      "RemoveBarriers", &RemoveBarriers,
      "A pass to remove all barrier instructions from the circuit.");
  m.def(
      "RemovePhaseOps", &RemovePhaseOps,
      "A pass to remove all Phase operations from the circuit. This includes "
      "conditional Phase operations, but not Phase operations inside "
      "CircBoxes, QControlBoxes or other nested structures.");
  m.def(
      "ZXGraphlikeOptimisation", &ZXGraphlikeOptimisation,
      "Attempt to optimise the circuit by simplifying in ZX calculus and "
      "extracting a circuit back out. Due to limitations in extraction, may "
      "not work if the circuit contains created or discarded qubits. As a "
      "resynthesis pass, this will ignore almost all optimisations achieved "
      "beforehand and may increase the cost of the circuit."
      "\n\n:param allow_swaps: Whether to allow implicit wire swaps (default "
      "True).",
      nb::arg("allow_swaps") = true);

  /* Pass generators */

  m.def(
      "RebaseCustom", &gen_rebase_pass,
      "Construct a custom rebase pass, given user-defined rebases for TK1 and "
      "CX. This pass:"
      "\n\n"
      "1. decomposes multi-qubit gates not in the set of gate types `gateset` "
      "to CX gates;\n"
      "2. if CX is not in `gateset`, replaces CX gates with `cx_replacement`;\n"
      "3. converts any single-qubit gates not in the gate type set to the form "
      ":math:`\\mathrm{Rz}(a)\\mathrm{Rx}(b)\\mathrm{Rz}(c)` (in "
      "matrix-multiplication order, i.e. reverse order in the "
      "circuit);\n"
      "4. applies the `tk1_replacement` function to each of these triples "
      ":math:`(a,b,c)` to generate replacement circuits."
      "\n\n"
      ":param gateset: the allowed operations in the rebased circuit "
      "(in addition, Measure and Reset operations are always allowed "
      "and are left alone; conditional operations may be present; and Phase "
      "gates may also be introduced by the rebase)"
      "\n:param cx_replacement: the equivalent circuit to replace a CX gate "
      "using two qubit gates from the desired basis (can use any single qubit "
      "OpTypes)"
      "\n:param tk1_replacement: a function which, given the parameters of an "
      "Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the desired "
      "basis"
      "\n:return: a pass that rebases to the given gate set (possibly "
      "including conditional and phase operations, and Measure and Reset",
      nb::arg("gateset"), nb::arg("cx_replacement"),
      nb::arg("tk1_replacement"));

  m.def(
      "RebaseCustom", &gen_rebase_pass_via_tk2,
      "Construct a custom rebase pass, given user-defined rebases for TK1 and "
      "TK2. This pass:"
      "\n\n"
      "1. decomposes multi-qubit gates not in the set of gate types `gateset` "
      "to TK2 gates;\n"
      "2. if TK2 is not in `gateset`, replaces TK2(a,b,c) gates via the "
      "`tk2_replacement` function;\n"
      "3. converts any single-qubit gates not in the gate type set to TK1;\n"
      "4. if TK2 is not in `gateset`. applies the `tk1_replacement` function "
      "to each TK1(a,b,c)."
      "\n\n"
      ":param gateset: the allowed operations in the rebased circuit "
      "(in addition, Measure and Reset always allowed "
      "and are left alone; conditional operations may be present; and Phase "
      "gates may also be introduced by the rebase)\n"
      ":param tk2_replacement: a function which, given the parameters (a,b,c) "
      "of an XXPhase(a)YYPhase(b)ZZPhase(c) triple, returns an equivalent "
      "circuit in the desired basis\n"
      ":param tk1_replacement: a function which, given the parameters (a,b,c) "
      "of an Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the "
      "desired basis\n"
      ":return: a pass that rebases to the given gate set (possibly including "
      "conditional and phase operations, and Measure and Reset)",
      nb::arg("gateset"), nb::arg("tk2_replacement"),
      nb::arg("tk1_replacement"));
  m.def(
      "AutoRebase", &gen_auto_rebase_pass,
      "Attempt to generate a rebase pass automatically for the given target "
      "gateset. Checks if there are known existing decompositions "
      "to target gateset and TK1 to target gateset and uses those to construct "
      "a custom rebase.\n"
      "Raises an error if no known decompositions can be found, in which case "
      "try using :py:meth:`~.RebaseCustom` with your own decompositions.\n\n"
      ":param gateset: Set of supported OpTypes, target gate set. "
      "(in addition, Measure and Reset operations are always allowed "
      "and are left alone; conditional operations may be present; and Phase "
      "gates may also be introduced by the rebase)\n"
      ":param allow_swaps: Whether to allow implicit wire swaps. Default to "
      "False.",
      nb::arg("gateset"), nb::arg("allow_swaps") = false);
  m.def(
      "EulerAngleReduction", &gen_euler_pass,
      "Uses Euler angle decompositions to squash all chains of P and Q "
      "rotations, "
      "where P,Q ∈ {Rx,Ry,Rz}. "
      "By default (`strict=False`), this pass will try to decompose the chains "
      "into pairs of -P-Q- or -Q-P- rotations, commuting any third rotation "
      "past multi-qubit gates. "
      "If `strict=True`, all chains will be decomposed to P-Q-P triples "
      "and no further optimisation is performed."
      "\n\n:param q: The type of the Q rotation (Q ∈ {Rx,Ry,Rz})."
      "\n:param p: The type of the P rotation (P ∈ {Rx,Ry,Rz}, P ≠ Q)."
      "\n:param strict: Optionally performs strict P-Q-P Euler decomposition"
      "\n:return: a pass that squashes chains of P and Q rotations",
      nb::arg("q"), nb::arg("p"), nb::arg("strict") = false);

  m.def(
      "CustomRoutingPass",
      [](const Architecture &arc,
         const nb::tket_custom::SequenceVec<RoutingMethodPtr> &config) {
        return gen_routing_pass(arc, config);
      },
      "Construct a pass to route to the connectivity graph of an "
      ":py:class:`~.Architecture`. Edge direction is ignored. "
      "\n\n"
      ":return: a pass that routes to the given device architecture ",
      nb::arg("arc"), nb::arg("config"));

  m.def(
      "RoutingPass", &gen_default_routing_pass,
      "Construct a pass to route to the connectivity graph of an "
      ":py:class:`~.Architecture`. Edge direction is ignored. "
      "Uses :py:class:`~.LexiLabellingMethod` and "
      ":py:class:`~.LexiRouteRoutingMethod`."
      "\n\n"
      ":return: a pass that routes to the given device architecture",
      nb::arg("arc"));

  m.def(
      "PlacementPass", &gen_placement_pass,
      ":param placer: The Placement used for relabelling."
      "\n:return: a pass to relabel :py:class:`~.Circuit` Qubits to "
      ":py:class:`~.Architecture` Nodes",
      nb::arg("placer"));

  m.def(
      "NaivePlacementPass", &gen_naive_placement_pass,
      ":param architecture: The Architecture used for relabelling."
      "\n:return: a pass to relabel :py:class:`~.Circuit` Qubits to "
      ":py:class:`~.Architecture` Nodes",
      nb::arg("architecture"));

  m.def(
      "FlattenRelabelRegistersPass", &gen_flatten_relabel_registers_pass,
      "Removes empty Quantum wires from the Circuit and relabels all Qubit to "
      "a register from passed name. \n\n:param label: Name to relabel "
      "remaining Qubit to, default 'q'."
      "\n:return: A pass that removes empty "
      "wires and relabels.",
      nb::arg("label") = q_default_reg());

  m.def(
      "RenameQubitsPass", &gen_rename_qubits_pass,
      "Rename some or all qubits. "
      "\n\n"
      ":param qubit_map: map from old to new qubit names",
      nb::arg("qubit_map"));

  m.def(
      "FullMappingPass",
      [](const Architecture &arc, const Placement::Ptr &placement_ptr,
         const nb::tket_custom::SequenceVec<RoutingMethodPtr> &config) {
        return gen_full_mapping_pass(arc, placement_ptr, config);
      },
      "Construct a pass to relabel :py:class:`~.Circuit` Qubits to "
      ":py:class:`~.Architecture` Nodes, and then route to the connectivity "
      "graph "
      "of an :py:class:`~.Architecture`. Edge direction is ignored."
      "\n\n:param arc: The architecture to use for connectivity information. "
      "\n:param placer: The Placement used for relabelling."
      "\n:param config: Parameters for routing, a list of RoutingMethod, each "
      "method is checked and run if applicable in turn."
      "\n:return: a pass to perform the remapping",
      nb::arg("arc"), nb::arg("placer"), nb::arg("config"));

  m.def(
      "DefaultMappingPass", &gen_default_mapping_pass,
      "Construct a pass to relabel :py:class:`~.Circuit` Qubits to "
      ":py:class:`~.Architecture` Nodes, and then route to the connectivity "
      "graph "
      "of the given :py:class:`~.Architecture`. Edge direction is ignored. "
      "Placement used "
      "is GraphPlacement."
      "\n\n:param arc: The Architecture used for connectivity information."
      "\n:param delay_measures: Whether to commute measurements to the end "
      "of the circuit, defaulting to true."
      "\n:return: a pass to perform the remapping",
      nb::arg("arc"), nb::arg("delay_measures") = true);

  m.def(
      "AASRouting", &gen_default_aas_routing_pass,
      "Construct a pass to relabel :py:class:`~.Circuit` Qubits to "
      ":py:class:`~.Architecture` Nodes, and then use architecture-aware "
      "synthesis to route the circuit. In the steps of the pass the circuit "
      "will be converted to CX, Rz, H gateset. The limited connectivity of the "
      ":py:class:`~.Architecture` is used for the routing. "
      "The direction of the edges is ignored. The placement used "
      "is GraphPlacement. This pass can take a few parameters for the "
      "routing, described below:"
      "\n\n- (unsigned) lookahead=1: parameter for the recursive iteration"
      "\n- (CNotSynthType) cnotsynthtype=CNotSynthType.Rec: CNOT synthesis type"
      "\n\nNB: The circuit needs to have at most as "
      "many qubits as the architecture has nodes. The resulting circuit will "
      "always have the same number of qubits as the architecture has nodes, "
      "even if the input circuit had fewer."
      "\n\n:param arc: target architecture"
      "\n:param \\**kwargs: parameters for routing (described above)"
      "\n:return: a pass to perform the remapping",
      nb::arg("arc"), nb::arg("kwargs"));

  m.def(
      "ComposePhasePolyBoxes", &ComposePhasePolyBoxes,
      "Pass to convert a given :py:class:`~.Circuit` to the CX, Rz, H gateset "
      "and compose "
      "phase polynomial boxes from the groups of the CX+Rz gates."
      "\n\n- (unsigned) min_size=0: minimal number of CX gates in each phase "
      "polynomial box: groups with a smaller number of CX gates are not "
      "affected by this transformation\n"
      "\n:return: a pass to perform the composition",
      nb::arg("min_size") = 0);

  m.def(
      "CXMappingPass", &gen_cx_mapping_pass_kwargs,
      "Construct a pass to convert all gates to CX, relabel "
      ":py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes, route "
      "to "
      "the "
      "connectivity graph of a :py:class:`~.Architecture` and decompose "
      "additional "
      "routing gates (SWAP and BRIDGE) to CX gates."
      "\n\n:param arc: The Architecture used for connectivity information."
      "\n:param placer: The placement used for relabelling."
      "\n:param \\**kwargs: Parameters for routing: "
      "(bool)directed_cx=false, (bool)delay_measures=true"
      "\n:return: a pass to perform the remapping",
      nb::arg("arc"), nb::arg("placer"), nb::arg("kwargs"));

  m.def(
      "CliffordSimp", &gen_clifford_simp_pass,
      "An optimisation pass that applies a number of rewrite rules for "
      "simplifying Clifford gate sequences, similar to Duncan & Fagan "
      "(https://arxiv.org/abs/1901.10114). Produces a circuit comprising TK1 "
      "gates and the two-qubit gate specified as the target."
      "\n\n:param allow_swaps: whether the rewriting may introduce implicit "
      "wire swaps"
      "\n:param target_2qb_gate: target two-qubit gate (either CX or TK2)"
      "\n:return: a pass to perform the rewriting",
      nb::arg("allow_swaps") = true, nb::arg("target_2qb_gate") = OpType::CX);

  m.def(
      "CliffordResynthesis", &gen_clifford_resynthesis_pass,
      "An optimisation pass that resynthesises Clifford subcircuits, trying "
      "to reduce the 2-qubit gate count as much as possible."
      "\n\n:param transform: optional user-provided resynthesis method to "
      "apply to all Clifford subcircuits (a function taking a Clifford "
      "circuit as an argument and returning an equivalent circuit); if not "
      "provided, a default resynthesis method is applied"
      "\n:param allow_swaps: whether the rewriting may introduce wire swaps "
      "(only relevant to the default resynthesis method used when the "
      "`transform` argument is not provided)"
      "\n:return: a pass to perform the rewriting",
      nb::arg("transform") = std::nullopt, nb::arg("allow_swaps") = true);

  m.def(
      "CliffordPushThroughMeasures", &gen_clifford_push_through_pass,
      "An optimisation pass that resynthesise a Clifford subcircuit "
      "before end of circuit Measurement operations by implementing "
      "the action of the Clifford as a mutual diagonalisation circuit "
      "and a permutation on output measurements realised as a series "
      "of classical operations."
      "\n: return: a pass to simplify end of circuit Clifford gates.");

  m.def(
      "DecomposeSwapsToCXs", &gen_decompose_routing_gates_to_cxs_pass,
      "Construct a pass to decompose SWAP and BRIDGE gates to CX gates, "
      "constraining connectivity to an :py:class:`~.Architecture`, optionally "
      "taking the directedness of the connectivity graph into account."
      "\n\n:param arc: The architecture to use for connectivity information."
      "\n:param respect_direction: Optionally takes the directedness of "
      "the connectivity graph into account."
      "\n:return: a pass to perform the decomposition",
      nb::arg("arc"), nb::arg("respect_direction") = false);

  m.def(
      "DecomposeSwapsToCircuit", &gen_user_defined_swap_decomp_pass,
      ":param replacement_circuit: An equivalent circuit to replace a "
      "SWAP gate with in the desired basis."
      "\n:return: a pass to replace all SWAP gates with the given circuit",
      nb::arg("replacement_circuit"));
  m.def(
      "OptimisePhaseGadgets", &gen_optimise_phase_gadgets,
      "Construct a pass that synthesises phase gadgets and converts to a "
      "circuit containing only CX, TK1 and Phase gates."
      "\n\n:param cx_config: A configuration of CXs to convert phase "
      "gadgets into."
      "\n:return: a pass to perform the synthesis",
      nb::arg("cx_config") = CXConfigType::Snake);
  m.def(
      "PauliExponentials", &gen_pauli_exponentials,
      "Construct a pass that converts a circuit into a graph of Pauli "
      "exponential boxes, with information"
      "\n\n:param strat: A synthesis strategy for the Pauli graph."
      "\n:param cx_config: A configuration of CXs to convert Pauli gadgets "
      "into."
      "\n:return: a pass to perform the simplification",
      nb::arg("strat") = Transforms::PauliSynthStrat::Sets,
      nb::arg("cx_config") = CXConfigType::Snake);
  m.def(
      "PauliSimp", &gen_synthesise_pauli_graph,
      "Construct a pass that converts a circuit into a graph of Pauli "
      "gadgets to account for commutation and phase folding, and "
      "resynthesises them as either individual gadgets, pairwise "
      "constructions, or by diagonalising sets of commuting gadgets.\n\n"
      "This pass will not preserve the global phase of the circuit."
      "\n\n:param strat: A synthesis strategy for the Pauli graph."
      "\n:param cx_config: A configuration of CXs to convert Pauli gadgets "
      "into."
      "\n:return: a pass to perform the simplification",
      nb::arg("strat") = Transforms::PauliSynthStrat::Sets,
      nb::arg("cx_config") = CXConfigType::Snake);
  m.def(
      "GuidedPauliSimp", &gen_special_UCC_synthesis,
      "Applies the ``PauliSimp`` optimisation pass to any region of the "
      "circuit contained within a :py:class:`~.CircBox`. This can be useful "
      "to focus the synthesis to target specific sets of commuting "
      "operations, rather than the default greedy approach."
      "\n\n:param strat: A synthesis strategy for the Pauli graph."
      "\n:param cx_config: A configuration of CXs to convert Pauli gadgets "
      "into."
      "\n:return: a pass to perform the simplification",
      nb::arg("strat") = Transforms::PauliSynthStrat::Sets,
      nb::arg("cx_config") = CXConfigType::Snake);
  m.def(
      "GreedyPauliSimp", &gen_greedy_pauli_simp,
      "Construct a pass that converts a circuit into a graph of Pauli "
      "gadgets to account for commutation and phase folding, and "
      "resynthesises them using a greedy algorithm adapted from "
      "arxiv.org/abs/2103.08602. The method for synthesising the "
      "final Clifford operator is adapted from "
      "arxiv.org/abs/2305.10966."
      "\n\nWARNING: this pass will not preserve the global phase of the "
      "circuit."
      "\n\n:param discount_rate: Rate used to discount the cost impact from "
      "gadgets that are further away. Default to 0.7."
      "\n:param depth_weight:  Degree of depth optimisation. Default to 0.3."
      "\n:param max_tqe_candidates:  Maximum number of 2-qubit Clifford "
      "gate candidates to evaluate at each step. Default to 500."
      "\n:param max_lookahead:  Maximum lookahead when evaluating each "
      "Clifford gate candidate. Default to 500."
      "\n:param seed:  Unsigned integer seed used for sampling candidates "
      "and tie breaking. Default to 0."
      "\n:param allow_zzphase: If set to True, allows the algorithm to "
      "implement 2-qubit rotations using ZZPhase gates when deemed "
      "optimal. Defaults to False."
      "\n:param thread_timeout: Sets maximum out of time spent finding a "
      "single solution in one thread."
      "\n:param only_reduce: Only returns modified circuit if it has "
      "fewer two-qubit gates."
      "\n:param trials: Sets maximum number of found solutions. The "
      "smallest circuit is returned, prioritising the number of 2qb-gates, "
      "then the number of gates, then the depth."
      "\n:return: a pass to perform the simplification",
      nb::arg("discount_rate") = 0.7, nb::arg("depth_weight") = 0.3,
      nb::arg("max_lookahead") = 500, nb::arg("max_tqe_candidates") = 500,
      nb::arg("seed") = 0, nb::arg("allow_zzphase") = false,
      nb::arg("thread_timeout") = 100, nb::arg("only_reduce") = false,
      nb::arg("trials") = 1);
  m.def(
      "PauliSquash", &PauliSquash,
      "Applies :py:meth:`PauliSimp` followed by "
      ":py:meth:`FullPeepholeOptimise`."
      "\n\n:param strat: a synthesis strategy for the Pauli graph"
      "\n:param cx_config: a configuration of CXs to convert Pauli gadgets into"
      "\n:return: a pass to perform the simplification",
      nb::arg("strat") = Transforms::PauliSynthStrat::Sets,
      nb::arg("cx_config") = CXConfigType::Snake);
  m.def(
      "SimplifyInitial",
      [](bool allow_classical, bool create_all_qubits, bool remove_redundancies,
         std::optional<std::shared_ptr<const Circuit>> xcirc_opt) -> PassPtr {
        PassPtr simpinit = gen_simplify_initial(
            allow_classical ? Transforms::AllowClassical::Yes
                            : Transforms::AllowClassical::No,
            create_all_qubits ? Transforms::CreateAllQubits::Yes
                              : Transforms::CreateAllQubits::No,
            xcirc_opt.value_or(nullptr));
        if (remove_redundancies) {
          std::vector<PassPtr> seq = {simpinit, RemoveRedundancies()};
          return std::make_shared<SequencePass>(seq);
        } else {
          return simpinit;
        }
      },
      "Simplify the circuit using knowledge of qubit state."
      "\n\n:param allow_classical: allow replacement of measurements on "
      "known state with classical set-bit operations"
      "\n:param create_all_qubits: automatically annotate all qubits as "
      "initialized to the zero state"
      "\n:param remove_redundancies: apply a :py:meth:`RemoveRedundancies` "
      "pass after the initial simplification"
      "\n:param xcirc: 1-qubit circuit implementing an X gate in the "
      "transformed circuit (if omitted, an X gate is used)"
      "\n:return: a pass to perform the simplification",
      nb::arg("allow_classical") = true, nb::arg("create_all_qubits") = false,
      nb::arg("remove_redundancies") = true, nb::arg("xcirc") = nb::none());
  m.def(
      "ContextSimp",
      [](bool allow_classical,
         std::optional<std::shared_ptr<const Circuit>> xcirc) {
        if (xcirc.has_value()) {
          return gen_contextual_pass(
              allow_classical ? Transforms::AllowClassical::Yes
                              : Transforms::AllowClassical::No,
              std::move(xcirc.value()));
        }
        return gen_contextual_pass(
            allow_classical ? Transforms::AllowClassical::Yes
                            : Transforms::AllowClassical::No);
      },
      "Applies simplifications enabled by knowledge of qubit state and "
      "discarded qubits."
      "\n\n:param allow_classical: allow replacement of measurements on "
      "known state with classical set-bit operations"
      "\n:param xcirc: 1-qubit circuit implementing an X gate in the "
      "transformed circuit (if omitted, an X gate is used)"
      "\n:return: a pass to perform the simplification",
      nb::arg("allow_classical") = true, nb::arg("xcirc") = nullptr);

  m.def(
      "ZZPhaseToRz", &ZZPhaseToRz,
      "Converts all ZZPhase gates in a circuit with angle 1 or -1 (half-turns) "
      "into two Rz gates each with a parameter value of 1 (half-turns). "
      "ZZPhase gates with parameter values other than 1 or -1 "
      "(half-turns) are left "
      "unchanged.\n\n"
      ":return: a pass to convert ZZPhase gates to Rz.");

  m.def(
      "CnXPairwiseDecomposition", &CnXPairwiseDecomposition,
      "Decompose CnX gates to 2-qubit gates and single qubit gates. "
      "For every two CnX gates, reorder their control qubits to improve "
      "the chance of gate cancellation");

  m.def(
      "RoundAngles", &RoundAngles,
      "Round angles to the nearest :math:`\\pi / 2^n`. "
      "\n\n:param n: precision parameter, must be >= 0 and < 32 "
      "\n:param only_zeros: if True, only round angles less than "
      ":math:`\\pi / 2^{n+1}` to zero, leave other angles alone (default "
      "False)",
      nb::arg("n"), nb::arg("only_zeros") = false);

  m.def(
      "RemoveImplicitQubitPermutation", &RemoveImplicitQubitPermutation,
      "Remove any implicit qubit permutation by appending SWAP gates."
      "\n\n"
      "Note that if the circuit contains measurements, they may become "
      "mid-circuit measurements in the transformed circuit.");

  m.def(
      "CustomPass", &CustomPass,
      "Generate a custom pass from a user-provided circuit transformation "
      "function."
      "\n\n"
      "It is the caller's responsibility to provide a valid transform."
      "\n\n"
      ":param transform: function taking a :py:class:`~.Circuit` as an "
      "argument "
      "and returning a new transformed circuit"
      "\n"
      ":param label: optional label for the pass"
      "\n:return: a pass to perform the transformation",
      nb::arg("transform"), nb::arg("label") = "");

  m.def(
      "CustomPassMap", &CustomPassMap,
      "Generate a custom pass from a user-provided circuit transformation "
      "function."
      "\n\n"
      "It is the caller's responsibility to provide a valid transform."
      "\n\n"
      ":param transform: function taking a :py:class:`~.Circuit` as an "
      "argument "
      "and returning a pair of a new transformed circuit and a pair of maps "
      "corresponding to the initial and final maps that the "
      "transformation makes."
      "\n"
      ":param label: optional label for the pass"
      "\n:return: a pass to perform the transformation",
      nb::arg("transform"), nb::arg("label") = "");
}

}  // namespace tket
