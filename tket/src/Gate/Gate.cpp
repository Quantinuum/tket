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

#include "tket/Gate/Gate.hpp"

#include <algorithm>
#include <stdexcept>
#include <tkrng/RNG.hpp>
#include <vector>

#include "symengine/eval_double.h"
#include "tket/Gate/GateUnitaryMatrix.hpp"
#include "tket/Gate/GateUnitaryMatrixError.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/OpType/OpTypeInfo.hpp"
#include "tket/Ops/Op.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/PauliTensor.hpp"

namespace tket {
using std::stringstream;

Op_ptr Gate::dagger() const {
  OpType optype = get_type();
  switch (optype) {
    case OpType::H:
    case OpType::X:
    case OpType::Y:
    case OpType::Z:
    case OpType::SWAP:
    case OpType::CH:
    case OpType::CX:
    case OpType::CY:
    case OpType::CZ:
    case OpType::CCX:
    case OpType::noop:
    case OpType::CSWAP:
    case OpType::ECR:
    case OpType::BRIDGE: {
      return get_op_ptr(optype);
    }
    case OpType::CnX:
    case OpType::CnZ:
    case OpType::CnY: {
      return get_op_ptr(optype, std::vector<Expr>(), n_qubits_);
    }
    case OpType::S: {
      return get_op_ptr(OpType::Sdg);
    }
    case OpType::Sdg: {
      return get_op_ptr(OpType::S);
    }
    case OpType::T: {
      return get_op_ptr(OpType::Tdg);
    }
    case OpType::Tdg: {
      return get_op_ptr(OpType::T);
    }
    case OpType::V: {
      return get_op_ptr(OpType::Vdg);
    }
    case OpType::Vdg: {
      return get_op_ptr(OpType::V);
    }
    case OpType::CV: {
      return get_op_ptr(OpType::CVdg);
    }
    case OpType::CVdg: {
      return get_op_ptr(OpType::CV);
    }
    case OpType::SX: {
      return get_op_ptr(OpType::SXdg);
    }
    case OpType::SXdg: {
      return get_op_ptr(OpType::SX);
    }
    case OpType::CSX: {
      return get_op_ptr(OpType::CSXdg);
    }
    case OpType::CSXdg: {
      return get_op_ptr(OpType::CSX);
    }
    case OpType::CS: {
      return get_op_ptr(OpType::CSdg);
    }
    case OpType::CSdg: {
      return get_op_ptr(OpType::CS);
    }
    case OpType::Phase:
    case OpType::CRz:
    case OpType::CRx:
    case OpType::CRy:
    case OpType::CU1:
    case OpType::U1:
    case OpType::Rz:
    case OpType::Ry:
    case OpType::Rx:
    case OpType::PhaseGadget:
    case OpType::CnRy:
    case OpType::CnRx:
    case OpType::CnRz:
    case OpType::XXPhase:
    case OpType::YYPhase:
    case OpType::ZZPhase:
    case OpType::XXPhase3:
    case OpType::ISWAP:
    case OpType::ESWAP: {
      return get_op_ptr(optype, minus_times(params_[0]), n_qubits_);
    }
    case OpType::GPI: {
      return get_op_ptr(optype, params_[0]);
    }
    case OpType::GPI2: {
      return get_op_ptr(optype, params_[0] + 1);
    }
    case OpType::AAMS: {
      return get_op_ptr(optype, {params_[0], params_[1] + 1, params_[2]});
    }
    case OpType::ZZMax: {
      // ZZMax.dagger = ZZPhase(-0.5)
      return get_op_ptr(OpType::ZZPhase, -0.5);
    }
    case OpType::FSim: {
      // FSim(a,b).dagger() == FSim(-a,-b)
      return get_op_ptr(
          OpType::FSim, {minus_times(params_[0]), minus_times(params_[1])});
    }
    case OpType::Sycamore: {
      return get_op_ptr(OpType::FSim, {-0.5, -1. / 6.});
    }
    case OpType::ISWAPMax: {
      return get_op_ptr(OpType::ISWAP, 3.);
    }
    case OpType::U2: {
      // U2(a,b).dagger() == U3(-pi/2,-b,-a)
      return get_op_ptr(
          OpType::U3, {-0.5, minus_times(params_[1]), minus_times(params_[0])});
    }
    case OpType::U3:
    case OpType::CU3:
      // U3(a,b,c).dagger() == U3(-a,-c.-b)
      {
        return get_op_ptr(
            optype, {minus_times(params_[0]), minus_times(params_[2]),
                     minus_times(params_[1])});
      }
    case OpType::TK1:
      // TK1(a,b,c).dagger() == TK1(-c,-b,-a)
      {
        return get_op_ptr(
            OpType::TK1, {minus_times(params_[2]), minus_times(params_[1]),
                          minus_times(params_[0])});
      }
    case OpType::TK2:
      return get_op_ptr(
          OpType::TK2, {minus_times(params_[0]), minus_times(params_[1]),
                        minus_times(params_[2])});
    case OpType::PhasedX:
    case OpType::NPhasedX:
    case OpType::TwinPhasedX:
    case OpType::PhasedXX:
      // PhasedX(a,b).dagger() == PhasedX(-a,b)
      // PhasedXX(a,b).dagger() == PhasedXX(-a,b)
      {
        return get_op_ptr(
            optype, {minus_times(params_[0]), params_[1]}, n_qubits_);
      }
    case OpType::PhasedISWAP:
      // PhasedISWAP(a,b).dagger() == PhasedISWAP(a,-b)
      {
        return get_op_ptr(
            OpType::PhasedISWAP, {params_[0], minus_times(params_[1])});
      }
    default: {
      throw BadOpType("Cannot compute dagger", optype);
    }
  }
}

Op_ptr Gate::transpose() const {
  OpType optype = get_type();
  switch (optype) {
    case OpType::Phase:
    case OpType::H:
    case OpType::X:
    case OpType::Z:
    case OpType::SWAP:
    case OpType::CH:
    case OpType::CX:
    case OpType::CZ:
    case OpType::CV:
    case OpType::CVdg:
    case OpType::CSX:
    case OpType::CSXdg:
    case OpType::CS:
    case OpType::CSdg:
    case OpType::CCX:
    case OpType::noop:
    case OpType::CSWAP:
    case OpType::BRIDGE:
    case OpType::S:
    case OpType::Sdg:
    case OpType::T:
    case OpType::V:
    case OpType::Vdg:
    case OpType::SX:
    case OpType::SXdg:
    case OpType::CRz:
    case OpType::CRx:
    case OpType::CnRz:
    case OpType::CnRx:
    case OpType::CU1:
    case OpType::U1:
    case OpType::Rz:
    case OpType::Rx:
    case OpType::PhaseGadget:
    case OpType::XXPhase:
    case OpType::YYPhase:
    case OpType::ZZPhase:
    case OpType::TK2:
    case OpType::XXPhase3:
    case OpType::ESWAP:
    case OpType::FSim: {
      return get_op_ptr(optype, params_);
    }
    case OpType::Y: {
      return get_op_ptr(OpType::U3, {3, 0.5, 0.5});
    }
    case OpType::Ry:
    case OpType::CRy:
    case OpType::CnRy: {
      return get_op_ptr(optype, minus_times(params_[0]), n_qubits_);
    }
    case OpType::CnX:
    case OpType::CnZ: {
      return get_op_ptr(optype, std::vector<Expr>(), n_qubits_);
    }
    case OpType::GPI:
    case OpType::GPI2: {
      return get_op_ptr(optype, -params_[0]);
    }
    case OpType::AAMS: {
      return get_op_ptr(optype, {params_[0], -params_[1], -params_[2]});
    }
    case OpType::U2: {
      // U2(a,b).transpose() == U2(b+1,a+1)
      return get_op_ptr(OpType::U2, {params_[1] + 1., params_[0] + 1.});
    }
    case OpType::U3:
    case OpType::CU3: {
      // U3(a,b,c).transpose() == U3(-a,c,b)
      return get_op_ptr(
          OpType::U3, {minus_times(params_[0]), params_[2], params_[1]});
    }
    case OpType::TK1: {
      // TK1(a,b,c).transpose() == TK1(c,b,a)
      return get_op_ptr(OpType::TK1, {params_[2], params_[1], params_[0]});
    }
    case OpType::PhasedX:
    case OpType::NPhasedX:
    case OpType::TwinPhasedX: {
      // PhasedX(a,b).transpose() == PhasedX(a,-b)
      return get_op_ptr(
          optype, {params_[0], minus_times(params_[1])}, n_qubits_);
    }
    case OpType::PhasedISWAP: {
      // PhasedISWAP(a,b).transpose() == PhasedISWAP(-a,b)
      return get_op_ptr(
          OpType::PhasedISWAP, {minus_times(params_[0]), params_[1]});
    }
    case OpType::PhasedXX: {
      // PhasedXX(a,b).transpose() == PhasedXX(-a,-b)
      return get_op_ptr(
          OpType::PhasedXX, {minus_times(params_[0]), minus_times(params_[1])});
    }

    default: {
      throw BadOpType("Cannot compute transpose", optype);
    }
  }
}

static bool params_contain_nan(const std::vector<Expr>& params) {
  static const std::regex nan_regex("\\bnan\\b");
  for (const Expr& e : params) {
    stringstream ss;
    ss << e;
    if (std::regex_search(ss.str(), nan_regex)) return true;
  }
  return false;
}

static double random_perturbation() {
  static RNG rng;
  int a = rng.get_size_t(10);
  return (a - 5) * EPS;
}

// If `to_doubles` is true, when an expression contains no free symbols evaluate
// it as a double. This is appropriate in the case where an expression has been
// perturbed, when there is no point retaining exact values for symbolic
// constants.
static std::vector<Expr> subs_all_params(
    const std::vector<Expr>& params, const SymEngine::map_basic_basic& sub_map,
    bool to_doubles = false) {
  std::vector<Expr> new_params;
  for (const Expr& p : params) {
    Expr psub = p.subs(sub_map);
    if (to_doubles && expr_free_symbols(psub).empty()) {
      new_params.push_back(SymEngine::eval_double(psub));
    } else {
      new_params.push_back(psub);
    }
  }
  return new_params;
}

Op_ptr Gate::symbol_substitution(
    const SymEngine::map_basic_basic& sub_map) const {
  // Perform symbolic substitution, but catch the case where the returned
  // expression is not a number, and in that case try to set a value by
  // perturbing the inputs. This deals with cases where expressions contain
  // terms that are undefined at specific values but where the singularity is
  // removable at the op level. (Non-removable singularities, which ought
  // strictly to fail substitution, will result in spectacularly wrong values,
  // but are unlikely to arise in practice.)
  //
  // This is a partial workaround for issues arising from squashing symbolic
  // rotations, where it is hard or impossible to handle special cases (e.g.
  // where the general formula reduces to one involving atan2(0,0)).
  //
  // A proper solution to this problem may have to wait for TKET 2.
  std::vector<Expr> new_params = subs_all_params(this->params_, sub_map);
  if (!params_contain_nan(new_params)) {  // happy path
    return get_op_ptr(this->type_, new_params, this->n_qubits_);
  }

  // Try perturbing all values in the map. May need several attempts in case
  // there are subexpressions on the boundary of validity, such as acos(1.0). If
  // we fail after 1000 attempts, give up.
  for (unsigned i = 0; i < 1000; i++) {
    SymEngine::map_basic_basic new_sub_map;
    for (const auto& pair : sub_map) {
      new_sub_map[pair.first] = Expr(pair.second) + random_perturbation();
    }
    std::vector<Expr> new_params_1 =
        subs_all_params(this->params_, new_sub_map, true);
    if (!params_contain_nan(new_params_1)) {
      return get_op_ptr(this->type_, new_params_1, this->n_qubits_);
    }
  }

  // Something really is fishy.
  std::stringstream msg;
  msg << "Failed to substitute values { ";
  for (const auto& pair : sub_map) {
    msg << Expr(pair.first) << " --> " << Expr(pair.second) << ", ";
  }
  msg << "} in operation " << get_name() << ".";
  throw SubstitutionFailure(msg.str());
}

std::optional<double> Gate::is_identity() const {
  static const std::optional<double> notid;
  const std::vector<Expr>& params = get_params();
  switch (get_type()) {
    case OpType::noop: {
      return 0.;
    }
    case OpType::Phase: {
      // This is _always_ the identity up to phase, but the method does not
      // allow us to return a symbolic phase, so we must reject in that case.
      std::optional<double> eval = eval_expr(params[0]);
      if (!eval) return notid;
      return eval.value();
    }
    case OpType::Rx:
    case OpType::Ry:
    case OpType::Rz:
    case OpType::PhasedX:
    case OpType::NPhasedX:
    case OpType::TwinPhasedX:
    case OpType::PhasedXX:
    case OpType::XXPhase:
    case OpType::YYPhase:
    case OpType::ZZPhase:
    case OpType::XXPhase3:
    case OpType::ESWAP:
    case OpType::AAMS: {
      Expr e = params[0];
      if (equiv_0(e, 4)) {
        return 0.;
      } else if (equiv_0(e + 2, 4)) {
        return 1.;
      } else
        return notid;
    }
    case OpType::U1:
    case OpType::CU1: {
      return equiv_0(params[0]) ? 0. : notid;
    }
    case OpType::U3: {
      Expr theta = params[0];
      if (equiv_0(params[1] + params[2])) {
        if (equiv_0(theta, 4)) {
          return 0.;
        } else if (equiv_0(theta + 2, 4)) {
          return 1.;
        } else
          return notid;
      } else
        return notid;
    }
    case OpType::CU3: {
      if (equiv_0(params[0], 4) && equiv_0(params[1] + params[2])) {
        return 0.;
      } else
        return notid;
    }
    case OpType::TK1: {
      Expr s = params[0] + params[2], t = params[1];
      if (equiv_0(s) && equiv_0(t)) {
        return (equiv_0(s, 4) ^ equiv_0(t, 4)) ? 1. : 0.;
      } else
        return notid;
    }
    case OpType::TK2: {
      bool pi_phase = false;
      for (const Expr& a : params) {
        if (equiv_0(a + 2, 4)) {
          pi_phase = !pi_phase;
        } else if (!equiv_0(a, 4)) {
          return notid;
        }
      }
      return pi_phase ? 1. : 0.;
    }
    case OpType::CRz:
    case OpType::CRx:
    case OpType::CRy:
    case OpType::PhaseGadget:
    case OpType::ISWAP:
    case OpType::CnRy:
    case OpType::CnRx:
    case OpType::CnRz: {
      return equiv_0(params[0], 4) ? 0. : notid;
    }
    case OpType::FSim: {
      return (equiv_0(params[0]) && equiv_0(params[1])) ? 0. : notid;
    }
    case OpType::PhasedISWAP: {
      return (equiv_0(params[0], 1) && equiv_0(params[1], 4)) ? 0. : notid;
    }
    default:
      return notid;
  }
}

bool Gate::is_clifford() const {
  if (is_clifford_type(type_)) return true;

  switch (type_) {
    case OpType::Rx:
    case OpType::Ry:
    case OpType::Rz:
    case OpType::U1:
    case OpType::U2:
    case OpType::U3:
    case OpType::TK1:
    case OpType::TK2:
    case OpType::XXPhase:
    case OpType::YYPhase:
    case OpType::ZZPhase:
    case OpType::XXPhase3:
      return std::all_of(params_.begin(), params_.end(), [](const Expr& e) {
        return equiv_0(4 * e);
      });
    case OpType::PhasedX:
    case OpType::NPhasedX:
    case OpType::TwinPhasedX:
    case OpType::PhasedXX:
      return std::all_of(
                 params_.begin(), params_.end(),
                 [](const Expr& e) { return equiv_0(4 * e); }) ||
             (equiv_0(2 * params_.at(0)) && equiv_0(8 * params_.at(1)));
    case OpType::ISWAP:
    case OpType::ESWAP:
      return equiv_0(2 * params_.at(0));
    case OpType::PhasedISWAP:
    case OpType::FSim:
      return equiv_0(4 * params_.at(0)) && equiv_0(2 * params_.at(1));
    case OpType::GPI:
      return equiv_0(8 * params_.at(0));
    case OpType::GPI2:
      return equiv_0(4 * params_.at(0));
    case OpType::AAMS:
      if (equiv_0(params_.at(0))) {
        return true;
      } else if (
          !equiv_0(4 * params_.at(0)) || !equiv_0(8 * params_.at(1)) ||
          !equiv_0(8 * params_.at(2))) {
        return false;
      } else if (equiv_0(2 * params_.at(0))) {
        return true;
      } else {
        return equiv_0(4 * params_.at(1)) && equiv_0(4 * params_.at(2));
      }
    default:
      return false;
  }
}

bool Gate::has_symmetry(unsigned port1, unsigned port2) const {
  const auto n_q = n_qubits();
  if (port1 >= n_q || port2 >= n_q) {
    throw std::out_of_range("port ids must be less than n_qubits");
  }
  if (port1 == port2) {
    // exchanging with self is always symmetric
    return true;
  }
  OpType optype = get_type();
  switch (optype) {
      // a two qubit gate G2 is symmetric
      // if and only if its matrix form (g2_{ij}), i,j in 0,1,2,3, satisfies
      // g2_01 == a02, g2_10 == g2_20, g2_13 == g2_23, g2_31 == g2_32,
      // g2_11 == g2_22, and g2_12 == g2_21. (Follows from stipulation SWAP G2
      // SWAP == G2) If G2 is a controlled one-qubit gate, G2 = CG1. Then G1
      // must satisfy g1_00 == 1, g1_01 == g1_10 == 0
    case OpType::CH:
    case OpType::CX:
    case OpType::CY:
    case OpType::ECR:
    case OpType::CV:
    case OpType::CVdg:
    case OpType::CSX:
    case OpType::CSXdg:
    case OpType::CRz:
    case OpType::CRx:
    case OpType::CRy: {
      // not symmetric
      return false;
    }
    case OpType::SWAP:
    case OpType::ISWAP:
    case OpType::ESWAP:
    case OpType::PhasedISWAP:
    case OpType::ISWAPMax:
    case OpType::CZ:
    case OpType::XXPhase:
    case OpType::YYPhase:
    case OpType::ZZPhase:
    case OpType::ZZMax:
    case OpType::FSim:
    case OpType::Sycamore:
    case OpType::TK2:
    case OpType::CS:
    case OpType::CSdg:
    case OpType::CU1: {
      // symmetric
      return true;
    }
    case OpType::CU3: {
      // symmetric if first param theta == 0
      const auto theta = get_params()[0];
      return equiv_0(theta);
    }
      // three qubit gates
    case OpType::CCX: {
      // 0 <-> 1 symmetry
      return port1 + port2 == 1;  // will be 2 or 3 for non-symmetric cases
    }
    case OpType::CSWAP: {
      // 1 <-> 2 symmetry
      return port1 + port2 == 3;  // will be 1 or 2 for non-symmetric cases
    }
    case OpType::BRIDGE: {
      // no symmetry
      return false;
    }
    case OpType::XXPhase3: {
      // completely symmetric
      return true;
    }
      // n (+1) qubit gates
    case OpType::CnX:
    case OpType::CnY:
    case OpType::CnRx:
    case OpType::CnRz:
    case OpType::CnRy: {
      // symmetry on first n ports not on n+1
      auto last_port = n_q - 1;
      return not(port1 == last_port || port2 == last_port);
    }
    case OpType::CnZ:
    case OpType::PhaseGadget:
    case OpType::NPhasedX: {
      // symmetry on all n (+1) ports
      return true;
    }
    default: {
      return false;
    }
  }
}

std::string Gate::get_name(bool latex) const {
  OpDesc desc = get_desc();
  if (params_.size() > 0) {
    std::stringstream name;
    if (latex) {
      name << desc.latex() << "(";
    } else {
      name << desc.name() << "(";
    }
    for (unsigned i = 0; i < params_.size(); ++i) {
      std::optional<double> reduced =
          eval_expr_mod(params_[i], desc.param_mod(i));
      if (reduced) {
        name << reduced.value();
      } else {
        name << params_[i];
      }
      if (i < params_.size() - 1) name << ", ";
    }
    name << ")";
    return name.str();
  } else {
    return Op::get_name(latex);
  }
}

std::string Gate::get_command_str(const unit_vector_t& args) const {
  if (get_type() == OpType::Measure) {
    std::stringstream out;
    out << get_name() << " " << args[0].repr() << " --> " << args[1].repr()
        << ";";
    return out.str();
  } else
    return Op::get_command_str(args);
}

unsigned Gate::n_qubits() const {
  OptUInt n = desc_.n_qubits();
  if (n == any) {
    return n_qubits_;
  } else {
    return n.value();
  }
}

bool Gate::is_equal(const Op& op_other) const {
  const Gate& other = dynamic_cast<const Gate&>(op_other);

  OpDesc desc = get_desc();
  if (n_qubits() != other.n_qubits()) return false;
  std::vector<Expr> params1 = this->get_params();
  std::vector<Expr> params2 = other.get_params();
  unsigned param_count = params1.size();
  if (params2.size() != param_count) return false;
  for (unsigned i = 0; i < param_count; i++) {
    if (!equiv_expr(params1[i], params2[i], desc.param_mod(i))) return false;
  }
  return true;
}

std::vector<Expr> Gate::get_params_reduced() const {
  OpDesc desc = get_desc();
  unsigned n_params = desc.n_params();
  std::vector<Expr> params(n_params);
  for (unsigned i = 0; i < n_params; i++) {
    Expr param = params_[i];
    std::optional<double> e = eval_expr_mod(param, desc.param_mod(i));
    if (e) {
      params[i] = e.value();
    } else {
      params[i] = param;
    }
  }
  return params;
}

std::vector<Expr> Gate::get_tk1_angles() const {
  const Expr half =
      SymEngine::div(SymEngine::integer(1), SymEngine::integer(2));
  const Expr quarter =
      SymEngine::div(SymEngine::integer(1), SymEngine::integer(4));
  const Expr eighth =
      SymEngine::div(SymEngine::integer(1), SymEngine::integer(8));
  switch (get_type()) {
    case OpType::noop: {
      return {0, 0, 0, 0};
    }
    case OpType::Z: {
      return {0, 0, 1, half};
    }
    case OpType::X: {
      return {0, 1, 0, half};
    }
    case OpType::Y: {
      return {half, 1, -half, half};
    }
    case OpType::S: {
      return {0, 0, half, quarter};
    }
    case OpType::Sdg: {
      return {0, 0, -half, -quarter};
    }
    case OpType::T: {
      return {0, 0, quarter, eighth};
    }
    case OpType::Tdg: {
      return {0, 0, -quarter, -eighth};
    }
    case OpType::V: {
      return {0, half, 0, 0};
    }
    case OpType::Vdg: {
      return {0, -half, 0, 0};
    }
    case OpType::SX: {
      return {0, half, 0, quarter};
    }
    case OpType::SXdg: {
      return {0, -half, 0, -quarter};
    }
    case OpType::H: {
      return {half, half, half, half};
    }
    case OpType::Rx: {
      return {0, params_.at(0), 0, 0};
    }
    case OpType::Ry: {
      return {half, params_.at(0), -half, 0};
    }
    case OpType::Rz:
    case OpType::PhaseGadget: {
      return {0, 0, params_.at(0), 0};
    }
    case OpType::U1: {
      return {0, 0, params_.at(0), params_.at(0) / 2};
    }
    case OpType::U2: {
      return {
          params_.at(0) + half, half, params_.at(1) - half,
          (params_.at(0) + params_.at(1)) / 2};
    }
    case OpType::U3: {
      return {
          params_.at(1) + half, params_.at(0), params_.at(2) - half,
          (params_.at(1) + params_.at(2)) / 2};
    }
    case OpType::GPI: {
      return {2 * params_.at(0), 1, 0, half};
    }
    case OpType::GPI2: {
      return {params_.at(0), half, -params_.at(0), 0};
    }
    case OpType::NPhasedX: {
      if (n_qubits_ != 1) {
        throw BadOpType(
            "OpType::NPhasedX can only be decomposed into a TK1 "
            "if it acts on a single qubit",
            OpType::NPhasedX);
      }
      return {params_.at(1), params_.at(0), minus_times(params_.at(1)), 0};
    }
    case OpType::PhasedX: {
      return {params_.at(1), params_.at(0), minus_times(params_.at(1)), 0};
    }
    case OpType::TK1: {
      return {params_.at(0), params_.at(1), params_.at(2), 0};
    }
    default: {
      throw BadOpType("Cannot compute TK1 angles", get_type());
    }
  }
}

std::vector<Expr> Gate::get_params() const { return params_; }

SymSet Gate::free_symbols() const { return expr_free_symbols(get_params()); }

/**
 * @brief The commutation colour of TK1(a,b,c)
 *
 * - commutes with everything if b == 0 && a == -c
 * - commutes with Z if b == 0
 * - commutes with X if a == c == 0
 * - commutes with nothing otherwise
 *
 * @param params the TK1 angles
 * @return std::optional<Pauli> commutation colour
 */
static std::optional<Pauli> tk1_commuting_basis(std::vector<Expr> params) {
  const bool p0_is_zero = equiv_0(params[0], 2);
  const bool p1_is_zero = equiv_0(params[1], 2);
  const bool p2_is_zero = equiv_0(params[2], 2);
  if (p1_is_zero && equiv_expr(params[0], -params[2], 2)) {
    return Pauli::I;
  } else if (p1_is_zero) {
    return Pauli::Z;
  } else if (p0_is_zero && p2_is_zero) {
    return Pauli::X;
  } else {
    return std::nullopt;
  }
}

/**
 * @brief The commutation colour of TK2(a,b,c)
 *
 * - commutes with everything if a == b == c
 * - commutes with X if b == c == 0
 * - commutes with Y if a == c == 0
 * - commutes with Z if a == b == 0
 * - commutes with nothing otherwise
 *
 * @param params the TK2 angles
 * @return std::optional<Pauli> commutation colour
 */
static std::optional<Pauli> tk2_commuting_basis(std::vector<Expr> params) {
  const bool p0_is_zero = equiv_0(params[0], 2);
  const bool p1_is_zero = equiv_0(params[1], 2);
  const bool p2_is_zero = equiv_0(params[2], 2);
  if (p0_is_zero && p1_is_zero && p2_is_zero) {
    return Pauli::I;
  } else if (p1_is_zero && p2_is_zero) {
    return Pauli::X;
  } else if (p0_is_zero && p2_is_zero) {
    return Pauli::Y;
  } else if (p0_is_zero && p1_is_zero) {
    return Pauli::Z;
  } else {
    return std::nullopt;
  }
}

std::optional<Pauli> Gate::commuting_basis(port_t i) const {
  unsigned n_q = n_qubits();
  if (i >= n_q) throw std::domain_error("Qubit index out of range");
  switch (get_type()) {
    case OpType::Phase: {
      return Pauli::I;
    }
    case OpType::X:
    case OpType::V:
    case OpType::Vdg:
    case OpType::SX:
    case OpType::SXdg:
    case OpType::Rx:
    case OpType::Y:
    case OpType::Ry:
    case OpType::noop:
    case OpType::H:
    case OpType::U3:
    case OpType::U2:
    case OpType::PhasedX:
    case OpType::Z:
    case OpType::S:
    case OpType::Sdg:
    case OpType::T:
    case OpType::Tdg:
    case OpType::Rz:
    case OpType::U1:
    case OpType::TK1: {
      return tk1_commuting_basis(get_tk1_angles());
    }
    case OpType::XXPhase:
    case OpType::XXPhase3: {
      return Pauli::X;
    }
    case OpType::YYPhase: {
      return Pauli::Y;
    }
    case OpType::CZ:
    case OpType::CRz:
    case OpType::CnRz:
    case OpType::CS:
    case OpType::CSdg:
    case OpType::CU1:
    case OpType::PhaseGadget:
    case OpType::ZZMax:
    case OpType::ZZPhase:
    case OpType::CnZ: {
      return Pauli::Z;
    }
    case OpType::NPhasedX:
    case OpType::TwinPhasedX: {
      return tk1_commuting_basis({params_[1], params_[0], -params_[1], 0.});
    }
    case OpType::TK2: {
      return tk2_commuting_basis(params_);
    }
    case OpType::CH:
    case OpType::CU3:
    case OpType::CSWAP: {
      if (i == 0) {
        return Pauli::Z;
      } else {
        return std::nullopt;
      }
    }
    case OpType::BRIDGE: {
      if (i == 0) {
        return Pauli::Z;
      } else if (i == 2) {
        return Pauli::X;
      } else {
        return Pauli::I;
      }
    }
    case OpType::CV:
    case OpType::CVdg:
    case OpType::CSX:
    case OpType::CSXdg:
    case OpType::CRx:
    case OpType::CnRx:
    case OpType::CX:
    case OpType::CCX:
    case OpType::CnX: {
      if (i == n_q - 1) {
        return Pauli::X;
      } else {
        return Pauli::Z;
      }
    }
    case OpType::ECR: {
      if (i == 1) {
        return Pauli::X;
      } else {
        return std::nullopt;
      }
    }
    case OpType::CY:
    case OpType::CRy:
    case OpType::CnRy:
    case OpType::CnY: {
      if (i == n_q - 1) {
        return Pauli::Y;
      } else {
        return Pauli::Z;
      }
    }
    case OpType::PhasedXX: {
      if (equiv_0(params_[0])) {
        return Pauli::I;
      } else if (equiv_0(2 * params_[1])) {
        return Pauli::X;
      } else {
        return std::nullopt;
      }
    }
    default: {
      return std::nullopt;
    }
  }
}

op_signature_t Gate::get_signature() const {
  std::optional<op_signature_t> sig = desc_.signature();
  if (sig)
    return *sig;
  else
    return op_signature_t(n_qubits_, EdgeType::Quantum);
}

nlohmann::json Gate::serialize() const {
  nlohmann::json j;
  OpType optype = get_type();
  j["type"] = optype;
  // if type has a fixed signature, don't store number of qubits
  if (!optypeinfo().at(optype).signature) {
    j["n_qb"] = n_qubits();
  }
  std::vector<Expr> params = get_params_reduced();
  if (!params.empty()) {
    j["params"] = params;
  }
  return j;
}

Op_ptr Gate::deserialize(const nlohmann::json& j) {
  OpType optype = j.at("type").get<OpType>();
  std::vector<Expr> params;
  if (j.contains("params")) {
    params = j.at("params").get<std::vector<Expr>>();
  }
  // if type has fixed number of qubits use it, otherwise it should have been
  // stored
  const auto& sig = optypeinfo().at(optype).signature;
  unsigned n_qb;
  if (sig) {
    auto check_quantum = [](unsigned sum, const EdgeType& e) {
      return sum + (e == EdgeType::Quantum ? 1 : 0);
    };
    n_qb = std::accumulate(sig->begin(), sig->end(), 0, check_quantum);
  } else {
    n_qb = j.at("n_qb").get<unsigned>();
  }
  return get_op_ptr(optype, params, n_qb);
}

Eigen::MatrixXcd Gate::get_unitary() const {
  try {
    return GateUnitaryMatrix::get_unitary(*this);
  } catch (const GateUnitaryMatrixError& e) {
    switch (e.cause) {
      case GateUnitaryMatrixError::Cause::GATE_NOT_IMPLEMENTED:
        throw BadOpType(get_type());
      case GateUnitaryMatrixError::Cause::SYMBOLIC_PARAMETERS:
        throw SymbolsNotSupported();
      default:
        throw e;
    }
  }
}

Gate::Gate(OpType type, const std::vector<Expr>& params, unsigned n_qubits)
    : Op(type), params_(params), n_qubits_(n_qubits) {
  if (!is_gate_type(type)) {
    throw BadOpType(type);
  }
  if (params.size() != optypeinfo().at(type).n_params()) {
    throw InvalidParameterCount();
  }
}

Gate::Gate() : Op(OpType::noop), params_() {}

}  // namespace tket
