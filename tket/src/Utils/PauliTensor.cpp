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

#include "tket/Utils/PauliTensor.hpp"

#include <tkassert/Assert.hpp>

namespace tket {

void to_json(nlohmann::json &, const no_coeff_t &) {}
void from_json(const nlohmann::json &, no_coeff_t &) {}

template <>
no_coeff_t default_coeff<no_coeff_t>() {
  return {};
}
template <>
quarter_turns_t default_coeff<quarter_turns_t>() {
  return 0;
}
template <>
Complex default_coeff<Complex>() {
  return 1.;
}
template <>
Expr default_coeff<Expr>() {
  return 1;
}

template <>
QubitPauliMap cast_container<QubitPauliMap, QubitPauliMap>(
    const QubitPauliMap &cont) {
  return cont;
}

template <>
QubitPauliMap cast_container<DensePauliMap, QubitPauliMap>(
    const DensePauliMap &cont) {
  QubitPauliMap res;
  for (unsigned i = 0; i < cont.size(); ++i) {
    Pauli pi = cont.at(i);
    if (pi != Pauli::I) res.insert({Qubit(i), cont.at(i)});
  }
  return res;
}

template <>
DensePauliMap cast_container<QubitPauliMap, DensePauliMap>(
    const QubitPauliMap &cont) {
  if (cont.empty()) return {};
  unsigned max_index = 0;
  for (const std::pair<const Qubit, Pauli> &pair : cont) {
    if (pair.first.reg_info() != register_info_t{UnitType::Qubit, 1} ||
        pair.first.reg_name() != q_default_reg())
      throw std::logic_error(
          "Cannot cast a QubitPauliMap with non-default register qubits to a "
          "DensePauliMap");
    unsigned i = pair.first.index().front();
    if (i > max_index) max_index = i;
  }
  DensePauliMap res(max_index + 1, Pauli::I);
  for (const std::pair<const Qubit, Pauli> &pair : cont) {
    res.at(pair.first.index().front()) = pair.second;
  }
  return res;
}

template <>
DensePauliMap cast_container<DensePauliMap, DensePauliMap>(
    const DensePauliMap &cont) {
  return cont;
}

template <>
no_coeff_t cast_coeff<no_coeff_t, no_coeff_t>(const no_coeff_t &) {
  return {};
}
template <>
quarter_turns_t cast_coeff<no_coeff_t, quarter_turns_t>(const no_coeff_t &) {
  return 0;
}
template <>
Complex cast_coeff<no_coeff_t, Complex>(const no_coeff_t &) {
  return 1.;
}
template <>
Expr cast_coeff<no_coeff_t, Expr>(const no_coeff_t &) {
  return 1.;
}

template <>
no_coeff_t cast_coeff<quarter_turns_t, no_coeff_t>(const quarter_turns_t &) {
  return {};
}
template <>
quarter_turns_t cast_coeff<quarter_turns_t, quarter_turns_t>(
    const quarter_turns_t &coeff) {
  return coeff;
}
template <>
Complex cast_coeff<quarter_turns_t, Complex>(const quarter_turns_t &coeff) {
  switch (coeff % 4) {
    case 0: {
      return 1.;
    }
    case 1: {
      return i_;
    }
    case 2: {
      return -1.;
    }
    default: {
      return -i_;
    }
  }
}
template <>
Expr cast_coeff<quarter_turns_t, Expr>(const quarter_turns_t &coeff) {
  switch (coeff % 4) {
    case 0: {
      return Expr(1);
    }
    case 1: {
      return Expr(SymEngine::I);
    }
    case 2: {
      return Expr(-1);
    }
    default: {
      return -Expr(SymEngine::I);
    }
  }
}

template <>
no_coeff_t cast_coeff<Complex, no_coeff_t>(const Complex &) {
  return {};
}
template <>
quarter_turns_t cast_coeff<Complex, quarter_turns_t>(const Complex &coeff) {
  if (std::abs(coeff - 1.) < EPS)
    return 0;
  else if (std::abs(coeff - i_) < EPS)
    return 1;
  else if (std::abs(coeff + 1.) < EPS)
    return 2;
  else if (std::abs(coeff + i_) < EPS)
    return 3;
  else
    throw std::logic_error(
        "Could not cast PauliTensor coefficient to quarter turns: not a power "
        "of i.");
}
template <>
Complex cast_coeff<Complex, Complex>(const Complex &coeff) {
  return coeff;
}
template <>
Expr cast_coeff<Complex, Expr>(const Complex &coeff) {
  return Expr(SymEngine::make_rcp<const SymEngine::ComplexDouble>(coeff));
}

template <>
no_coeff_t cast_coeff<Expr, no_coeff_t>(const Expr &) {
  return {};
}
template <>
quarter_turns_t cast_coeff<Expr, quarter_turns_t>(const Expr &coeff) {
  std::optional<Complex> ev = eval_expr_c(coeff);
  if (ev)
    return cast_coeff<Complex, quarter_turns_t>(*ev);
  else
    throw std::logic_error(
        "Could not cast symbolic PauliTensor to quarter turns.");
}
template <>
Complex cast_coeff<Expr, Complex>(const Expr &coeff) {
  std::optional<Complex> ev = eval_expr_c(coeff);
  if (ev)
    return *ev;
  else
    throw std::logic_error(
        "Could not cast symbolic PauliTensor to complex coefficient.");
}
template <>
Expr cast_coeff<Expr, Expr>(const Expr &coeff) {
  return coeff;
}

template <>
int compare_containers<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  QubitPauliMap::const_iterator p1_it = first.begin();
  QubitPauliMap::const_iterator p2_it = second.begin();
  while (p1_it != first.end()) {
    if (p1_it->second == Pauli::I) {
      ++p1_it;
      continue;
    }
    while (p2_it != second.end() && p2_it->second == Pauli::I) {
      ++p2_it;
    }
    if (p2_it == second.end()) return 1;
    // QubitPauliMap order should reflect ILO
    // i.e. IZ < ZI (Zq1 < Zq0)
    // Hence we first order by reverse of leading qubit
    if (p1_it->first < p2_it->first) return 1;
    if (p2_it->first < p1_it->first) return -1;
    // and then by increasing order of Pauli letter on the same qubit
    if (p1_it->second < p2_it->second) return -1;
    if (p1_it->second > p2_it->second) return 1;
    ++p1_it;
    ++p2_it;
  }
  while (p2_it != second.end() && p2_it->second == Pauli::I) {
    ++p2_it;
  }
  return (p2_it == second.end()) ? 0 : -1;
}

