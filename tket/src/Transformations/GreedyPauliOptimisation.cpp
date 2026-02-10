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

#include "tket/Transformations/GreedyPauliOptimisation.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <queue>
#include <random>

#include "tket/Circuit/PauliExpBoxes.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Transformations/GreedyPauliOptimisationLookupTables.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {

namespace Transforms {

namespace GreedyPauliSimp {

static TQE sample_random_tqe(const std::vector<TQE>& vec, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
  size_t random_index = dist(rng);
  auto it = vec.begin();
  std::advance(it, random_index);
  return *it;
}

static std::vector<TQE> sample_tqes(
    const std::set<TQE>& tqes, size_t k, unsigned seed) {
  // https://stackoverflow.com/a/59090754
  size_t unsampled_sz = tqes.size();
  auto first = std::begin(tqes);
  std::vector<TQE> vec;
  std::mt19937 rng(seed);
  vec.reserve(std::min(k, unsampled_sz));
  for (k = std::min(k, unsampled_sz); k != 0; ++first) {
    auto r = std::uniform_int_distribution<std::size_t>(0, --unsampled_sz)(rng);
    if (r < k) {
      vec.push_back(*first);
      --k;
    }
  }
  return vec;
}

static void apply_rot2q_to_circ(const Rotation2Q& rot, Circuit& circ) {
  if (rot.p_a == Pauli::X) {
    circ.add_op<unsigned>(OpType::H, {rot.a});
  } else if (rot.p_a == Pauli::Y) {
    circ.add_op<unsigned>(OpType::V, {rot.a});
  }
  if (rot.p_b == Pauli::X) {
    circ.add_op<unsigned>(OpType::H, {rot.b});
  } else if (rot.p_b == Pauli::Y) {
    circ.add_op<unsigned>(OpType::V, {rot.b});
  }
  circ.add_op<unsigned>(OpType::ZZPhase, rot.angle, {rot.a, rot.b});
  if (rot.p_a == Pauli::X) {
    circ.add_op<unsigned>(OpType::H, {rot.a});
  } else if (rot.p_a == Pauli::Y) {
    circ.add_op<unsigned>(OpType::Vdg, {rot.a});
  }
  if (rot.p_b == Pauli::X) {
    circ.add_op<unsigned>(OpType::H, {rot.b});
  } else if (rot.p_b == Pauli::Y) {
    circ.add_op<unsigned>(OpType::Vdg, {rot.b});
  }
}

static void apply_tqe_to_circ(const TQE& tqe, Circuit& circ) {
  const TQEType& gate_type = tqe.type;
  const unsigned& a = tqe.a;
  const unsigned& b = tqe.b;
  switch (gate_type) {
    case TQEType::XX:
      circ.add_op<unsigned>(OpType::H, {a});
      circ.add_op<unsigned>(OpType::CX, {a, b});
      circ.add_op<unsigned>(OpType::H, {a});
      break;
    case TQEType::XY:
      circ.add_op<unsigned>(OpType::H, {a});
      circ.add_op<unsigned>(OpType::CY, {a, b});
      circ.add_op<unsigned>(OpType::H, {a});
      break;
    case TQEType::XZ:
      circ.add_op<unsigned>(OpType::CX, {b, a});
      break;
    case TQEType::YX:
      circ.add_op<unsigned>(OpType::H, {b});
      circ.add_op<unsigned>(OpType::CY, {b, a});
      circ.add_op<unsigned>(OpType::H, {b});
      break;
    case TQEType::YY:
      circ.add_op<unsigned>(OpType::V, {a});
      circ.add_op<unsigned>(OpType::CY, {a, b});
      circ.add_op<unsigned>(OpType::Vdg, {a});
      break;
    case TQEType::YZ:
      circ.add_op<unsigned>(OpType::CY, {b, a});
      break;
    case TQEType::ZX:
      circ.add_op<unsigned>(OpType::CX, {a, b});
      break;
    case TQEType::ZY:
      circ.add_op<unsigned>(OpType::CY, {a, b});
      break;
    case TQEType::ZZ:
      circ.add_op<unsigned>(OpType::CZ, {a, b});
      break;
  }
}

// return the sum of the cost increases on remaining tableau nodes
static double default_tableau_tqe_cost(
    const std::vector<PauliNode_ptr>& rows,
    const std::vector<unsigned>& remaining_indices, const TQE& tqe,
    const unsigned& max_lookahead) {
  double cost = 0;
  unsigned count = 0;
  for (const unsigned& index : remaining_indices) {
    cost += rows[index]->tqe_cost_increase(tqe);
    if (++count >= max_lookahead) break;
  }
  return cost;
}

// return the weighted sum of the cost increases on remaining nodes
// we discount the weight after each set
static double default_pauliexp_tqe_cost(
    const double discount_rate,
    const std::vector<std::vector<PauliNode_ptr>>& rotation_sets,
    const std::vector<PauliNode_ptr>& rows, const TQE& tqe,
    const unsigned& max_lookahead) {
  double discount = 1 / (1 + discount_rate);
  double weight = 1;
  double exp_cost = 0;
  double tab_cost = 0;
  unsigned count = 0;
  for (const std::vector<PauliNode_ptr>& rotation_set : rotation_sets) {
    int r_cost = 0;
    for (const PauliNode_ptr& node : rotation_set) {
      r_cost += node->tqe_cost_increase(tqe);
      if (++count >= max_lookahead) break;
    }
    exp_cost += weight * r_cost;
    if (count >= max_lookahead) break;
    weight *= discount;
  }
  for (const PauliNode_ptr& node : rows) {
    tab_cost += weight * node->tqe_cost_increase(tqe);
    if (++count >= max_lookahead) break;
  }
  return exp_cost + tab_cost;
}

// given a map from TQE to an array of costs, and an optional map
// specifying the costs for implementing some 2q rotations directly
// as ZZPhase gates. Select the TQEs and 2q rotations with the minimum
// weighted sum of minmax-normalised costs. Cost arrays must all be the same
// length (enforced by the template parameter).
template <size_t n_costs>
static std::pair<std::vector<TQE>, std::vector<Rotation2Q>> minmax_selection(
    const std::vector<std::pair<TQE, std::array<double, n_costs>>>&
        tqe_candidates_cost,
    const std::vector<std::pair<Rotation2Q, std::array<double, n_costs>>>&
        rot2q_gates_cost,
    const std::array<double, n_costs>& weights) {
  static_assert(n_costs > 0, "n_costs must be greater than 0");
  // for each cost type, store its min and max
  std::array<double, n_costs> mins = tqe_candidates_cost.begin()->second;
  std::array<double, n_costs> maxs = tqe_candidates_cost.begin()->second;
  for (const auto& pair : tqe_candidates_cost) {
    for (unsigned cost_index = 0; cost_index < n_costs; cost_index++) {
      if (pair.second[cost_index] < mins[cost_index]) {
        mins[cost_index] = pair.second[cost_index];
      }
      if (pair.second[cost_index] > maxs[cost_index]) {
        maxs[cost_index] = pair.second[cost_index];
      }
    }
  }
  // valid_indices stores the indices of the costs where min!=max
  std::vector<unsigned> valid_indices;
  for (unsigned cost_index = 0; cost_index < n_costs; cost_index++) {
    if (mins[cost_index] != maxs[cost_index]) {
      valid_indices.push_back(cost_index);
    }
  }
  // if all have the same cost, return the first one
  if (valid_indices.size() == 0) {
    std::vector<Rotation2Q> rot2qs;
    rot2qs.reserve(rot2q_gates_cost.size());
    std::transform(
        rot2q_gates_cost.begin(), rot2q_gates_cost.end(),
        std::back_inserter(rot2qs),
        [](const auto& pair) { return pair.first; });
    std::vector<TQE> selected_tqes;
    selected_tqes.reserve(tqe_candidates_cost.size());
    std::transform(
        tqe_candidates_cost.begin(), tqe_candidates_cost.end(),
        std::back_inserter(selected_tqes),
        [](const auto& pair) { return pair.first; });
    return {selected_tqes, rot2qs};
  }
  // if only one cost variable, no need to normalise
  if (valid_indices.size() == 1) {
    auto it = tqe_candidates_cost.begin();
    double min_cost = it->second[valid_indices[0]];
    std::vector<TQE> min_tqes = {it->first};
    for (; it != tqe_candidates_cost.end(); it++) {
      if (it->second[valid_indices[0]] < min_cost) {
        min_tqes = {it->first};
        min_cost = it->second[valid_indices[0]];
      } else if (it->second[valid_indices[0]] == min_cost) {
        min_tqes.push_back(it->first);
      }
    }
    std::vector<Rotation2Q> min_rot2qs;
    for (auto it2 = rot2q_gates_cost.begin(); it2 != rot2q_gates_cost.end();
         it2++) {
      if (it2->second[valid_indices[0]] < min_cost) {
        min_tqes.clear();
        min_cost = it2->second[valid_indices[0]];
        min_rot2qs = {it2->first};
      } else if (it2->second[valid_indices[0]] == min_cost) {
        min_rot2qs.push_back(it2->first);
      }
    }
    return {min_tqes, min_rot2qs};
  }
  // find the tqe with the minimum normalised cost
  auto it = tqe_candidates_cost.begin();
  double min_cost = 0;
  std::vector<TQE> min_tqes = {it->first};
  // initialise min_cost
  for (const auto& cost_index : valid_indices) {
    min_cost += weights[cost_index] *
                (it->second[cost_index] - mins[cost_index]) /
                (maxs[cost_index] - mins[cost_index]);
  }
  it++;
  // iterate all tqes
  for (; it != tqe_candidates_cost.end(); it++) {
    double cost = 0;
    for (const auto& cost_index : valid_indices) {
      cost += weights[cost_index] *
              (it->second[cost_index] - mins[cost_index]) /
              (maxs[cost_index] - mins[cost_index]);
    }
    if (cost < min_cost) {
      min_cost = cost;
      min_tqes = {it->first};
    } else if (cost == min_cost) {
      min_tqes.push_back(it->first);
    }
  }
  std::vector<Rotation2Q> min_rot2qs;
  for (auto it2 = rot2q_gates_cost.begin(); it2 != rot2q_gates_cost.end();
       it2++) {
    double cost = 0;
    for (const auto& cost_index : valid_indices) {
      cost += weights[cost_index] *
              (it2->second[cost_index] - mins[cost_index]) /
              (maxs[cost_index] - mins[cost_index]);
    }
    if (cost < min_cost) {
      min_cost = cost;
      min_tqes.clear();
      min_rot2qs = {it2->first};
    } else if (cost == min_cost) {
      min_rot2qs.push_back(it2->first);
    }
  }
  return {min_tqes, min_rot2qs};
}

// simple struct that tracks the depth on each qubit
struct DepthTracker {
  std::vector<unsigned> qubit_depth;
  unsigned max_depth;
  DepthTracker(unsigned n) : qubit_depth(n, 0), max_depth(0) {};

