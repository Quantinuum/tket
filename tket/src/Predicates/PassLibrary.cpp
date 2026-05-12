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

#include "tket/Predicates/PassLibrary.hpp"

#include <memory>
#include <optional>

#include "tket/Converters/Converters.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/Predicates.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/ContextualReduction.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/MeasurePass.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Transformations/Transform.hpp"
#include "tket/ZX/Rewrite.hpp"

namespace tket {

template <typename T>
static PassPtr gate_translation_pass(
    const T &transform, OpTypeSet after_set, bool respect_connectivity,
    const std::string &name) {
  after_set.insert(OpType::Measure);
  after_set.insert(OpType::Collapse);
  after_set.insert(OpType::Reset);
  after_set.insert(OpType::Phase);
  PredicatePtrMap precons;
  std::type_index ti = typeid(ConnectivityPredicate);
  PredicatePtr out_gateset = std::make_shared<GateSetPredicate>(after_set);
  PredicatePtr max2qb = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtrMap postcon_spec = {
      CompilationUnit::make_type_pair(out_gateset),
      CompilationUnit::make_type_pair(max2qb)};
  PredicateClassGuarantees g_postcons;
  if (!respect_connectivity)
    g_postcons.insert({ti, Guarantee::Clear});  // synthesis passes do not in
                                                // general preserve connectivity
  PostConditions postcon{postcon_spec, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = name;
  PassPtr ptr = std::make_shared<StandardPass>(precons, transform, postcon, j);
  return ptr;
}

const PassPtr &SynthesiseTK() {
  static const PassPtr pp(gate_translation_pass(
      Transforms::synthesise_tk(), {OpType::TK1, OpType::TK2}, true,
      "SynthesiseTK"));
  return pp;
}
const PassPtr &SynthesiseTket() {
  static const PassPtr pp(gate_translation_pass(
      Transforms::synthesise_tket(), {OpType::TK1, OpType::CX}, true,
      "SynthesiseTket"));
  return pp;
}
const PassPtr &RebaseTket() {
  static const PassPtr pp(gate_translation_pass(
      Transforms::rebase_tket(), {OpType::CX, OpType::TK1}, true,
      "RebaseTket"));
  return pp;
}

const PassPtr &RebaseUFR() {
  static const PassPtr pp(gate_translation_pass(
      Transforms::rebase_UFR(), {OpType::CX, OpType::Rz, OpType::H}, true,
      "RebaseUFR"));
  return pp;
}

const PassPtr &RxFromSX() {
  static const PassPtr pp([]() {
    Transform t{[](Circuit &circ) {
      VertexVec verts_sx;
      VertexVec verts_sxdg;
      BGL_FORALL_VERTICES(v, circ.dag, DAG) {
        OpType optype = circ.get_Op_ptr_from_Vertex(v)->get_type();
        if (optype == OpType::SX) {
          verts_sx.push_back(v);
        } else if (optype == OpType::SXdg) {
          verts_sxdg.push_back(v);
        }
      }
      if (verts_sx.empty() && verts_sxdg.empty()) {
        return false;
      }
      Circuit c_sx(1);
      c_sx.add_op<unsigned>(OpType::Rx, 0.5, {0});
      Circuit c_sxdg(1);
      c_sxdg.add_op<unsigned>(OpType::Rx, -0.5, {0});
      VertexList bin;
      for (const Vertex &v : verts_sx) {
        circ.substitute(c_sx, v, Circuit::VertexDeletion::No);
        circ.add_phase(0.25);
        bin.push_back(v);
      }
      for (const Vertex &v : verts_sxdg) {
        circ.substitute(c_sxdg, v, Circuit::VertexDeletion::No);
        circ.add_phase(-0.25);
        bin.push_back(v);
      }
      circ.remove_vertices(
          bin, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);
      return true;
    }};
    PredicatePtrMap s_ps;
    PredicateClassGuarantees g_postcons{
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    PredicatePtrMap precons;
    // record pass config
    nlohmann::json j;
    j["name"] = "RxFromSX";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &RemoveRedundancies() {
  static const PassPtr pp([]() {
    Transform t = Transforms::remove_redundancies();
    PostConditions postcon = {{}, {}, Guarantee::Preserve};
    PredicatePtrMap precons;
    // record pass config
    nlohmann::json j;
    j["name"] = "RemoveRedundancies";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &CommuteThroughMultis() {
  static const PassPtr pp([]() {
    Transform t = Transforms::commute_through_multis();
    PostConditions postcon = {{}, {}, Guarantee::Preserve};
    PredicatePtrMap precons;
    // record pass config
    nlohmann::json j;
    j["name"] = "CommuteThroughMultis";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &DecomposeArbitrarilyControlledGates() {
  static const PassPtr pp([]() {
    Transform t = Transforms::decomp_arbitrary_controlled_gates();
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
    PredicatePtrMap precons;
    // record pass config
    nlohmann::json j;
    j["name"] = "DecomposeArbitrarilyControlledGates";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &DecomposeMultiQubitsCX() {
  static const PassPtr pp([]() {
    Transform t = Transforms::decompose_multi_qubits_CX();
    /* Spits out CX + any single qb gates */
    OpTypeSet ots = {OpType::CX};
    op_signature_t noargs = {};
    op_signature_t singleq = {EdgeType::Quantum};
    ots.insert(all_projective_types().begin(), all_projective_types().end());
    for (const std::pair<const OpType, OpTypeInfo> &ott : optypeinfo()) {
      if (!ott.second.signature || *ott.second.signature == singleq ||
          *ott.second.signature == noargs)
        ots.insert(ott.first);
    }
    PredicatePtrMap precons;
    PredicatePtr outp_gates = std::make_shared<GateSetPredicate>(ots);
    PredicatePtr twoqbpred = std::make_shared<MaxTwoQubitGatesPredicate>();
    PredicatePtrMap spec_postcons = {
        CompilationUnit::make_type_pair(outp_gates),
        CompilationUnit::make_type_pair(twoqbpred)};
    PredicateClassGuarantees g_postcons;
    PostConditions postcon{spec_postcons, g_postcons, Guarantee::Preserve};
    // record pass config
    nlohmann::json j;
    j["name"] = "DecomposeMultiQubitsCX";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &DecomposeSingleQubitsTK1() {
  static const PassPtr pp([]() {
    Transform t = Transforms::decompose_single_qubits_TK1();
    /* Spits out TK1 + any multi qb gates */
    OpTypeSet ots = {OpType::TK1};
    op_signature_t singleq = {EdgeType::Quantum};
    ots.insert(all_projective_types().begin(), all_projective_types().end());
    for (const std::pair<const OpType, OpTypeInfo> &ott : optypeinfo()) {
      if (!ott.second.signature || *ott.second.signature != singleq)
        ots.insert(ott.first);
    }
    PredicatePtrMap precons;
    PredicatePtr outp_gates = std::make_shared<GateSetPredicate>(ots);
    PredicatePtrMap spec_postcons = {
        CompilationUnit::make_type_pair(outp_gates)};
    PredicateClassGuarantees g_postcons;
    PostConditions postcon{spec_postcons, g_postcons, Guarantee::Preserve};
    // record pass config
    nlohmann::json j;
    j["name"] = "DecomposeSingleQubitsTK1";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

PassPtr ComposePhasePolyBoxes(const unsigned min_size) {
  /**
   * converts a circuit containing all possible gates to a circuit
   * containing only phase poly boxes + H gates (and measure + reset + collapse
   * + barrier)
   * this pass will replace all wire swaps in the given circuit and they will be
   * included in the last or an additional phase poly boxes
   */

  Transform t =
      (Transforms::rebase_UFR() >>
       Transforms::compose_phase_poly_boxes(min_size));

  PredicatePtr noclas = std::make_shared<NoClassicalControlPredicate>();

  PredicatePtrMap precons{CompilationUnit::make_type_pair(noclas)};

  PredicatePtr no_wire_swap = std::make_shared<NoWireSwapsPredicate>();

  PredicatePtrMap s_postcons{
      CompilationUnit::make_type_pair(noclas),
      CompilationUnit::make_type_pair(no_wire_swap)};
  PostConditions postcon{s_postcons, {}, Guarantee::Preserve};

  nlohmann::json j;
  j["name"] = "ComposePhasePolyBoxes";
  j["min_size"] = min_size;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr DecomposeBoxes(
    const std::unordered_set<OpType> &excluded_types,
    const std::unordered_set<std::string> &excluded_opgroups,
    const std::optional<std::unordered_set<OpType>> &included_types,
    const std::optional<std::unordered_set<std::string>> &included_opgroups) {
  Transform t = Transforms::decomp_boxes(
      excluded_types, excluded_opgroups, included_types, included_opgroups);
  PredicatePtrMap s_ps;
  /**
   * Preserves Max2QubitGatesPredicate since any box with >2 qubits is
   * already invalid.
   * Preserves ConnectivityPredicate (and DirectednessPredicate) since the
   * verification looks inside CircBoxes and any other boxes with >2
   * qubits are already invalid.
   * Most others are preserved since the predicates look within CircBoxes.
   *
   * Invalidates GateSetPredicate because it doesn't look inside boxes or
   * account for the gate set of their decomposition.
   */
  PredicateClassGuarantees g_postcons = {
      {typeid(GateSetPredicate), Guarantee::Clear},
  };
  PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "DecomposeBoxes";
  j["excluded_types"] = excluded_types;
  j["excluded_opgroups"] = excluded_opgroups;
  if (included_types) j["included_types"] = *included_types;
  if (included_opgroups) j["included_opgroups"] = *included_opgroups;
  return std::make_shared<StandardPass>(s_ps, t, postcon, j);
}

const PassPtr &SquashTK1() {
  static const PassPtr pp([]() {
    Transform t = Transforms::squash_1qb_to_tk1();
    PredicatePtrMap s_ps;
    PredicateClassGuarantees g_postcons{
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    // record pass config
    nlohmann::json j;
    j["name"] = "SquashTK1";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

const PassPtr &DecomposeBridges() {
  static const PassPtr pp([]() {
    Transform t = Transforms::decompose_BRIDGE_to_CX();
    PredicatePtrMap s_ps;
    PredicateClassGuarantees g_postcons{
        {typeid(GateSetPredicate), Guarantee::Clear},
        {typeid(DirectednessPredicate), Guarantee::Clear}};
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "DecomposeBridges";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

const PassPtr &FlattenRegisters() {
  static const PassPtr pp([]() {
    Transform t =
        Transform([](Circuit &circ, std::shared_ptr<unit_bimaps_t> maps) {
          if (circ.is_simple()) return false;
          unit_map_t qmap = circ.flatten_registers();
          update_maps(maps, qmap, qmap);
          return true;
        });
    PredicatePtrMap s_ps;
    PredicatePtr simple = std::make_shared<DefaultRegisterPredicate>();
    PredicatePtrMap spec_postcons{CompilationUnit::make_type_pair(simple)};
    PredicateClassGuarantees g_postcons{
        {typeid(ConnectivityPredicate), Guarantee::Clear},
        {typeid(DirectednessPredicate), Guarantee::Clear}};
    PostConditions postcon{spec_postcons, g_postcons, Guarantee::Preserve};
    // record pass config
    nlohmann::json j;
    j["name"] = "FlattenRegisters";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

const PassPtr &RemoveBarriers() {
  static const PassPtr pp([]() {
    Transform t = Transform([](Circuit &circ) {
      VertexList barriers;
      BGL_FORALL_VERTICES(v, circ.dag, DAG) {
        if (circ.get_OpType_from_Vertex(v) == OpType::Barrier) {
          barriers.push_back(v);
        }
      }
      circ.remove_vertices(
          barriers, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);
      return !barriers.empty();
    });
    const PredicatePtrMap no_precons;
    const PredicatePtr no_barriers = std::make_shared<NoBarriersPredicate>();
    PredicatePtrMap no_barriers_con{
        CompilationUnit::make_type_pair(no_barriers)};
    PredicateClassGuarantees preserve_all;
    PostConditions postcons{no_barriers_con, preserve_all, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "RemoveBarriers";
    return std::make_shared<StandardPass>(no_precons, t, postcons, j);
  }());
  return pp;
}

const PassPtr &DelayMeasures(const bool allow_partial) {
  auto f = [](bool allow_partial) {
    Transform t = Transforms::delay_measures(allow_partial);

    PredicatePtrMap precon;
    PostConditions postcon;

    if (!allow_partial) {
      PredicatePtr delaymeaspred =
          std::make_shared<CommutableMeasuresPredicate>();
      precon = {CompilationUnit::make_type_pair(delaymeaspred)};

      PredicatePtr midmeaspred = std::make_shared<NoMidMeasurePredicate>();
      PredicatePtrMap spec_postcons = {
          CompilationUnit::make_type_pair(midmeaspred)};
      postcon = {spec_postcons, {}, Guarantee::Preserve};
    }

    nlohmann::json j;
    j["name"] = "DelayMeasures";
    j["allow_partial"] = allow_partial;
    return std::make_shared<StandardPass>(precon, t, postcon, j);
  };
  static const PassPtr delay(f(false));
  static const PassPtr try_delay(f(true));
  return allow_partial ? try_delay : delay;
}

const PassPtr &RemoveDiscarded() {
  static const PassPtr pp([]() {
    Transform t = Transforms::remove_discarded_ops();
    PredicatePtrMap no_precons;
    PostConditions postcon = {{}, {}, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "RemoveDiscarded";
    return std::make_shared<StandardPass>(no_precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &SimplifyMeasured() {
  static const PassPtr pp([]() {
    Transform t = Transforms::simplify_measured();
    PredicatePtrMap no_precons;

    // GateSetPredicate not preserved because classical gates may be
    // introduced.
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear}};

    PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "SimplifyMeasured";
    return std::make_shared<StandardPass>(no_precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &NormaliseTK2() {
  static const PassPtr pp([]() {
    Transform t = Transforms::normalise_TK2();
    PredicatePtrMap no_precons;

    // GateSetPredicate not preserved because single-qubit gates may be added
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear}};

    PredicatePtr normalisedpred = std::make_shared<NormalisedTK2Predicate>();
    PredicatePtrMap spec_postcons = {
        CompilationUnit::make_type_pair(normalisedpred)};
    PostConditions postcon = {spec_postcons, g_postcons, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "NormaliseTK2";
    return std::make_shared<StandardPass>(no_precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &ZZPhaseToRz() {
  static const PassPtr pp([]() {
    Transform t = Transforms::ZZPhase_to_Rz();
    // GateSetPredicate not preserved because ZZPhase gates may be converted to
    // Rz gates
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
    PredicatePtrMap precons;
    // record pass config
    nlohmann::json j;
    j["name"] = "ZZPhaseToRz";
    return std::make_shared<StandardPass>(precons, t, postcon, j);
  }());
  return pp;
}

const PassPtr &SquashRzPhasedX() {
  static const PassPtr pp([]() {
    Transform t = Transforms::squash_1qb_to_Rz_PhasedX();
    PredicatePtrMap s_ps;
    PredicateClassGuarantees g_postcons{
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "SquashRzPhasedX";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

const PassPtr &CnXPairwiseDecomposition() {
  static const PassPtr pp([]() {
    Transform t = Transforms::cnx_pairwise_decomposition();
    PredicatePtrMap s_ps;
    PredicateClassGuarantees g_postcons{
        {typeid(GateSetPredicate), Guarantee::Clear}};
    PostConditions postcon{s_ps, g_postcons, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "CnXPairwiseDecomposition";
    return std::make_shared<StandardPass>(s_ps, t, postcon, j);
  }());
  return pp;
}

const PassPtr &RemoveImplicitQubitPermutation() {
  static const PassPtr pp([]() {
    Transform t = Transform([](Circuit &circ) {
      bool has_implicit_wire_swaps = circ.has_implicit_wireswaps();
      circ.replace_all_implicit_wire_swaps();
      return has_implicit_wire_swaps;
    });
    PredicatePtrMap precons;
    PredicatePtr no_wire_swap = std::make_shared<NoWireSwapsPredicate>();
    PredicatePtrMap specific_postcons = {
        CompilationUnit::make_type_pair(no_wire_swap)};
    PredicateClassGuarantees generic_postcons;
    Guarantee default_postcon = Guarantee::Preserve;
    PostConditions postcons{
        specific_postcons, generic_postcons, default_postcon};
    PredicateClassGuarantees g_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear},
        {typeid(NoMidMeasurePredicate), Guarantee::Clear}};
    nlohmann::json j;
    j["name"] = "RemoveImplicitQubitPermutation";
    return std::make_shared<StandardPass>(precons, t, postcons, j);
  }());
  return pp;
}

const PassPtr &ZXGraphlikeOptimisation(bool allow_swaps) {
  auto create_pass_ptr = []<bool allow_wire_swaps>() -> PassPtr {
    Transform t = Transform([](Circuit &circ) {
      std::optional<std::string> name = circ.get_name();
      zx::ZXDiagram diag = circuit_to_zx(circ).first;
      zx::Rewrite::to_graphlike_form().apply(diag);
      zx::Rewrite::reduce_graphlike_form().apply(diag);
      zx::Rewrite::to_MBQC_diag().apply(diag);
      Circuit c = zx_to_circuit(diag);
      qubit_vector_t orig_qs = circ.all_qubits();
      qubit_vector_t c_qs = c.all_qubits();
      qubit_map_t qmap;
      for (unsigned i = 0; i < orig_qs.size(); ++i)
        qmap.insert({c_qs.at(i), orig_qs.at(i)});
      c.rename_units<Qubit, Qubit>(qmap);
      if constexpr (!allow_wire_swaps) {
        c.replace_all_implicit_wire_swaps();
      }
      circ = c;
      if (name.has_value()) {
        circ.set_name(*name);
      }
      return true;
    });
    OpTypeSet in_optypes = {OpType::Input, OpType::Output, OpType::noop,
                            OpType::SWAP,  OpType::H,      OpType::Rz,
                            OpType::Rx,    OpType::X,      OpType::Z,
                            OpType::CX,    OpType::CZ};
    PredicatePtrMap precons = {
        CompilationUnit::make_type_pair(
            std::make_shared<GateSetPredicate>(in_optypes)),
        CompilationUnit::make_type_pair(
            std::make_shared<NoClassicalBitsPredicate>())};
    PredicatePtrMap specific_postcons;
    Guarantee default_postcon = Guarantee::Preserve;
    PredicateClassGuarantees generic_postcons = {
        {typeid(GateSetPredicate), Guarantee::Clear},
        {typeid(ConnectivityPredicate), Guarantee::Clear},
        {typeid(NoWireSwapsPredicate), Guarantee::Clear}};
    PostConditions postcons{
        specific_postcons, generic_postcons, default_postcon};
    nlohmann::json j;
    j["name"] = "ZXGraphlikeOptimisation";
    return std::make_shared<StandardPass>(precons, t, postcons, j);
  };

  static const PassPtr pp{create_pass_ptr.operator()<true>()};
  if (allow_swaps) {
    return pp;
  }

  static const PassPtr pp_no_implicit_swaps{
      create_pass_ptr.operator()<false>()};
  return pp_no_implicit_swaps;
}

const PassPtr &RemovePhaseOps() {
  static const PassPtr pp([]() {
    Transform t = Transform([](Circuit &circ) {
      VertexList phases;
      BGL_FORALL_VERTICES(v, circ.dag, DAG) {
        Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
        OpType optype = op->get_type();
        while (optype == OpType::Conditional) {
          op = static_cast<const Conditional &>(*op).get_op();
          optype = op->get_type();
        }
        if (optype == OpType::Phase) {
          phases.push_back(v);
        }
      }
      circ.remove_vertices(
          phases, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);
      return !phases.empty();
    });
    const PredicatePtrMap no_precons;
    PostConditions postcons = {{}, {}, Guarantee::Preserve};
    nlohmann::json j;
    j["name"] = "RemovePhaseOps";
    return std::make_shared<StandardPass>(no_precons, t, postcons, j);
  }());
  return pp;
}

}  // namespace tket