template <>
int compare_containers<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second) {
  DensePauliMap::const_iterator p1_it = first.begin();
  DensePauliMap::const_iterator p2_it = second.begin();
  while (p1_it != first.end() && p2_it != second.end()) {
    if (*p1_it == Pauli::I) {
      if (*p2_it != Pauli::I) return -1;
    } else if (*p2_it == Pauli::I)
      return 1;
    else if (*p1_it < *p2_it)
      return -1;
    else if (*p1_it > *p2_it)
      return 1;
    ++p1_it;
    ++p2_it;
  }
  while (p1_it != first.end() && *p1_it == Pauli::I) ++p1_it;
  if (p1_it != first.end()) return 1;
  while (p2_it != second.end() && *p2_it == Pauli::I) ++p2_it;
  return (p2_it == second.end()) ? 0 : -1;
}

template <>
int compare_coeffs<no_coeff_t>(const no_coeff_t &, const no_coeff_t &) {
  return 0;
}
template <>
int compare_coeffs<quarter_turns_t>(
    const quarter_turns_t &first, const quarter_turns_t &second) {
  if (first % 4 < second % 4) return -1;
  return (first % 4 == second % 4) ? 0 : 1;
}
template <>
int compare_coeffs<Complex>(const Complex &first, const Complex &second) {
  if (first.real() < second.real()) return -1;
  if (first.real() > second.real()) return 1;
  if (first.imag() < second.imag()) return -1;
  return (first.imag() == second.imag()) ? 0 : 1;
}
template <>
int compare_coeffs<Expr>(const Expr &first, const Expr &second) {
  // Comparison of SymEngine expressions will distinguish between e.g. integer
  // 1, double 1., and complex 1., so only use for actual symbolic expressions
  std::optional<Complex> reduced_first = eval_expr_c(first);
  std::optional<Complex> reduced_second = eval_expr_c(second);
  if (reduced_first && reduced_second)
    return compare_coeffs<Complex>(*reduced_first, *reduced_second);
  else
    return first.get_basic()->compare(second);
}

