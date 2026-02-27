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

#include "tket/Predicates/PassGenerators.hpp"

#include <algorithm>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <sstream>
#include <string>

#include "tket/ArchAwareSynth/SteinerForest.hpp"
#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Converters/PhasePoly.hpp"
#include "tket/Mapping/LexiLabelling.hpp"
#include "tket/Mapping/LexiRouteRoutingMethod.hpp"
#include "tket/Mapping/MappingManager.hpp"
#include "tket/Mapping/RoutingMethodJson.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Placement/Placement.hpp"
#include "tket/Predicates/CompilationUnit.hpp"
#include "tket/Predicates/CompilerPass.hpp"
#include "tket/Predicates/PassLibrary.hpp"
#include "tket/Predicates/Predicates.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/CliffordOptimisation.hpp"
#include "tket/Transformations/CliffordResynthesis.hpp"
#include "tket/Transformations/ContextualReduction.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/GreedyPauliOptimisation.hpp"
#include "tket/Transformations/OptimisationPass.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Transformations/ThreeQubitSquash.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {

PassPtr gen_rebase_pass(
    const OpTypeSet& allowed_gates, const Circuit& cx_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement) {
  Transform t = Transforms::rebase_factory(
      allowed_gates, cx_replacement, tk1_replacement);

  PredicatePtrMap precons;
  OpTypeSet all_types(allowed_gates);
  all_types.insert(OpType::Measure);
  all_types.insert(OpType::Collapse);
  all_types.insert(OpType::Reset);
  all_types.insert(OpType::Phase);
  PredicatePtr postcon1 = std::make_shared<GateSetPredicate>(all_types);
  PredicatePtr postcon2 = std::make_shared<MaxTwoQubitGatesPredicate>();
  std::pair<const std::type_index, PredicatePtr> pair2 =
      CompilationUnit::make_type_pair(postcon1);
  PredicatePtrMap s_postcons{pair2, CompilationUnit::make_type_pair(postcon2)};
  PredicateClassGuarantees g_postcons{{pair2.first, Guarantee::Clear}};
  PostConditions pc = {s_postcons, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "RebaseCustom";
  j["basis_allowed"] = allowed_gates;
  j["basis_cx_replacement"] = cx_replacement;
  j["basis_tk1_replacement"] =
      "SERIALIZATION OF FUNCTIONS IS NOT YET SUPPORTED";
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_rebase_pass_via_tk2(
    const OpTypeSet& allowed_gates,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk2_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement) {
  Transform t = Transforms::rebase_factory_via_tk2(
      allowed_gates, tk1_replacement, tk2_replacement);

  PredicatePtrMap precons;
  OpTypeSet all_types(allowed_gates);
  all_types.insert(OpType::Measure);
  all_types.insert(OpType::Collapse);
  all_types.insert(OpType::Reset);
  all_types.insert(OpType::Phase);
  PredicatePtr postcon1 = std::make_shared<GateSetPredicate>(all_types);
  PredicatePtr postcon2 = std::make_shared<MaxTwoQubitGatesPredicate>();
  std::pair<const std::type_index, PredicatePtr> pair2 =
      CompilationUnit::make_type_pair(postcon1);
  PredicatePtrMap s_postcons{pair2, CompilationUnit::make_type_pair(postcon2)};
  PredicateClassGuarantees g_postcons{{pair2.first, Guarantee::Clear}};
  PostConditions pc = {s_postcons, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "RebaseCustomViaTK2";
  j["basis_allowed"] = allowed_gates;
  j["basis_tk1_replacement"] =
      "SERIALIZATION OF FUNCTIONS IS NOT YET SUPPORTED";
  j["basis_tk2_replacement"] =
      "SERIALIZATION OF FUNCTIONS IS NOT YET SUPPORTED";
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_squash_pass(
    const OpTypeSet& singleqs,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement,
    bool always_squash_symbols) {
  Transform t = Transforms::squash_factory(
      singleqs, tk1_replacement, always_squash_symbols);
  PostConditions postcon = {{}, {}, Guarantee::Preserve};
  PredicatePtrMap precons;
  // record pass config
  nlohmann::json j;
  j["name"] = "SquashCustom";
  j["basis_singleqs"] = singleqs;
  j["basis_tk1_replacement"] =
      "SERIALIZATION OF FUNCTIONS IS NOT YET SUPPORTED";
  j["always_squash_symbols"] = always_squash_symbols;
  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

static std::function<Circuit(const Expr&, const Expr&, const Expr&)>
find_tk1_replacement(const OpTypeSet& gateset) {
  if (gateset.contains(OpType::TK1)) {
    return CircPool::tk1_to_tk1;
  }
  if (gateset.contains(OpType::U3)) {
    return CircPool::tk1_to_u3;
  }
  if (gateset.contains(OpType::Rz) && gateset.contains(OpType::X) &&
      gateset.contains(OpType::SX)) {
    return CircPool::tk1_to_rzxsx;
  }
  if (gateset.contains(OpType::PhasedX) && gateset.contains(OpType::Rz)) {
    return CircPool::tk1_to_PhasedXRz;
  }
  if (gateset.contains(OpType::Rz) && gateset.contains(OpType::Rx)) {
    return CircPool::tk1_to_rzrx;
  }
  if (gateset.contains(OpType::Rx) && gateset.contains(OpType::Ry)) {
    return CircPool::tk1_to_rxry;
  }
  if (gateset.contains(OpType::Rz) && gateset.contains(OpType::H)) {
    return CircPool::tk1_to_rzh;
  }
  if (gateset.contains(OpType::Rz) && gateset.contains(OpType::SX)) {
    return CircPool::tk1_to_rzsx;
  }
  if (gateset.contains(OpType::GPI) && gateset.contains(OpType::GPI2)) {
    return CircPool::TK1_using_GPI;
  }
  if (gateset.contains(OpType::PhasedX)) {
    return CircPool::tk1_to_PhasedX;
  }
  throw Unsupported("No known decomposition from TK1 to available gateset.");
}

static Circuit find_cx_replacement(const OpTypeSet& gateset, bool allow_swaps) {
  if (gateset.contains(OpType::CX)) {
    return CircPool::CX();
  }
  if (gateset.contains(OpType::ZZMax)) {
    return CircPool::CX_using_ZZMax();
  }
  if (gateset.contains(OpType::ISWAPMax)) {
    return allow_swaps ? CircPool::CX_using_ISWAPMax_and_swap()
                       : CircPool::CX_using_ISWAPMax();
  }
  if (gateset.contains(OpType::XXPhase)) {
    return CircPool::CX_using_XXPhase_0();
  }
  if (gateset.contains(OpType::ECR)) {
    return CircPool::CX_using_ECR();
  }
  if (gateset.contains(OpType::CZ)) {
    return CircPool::H_CZ_H();
  }
  if (gateset.contains(OpType::AAMS)) {
    return CircPool::CX_using_AAMS();
  }
  throw Unsupported("No known decomposition from CX to available gateset.");
}

static std::function<Circuit(const Expr&, const Expr&, const Expr&)>
find_tk2_replacement(const OpTypeSet& gateset, bool allow_swaps) {
  if (gateset.contains(OpType::TK2)) {
    return allow_swaps ? CircPool::TK2_using_TK2_or_swap
                       : CircPool::TK2_using_TK2;
  }
  if (gateset.contains(OpType::ZZPhase)) {
    return allow_swaps ? CircPool::TK2_using_ZZPhase_and_swap
                       : CircPool::TK2_using_ZZPhase;
  }
  if (gateset.contains(OpType::CX)) {
    return allow_swaps ? CircPool::TK2_using_CX_and_swap
                       : CircPool::TK2_using_CX;
  }
  if (gateset.contains(OpType::ZZMax)) {
    return allow_swaps ? CircPool::TK2_using_ZZMax_and_swap
                       : CircPool::TK2_using_ZZMax;
  }
  if (gateset.contains(OpType::ISWAPMax)) {
    return allow_swaps ? CircPool::TK2_using_ISWAPMax_and_swap
                       : CircPool::TK2_using_ISWAPMax;
  }
  if (gateset.contains(OpType::AAMS)) {
    return CircPool::TK2_using_AAMS;
  }
  throw Unsupported("No known decomposition from TK2 to available gateset.");
}

PassPtr gen_auto_rebase_pass(const OpTypeSet& allowed_gates, bool allow_swaps) {
  auto find_rebase = [allowed_gates, allow_swaps]() {
    auto tk1_replacement = find_tk1_replacement(allowed_gates);
    if (allowed_gates.contains(OpType::CX) &&
        !allowed_gates.contains(OpType::TK2) && !allow_swaps) {
      return Transforms::rebase_factory(
          allowed_gates, CircPool::CX(), tk1_replacement);
    }
    try {
      return Transforms::rebase_factory_via_tk2(
          allowed_gates, tk1_replacement,
          find_tk2_replacement(allowed_gates, allow_swaps));
    } catch (const Unsupported&) {
    }
    try {
      return Transforms::rebase_factory(
          allowed_gates, find_cx_replacement(allowed_gates, allow_swaps),
          tk1_replacement);
    } catch (const Unsupported&) {
      throw Unsupported(
          "No known decomposition from CX or TK2 to available gateset.");
    }
  };
  Transform t = find_rebase();
  PredicatePtrMap precons;
  OpTypeSet all_types(allowed_gates);
  all_types.insert(OpType::Measure);
  all_types.insert(OpType::Reset);
  all_types.insert(OpType::Phase);
  PredicatePtr postcon1 = std::make_shared<GateSetPredicate>(all_types);
  PredicatePtr postcon2 = std::make_shared<MaxTwoQubitGatesPredicate>();
  std::pair<const std::type_index, PredicatePtr> pair2 =
      CompilationUnit::make_type_pair(postcon1);
  PredicatePtrMap s_postcons{pair2, CompilationUnit::make_type_pair(postcon2)};
  PredicateClassGuarantees g_postcons{{pair2.first, Guarantee::Clear}};
  PostConditions pc = {s_postcons, g_postcons, Guarantee::Preserve};
  // record pass config
  // sort the gateset
  std::vector<OpType> sorted_gates(allowed_gates.begin(), allowed_gates.end());
  std::sort(sorted_gates.begin(), sorted_gates.end());
  nlohmann::json j;
  j["name"] = "AutoRebase";
  j["basis_allowed"] = sorted_gates;
  j["allow_swaps"] = allow_swaps;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_auto_squash_pass(const OpTypeSet& singleqs) {
  auto find_squash = [singleqs]() {
    if (singleqs.contains(OpType::Rz) && singleqs.contains(OpType::PhasedX)) {
      return Transforms::squash_1qb_to_Rz_PhasedX(false);
    } else {
      auto tk1_replacement = find_tk1_replacement(singleqs);
      return Transforms::squash_factory(singleqs, tk1_replacement, false);
    }
  };
  Transform t = find_squash();
  PredicateClassGuarantees g_postcons = {
      {typeid(GateSetPredicate), Guarantee::Clear}};
  PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
  PredicatePtrMap precons;
  // record pass config
  // sort the gateset
  std::vector<OpType> sorted_gates(singleqs.begin(), singleqs.end());
  std::sort(sorted_gates.begin(), sorted_gates.end());
  nlohmann::json j;
  j["name"] = "AutoSquash";
  j["basis_singleqs"] = sorted_gates;
  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

// converting chains of p, q rotations to minimal triplets of p,q-rotations (p,
// q in {Rx,Ry,Rz})
PassPtr gen_euler_pass(const OpType& q, const OpType& p, bool strict) {
  PredicatePtrMap precons;

  Transform t = Transforms::squash_1qb_to_pqp(q, p, strict);
  PostConditions pc{{}, {}, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "EulerAngleReduction";
  j["euler_q"] = q;
  j["euler_p"] = p;
  j["euler_strict"] = strict;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_clifford_simp_pass(bool allow_swaps, OpType target_2qb_gate) {
  Transform t = Transforms::clifford_simp(allow_swaps, target_2qb_gate);
  PredicatePtrMap precons;
  PredicateClassGuarantees g_postcons;
  if (allow_swaps) {
    g_postcons = {
        {typeid(ConnectivityPredicate), Guarantee::Clear},
        {typeid(NoWireSwapsPredicate), Guarantee::Clear},
        {typeid(DirectednessPredicate), Guarantee::Clear}};
  }
  PostConditions postcon{{}, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "CliffordSimp";
  j["allow_swaps"] = allow_swaps;
  j["target_2qb_gate"] = target_2qb_gate;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_clifford_resynthesis_pass(
    std::optional<std::function<Circuit(const Circuit&)>> transform,
    bool allow_swaps) {
  Transform t = Transforms::clifford_resynthesis(transform, allow_swaps);
  PredicatePtrMap precons;
  PostConditions pc{{}, {}, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "CliffordResynthesis";
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_clifford_push_through_pass() {
  // Expects: Measure operations at end of circuit with
  // no classical gates to outputs to work, but
  // just makes no modification if this is not true
  Transform t = Transforms::push_cliffords_through_measures();
  PredicatePtrMap precons;
  // mutual diagonalisation circuit is not architecture aware
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear},
      {typeid(DirectednessPredicate), Guarantee::Clear}};

  PostConditions pc{{}, {}, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "CliffordPushThroughMeasures";
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_flatten_relabel_registers_pass(const std::string& label) {
  Transform t =
      Transform([=](Circuit& circuit, std::shared_ptr<unit_bimaps_t> maps) {
        unsigned n_qubits = circuit.n_qubits();
        circuit.remove_blank_wires(true);
        bool changed = circuit.n_qubits() < n_qubits;
        std::map<Qubit, Qubit> relabelling_map;
        std::vector<Qubit> all_qubits = circuit.all_qubits();
        for (unsigned i = 0; i < all_qubits.size(); i++) {
          relabelling_map.insert({all_qubits[i], Qubit(label, i)});
        }

        circuit.rename_units(relabelling_map);
        changed |= update_maps(maps, relabelling_map, relabelling_map);
        return changed;
      });
  PredicatePtrMap precons = {};
  PostConditions postcons = {};

  nlohmann::json j;
  j["name"] = "FlattenRelabelRegistersPass";
  j["label"] = label;
  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

PassPtr gen_rename_qubits_pass(const std::map<Qubit, Qubit>& qm) {
  Transform t =
      Transform([=](Circuit& circ, std::shared_ptr<unit_bimaps_t> maps) {
        bool changed = circ.rename_units(qm);
        changed |= update_maps(maps, qm, qm);
        return changed;
      });
  PredicatePtrMap precons = {};
  PostConditions postcons = {
      {},
      {{typeid(DefaultRegisterPredicate), Guarantee::Clear}},
      Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "RenameQubitsPass";
  j["qubit_map"] = qm;

  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

PassPtr gen_placement_pass(const Placement::Ptr& placement_ptr) {
  Transform::Transformation trans = [=](Circuit& circ,
                                        std::shared_ptr<unit_bimaps_t> maps) {
    // Fall back to line placement if graph placement fails
    bool changed;
    try {
      changed = placement_ptr->place(circ, maps);
    } catch (const std::runtime_error& e) {
      std::stringstream ss;
      ss << "PlacementPass failed with message: " << e.what()
         << " Fall back to LinePlacement.";
      tket_log()->warn(ss.str());
      Placement::Ptr line_placement_ptr = std::make_shared<LinePlacement>(
          placement_ptr->get_architecture_ref());
      changed = line_placement_ptr->place(circ, maps);
    }
    return changed;
  };
  Transform t = Transform(trans);
  PredicatePtr twoqbpred = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtr n_qubit_pred = std::make_shared<MaxNQubitsPredicate>(
      placement_ptr->get_architecture_ref().n_nodes());

  PredicatePtrMap precons{
      CompilationUnit::make_type_pair(twoqbpred),
      CompilationUnit::make_type_pair(n_qubit_pred)};
  PredicatePtr placement_pred = std::make_shared<PlacementPredicate>(
      placement_ptr->get_architecture_ref());
  PredicatePtrMap s_postcons{CompilationUnit::make_type_pair(placement_pred)};
  PostConditions pc{s_postcons, {}, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "PlacementPass";
  j["placement"] = placement_ptr;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_naive_placement_pass(const Architecture& arc) {
  Transform::Transformation trans = [=](Circuit& circ,
                                        std::shared_ptr<unit_bimaps_t> maps) {
    Placement np(arc);
    return np.place(circ, maps);
  };
  Transform t = Transform(trans);
  PredicatePtr n_qubit_pred =
      std::make_shared<MaxNQubitsPredicate>(arc.n_nodes());

  PredicatePtrMap precons{CompilationUnit::make_type_pair(n_qubit_pred)};
  PredicatePtr placement_pred = std::make_shared<PlacementPredicate>(arc);
  PredicatePtrMap s_postcons{CompilationUnit::make_type_pair(placement_pred)};
  PostConditions pc{s_postcons, {}, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "NaivePlacementPass";
  j["architecture"] = arc;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_full_mapping_pass(
    const Architecture& arc, const Placement::Ptr& placement_ptr,
    const std::vector<RoutingMethodPtr>& config) {
  std::vector<PassPtr> vpp = {
      gen_placement_pass(placement_ptr), gen_routing_pass(arc, config),
      gen_naive_placement_pass(arc)};
  return std::make_shared<SequencePass>(vpp);
}

PassPtr gen_default_mapping_pass(const Architecture& arc, bool delay_measures) {
  PassPtr return_pass = gen_full_mapping_pass(
      arc, std::make_shared<GraphPlacement>(arc),
      {std::make_shared<LexiLabellingMethod>(),
       std::make_shared<LexiRouteRoutingMethod>()});
  if (delay_measures) {
    return_pass = return_pass >> DelayMeasures();
  }
  return return_pass;
}

PassPtr gen_cx_mapping_pass(
    const Architecture& arc, const Placement::Ptr& placement_ptr,
    const std::vector<RoutingMethodPtr>& config, bool directed_cx,
    bool delay_measures) {
  OpTypeSet gate_set = all_single_qubit_types();
  gate_set.insert(OpType::CX);
  PassPtr rebase_pass = gen_auto_rebase_pass(gate_set);
  PassPtr return_pass =
      rebase_pass >> gen_full_mapping_pass(arc, placement_ptr, config);
  if (delay_measures) return_pass = return_pass >> DelayMeasures();
  return_pass = return_pass >> rebase_pass >>
                gen_decompose_routing_gates_to_cxs_pass(arc, directed_cx);
  Transform t{[=](Circuit& circ, std::shared_ptr<unit_bimaps_t> maps) {
    // Relabel all two-qubit gates
    CompilationUnit cu(circ);
    bool changed = rebase_pass->apply(cu);
    circ = cu.get_circ_ref();
    // Now try placement, falling back on "LinePlacement" if "GraphPlacement"
    // fails
    try {
      changed |= placement_ptr->place(circ, maps);
    } catch (const std::runtime_error& e) {
      std::stringstream ss;
      ss << "PlacementPass failed with message: " << e.what()
         << " Fall back to LinePlacement.";
      tket_log()->warn(ss.str());
      Placement::Ptr line_placement_ptr = std::make_shared<LinePlacement>(
          placement_ptr->get_architecture_ref());
      changed |= line_placement_ptr->place(circ, maps);
    }
    // Now route
    MappingManager mm(std::make_shared<Architecture>(arc));
    changed |= mm.route_circuit_with_maps(circ, config, maps);

    // Now decompose routing gates to cx gates (this won't change maps)
    CompilationUnit cu1(circ);
    if (delay_measures) {
      changed |= DelayMeasures()->apply(cu1);
    }
    changed |=
        gen_decompose_routing_gates_to_cxs_pass(arc, directed_cx)->apply(cu1);
    circ = cu1.get_circ_ref();
    return changed;
  }};
  PassConditions conditions = return_pass->get_conditions();
  nlohmann::json j;
  j["name"] = "CXMappingPass";
  j["architecture"] = arc;
  j["placement"] = placement_ptr;
  j["routing_config"] = config;
  j["directed"] = directed_cx;
  j["delay_measures"] = delay_measures;
  return std::make_shared<StandardPass>(
      conditions.first, t, conditions.second, j);
}

PassPtr gen_routing_pass(
    const Architecture& arc, const std::vector<RoutingMethodPtr>& config) {
  Transform::Transformation trans = [=](Circuit& circ,
                                        std::shared_ptr<unit_bimaps_t> maps) {
    MappingManager mm(std::make_shared<Architecture>(arc));
    return mm.route_circuit_with_maps(circ, config, maps);
  };
  Transform t = Transform(trans);

  PredicatePtr twoqbpred = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtr n_qubit_pred =
      std::make_shared<MaxNQubitsPredicate>(arc.n_nodes());
  PredicatePtrMap precons{
      CompilationUnit::make_type_pair(twoqbpred),
      CompilationUnit::make_type_pair(n_qubit_pred)};

  PredicatePtr postcon1 = std::make_shared<ConnectivityPredicate>(arc);
  std::pair<const std::type_index, PredicatePtr> pair1 =
      CompilationUnit::make_type_pair(postcon1);
  PredicatePtr postcon2 = std::make_shared<NoWireSwapsPredicate>();
  PredicatePtrMap s_postcons{pair1, CompilationUnit::make_type_pair(postcon2)};
  std::type_index gateset_ti = typeid(GateSetPredicate);
  // clears all Routing predicates apart from the ones which are specified
  // clears all GateSet predicates (inserts SWAPs and BRIDGEs)
  PredicateClassGuarantees g_postcons{
      {pair1.first, Guarantee::Clear},
      {gateset_ti, Guarantee::Clear},
      {typeid(MaxTwoQubitGatesPredicate), Guarantee::Clear}};
  PostConditions pc{s_postcons, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "RoutingPass";
  j["routing_config"] = config;
  j["architecture"] = arc;

  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_placement_pass_phase_poly(
    const Architecture& arc, unsigned maximum_matches, unsigned timeout,
    unsigned maximum_pattern_gates, unsigned maximum_pattern_depth) {
  Transform::Transformation trans = [=](Circuit& circ,
                                        std::shared_ptr<unit_bimaps_t> maps) {
    if (arc.n_nodes() < circ.n_qubits()) {
      throw CircuitInvalidity(
          "Circuit has more qubits than the architecture has nodes.");
    }

    qubit_vector_t q_vec = circ.all_qubits();
    std::map<Qubit, Node> qubit_to_nodes;
    unsigned counter = 0;

    for (Node x : arc.nodes()) {
      if (counter < circ.n_qubits()) {
        qubit_to_nodes.insert({q_vec[counter], x});
        ++counter;
      }
    }

    circ.rename_units(qubit_to_nodes);
    update_maps(maps, qubit_to_nodes, qubit_to_nodes);

    return true;
  };
  Transform t = Transform(trans);

  PredicatePtr no_wire_swap = std::make_shared<NoWireSwapsPredicate>();
  PredicatePtrMap precons{CompilationUnit::make_type_pair(no_wire_swap)};

  PredicatePtr placement_pred = std::make_shared<PlacementPredicate>(arc);
  PredicatePtr n_qubit_pred =
      std::make_shared<MaxNQubitsPredicate>(arc.n_nodes());

  PredicatePtrMap s_postcons{
      CompilationUnit::make_type_pair(placement_pred),
      CompilationUnit::make_type_pair(n_qubit_pred),
      CompilationUnit::make_type_pair(no_wire_swap)};

  PostConditions pc{s_postcons, {}, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "PlacementPass";
  Placement::Ptr pp = std::make_shared<GraphPlacement>(
      arc, maximum_matches, timeout, maximum_pattern_gates,
      maximum_pattern_depth);
  j["params"]["placement"] = pp;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr aas_routing_pass(
    const Architecture& arc, const unsigned lookahead,
    const aas::CNotSynthType cnotsynthtype) {
  Transform::SimpleTransformation trans = [=](Circuit& circ) {
    // check input:
    if (lookahead == 0) {
      throw std::logic_error("lookahead must be > 0");
    }
    if (arc.n_nodes() < circ.n_qubits()) {
      throw CircuitInvalidity(
          "Circuit has more qubits than the architecture has nodes.");
    }

    // this pass is not able to handle implicit wire swaps
    // this is additionally assured by a precondition of this pass
    TKET_ASSERT(!circ.has_implicit_wireswaps());

    qubit_vector_t all_qu = circ.all_qubits();

    Circuit input_circ = circ;

    VertexList bin;

    BGL_FORALL_VERTICES(v, circ.dag, DAG) {
      if (!circ.detect_boundary_Op(v)) {
        bin.push_back(v);
      }
    }

    circ.remove_vertices(
        bin, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);

    for (const Command& com : input_circ) {
      OpType ot = com.get_op_ptr()->get_type();
      unit_vector_t qbs = com.get_args();
      switch (ot) {
        case OpType::PhasePolyBox: {
          Op_ptr op = com.get_op_ptr();
          const auto& ppb = dynamic_cast<const PhasePolyBox&>(*op);

          Circuit result =
              aas::phase_poly_synthesis(arc, ppb, lookahead, cnotsynthtype);

          for (const Command& res_com : result) {
            OpType ot = res_com.get_op_ptr()->get_type();
            unit_vector_t res_qbs = res_com.get_args();

            switch (ot) {
              case OpType::CX: {
                circ.add_op<Node>(ot, {Node(res_qbs[0]), Node(res_qbs[1])});
                break;
              }
              case OpType::Rz: {
                auto angle = res_com.get_op_ptr()->get_params().at(0);
                circ.add_op<Node>(ot, angle, {Node(res_qbs[0])});
                break;
              }
              default: {
                TKET_ASSERT(!"Invalid gate type in phase poly box");
              }
            }
          }
          break;
        }
        case OpType::H: {
          circ.add_op<Node>(ot, {Node(qbs[0])});
          break;
        }
        case OpType::Collapse: {
          circ.add_op<Node>(ot, {Node(qbs[0])});
          break;
        }
        case OpType::Reset: {
          circ.add_op<Node>(ot, {Node(qbs[0])});
          break;
        }
        case OpType::Measure: {
          circ.add_measure(Node(qbs[0]), Bit(qbs[1]));
          break;
        }
        default: {
          throw BadOpType("Invalid gate in input of AAS routing", ot);
        }
      }
    }

    return true;
  };
  Transform t = Transform(trans);

  PredicatePtr placedpred = std::make_shared<PlacementPredicate>(arc);
  PredicatePtr n_qubit_pred =
      std::make_shared<MaxNQubitsPredicate>(arc.n_nodes());
  PredicatePtr no_wire_swap = std::make_shared<NoWireSwapsPredicate>();

  PredicatePtrMap precons{
      CompilationUnit::make_type_pair(placedpred),
      CompilationUnit::make_type_pair(n_qubit_pred),
      CompilationUnit::make_type_pair(no_wire_swap)};

  PredicatePtr postcon1 = std::make_shared<ConnectivityPredicate>(arc);
  std::pair<const std::type_index, PredicatePtr> pair1 =
      CompilationUnit::make_type_pair(postcon1);
  PredicatePtr postcon2 = std::make_shared<NoWireSwapsPredicate>();
  PredicatePtrMap s_postcons{pair1, CompilationUnit::make_type_pair(postcon2)};
  std::type_index gateset_ti = typeid(GateSetPredicate);
  PredicateClassGuarantees g_postcons{
      {pair1.first, Guarantee::Clear}, {gateset_ti, Guarantee::Clear}};
  PostConditions pc{s_postcons, g_postcons, Guarantee::Preserve};

  nlohmann::json j;
  j["name"] = "AASRoutingPass";
  j["architecture"] = arc;

  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_full_mapping_pass_phase_poly(
    const Architecture& arc, const unsigned lookahead,
    const aas::CNotSynthType cnotsynthtype,
    unsigned graph_placement_maximum_matches, unsigned graph_placement_timeout,
    unsigned graph_placement_maximum_pattern_gates,
    unsigned graph_placement_maximum_pattern_depth) {
  return ComposePhasePolyBoxes() >>
         gen_placement_pass_phase_poly(
             arc, graph_placement_maximum_matches, graph_placement_timeout,
             graph_placement_maximum_pattern_gates,
             graph_placement_maximum_pattern_depth) >>
         aas_routing_pass(arc, lookahead, cnotsynthtype);
}

PassPtr gen_directed_cx_routing_pass(
    const Architecture& arc, const std::vector<RoutingMethodPtr>& config) {
  OpTypeSet multis = {OpType::CX, OpType::BRIDGE, OpType::SWAP};
  OpTypeSet gate_set = all_single_qubit_types();
  gate_set.insert(multis.begin(), multis.end());

  return gen_routing_pass(arc, config) >>
         gen_rebase_pass(gate_set, CircPool::CX(), CircPool::tk1_to_tk1) >>
         gen_decompose_routing_gates_to_cxs_pass(arc, true);
}

PassPtr gen_decompose_routing_gates_to_cxs_pass(
    const Architecture& arc, bool directed) {
  PredicateClassGuarantees g_postcons{
      {typeid(GateSetPredicate), Guarantee::Clear}};
  PredicatePtrMap precons;
  PredicatePtrMap s_postcons;
  Transform t = Transforms::decompose_SWAP_to_CX(arc) >>
                Transforms::decompose_BRIDGE_to_CX() >>
                Transforms::remove_redundancies();
  if (directed) {
    PredicatePtr twoqbpred = std::make_shared<MaxTwoQubitGatesPredicate>();
    PredicatePtr connected = std::make_shared<ConnectivityPredicate>(arc);
    PredicatePtr directedpred = std::make_shared<DirectednessPredicate>(arc);
    precons = {CompilationUnit::make_type_pair(connected)};
    s_postcons = {
        CompilationUnit::make_type_pair(directedpred),
        CompilationUnit::make_type_pair(twoqbpred)};
    t = t >> Transforms::decompose_CX_directed(arc) >>
        Transforms::remove_redundancies();
  }
  PostConditions pc{s_postcons, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "DecomposeSwapsToCXs";
  j["directed"] = directed;
  j["architecture"] = arc;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr gen_user_defined_swap_decomp_pass(const Circuit& replacement_circ) {
  Transform t = Transforms::decompose_SWAP(replacement_circ);
  PredicateClassGuarantees g_postcons{
      {typeid(GateSetPredicate), Guarantee::Clear}};
  PostConditions pc{{}, g_postcons, Guarantee::Preserve};
  PredicatePtrMap precons;

  // record pass config
  nlohmann::json j;
  j["name"] = "DecomposeSwapsToCircuit";
  j["swap_replacement"] = replacement_circ;
  return std::make_shared<StandardPass>(precons, t, pc, j);
}

PassPtr KAKDecomposition(
    OpType target_2qb_gate, double cx_fidelity, bool allow_swaps) {
  Transform t =
      Transforms::two_qubit_squash(target_2qb_gate, cx_fidelity, allow_swaps);
  PredicatePtrMap precons;
  PredicateClassGuarantees g_postcons = {
      {typeid(DirectednessPredicate), Guarantee::Clear},
      {typeid(CliffordCircuitPredicate), Guarantee::Clear}};
  PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "KAKDecomposition";
  j["target_2qb_gate"] = target_2qb_gate;
  j["fidelity"] = cx_fidelity;
  j["allow_swaps"] = allow_swaps;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr DecomposeTK2(bool allow_swaps) { return DecomposeTK2({}, allow_swaps); }
PassPtr DecomposeTK2(const Transforms::TwoQbFidelities& fid, bool allow_swaps) {
  Transform t = Transforms::decompose_TK2(fid, allow_swaps);
  PredicatePtr normalised_tk2 = std::make_shared<NormalisedTK2Predicate>();
  PredicatePtrMap precons{CompilationUnit::make_type_pair(normalised_tk2)};
  PredicateClassGuarantees preserve_all;
  PostConditions postcons{{}, preserve_all, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "DecomposeTK2";
  j["allow_swaps"] = allow_swaps;
  nlohmann::json fid_json;
  fid_json["CX"] = fid.CX_fidelity;
  if (fid.ZZPhase_fidelity.has_value()) {
    std::visit(
        overloaded{
            [&fid_json](double arg) { fid_json["ZZPhase"] = arg; },
            [&fid_json](std::function<double(double)>) {
              fid_json["ZZPhase"] =
                  "SERIALIZATION OF FUNCTIONS IS NOT SUPPORTED";
            }},
        fid.ZZPhase_fidelity.value());
  } else {
    fid_json["ZZPhase"] = std::nullptr_t{};
  }
  fid_json["ZZMax"] = fid.ZZMax_fidelity;
  j["fidelities"] = fid_json;
  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

PassPtr ThreeQubitSquash(bool allow_swaps) {
  Transform t = Transforms::two_qubit_squash(allow_swaps) >>
                Transforms::three_qubit_squash() >>
                Transforms::clifford_simp(allow_swaps);
  OpTypeSet ots{all_single_qubit_types()};
  ots.insert(all_classical_types().begin(), all_classical_types().end());
  ots.insert(OpType::CX);
  PredicatePtr gate_pred = std::make_shared<GateSetPredicate>(ots);
  PredicatePtrMap precons{CompilationUnit::make_type_pair(gate_pred)};
  PredicateClassGuarantees g_postcons = {
      {typeid(DirectednessPredicate), Guarantee::Clear},
      {typeid(CliffordCircuitPredicate), Guarantee::Clear}};
  PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "ThreeQubitSquash";
  j["allow_swaps"] = allow_swaps;
  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr PeepholeOptimise2Q(bool allow_swaps) {
  OpTypeSet after_set = {OpType::TK1,      OpType::CX,    OpType::Measure,
                         OpType::Collapse, OpType::Reset, OpType::Phase};
  PredicatePtrMap precons = {};
  PredicatePtr out_gateset = std::make_shared<GateSetPredicate>(after_set);
  PredicatePtr max2qb = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtrMap postcon_spec = {
      CompilationUnit::make_type_pair(out_gateset),
      CompilationUnit::make_type_pair(max2qb)};
  PredicateClassGuarantees g_postcons;
  if (allow_swaps) {
    g_postcons = {
        {typeid(ConnectivityPredicate), Guarantee::Clear},
        {typeid(NoWireSwapsPredicate), Guarantee::Clear},
    };
  }
  g_postcons.insert({typeid(DirectednessPredicate), Guarantee::Clear});
  g_postcons.insert({typeid(CliffordCircuitPredicate), Guarantee::Clear});
  PostConditions postcon{postcon_spec, g_postcons, Guarantee::Preserve};
  // record pass config
  nlohmann::json j;
  j["name"] = "PeepholeOptimise2Q";
  j["allow_swaps"] = allow_swaps;
  return std::make_shared<StandardPass>(
      precons, Transforms::peephole_optimise_2q(allow_swaps), postcon, j);
}

PassPtr FullPeepholeOptimise(bool allow_swaps, OpType target_2qb_gate) {
  OpTypeSet after_set = {
      OpType::TK1, OpType::Measure, OpType::Collapse, OpType::Reset,
      OpType::Phase};
  after_set.insert(target_2qb_gate);
  PredicatePtrMap precons = {};
  after_set.insert(all_classical_types().begin(), all_classical_types().end());
  PredicatePtr out_gateset = std::make_shared<GateSetPredicate>(after_set);
  PredicatePtr max2qb = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtrMap postcon_spec = {
      CompilationUnit::make_type_pair(out_gateset),
      CompilationUnit::make_type_pair(max2qb)};
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear}};
  PostConditions postcon = {postcon_spec, g_postcons, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "FullPeepholeOptimise";
  j["allow_swaps"] = allow_swaps;
  j["target_2qb_gate"] = target_2qb_gate;
  return std::make_shared<StandardPass>(
      precons, Transforms::full_peephole_optimise(allow_swaps, target_2qb_gate),
      postcon, j);
}

PassPtr gen_optimise_phase_gadgets(CXConfigType cx_config) {
  Transform t = Transforms::optimise_via_PhaseGadget(cx_config);
  PredicatePtr ccontrol_pred = std::make_shared<NoClassicalControlPredicate>();
  PredicatePtrMap precons{CompilationUnit::make_type_pair(ccontrol_pred)};
  OpTypeSet after_set{OpType::Measure, OpType::Collapse, OpType::Reset,
                      OpType::Phase,   OpType::TK1,      OpType::CX};
  std::type_index ti = typeid(ConnectivityPredicate);
  PredicatePtr out_gateset = std::make_shared<GateSetPredicate>(after_set);
  PredicatePtr max2qb = std::make_shared<MaxTwoQubitGatesPredicate>();
  PredicatePtrMap postcon_spec = {
      CompilationUnit::make_type_pair(out_gateset),
      CompilationUnit::make_type_pair(max2qb)};
  PredicateClassGuarantees g_postcons{{ti, Guarantee::Clear}};
  PostConditions postcon{postcon_spec, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "OptimisePhaseGadgets";
  j["cx_config"] = cx_config;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_pairwise_pauli_gadgets(CXConfigType cx_config) {
  Transform t = Transforms::pairwise_pauli_gadgets(cx_config);
  PredicatePtr ccontrol_pred = std::make_shared<NoClassicalControlPredicate>();
  PredicatePtr simple = std::make_shared<DefaultRegisterPredicate>();
  PredicatePtrMap precons = {
      CompilationUnit::make_type_pair(simple),
      CompilationUnit::make_type_pair(ccontrol_pred)};
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear},
      {typeid(NoWireSwapsPredicate), Guarantee::Clear}};
  OpTypeSet ots2 = {OpType::CX, OpType::Z,  OpType::X,  OpType::S,
                    OpType::V,  OpType::U1, OpType::U2, OpType::U3};
  ots2.insert(all_projective_types().begin(), all_projective_types().end());
  PredicatePtr outp_gates = std::make_shared<GateSetPredicate>(ots2);
  PredicatePtrMap spec_postcons = {CompilationUnit::make_type_pair(outp_gates)};
  PostConditions postcon{spec_postcons, g_postcons, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "OptimisePairwiseGadgets";
  j["cx_config"] = cx_config;
  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_pauli_exponentials(
    Transforms::PauliSynthStrat strat, CXConfigType cx_config) {
  Transform t = Transforms::synthesise_pauli_graph(strat, cx_config);
  PredicatePtr ccontrol_pred = std::make_shared<NoClassicalControlPredicate>();
  PredicatePtr mid_pred = std::make_shared<NoMidMeasurePredicate>();
  OpTypeSet ins = {OpType::Z,       OpType::X,           OpType::Y,
                   OpType::S,       OpType::Sdg,         OpType::V,
                   OpType::Vdg,     OpType::H,           OpType::CX,
                   OpType::CY,      OpType::CZ,          OpType::SWAP,
                   OpType::Rz,      OpType::Rx,          OpType::Ry,
                   OpType::T,       OpType::Tdg,         OpType::ZZMax,
                   OpType::ZZPhase, OpType::PhaseGadget, OpType::XXPhase,
                   OpType::YYPhase, OpType::PauliExpBox, OpType::Measure,
                   OpType::PhasedX, OpType::SX,          OpType::SXdg};
  PredicatePtr in_gates = std::make_shared<GateSetPredicate>(ins);
  PredicatePtrMap precons{
      CompilationUnit::make_type_pair(ccontrol_pred),
      CompilationUnit::make_type_pair(mid_pred),
      CompilationUnit::make_type_pair(in_gates)};
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear},
      {typeid(NoWireSwapsPredicate), Guarantee::Clear}};
  PostConditions postcon{{}, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "PauliExponentials";
  j["cx_config"] = cx_config;
  j["pauli_synth_strat"] = strat;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_synthesise_pauli_graph(
    Transforms::PauliSynthStrat strat, CXConfigType cx_config) {
  std::vector<PassPtr> seq = {
      gen_pauli_exponentials(strat, cx_config), DecomposeBoxes()};
  return std::make_shared<SequencePass>(seq);
}

PassPtr gen_greedy_pauli_simp(
    double discount_rate, double depth_weight, unsigned max_lookahead,
    unsigned max_tqe_candidates, unsigned seed, bool allow_zzphase,
    unsigned thread_timeout, bool only_reduce, unsigned trials) {
  Transform t = Transform([discount_rate, depth_weight, max_lookahead,
                           max_tqe_candidates, seed, allow_zzphase,
                           thread_timeout, only_reduce, trials](Circuit& circ) {
    Transform gpo = Transforms::greedy_pauli_optimisation(
        discount_rate, depth_weight, max_lookahead, max_tqe_candidates, seed,
        allow_zzphase, thread_timeout, trials);
    if (only_reduce) {
      Circuit gpo_circ = circ;
      // comparison will be inaccurate if circuit has PauliExpBox
      gpo_circ.decompose_boxes_recursively();
      unsigned original_n_2qb_gates = gpo_circ.count_n_qubit_gates(2);
      unsigned original_n_gates = gpo_circ.n_gates();
      unsigned original_depth = gpo_circ.depth();
      if (gpo.apply(gpo_circ) &&
          std::make_tuple(
              gpo_circ.count_n_qubit_gates(2), gpo_circ.n_gates(),
              gpo_circ.depth()) <
              std::make_tuple(
                  original_n_2qb_gates, original_n_gates, original_depth)) {
        circ = gpo_circ;
        return true;
      }
      return false;
    }
    return gpo.apply(circ);
  });
  OpTypeSet ins = {
      OpType::Phase,
      OpType::Z,
      OpType::X,
      OpType::Y,
      OpType::S,
      OpType::Sdg,
      OpType::V,
      OpType::Vdg,
      OpType::H,
      OpType::CX,
      OpType::CY,
      OpType::CZ,
      OpType::SWAP,
      OpType::Rz,
      OpType::Rx,
      OpType::Ry,
      OpType::T,
      OpType::Tdg,
      OpType::ZZMax,
      OpType::ZZPhase,
      OpType::PhaseGadget,
      OpType::XXPhase,
      OpType::YYPhase,
      OpType::Measure,
      OpType::PhasedX,
      OpType::TK1,
      OpType::TwinPhasedX,
      OpType::PhasedXX,
      OpType::Reset,
      OpType::Conditional,
      OpType::PauliExpBox,
      OpType::PauliExpPairBox,
      OpType::PauliExpCommutingSetBox};

  ins.insert(all_classical_types().begin(), all_classical_types().end());
  PredicatePtr in_gates = std::make_shared<GateSetPredicate>(ins);
  PredicatePtrMap precons{CompilationUnit::make_type_pair(in_gates)};
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear},
      {typeid(NoWireSwapsPredicate), Guarantee::Clear}};
  PostConditions postcon{{}, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "GreedyPauliSimp";
  j["discount_rate"] = discount_rate;
  j["depth_weight"] = depth_weight;
  j["max_lookahead"] = max_lookahead;
  j["max_tqe_candidates"] = max_tqe_candidates;
  j["seed"] = seed;
  j["allow_zzphase"] = allow_zzphase;
  j["thread_timeout"] = thread_timeout;
  j["only_reduce"] = only_reduce;
  j["trials"] = trials;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_special_UCC_synthesis(
    Transforms::PauliSynthStrat strat, CXConfigType cx_config) {
  Transform t = Transforms::special_UCC_synthesis(strat, cx_config);
  PredicatePtr ccontrol_pred = std::make_shared<NoClassicalControlPredicate>();
  PredicatePtrMap precons{CompilationUnit::make_type_pair(ccontrol_pred)};
  PredicateClassGuarantees g_postcons = {
      {typeid(ConnectivityPredicate), Guarantee::Clear},
      {typeid(NoWireSwapsPredicate), Guarantee::Clear}};
  PostConditions postcon{{}, g_postcons, Guarantee::Preserve};

  // record pass config
  nlohmann::json j;
  j["name"] = "GuidedPauliSimp";
  j["cx_config"] = cx_config;
  j["pauli_synth_strat"] = strat;

  return std::make_shared<StandardPass>(precons, t, postcon, j);
}

PassPtr gen_simplify_initial(
    Transforms::AllowClassical allow_classical,
    Transforms::CreateAllQubits create_all_qubits,
    std::shared_ptr<const Circuit> xcirc) {
  Transform t =
      Transforms::simplify_initial(allow_classical, create_all_qubits, xcirc);
  PredicatePtrMap no_precons;
  PredicateClassGuarantees g_postcons;

  // GateSetPredicate not preserved because X gates (or their specified
  // equivalents) may be introduced, and if allow_classical then classical
  // gates may also be introduced.
  g_postcons[typeid(GateSetPredicate)] = Guarantee::Clear;

  PostConditions postcon = {{}, g_postcons, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "SimplifyInitial";
  j["allow_classical"] = (allow_classical == Transforms::AllowClassical::Yes);
  j["create_all_qubits"] =
      (create_all_qubits == Transforms::CreateAllQubits::Yes);
  if (xcirc) j["x_circuit"] = *xcirc;
  return std::make_shared<StandardPass>(no_precons, t, postcon, j);
}

PassPtr gen_contextual_pass(
    Transforms::AllowClassical allow_classical,
    std::shared_ptr<const Circuit> xcirc) {
  std::vector<PassPtr> seq = {
      RemoveDiscarded(), SimplifyMeasured(),
      gen_simplify_initial(
          allow_classical, Transforms::CreateAllQubits::No, xcirc),
      RemoveRedundancies()};
  return std::make_shared<SequencePass>(seq);
}

PassPtr PauliSquash(Transforms::PauliSynthStrat strat, CXConfigType cx_config) {
  std::vector<PassPtr> seq = {
      gen_synthesise_pauli_graph(strat, cx_config), FullPeepholeOptimise()};
  return std::make_shared<SequencePass>(seq);
}

PassPtr RoundAngles(unsigned n, bool only_zeros) {
  Transform t = Transforms::round_angles(n, only_zeros);
  PredicatePtrMap precons;
  PostConditions postcons = {{}, {}, Guarantee::Preserve};
  nlohmann::json j;
  j["name"] = "RoundAngles";
  j["n"] = n;
  j["only_zeros"] = only_zeros;
  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

PassPtr CustomPass(
    std::function<Circuit(const Circuit&)> transform,
    const std::string& label) {
  Transform t{[transform](Circuit& circ) {
    Circuit circ_out = transform(circ);
    bool success = circ_out != circ;
    circ = circ_out;
    return success;
  }};

  PredicatePtrMap precons;
  PostConditions postcons;
  nlohmann::json j;
  j["name"] = "CustomPass";
  j["label"] = label;
  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

PassPtr CustomPassMap(
    std::function<
        std::pair<Circuit, std::pair<unit_map_t, unit_map_t>>(const Circuit&)>
        transform,
    const std::string& label) {
  Transform t{[=](Circuit& circ, std::shared_ptr<unit_bimaps_t> maps) {
    // run transformation
    std::pair<Circuit, std::pair<unit_map_t, unit_map_t>> circ_maps =
        transform(circ);

    // get circuit
    Circuit circ_out = circ_maps.first;
    bool success = circ_out != circ;
    circ = circ_out;

    // get maps
    unit_bimap_t init_m, final_m;
    for (const auto& [key, value] : circ_maps.second.first) {
      init_m.insert({key, value});
    }
    for (const auto& [key, value] : circ_maps.second.second) {
      final_m.insert({key, value});
    }
    maps->initial = init_m;
    maps->final = final_m;

    return success;
  }};

  PredicatePtrMap precons;
  PostConditions postcons;
  nlohmann::json j;
  j["name"] = "CustomPassMap";
  j["label"] = label;
  return std::make_shared<StandardPass>(precons, t, postcons, j);
}

}  // namespace tket