  unsigned gate_depth(unsigned a, unsigned b) const {
    return std::max(qubit_depth[a], qubit_depth[b]) + 1;
  };
  void add_1q_gate(unsigned a) {
    qubit_depth[a]++;
    if (qubit_depth[a] > max_depth) {
      max_depth = qubit_depth[a];
    }
  };
  void add_2q_gate(unsigned a, unsigned b) {
    unsigned new_gate_depth = gate_depth(a, b);
    qubit_depth[a] = new_gate_depth;
    qubit_depth[b] = new_gate_depth;
    if (new_gate_depth > max_depth) {
      max_depth = new_gate_depth;
    }
  };
};

/**
 * @brief Synthesise a vector of PauliPropagation
 */
static void tableau_row_nodes_synthesis(
    std::vector<PauliNode_ptr>& rows, Circuit& circ,
    DepthTracker& depth_tracker, double depth_weight, unsigned max_lookahead,
    unsigned max_tqe_candidates, unsigned seed,
    std::shared_ptr<std::atomic<bool>> stop_flag) {
  // only consider nodes with a non-zero cost
  std::vector<unsigned> remaining_indices;
  for (unsigned i = 0; i < rows.size(); i++) {
    if (rows[i]->tqe_cost() > 0) {
      remaining_indices.push_back(i);
    }
  }
  while (remaining_indices.size() != 0) {
    // check early termination
    if (stop_flag.get()->load()) {
      return;
    };
    // get nodes with min cost
    std::vector<unsigned> min_nodes_indices = {remaining_indices[0]};
    unsigned min_cost = rows[remaining_indices[0]]->tqe_cost();
    for (unsigned i = 1; i < remaining_indices.size(); i++) {
      unsigned node_cost = rows[remaining_indices[i]]->tqe_cost();
      if (node_cost == min_cost) {
        min_nodes_indices.push_back(remaining_indices[i]);
      } else if (node_cost < min_cost) {
        min_nodes_indices = {remaining_indices[i]};
        min_cost = node_cost;
      }
    }
    // for each node with min cost, find the list of tqe gates that can reduce
    // its cost
    std::set<TQE> tqe_candidates;
    TKET_ASSERT(min_nodes_indices.size() > 0);
    for (const unsigned& index : min_nodes_indices) {
      std::vector<TQE> node_reducing_tqes = rows[index]->reduction_tqes();
      tqe_candidates.insert(
          node_reducing_tqes.begin(), node_reducing_tqes.end());
    }
    // sample
    std::vector<TQE> sampled_tqes =
        sample_tqes(tqe_candidates, max_tqe_candidates, seed);
    // for each tqe we compute a vector of cost factors which will
    // be combined to make the final decision.
    // we currently only consider tqe_cost and gate_depth.
    std::vector<std::pair<TQE, std::array<double, 2>>> tqe_candidates_cost;
    for (const TQE& tqe : sampled_tqes) {
      tqe_candidates_cost.push_back(
          {tqe,
           {default_tableau_tqe_cost(
                rows, remaining_indices, tqe, max_lookahead),
            static_cast<double>(depth_tracker.gate_depth(tqe.a, tqe.b))}});
    }
    TKET_ASSERT(tqe_candidates_cost.size() > 0);
    // select the best one
    auto [min_tqes, _] =
        minmax_selection(tqe_candidates_cost, {}, {1, depth_weight});
    TQE selected_tqe = sample_random_tqe(min_tqes, seed);
    // apply TQE
    apply_tqe_to_circ(selected_tqe, circ);
    // update depth tracker
    depth_tracker.add_2q_gate(selected_tqe.a, selected_tqe.b);
    // remove finished nodes
    for (unsigned i = remaining_indices.size(); i-- > 0;) {
      unsigned node_index = remaining_indices[i];
      rows[node_index]->update(selected_tqe);
      if (rows[node_index]->tqe_cost() == 0) {
        remaining_indices.erase(remaining_indices.begin() + i);
      }
    }
  }
  // apply local Cliffords
  for (PauliNode_ptr& node_ptr : rows) {
    PauliPropagation& node = dynamic_cast<PauliPropagation&>(*node_ptr);
    auto [q_index, supp_z, supp_x] = node.first_support();
    // transform supp_z,supp_x to Z,X
    std::vector<OpType> optype_list = AA_TO_ZX.at({supp_z, supp_x});
    for (auto it = optype_list.rbegin(); it != optype_list.rend(); ++it) {
      circ.add_op<unsigned>(SQ_CLIFF_DAGGER.at(*it), {q_index});
      node.update(*it, q_index);
    }
    // remove signs
    if (!node.z_sign()) {
      circ.add_op<unsigned>(OpType::X, {q_index});
      node.update(OpType::X, q_index);
    }
    if (!node.x_sign()) {
      circ.add_op<unsigned>(OpType::Z, {q_index});
      node.update(OpType::Z, q_index);
    }
    if (q_index != node.qubit_index()) {
      circ.add_op<unsigned>(OpType::SWAP, {q_index, node.qubit_index()});
      for (PauliNode_ptr& node_ptr2 : rows) {
        node_ptr2->swap(q_index, node.qubit_index());
      }
    }
  }
}

/**
 * @brief Given a vector of sets of PauliNodes, implement any node in the
 * first set where the tqe_cost is zero. Remove implemented nodes and the first
 * set if empty.
 *
 * @param rotation_sets
 * @param circ
 * @return true if the first set is now empty and removed
 * @return false
 */
static void consume_nodes(
    std::vector<std::vector<PauliNode_ptr>>& rotation_sets, Circuit& circ,
    DepthTracker& depth_tracker, double discount_rate, double depth_weight) {
  if (rotation_sets.empty()) {
    return;
  }
  while (true) {
    std::vector<PauliNode_ptr>& first_set = rotation_sets[0];
    for (unsigned i = first_set.size(); i-- > 0;) {
      PauliNode_ptr& node_ptr = first_set[i];
      switch (node_ptr->get_type()) {
        case PauliNodeType::Reset: {
          if (node_ptr->tqe_cost() > 0) continue;
          Reset& node = dynamic_cast<Reset&>(*node_ptr);
          auto [q_index, supp_z, supp_x] = node.first_support();
          // conjugate the pair to +Z/X
          std::vector<OpType> optype_list = AA_TO_ZX.at({supp_z, supp_x});
          for (auto it = optype_list.rbegin(); it != optype_list.rend(); ++it) {
            circ.add_op<unsigned>(SQ_CLIFF_DAGGER.at(*it), {q_index});
          }
          if (!node.z_sign()) {
            circ.add_op<unsigned>(OpType::X, {q_index});
          }
          if (!node.x_sign()) {
            circ.add_op<unsigned>(OpType::Z, {q_index});
          }
          circ.add_op<unsigned>(OpType::Reset, {q_index});
          if (!node.z_sign()) {
            circ.add_op<unsigned>(OpType::X, {q_index});
          }
          if (!node.x_sign()) {
            circ.add_op<unsigned>(OpType::Z, {q_index});
          }
          for (auto it = optype_list.begin(); it != optype_list.end(); ++it) {
            circ.add_op<unsigned>(*it, {q_index});
          }
          first_set.erase(first_set.begin() + i);
          break;
        }
        case PauliNodeType::MidMeasure: {
          if (node_ptr->tqe_cost() > 0) continue;
          MidMeasure& node = dynamic_cast<MidMeasure&>(*node_ptr);
          auto [q_index, supp] = node.first_support();
          // Conjugate the Pauli to +Z
          switch (supp) {
            case Pauli::Z: {
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::X, {q_index});
              }
              circ.add_measure(q_index, node.bit());
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::X, {q_index});
              }
              break;
            }
            case Pauli::X: {
              circ.add_op<unsigned>(OpType::H, {q_index});
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::X, {q_index});
              }
              circ.add_measure(q_index, node.bit());
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::X, {q_index});
              }
              circ.add_op<unsigned>(OpType::H, {q_index});
              break;
            }
            case Pauli::Y: {
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::Vdg, {q_index});
              } else {
                circ.add_op<unsigned>(OpType::V, {q_index});
              }
              circ.add_measure(q_index, node.bit());
              if (!node.sign()) {
                circ.add_op<unsigned>(OpType::V, {q_index});
              } else {
                circ.add_op<unsigned>(OpType::Vdg, {q_index});
              }
              break;
            }
            default: {
              TKET_ASSERT(false);
            }
          }
          first_set.erase(first_set.begin() + i);
          break;
        }
        case PauliNodeType::ClassicalNode: {
          // always implement Classical nodes
          ClassicalNode& node = dynamic_cast<ClassicalNode&>(*node_ptr);
          circ.add_op<UnitID>(node.op(), node.args());
          first_set.erase(first_set.begin() + i);
          break;
        }
        case PauliNodeType::ConditionalBlock: {
          // conditionals are implemented as a conditioned sequence of
          // PauliExpBoxes and subsequently optimised by recursively calling
          // greedy_pauli_optimisation
          ConditionalBlock& node = dynamic_cast<ConditionalBlock&>(*node_ptr);
          const std::vector<unsigned> cond_bits = node.cond_bits();
          const unsigned cond_value = node.cond_value();
          std::vector<unsigned> qubits;
          for (unsigned i = 0; i < circ.n_qubits(); i++) {
            qubits.push_back(i);
          }
          Circuit cond_circ(circ.n_qubits());
          for (const auto& t : node.rotations()) {
            const std::vector<Pauli>& string = std::get<0>(t);
            bool sign = std::get<1>(t);
            Expr angle = sign ? std::get<2>(t) : -std::get<2>(t);
            Op_ptr peb_op =
                std::make_shared<PauliExpBox>(SymPauliTensor(string, angle));
            cond_circ.add_op<unsigned>(peb_op, qubits);
          }
          greedy_pauli_optimisation(discount_rate, depth_weight)
              .apply(cond_circ);
          // replace implicit wire swaps
          cond_circ.replace_all_implicit_wire_swaps();
          Op_ptr cond = std::make_shared<Conditional>(
              std::make_shared<CircBox>(cond_circ), (unsigned)cond_bits.size(),
              cond_value);
          std::vector<unsigned> args = cond_bits;
          for (unsigned i = 0; i < cond_circ.n_qubits(); i++) {
            args.push_back(i);
          }
          circ.add_op<unsigned>(cond, args);
          first_set.erase(first_set.begin() + i);
          break;
        }
        case PauliNodeType::PauliRotation: {
          if (node_ptr->tqe_cost() > 0) continue;
          PauliRotation& node = dynamic_cast<PauliRotation&>(*node_ptr);
          auto [q_index, supp] = node.first_support();
          depth_tracker.add_1q_gate(q_index);
          OpType rot_type;
          switch (supp) {
            case Pauli::Y: {
              rot_type = OpType::Ry;
              break;
            }
            case Pauli::Z: {
              rot_type = OpType::Rz;
              break;
            }
            case Pauli::X: {
              rot_type = OpType::Rx;
              break;
            }
            default:
              // support can't be Pauli::I
              TKET_ASSERT(false);
          }
          circ.add_op<unsigned>(rot_type, node.angle(), {q_index});
          first_set.erase(first_set.begin() + i);
          break;
        }
        default:
          TKET_ASSERT(false);
      }
    }
    if (first_set.empty()) {
      rotation_sets.erase(rotation_sets.begin());
      if (rotation_sets.empty()) {
        return;
      }
    } else {
      return;
    }
  }
}