std::set<Qubit> common_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  std::set<Qubit> common;
  for (const std::pair<const Qubit, Pauli> &p : first) {
    if (p.second == Pauli::I) continue;
    QubitPauliMap::const_iterator found = second.find(p.first);
    if (found != second.end() && found->second == p.second)
      common.insert(p.first);
  }
  return common;
}

std::set<unsigned> common_indices(
    const DensePauliMap &first, const DensePauliMap &second) {
  std::set<unsigned> common;
  unsigned min_size = std::min(first.size(), second.size());
  for (unsigned i = 0; i < min_size; ++i) {
    Pauli p = first.at(i);
    if (p != Pauli::I && p == second.at(i)) common.insert(i);
  }
  return common;
}

std::set<Qubit> own_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  std::set<Qubit> own;
  for (const std::pair<const Qubit, Pauli> &p : first) {
    if (p.second == Pauli::I) continue;
    QubitPauliMap::const_iterator found = second.find(p.first);
    if (found == second.end() || found->second == Pauli::I) own.insert(p.first);
  }
  return own;
}

std::set<unsigned> own_indices(
    const DensePauliMap &first, const DensePauliMap &second) {
  std::set<unsigned> own;
  unsigned min_size = std::min(first.size(), second.size());
  for (unsigned i = 0; i < min_size; ++i) {
    if (first.at(i) != Pauli::I && second.at(i) == Pauli::I) own.insert(i);
  }
  for (unsigned i = min_size; i < first.size(); ++i) {
    if (first.at(i) != Pauli::I) own.insert(i);
  }
  return own;
}

std::set<Qubit> conflicting_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  std::set<Qubit> conflicts;
  for (const std::pair<const Qubit, Pauli> &p : first) {
    if (p.second == Pauli::I) continue;
    QubitPauliMap::const_iterator found = second.find(p.first);
    if (found != second.end() && found->second != Pauli::I &&
        found->second != p.second)
      conflicts.insert(p.first);
  }
  return conflicts;
}

std::set<unsigned> conflicting_indices(
    const DensePauliMap &first, const DensePauliMap &second) {
  std::set<unsigned> conflicts;
  unsigned min_size = std::min(first.size(), second.size());
  for (unsigned i = 0; i < min_size; ++i) {
    Pauli p = first.at(i);
    Pauli p2 = second.at(i);
    if (p != Pauli::I && p2 != Pauli::I && p != p2) conflicts.insert(i);
  }
  return conflicts;
}

template <>
bool commuting_containers<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  return (conflicting_qubits(first, second).size() % 2) == 0;
}

template <>
bool commuting_containers<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second) {
  return (conflicting_indices(first, second).size() % 2) == 0;
}

template <>
void print_paulis<QubitPauliMap>(
    std::ostream &os, const QubitPauliMap &paulis) {
  os << "(";
  QubitPauliMap::const_iterator i = paulis.begin();
  while (i != paulis.end()) {
    switch (i->second) {
      case Pauli::I: {
        os << "I";
        break;
      }
      case Pauli::X: {
        os << "X";
        break;
      }
      case Pauli::Y: {
        os << "Y";
        break;
      }
      case Pauli::Z: {
        os << "Z";
        break;
      }
    }
    os << i->first.repr();
    ++i;
    if (i != paulis.end()) os << ", ";
  }
  os << ")";
}

