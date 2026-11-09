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

#include "tket/Converters/PhasePoly.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tklog/TketLog.hpp>
#include <vector>

#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Converters/Gauss.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Ops/OpJsonFactory.hpp"
#include "tket/Ops/OpPtr.hpp"
#include "tket/Utils/Expression.hpp"

namespace tket {

/* Used only for gray_synth */
struct SynthStruct {
  std::list<phase_term_t> terms;
  std::set<unsigned> remaining_indices;
  std::optional<unsigned> target;
};

// update the phase gadgets when adding a CNOT
static void adjust_vectors(
    unsigned ctrl, unsigned tgt, std::list<SynthStruct>& Q) {
  for (SynthStruct& S : Q) {
    for (std::pair<std::vector<bool>, Expr>& term : S.terms) {
      std::vector<bool>& vec = term.first;
      vec[ctrl] = vec[ctrl] ^ vec[tgt];
    }
  }
}

// see https://arxiv.org/pdf/1712.01859.pdf p12, line 18
// get qubit with either greatest or least hamming weight
static unsigned find_best_split(
    const std::list<phase_term_t>& terms, const std::set<unsigned>& indices) {
  int max = -1;
  int max_i = -1;
  for (unsigned i : indices) {
    int num_ones = std::count_if(
        terms.begin(), terms.end(),
        [=](const std::pair<std::vector<bool>, Expr>& term) {
          return term.first[i];
        });
    int num_zeros = terms.size() - num_ones;

    if (num_zeros > max || num_ones > max) {
      max = num_zeros > num_ones ? num_zeros : num_ones;
      max_i = i;
    }
  }

  return (unsigned)max_i;
}

// divide into S0 and S1 based on value at qubit j
static std::pair<std::list<phase_term_t>, std::list<phase_term_t>> split(
    std::list<phase_term_t>& terms, int j) {
  std::list<phase_term_t> zeros;
  std::list<phase_term_t> ones;

  while (!terms.empty()) {
    if (terms.front().first[j])
      ones.splice(ones.end(), terms, terms.begin());
    else
      zeros.splice(zeros.end(), terms, terms.begin());
  }

  return std::make_pair(zeros, ones);
}

/* see: arXiv:1712.01859 */
Circuit gray_synth(
    unsigned n_qubits, const std::list<phase_term_t>& parities,
    const MatrixXb& linear_transformation) {
  MatrixXb A = linear_transformation;
  Circuit circ(n_qubits);
  std::list<SynthStruct> Q;  // used to recur over
  std::set<unsigned>
      indices;  // correspond to qubits which have to be recurred over
  for (unsigned i = 0; i < n_qubits; ++i) indices.insert(i);
  Q.push_front({parities, indices, std::nullopt});

  while (!Q.empty()) {
    SynthStruct S = Q.front();
    Q.pop_front();

    if (S.terms.size() == 0) continue;
    // only one column in GraySynth matrix being recurred over
    else if (S.terms.size() == 1 && S.target) {
      // special case to avoid doing extra recursion
      unsigned tgt = *(S.target);
      auto& [vec, angle] = S.terms.front();
      for (unsigned ctrl = 0; ctrl < vec.size(); ++ctrl) {
        if (ctrl != tgt && vec[ctrl]) {
          circ.add_op<unsigned>(OpType::CX, {ctrl, tgt});
          adjust_vectors(ctrl, tgt, Q);
          for (unsigned i = 0; i < A.rows(); i++) {
            // do column operation on linear
            // transformation
            // this will allow us to correct for the CXs
            // we produce here using Gaussian elim
            A(i, ctrl) ^= A(i, tgt);
          }
        }
      }
      circ.add_op<unsigned>(OpType::Rz, angle, {tgt});
    } else if (!S.remaining_indices.empty()) {
      unsigned i = find_best_split(S.terms, S.remaining_indices);
      auto [S0, S1] = split(S.terms, i);

      S.remaining_indices.erase(i);

      if (S.target) {
        Q.push_front({S1, S.remaining_indices, S.target});
      } else {
        Q.push_front({S1, S.remaining_indices, i});
      }
      Q.push_front({S0, S.remaining_indices, S.target});
    }
  }
  DiagMatrix m(A);
  CXMaker cxmaker(n_qubits, false);
  m.gauss(cxmaker);
  circ.append(cxmaker._circ.dagger());
  return circ;
}

PhasePolyBox::PhasePolyBox(const Circuit& circ)
    : Box(OpType::PhasePolyBox), n_qubits_(circ.n_qubits()) {
  Circuit newcirc(circ);

  // check for classical bits
  if (newcirc.n_bits() != 0)
    throw std::invalid_argument(
        "Cannot construct phase polynomial from classical controlled "
        "gates");

  // check the gateset of the circuit
  for (const Command& com : newcirc) {
    OpType ot = com.get_op_ptr()->get_type();

    switch (ot) {
      case OpType::CX: {
        break;
      }
      case OpType::Rz: {
        break;
      }
      default: {
        throw BadOpType("Only CXs and Rzs allowed in Phase Polynomials", ot);
      }
    }
  }

  // replace wireswaps with three CX
  while (newcirc.has_implicit_wireswaps()) {
    bool foundswap = false;

    qubit_map_t perm = newcirc.implicit_qubit_permutation();
    for (const std::pair<const Qubit, Qubit>& pair : perm) {
      if (pair.first != pair.second) {
        if (!foundswap) {
          newcirc.replace_implicit_wire_swap(pair.first, pair.second);
          foundswap = true;
        }
      }
    }
  }

  // generate box
  signature_ = op_signature_t(n_qubits_, EdgeType::Quantum);
  unsigned i = 0;
  for (const Qubit& qb : newcirc.all_qubits()) {
    qubit_indices_.insert({qb, i});
    ++i;
  }
  linear_transformation_ = MatrixXb::Identity(n_qubits_, n_qubits_);
  for (const Command& com : newcirc) {
    OpType ot = com.get_op_ptr()->get_type();
    unit_vector_t qbs = com.get_args();
    if (ot == OpType::CX) {
      unsigned ctrl = qubit_indices_.left.at(Qubit(qbs[0]));
      unsigned target = qubit_indices_.left.at(Qubit(qbs[1]));
      for (unsigned j = 0; j < n_qubits_; ++j) {
        linear_transformation_.row(target)[j] ^=
            linear_transformation_.row(ctrl)[j];
      }
    } else if (ot == OpType::Rz) {
      unsigned qb = qubit_indices_.left.at(Qubit(qbs[0]));
      std::vector<bool> boolvec(n_qubits_);
      for (unsigned j = 0; j < n_qubits_; ++j) {
        boolvec[j] = linear_transformation_.row(qb)[j];
      }
      PhasePolynomial::iterator pp_it = phase_polynomial_.find(boolvec);
      if (pp_it == phase_polynomial_.end())
        phase_polynomial_[boolvec] = com.get_op_ptr()->get_params().at(0);
      else
        pp_it->second += com.get_op_ptr()->get_params().at(0);
    } else
      TKET_ASSERT(!"Only CXs and Rzs allowed in Phase Polynomials");
  }
}

PhasePolyBox::PhasePolyBox(
    unsigned n_qubits, const boost::bimap<Qubit, unsigned>& qubit_indices,
    const PhasePolynomial& phase_polynomial,
    const MatrixXb& linear_transformation)
    : Box(OpType::PhasePolyBox),
      n_qubits_(n_qubits),
      qubit_indices_(qubit_indices),
      phase_polynomial_(phase_polynomial),
      linear_transformation_(linear_transformation) {
  for (const auto& pair : qubit_indices_) {
    if (pair.right >= n_qubits) {
      throw std::invalid_argument(
          "The creation of a phasepolybox failed: index in qubit "
          "list is out of range");
    }
  }

  for (auto const& ps : phase_polynomial_) {
    if (ps.first.size() != n_qubits_) {
      throw std::invalid_argument(
          "The creation of a phasepolybox failed: PhasePolynomial "
          "does not match the given number of qubits");
    }

    if (std::none_of(
            ps.first.begin(), ps.first.end(), [](bool x) { return x; })) {
      throw std::invalid_argument(
          "The creation of a phasepolybox failed: PhasePolynomial "
          "contains invalid element");
    }
  }

  if (linear_transformation_.rows() != n_qubits) {
    throw std::invalid_argument(
        "The creation of a phasepolybox failed: row size of the "
        "linear transformation does not match the number of qubits");
  }

  if (linear_transformation_.cols() != n_qubits) {
    throw std::invalid_argument(
        "The creation of a phasepolybox failed: cols size of the "
        "linear transformation does not match the number of qubits");
  }

  signature_ = op_signature_t(n_qubits_, EdgeType::Quantum);
}

/*
 * Note: The generate circuit method no longer uses the input qubits to rename
 * the units of the generated circuit. This is because doing this contradicts
 * the expectations of the decompose boxes method and was causing errors in some
 * cases when using custom registers.
 *
 * Usually boxes don't know about external
 * qubit names, because this info is stored in the way the box is wired into a
 * circuit. For some reason the PhasePolyBox box was written so that it does
 * know about the names of qubits it was wired to at generation time (either
 * deduced from the circuit it was generated from or provided by a map). This
 * internal information is used in some of the AAS compilation strategies.
 * Although it appears to me that this could be handled in a better way I don't
 * have time to refactor the AAS code at the moment. So I added a method
 * generate_circuit_with_original_placement (see below) that uses the logic that
 * the AAS code expects (the generate_circuit logic), and this is used in the
 * AAS code.
 */

void PhasePolyBox::generate_circuit() const {
  std::list<phase_term_t> phases;
  for (phase_term_t phase : phase_polynomial_) phases.push_back(phase);
  Circuit circ = gray_synth(n_qubits_, phases, linear_transformation_);
  circ_ = std::make_shared<Circuit>(circ);
}

Circuit PhasePolyBox::generate_circuit_with_original_placement() const {
  std::list<phase_term_t> phases;
  for (phase_term_t phase : phase_polynomial_) phases.push_back(phase);
  Circuit circ = gray_synth(n_qubits_, phases, linear_transformation_);
  unit_map_t qmap;
  for (const auto& pair : qubit_indices_.right) {
    qmap.insert({Qubit(q_default_reg(), pair.first), pair.second});
  }
  circ.rename_units(qmap);
  return circ;
}

PhasePolyBox::PhasePolyBox(const PhasePolyBox& other)
    : Box(other),
      n_qubits_(other.n_qubits_),
      qubit_indices_(other.qubit_indices_),
      phase_polynomial_(other.phase_polynomial_),
      linear_transformation_(other.linear_transformation_) {}

bool PhasePolyBox::is_clifford() const {
  return std::all_of(
      phase_polynomial_.begin(), phase_polynomial_.end(),
      [](const phase_term_t& pair) { return equiv_0(4 * pair.second); });
}

Op_ptr PhasePolyBox::symbol_substitution(
    const SymEngine::map_basic_basic& sub_map) const {
  Circuit new_circ(*to_circuit());
  new_circ.symbol_substitution(sub_map);
  return std::make_shared<PhasePolyBox>(new_circ);
}

SymSet PhasePolyBox::free_symbols() const {
  return to_circuit()->free_symbols();
}

// Dynamic Eigen matrix requires special treatment to load, to allocate memory
template <typename T>
Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> load_dynamic_matrix(
    const nlohmann::json& j, std::size_t rows, std::size_t cols) {
  Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> mat(rows, cols);

  for (std::size_t row = 0; row < j.size(); ++row) {
    const auto& j_row = j.at(row);
    for (std::size_t col = 0; col < j_row.size(); ++col) {
      mat(row, col) = j_row.at(col).get<T>();
    }
  }
  return mat;
}

nlohmann::json PhasePolyBox::to_json(const Op_ptr& op) {
  const auto& box = static_cast<const PhasePolyBox&>(*op);
  nlohmann::json j = core_box_json(box);
  j["n_qubits"] = box.get_n_qubits();
  j["qubit_indices"] = nlohmann::json::array();
  for (const auto& pair : box.get_qubit_indices()) {
    nlohmann::json ind_j;
    ind_j.push_back(pair.left);
    ind_j.push_back(pair.right);

    j["qubit_indices"].push_back(ind_j);
  }

  j["phase_polynomial"] = box.get_phase_polynomial();
  j["linear_transformation"] = box.get_linear_transformation();
  return j;
}

Op_ptr PhasePolyBox::from_json(const nlohmann::json& j) {
  boost::bimap<Qubit, unsigned> q_ind;
  for (const auto& j_ar : j.at("qubit_indices")) {
    q_ind.insert({j_ar.at(0), j_ar.at(1)});
  }
  const unsigned n_qb = j.at("n_qubits").get<unsigned>();
  const MatrixXb& lin_trans =
      load_dynamic_matrix<bool>(j.at("linear_transformation"), n_qb, n_qb);

  PhasePolyBox box = PhasePolyBox(
      n_qb, q_ind, j.at("phase_polynomial").get<PhasePolynomial>(), lin_trans);
  return set_box_id(
      box,
      boost::lexical_cast<boost::uuids::uuid>(j.at("id").get<std::string>()));
}

REGISTER_OPFACTORY(PhasePolyBox, PhasePolyBox)

CircToPhasePolyConversion::CircToPhasePolyConversion(
    const Circuit& circ, unsigned min_size) {
  min_size_ = min_size;
  circ_ = circ;
  box_size_ = 0;

  nq_ = circ_.n_qubits();

  qubit_types_ = std::vector<QubitType>(nq_, QubitType::pre);

  unsigned i = 0;
  for (const Qubit& qb : circ_.all_qubits()) {
    qubit_indices_.insert({qb, i});
    ++i;
  }

  empty_circ_ = Circuit(circ_);

  VertexList bin;

  BGL_FORALL_VERTICES(v, empty_circ_.dag, DAG) {
    if (!empty_circ_.detect_boundary_Op(v)) {
      bin.push_back(v);
    }
  }

  empty_circ_.remove_vertices(
      bin, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);

  input_circ_ = Circuit(empty_circ_);
  box_circ_ = Circuit(nq_);
  post_circ_ = Circuit(empty_circ_);

  all_qu_ = circ_.all_qubits();
}

void CircToPhasePolyConversion::add_phase_poly_box() {
  qubit_types_.assign(nq_, QubitType::pre);

  if (box_size_ >= min_size_) {
    PhasePolyBox ppbox(box_circ_);
    circ_.add_box(ppbox, all_qu_);
  } else {
    for (const Command& com : box_circ_) {
      OpType ot = com.get_op_ptr()->get_type();
      TKET_ASSERT(ot == OpType::CX || ot == OpType::Rz);
      unit_vector_t args;
      for (const UnitID& qb : com.get_args()) {
        // box_circ_ uses dense default indices; circ_ uses the original units.
        args.push_back(all_qu_.at(qb.index().at(0)));
      }
      circ_.add_op<UnitID>(com.get_op_ptr(), args);
    }
  }

  for (const Command& post_com : post_circ_) {
    OpType post_ot = post_com.get_op_ptr()->get_type();
    // no other type should be in this list
    bool expected = (post_ot == OpType::H) || (post_ot == OpType::Measure) ||
                    (post_ot == OpType::Collapse) || (post_ot == OpType::Reset);
    TKET_ASSERT(expected);
    circ_.add_op<UnitID>(post_com.get_op_ptr(), post_com.get_args());
  }

  post_circ_ = Circuit(empty_circ_);
  box_circ_ = Circuit(nq_);
  box_size_ = 0;
}

void CircToPhasePolyConversion::convert() {
  for (const Command& com : circ_) {
    OpType ot = com.get_op_ptr()->get_type();
    unit_vector_t qbs = com.get_args();
    switch (ot) {
      case OpType::CX: {
        input_circ_.add_op<UnitID>(ot, {qbs[0], qbs[1]});
        break;
      }
      case OpType::Rz: {
        auto angle = com.get_op_ptr()->get_params().at(0);
        input_circ_.add_op<UnitID>(ot, angle, {qbs[0]});
        break;
      }
      case OpType::H:
      case OpType::Collapse:
      case OpType::Reset: {
        input_circ_.add_op<UnitID>(ot, {qbs[0]});
        break;
      }
      case OpType::Measure: {
        input_circ_.add_op<UnitID>(ot, {qbs[0], qbs[1]});
        break;
      }
      case OpType::Barrier: {
        input_circ_.add_barrier(qbs);
        break;
      }
      case OpType::Phase: {
        input_circ_.add_op<UnitID>(
            ot, com.get_op_ptr()->get_params().at(0), qbs);
        break;
      }
      default: {
        throw BadOpType(
            "Please rebase with the compiler pass RebaseUFR to only CX, Rz, H, "
            "measure, reset, collapse, barrier gates. Found gate of different "
            "type",
            ot);
      }
    }
  }

  VertexList bin;

  BGL_FORALL_VERTICES(v, circ_.dag, DAG) {
    if (!circ_.detect_boundary_Op(v)) {
      bin.push_back(v);
    }
  }

  circ_.remove_vertices(
      bin, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::Yes);

  /*
this for loop checks all gates in the circuits and try to
find the biggest possible sub circuits which contains only
CX+Rz Gates. This is done in a way that all qubits get
marked if they are currently outside before (pre), in (in)
or outside after (post) of a currently constructed box. If
a qubit is in the pre state and a gate, which is no valid
type in a box should be added, the type is not changed and
the gate is added. If a CX or Rz gate should be added to
this qubit the type is changes to in and the gate is added
to the box. If another gate should be added to a qubit in
state, the state stays the same for CX and Rz and they are
added to the box. If the added gate has a different type
the type of the qubit is changed to post and the gate is
added after the box. If a CX or Rz gate should be added to
a qubit in the post state, the current box is added and a
new box is started to which the current gate is added. All
qubits states are reset to pre.
*/
  for (const Command& com : input_circ_) {
    OpType ot = com.get_op_ptr()->get_type();
    unit_vector_t qbs = com.get_args();
    switch (ot) {
      case OpType::Barrier: {
        add_phase_poly_box();

        circ_.add_barrier(qbs);
        break;
      }
      case OpType::CX: {
        unsigned ctrl = qubit_indices_.at(Qubit(qbs[0]));
        unsigned target = qubit_indices_.at(Qubit(qbs[1]));
        if ((qubit_types_[ctrl] == QubitType::in) &&
            (qubit_types_[target] == QubitType::in)) {
          box_circ_.add_op<unsigned>(OpType::CX, {ctrl, target});
        } else if (
            (qubit_types_[ctrl] == QubitType::pre) &&
            (qubit_types_[target] == QubitType::in)) {
          qubit_types_[ctrl] = QubitType::in;
          box_circ_.add_op<unsigned>(OpType::CX, {ctrl, target});
        } else if (
            (qubit_types_[ctrl] == QubitType::in) &&
            (qubit_types_[target] == QubitType::pre)) {
          qubit_types_[target] = QubitType::in;
          box_circ_.add_op<unsigned>(OpType::CX, {ctrl, target});
        } else if (
            (qubit_types_[ctrl] == QubitType::pre) &&
            (qubit_types_[target] == QubitType::pre)) {
          qubit_types_[ctrl] = QubitType::in;
          qubit_types_[target] = QubitType::in;
          box_circ_.add_op<unsigned>(OpType::CX, {ctrl, target});
        } else if (
            (qubit_types_[ctrl] == QubitType::post) ||
            (qubit_types_[target] == QubitType::post)) {
          add_phase_poly_box();

          qubit_types_[ctrl] = QubitType::in;
          qubit_types_[target] = QubitType::in;
          box_circ_.add_op<unsigned>(OpType::CX, {ctrl, target});
        } else {
          // no other types should be in this list
          TKET_ASSERT(!"Invalid Qubit Type in Phase Poly Box creation");
        }
        ++box_size_;
        break;
      }
      case OpType::Rz: {
        unsigned qb = qubit_indices_.at(Qubit(qbs[0]));
        auto angle = com.get_op_ptr()->get_params().at(0);
        switch (qubit_types_[qb]) {
          case QubitType::pre: {
            box_circ_.add_op<unsigned>(OpType::Rz, angle, {qb});
            qubit_types_[qb] = QubitType::in;
            break;
          }
          case QubitType::in: {
            box_circ_.add_op<unsigned>(OpType::Rz, angle, {qb});
            break;
          }
          case QubitType::post: {
            add_phase_poly_box();

            qubit_types_[qb] = QubitType::in;
            box_circ_.add_op<unsigned>(OpType::Rz, angle, {qb});
            break;
          }
          default: {
            // no other types should be in this list
            TKET_ASSERT(!"Invalid Qubit Type in Phase Poly Box creation");
          }
        }
        break;
      }
      case OpType::H:
      case OpType::Collapse:
      case OpType::Reset: {
        unsigned qb = qubit_indices_.at(Qubit(qbs[0]));
        switch (qubit_types_[qb]) {
          case QubitType::pre: {
            circ_.add_op<UnitID>(ot, {qbs[0]});
            break;
          }
          case QubitType::in: {
            post_circ_.add_op<UnitID>(ot, {qbs[0]});
            qubit_types_[qb] = QubitType::post;
            break;
          }
          case QubitType::post: {
            post_circ_.add_op<UnitID>(ot, {qbs[0]});
            break;
          }
          default: {
            // no other types should be in this list
            TKET_ASSERT(!"Invalid Qubit Type in Phase Poly Box creation");
          }
        }
        break;
      }
      case OpType::Measure: {
        unsigned qb = qubit_indices_.at(Qubit(qbs[0]));

        switch (qubit_types_[qb]) {
          case QubitType::pre: {
            circ_.add_op<UnitID>(ot, {qbs[0], qbs[1]});
            break;
          }
          case QubitType::in: {
            post_circ_.add_op<UnitID>(ot, {qbs[0], qbs[1]});
            qubit_types_[qb] = QubitType::post;
            break;
          }
          case QubitType::post: {
            post_circ_.add_op<UnitID>(ot, {qbs[0], qbs[1]});
            break;
          }
          default: {
            // no other types should be in this list
            TKET_ASSERT(!"Invalid Qubit Type in Phase Poly Box creation");
          }
        }
        break;
      }
      case OpType::Phase: {
        circ_.add_op<unsigned>(ot, com.get_op_ptr()->get_params().at(0), {});
        break;
      }
      default: {
        throw BadOpType(
            "Please rebase with the compiler pass RebaseUFR to only CX, Rz, H, "
            "measure, reset, collapse, barrier gates. Found gate of different "
            "type",
            ot);
      }
    }
  }

  // add the last box to the circuit
  add_phase_poly_box();
}

Circuit CircToPhasePolyConversion::get_circuit() const { return circ_; }

}  // namespace tket