/**
 * @brief Synthesise a vector of unordered rotation sets
 */
static void pauli_exps_synthesis(
    std::vector<std::vector<PauliNode_ptr>>& rotation_sets,
    std::vector<PauliNode_ptr>& rows, Circuit& circ,
    DepthTracker& depth_tracker, double discount_rate, double depth_weight,
    unsigned max_lookahead, unsigned max_tqe_candidates, unsigned seed,
    bool allow_zzphase, std::shared_ptr<std::atomic<bool>> stop_flag) {
  while (true) {
    // check timeout
    if (stop_flag.get()->load()) {
      return;
    };

    consume_nodes(
        rotation_sets, circ, depth_tracker, discount_rate, depth_weight);

    if (rotation_sets.empty()) break;

    std::vector<PauliNode_ptr>& first_set = rotation_sets[0];
    // get nodes with min cost
    std::vector<unsigned> min_nodes_indices = {0};
    unsigned min_cost = first_set[0]->tqe_cost();
    for (unsigned i = 1; i < first_set.size(); i++) {
      unsigned node_cost = first_set[i]->tqe_cost();
      if (node_cost == min_cost) {
        min_nodes_indices.push_back(i);
      } else if (node_cost < min_cost) {
        min_nodes_indices = {i};
        min_cost = node_cost;
      }
    }
    std::set<TQE> tqe_candidates;
    for (const unsigned& index : min_nodes_indices) {
      std::vector<TQE> node_reducing_tqes = first_set[index]->reduction_tqes();
      tqe_candidates.insert(
          node_reducing_tqes.begin(), node_reducing_tqes.end());
    }
    // sample
    std::vector<TQE> sampled_tqes =
        sample_tqes(tqe_candidates, max_tqe_candidates, seed);

    // for each tqe we compute costs which might subject to normalisation
    std::vector<std::pair<TQE, std::array<double, 2>>> tqe_candidates_cost;
    for (const TQE& tqe : sampled_tqes) {
      tqe_candidates_cost.push_back(
          {tqe,
           {default_pauliexp_tqe_cost(
                discount_rate, rotation_sets, rows, tqe, max_lookahead),
            static_cast<double>(depth_tracker.gate_depth(tqe.a, tqe.b))}});
    }
    std::vector<std::pair<Rotation2Q, std::array<double, 2>>> rot2q_gates_cost;
    if (allow_zzphase) {
      // implementing a 2q rotation directly will result in a
      // -1 tqe cost change in the first rotation set and 0 elsewhere.
      // If multiple 2q rotations are worth implementing directly, we
      // implement all of them to avoid doing the same cost calculation
      // in the next rounds.
      for (unsigned i = 0; i < first_set.size(); i++) {
        if (first_set[i]->get_type() == PauliNodeType::PauliRotation &&
            first_set[i]->tqe_cost() == 1) {
          const PauliRotation& node =
              dynamic_cast<const PauliRotation&>(*first_set[i]);
          std::vector<unsigned> supps;
          std::vector<Pauli> paulis;
          for (unsigned j = 0; j < node.string().size(); j++) {
            if (node.string()[j] != Pauli::I) {
              supps.push_back(j);
              paulis.push_back(node.string()[j]);
            }
          }
          rot2q_gates_cost.push_back(
              {{paulis[0], paulis[1], supps[0], supps[1], node.angle(), i},
               {-1, static_cast<double>(
                        depth_tracker.gate_depth(supps[0], supps[1]))}});
        }
      }
    }
    // select the best one
    auto [min_tqes, min_rot2qs] = minmax_selection(
        tqe_candidates_cost, rot2q_gates_cost, {1, depth_weight});
    if (min_rot2qs.empty()) {
      TQE selected_tqe = sample_random_tqe(min_tqes, seed);
      // apply TQE
      apply_tqe_to_circ(selected_tqe, circ);
      depth_tracker.add_2q_gate(selected_tqe.a, selected_tqe.b);
      for (std::vector<PauliNode_ptr>& rotation_set : rotation_sets) {
        for (PauliNode_ptr& node : rotation_set) {
          node->update(selected_tqe);
        }
      }
      for (PauliNode_ptr& row : rows) {
        row->update(selected_tqe);
      }
    } else {
      // apply 2q rotations directly
      std::sort(
          min_rot2qs.begin(), min_rot2qs.end(),
          [](const Rotation2Q& r1, const Rotation2Q& r2) {
            return r1.index > r2.index;
          });
      for (const Rotation2Q& rot : min_rot2qs) {
        apply_rot2q_to_circ(rot, circ);
        first_set.erase(first_set.begin() + rot.index);
      }
    }
  }
}