template <>
void print_paulis<DensePauliMap>(
    std::ostream &os, const DensePauliMap &paulis) {
  for (const Pauli &p : paulis) {
    switch (p) {
      case Pauli::I: {
        os << "I";
        break;
      }
      case Pauli::X: {
        os << "X";
        break;
      }
      case Pauli::Y: {
        os << "Y";
        break;
      }
      case Pauli::Z: {
        os << "Z";
        break;
      }
    }
  }
}

template <>
void print_coeff<no_coeff_t>(std::ostream &, const no_coeff_t &) {}

template <>
void print_coeff<quarter_turns_t>(
    std::ostream &os, const quarter_turns_t &coeff) {
  switch (coeff % 4) {
    case 1: {
      os << "i*";
      break;
    }
    case 2: {
      os << "-";
      break;
    }
    case 3: {
      os << "-i*";
      break;
    }
    default: {
      break;
    }
  }
}

template <>
void print_coeff<Complex>(std::ostream &os, const Complex &coeff) {
  if (coeff == -1.) {
    os << "-";
  } else if (coeff != 1.) {
    os << coeff << "*";
  }
}

template <>
void print_coeff<Expr>(std::ostream &os, const Expr &coeff) {
  // Expressions distinguish integers from floating-points
  if (coeff == -1. || coeff == -1) {
    os << "-";
  } else if (coeff != 1. && coeff != 1) {
    os << "(" << coeff << ")*";
  }
}

template <>
void hash_combine_paulis<QubitPauliMap>(
    std::size_t &seed, const QubitPauliMap &paulis) {
  for (const std::pair<const Qubit, Pauli> &qp : paulis) {
    if (qp.second != Pauli::I) {
      boost::hash_combine(seed, qp.first);
      boost::hash_combine(seed, qp.second);
    }
  }
}

template <>
void hash_combine_paulis<DensePauliMap>(
    std::size_t &seed, const DensePauliMap &paulis) {
  DensePauliMap::const_reverse_iterator i = paulis.rbegin();
  while (i != paulis.rend() && *i == Pauli::I) {
    ++i;
  }
  while (i != paulis.rend()) {
    boost::hash_combine(seed, *i);
    ++i;
  }
}

template <>
void hash_combine_coeff<no_coeff_t>(std::size_t &, const no_coeff_t &) {}

template <>
void hash_combine_coeff<quarter_turns_t>(
    std::size_t &seed, const quarter_turns_t &coeff) {
  boost::hash_combine(seed, coeff % 4);
}

template <>
void hash_combine_coeff<Complex>(std::size_t &seed, const Complex &coeff) {
  boost::hash_combine(seed, coeff);
}

template <>
void hash_combine_coeff<Expr>(std::size_t &seed, const Expr &coeff) {
  boost::hash_combine(seed, coeff.get_basic()->hash());
}

template <>
unsigned n_ys<QubitPauliMap>(const QubitPauliMap &paulis) {
  unsigned n = 0;
  for (const std::pair<const Qubit, Pauli> &qp : paulis) {
    if (qp.second == Pauli::Y) ++n;
  }
  return n;
}

template <>
unsigned n_ys<DensePauliMap>(const DensePauliMap &paulis) {
  unsigned n = 0;
  for (const Pauli &p : paulis) {
    if (p == Pauli::Y) ++n;
  }
  return n;
}

const std::map<std::pair<Pauli, Pauli>, std::pair<quarter_turns_t, Pauli>> &
get_mult_matrix() {
  static const std::map<
      std::pair<Pauli, Pauli>, std::pair<quarter_turns_t, Pauli>>
      mult_matrix{
          {{Pauli::I, Pauli::I}, {0, Pauli::I}},
          {{Pauli::I, Pauli::X}, {0, Pauli::X}},
          {{Pauli::I, Pauli::Y}, {0, Pauli::Y}},
          {{Pauli::I, Pauli::Z}, {0, Pauli::Z}},
          {{Pauli::X, Pauli::I}, {0, Pauli::X}},
          {{Pauli::X, Pauli::X}, {0, Pauli::I}},
          {{Pauli::X, Pauli::Y}, {1, Pauli::Z}},
          {{Pauli::X, Pauli::Z}, {3, Pauli::Y}},
          {{Pauli::Y, Pauli::I}, {0, Pauli::Y}},
          {{Pauli::Y, Pauli::X}, {3, Pauli::Z}},
          {{Pauli::Y, Pauli::Y}, {0, Pauli::I}},
          {{Pauli::Y, Pauli::Z}, {1, Pauli::X}},
          {{Pauli::Z, Pauli::I}, {0, Pauli::Z}},
          {{Pauli::Z, Pauli::X}, {1, Pauli::Y}},
          {{Pauli::Z, Pauli::Y}, {3, Pauli::X}},
          {{Pauli::Z, Pauli::Z}, {0, Pauli::I}},
  };
  return mult_matrix;
}

template <>
std::pair<quarter_turns_t, QubitPauliMap> multiply_strings<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second) {
  quarter_turns_t total_turns = 0;
  QubitPauliMap result;
  QubitPauliMap::const_iterator fi = first.begin();
  QubitPauliMap::const_iterator si = second.begin();
  while (fi != first.end()) {
    while (si != second.end() && si->first < fi->first) {
      result.insert(*si);
      ++si;
    }
    if (si != second.end() && si->first == fi->first) {
      // Pauli in the same position, so need to multiply
      const std::pair<quarter_turns_t, Pauli> &prod =
          get_mult_matrix().at({fi->second, si->second});
      total_turns += prod.first;
      if (prod.second != Pauli::I) {
        result.insert({fi->first, prod.second});
      }
      ++si;
    } else {
      result.insert(*fi);
    }
    ++fi;
  }
  while (si != second.end()) {
    result.insert(*si);
    ++si;
  }
  return {total_turns, result};
}

template <>
std::pair<quarter_turns_t, DensePauliMap> multiply_strings<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second) {
  quarter_turns_t total_turns = 0;
  DensePauliMap result;
  DensePauliMap::const_iterator fi = first.begin();
  DensePauliMap::const_iterator si = second.begin();
  while (fi != first.end() && si != second.end()) {
    const std::pair<quarter_turns_t, Pauli> &prod =
        get_mult_matrix().at({*fi, *si});
    total_turns += prod.first;
    result.push_back(prod.second);
    ++fi;
    ++si;
  }
  while (fi != first.end()) {
    result.push_back(*fi);
    ++fi;
  }
  while (si != second.end()) {
    result.push_back(*si);
    ++si;
  }
  return {total_turns, result};
}

template <>
no_coeff_t multiply_coeffs<no_coeff_t>(const no_coeff_t &, const no_coeff_t &) {
  return {};
}

template <>
quarter_turns_t multiply_coeffs<quarter_turns_t>(
    const quarter_turns_t &first, const quarter_turns_t &second) {
  return (first + second) % 4;
}

template <>
Complex multiply_coeffs<Complex>(const Complex &first, const Complex &second) {
  return first * second;
}

template <>
Expr multiply_coeffs<Expr>(const Expr &first, const Expr &second) {
  return first * second;
}

static const CmplxSpMat const_2x2_matrix(
    Complex tl, Complex tr, Complex bl, Complex br) {
  CmplxSpMat m(2, 2);
  if (tl != czero) {
    m.insert(0, 0) = tl;
  }
  if (tr != czero) {
    m.insert(0, 1) = tr;
  }
  if (bl != czero) {
    m.insert(1, 0) = bl;
  }
  if (br != czero) {
    m.insert(1, 1) = br;
  }
  return m;
}