Circuit greedy_pauli_set_synthesis(
    const std::vector<SymPauliTensor>& unordered_set, double depth_weight,
    unsigned max_lookahead, unsigned max_tqe_candidates, unsigned seed,
    bool allow_zzphase) {
  if (max_lookahead == 0) {
    throw GreedyPauliSimpError("max_lookahead must be greater than 0.");
  }
  if (max_tqe_candidates == 0) {
    throw GreedyPauliSimpError("max_tqe_candidates must be greater than 0.");
  }

  if (unordered_set.size() == 0) {
    return Circuit();
  }
  unsigned n_qubits = unordered_set[0].string.size();
  Circuit c(n_qubits);
  auto [rotation_set, rows] = gpg_from_unordered_set(unordered_set);
  std::vector<std::vector<PauliNode_ptr>> rotation_sets{rotation_set};
  DepthTracker depth_tracker(n_qubits);
  // synthesise Pauli exps
  std::shared_ptr<std::atomic<bool>> dummy_stop_flag =
      std::make_shared<std::atomic<bool>>(false);
  pauli_exps_synthesis(
      rotation_sets, rows, c, depth_tracker, 0, depth_weight, max_lookahead,
      max_tqe_candidates, seed, allow_zzphase, dummy_stop_flag);
  // synthesise the tableau
  tableau_row_nodes_synthesis(
      rows, c, depth_tracker, depth_weight, max_lookahead, max_tqe_candidates,
      seed, dummy_stop_flag);
  c.replace_SWAPs();
  return c;
}

Circuit greedy_pauli_graph_synthesis_flag(
    const Circuit& circ, std::shared_ptr<std::atomic<bool>> stop_flag,
    double discount_rate, double depth_weight, unsigned max_lookahead,
    unsigned max_tqe_candidates, unsigned seed, bool allow_zzphase) {
  if (max_lookahead == 0) {
    throw GreedyPauliSimpError("max_lookahead must be greater than 0.");
  }
  if (max_tqe_candidates == 0) {
    throw GreedyPauliSimpError("max_tqe_candidates must be greater than 0.");
  }
  Circuit circ_flat(circ);
  circ_flat.replace_all_implicit_wire_swaps();
  unsigned n_qubits = circ_flat.n_qubits();
  unsigned n_bits = circ_flat.n_bits();
  // empty circuit
  Circuit new_circ(n_qubits, n_bits);
  std::optional<std::string> name = circ_flat.get_name();
  if (name != std::nullopt) {
    new_circ.set_name(name.value());
  }
  unit_map_t unit_map = circ_flat.flatten_registers();
  unit_map_t rev_unit_map;
  for (const auto& pair : unit_map) {
    rev_unit_map.insert({pair.second, pair.first});
  }

  GPGraph gpg(circ_flat.all_qubits(), circ_flat.all_bits());
  for (const Command& cmd : circ_flat.get_commands()) {
    if (stop_flag.get()->load()) {
      return Circuit();
    }
    gpg.apply_gate_at_end(cmd);
  }

  // We regularly check whether the timeout has occurred
  if (stop_flag.get()->load()) {
    return Circuit();
  }

  auto [rotation_sets, rows, measures] = gpg.get_sequence(stop_flag);

  if (stop_flag.get()->load()) {
    return Circuit();
  }

  DepthTracker depth_tracker(n_qubits);
  // synthesise Pauli exps
  pauli_exps_synthesis(
      rotation_sets, rows, new_circ, depth_tracker, discount_rate, depth_weight,
      max_lookahead, max_tqe_candidates, seed, allow_zzphase, stop_flag);

  if (stop_flag.get()->load()) {
    return Circuit();
  }
  // synthesise the tableau
  tableau_row_nodes_synthesis(
      rows, new_circ, depth_tracker, depth_weight, max_lookahead,
      max_tqe_candidates, seed, stop_flag);

  if (stop_flag.get()->load()) {
    return Circuit();
  }

  for (auto it = measures.begin(); it != measures.end(); ++it) {
    new_circ.add_measure(it->left, it->right);
  }
  new_circ.rename_units(rev_unit_map);
  new_circ.replace_SWAPs();
  return new_circ;
}