static const CmplxSpMat &pauli_sparse_mat(Pauli p) {
  static const CmplxSpMat I_mat = const_2x2_matrix(1, 0, 0, 1);
  static const CmplxSpMat X_mat = const_2x2_matrix(0, 1, 1, 0);
  static const CmplxSpMat Y_mat = const_2x2_matrix(0, -i_, i_, 0);
  static const CmplxSpMat Z_mat = const_2x2_matrix(1, 0, 0, -1);
  switch (p) {
    case Pauli::X:
      return X_mat;
    case Pauli::Y:
      return Y_mat;
    case Pauli::Z:
      return Z_mat;
    default:
      TKET_ASSERT(p == Pauli::I);
      return I_mat;
  }
}

template <>
CmplxSpMat to_sparse_matrix<QubitPauliMap>(const QubitPauliMap &paulis) {
  DensePauliMap matrix_paulis;
  for (const std::pair<const Qubit, Pauli> &pair : paulis)
    matrix_paulis.push_back(pair.second);
  return to_sparse_matrix<DensePauliMap>(matrix_paulis);
}
template <>
CmplxSpMat to_sparse_matrix<DensePauliMap>(const DensePauliMap &paulis) {
  CmplxSpMat result = CmplxSpMat(1, 1);
  result.insert(0, 0) = 1.;
  for (Pauli p : paulis) {
    const CmplxSpMat pauli_mat = pauli_sparse_mat(p);
    result = Eigen::KroneckerProductSparse(result, pauli_mat).eval();
  }
  return result;
}

template <>
CmplxSpMat to_sparse_matrix<QubitPauliMap>(
    const QubitPauliMap &paulis, unsigned n_qubits) {
  qubit_vector_t qubits(n_qubits);
  for (unsigned i = 0; i < n_qubits; ++i) qubits.at(i) = Qubit(i);
  return to_sparse_matrix<QubitPauliMap>(paulis, qubits);
}
template <>
CmplxSpMat to_sparse_matrix<DensePauliMap>(
    const DensePauliMap &paulis, unsigned n_qubits) {
  if (n_qubits < paulis.size())
    throw std::logic_error(
        "Called to_sparse_matrix for fewer qubits than in the Pauli string.");
  DensePauliMap matrix_paulis = paulis;
  for (unsigned i = paulis.size(); i < n_qubits; ++i)
    matrix_paulis.push_back(Pauli::I);
  return to_sparse_matrix<DensePauliMap>(matrix_paulis);
}

template <>
CmplxSpMat to_sparse_matrix<QubitPauliMap>(
    const QubitPauliMap &paulis, const qubit_vector_t &qubits) {
  DensePauliMap matrix_paulis(qubits.size(), Pauli::I);
  std::map<Qubit, unsigned> index_map;
  for (const Qubit &q : qubits)
    index_map.insert({q, (unsigned)index_map.size()});
  if (index_map.size() != qubits.size())
    throw std::logic_error(
        "Qubit list given to to_sparse_matrix contains repeats.");
  for (const std::pair<const Qubit, Pauli> &pair : paulis) {
    std::map<Qubit, unsigned>::iterator found = index_map.find(pair.first);
    if (found == index_map.end())
      throw std::logic_error(
          "Qubit list given to to_sparse_matrix doesn't contain " +
          pair.first.repr());
    matrix_paulis.at(found->second) = pair.second;
  }
  return to_sparse_matrix<DensePauliMap>(matrix_paulis);
}
template <>
CmplxSpMat to_sparse_matrix<DensePauliMap>(
    const DensePauliMap &paulis, const qubit_vector_t &qubits) {
  return to_sparse_matrix<QubitPauliMap>(
      cast_container<DensePauliMap, QubitPauliMap>(paulis), qubits);
}

}  // namespace tket