Circuit greedy_pauli_graph_synthesis(
    const Circuit& circ, double discount_rate, double depth_weight,
    unsigned max_lookahead, unsigned max_tqe_candidates, unsigned seed,
    bool allow_zzphase) {
  std::shared_ptr<std::atomic<bool>> dummy_stop_flag =
      std::make_shared<std::atomic<bool>>(false);
  return greedy_pauli_graph_synthesis_flag(
      circ, dummy_stop_flag, discount_rate, depth_weight, max_lookahead,
      max_tqe_candidates, seed, allow_zzphase);
}

}  // namespace GreedyPauliSimp

Transform greedy_pauli_optimisation(
    double discount_rate, double depth_weight, unsigned max_lookahead,
    unsigned max_tqe_candidates, unsigned seed, bool allow_zzphase,
    unsigned thread_timeout, unsigned trials) {
  return Transform([discount_rate, depth_weight, max_lookahead,
                    max_tqe_candidates, seed, allow_zzphase, thread_timeout,
                    trials](Circuit& circ) {
    std::mt19937 seed_gen(seed);
    std::vector<Circuit> circuits;
    circuits.reserve(trials);
    unsigned threads_started = 0;

    while (threads_started < trials) {
      std::shared_ptr<std::atomic<bool>> stop_flag =
          std::make_shared<std::atomic<bool>>(false);
      std::future<Circuit> future = std::async(
          std::launch::async,
          [&, stop_flag]() {  // Capture `stop_flag` explicitly in the lambda
            return GreedyPauliSimp::greedy_pauli_graph_synthesis_flag(
                circ, stop_flag, discount_rate, depth_weight, max_lookahead,
                max_tqe_candidates, seed_gen(), allow_zzphase);
          });
      threads_started++;

      if (future.wait_for(std::chrono::seconds(thread_timeout)) ==
          std::future_status::ready) {
        circuits.emplace_back();
        circuits.back() = future.get();
        circuits.back().decompose_boxes_recursively();
      } else {
        // If the thread isn't complete within time, prompt cancelling the
        // optimisation and break from while loop
        *stop_flag = true;
        break;
      }
    }

    // Return the smallest circuit if any were found within the single
    // thread_timeout
    // If none are found then return false
    if (circuits.empty()) return false;
    auto min = std::min_element(
        circuits.begin(), circuits.end(),
        [](const Circuit& a, const Circuit& b) {
          const auto two_qubit_gates_a = a.count_n_qubit_gates(2);
          const auto two_qubit_gates_b = b.count_n_qubit_gates(2);
          if (two_qubit_gates_a != two_qubit_gates_b) {
            return two_qubit_gates_a < two_qubit_gates_b;
          }
          const auto n_gates_a = a.n_gates();
          const auto n_gates_b = b.n_gates();
          if (n_gates_a != n_gates_b) {
            return n_gates_a < n_gates_b;
          }
          return a.depth() < b.depth();
        });
    circ = std::move(*min);
    return true;
  });
}

}  // namespace Transforms

}  // namespace tket
