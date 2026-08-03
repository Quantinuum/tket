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

#pragma once

#include "tket/Utils/Constants.hpp"
#include "tket/Utils/EigenConfig.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/Json.hpp"
#include "tket/Utils/UnitID.hpp"

#include "tket_export.h"

namespace tket {

/** Symbols for the Pauli operators (and identity) */
enum Pauli { I = 0, X, Y, Z };

NLOHMANN_JSON_SERIALIZE_ENUM(
    Pauli, {
               {Pauli::I, "I"},
               {Pauli::X, "X"},
               {Pauli::Y, "Y"},
               {Pauli::Z, "Z"},
           });

/**
 * Whenever a decomposition choice of Pauli gadgets is presented,
 * users may use either Snake (a.k.a. cascade, ladder), Tree (i.e. CX
 * balanced tree) or Star (i.e. CXs target a common qubit).
 */
enum class CXConfigType { Snake, Tree, Star, MultiQGate };

NLOHMANN_JSON_SERIALIZE_ENUM(
    CXConfigType, {{CXConfigType::Snake, "Snake"},
                   {CXConfigType::Tree, "Tree"},
                   {CXConfigType::Star, "Star"},
                   {CXConfigType::MultiQGate, "MultiQGate"}});

typedef Eigen::SparseMatrix<Complex, Eigen::ColMajor> CmplxSpMat;

/*******************************************************************************
 * SCALAR COEFFICIENTS
 ******************************************************************************/

/**
 * A trivial option for PauliTensor to represent Pauli strings up to global
 * scalar.
 *
 * Treated as +1 for casting to other coefficients and matrix evaluation.
 */
struct no_coeff_t {};

JSON_DECL(no_coeff_t)

/**
 * A fourth root of unity {1, i, -1, -i}, represented as an unsigned integer
 * giving the power of i.
 *
 * E.g. val % 4:
 * 0: +1
 * 1: +i
 * 2: -1
 * 3: -i
 *
 * These are the phase coefficients generated in the Pauli group. Whilst
 * stabilisers are restricted to {1, -1}, the imaginary numbers are required for
 * closure under multiplication. For settings where a real value is needed, use
 * PauliTensor::is_real_negative() which asserts the value is real (throws an
 * exception otherwise) and returns a bool value to distinguish.
 */
typedef unsigned quarter_turns_t;

/**
 * Other options for scalar coefficients include:
 *
 * Complex; floating-point complex number. Mostly used by the python binder to
 * present a phaseful interface with guaranteed success on converting to a
 * sparse matrix.
 *
 * Expr; a symbolic expression for a (possibly complex) coefficient. This tends
 * to be used in the context of Pauli exponentials of tracking the rotation
 * angle (along with its phase), where such synthesis functions map cP to
 * exp(i*cP*pi/2) (i.e. the coefficient is the angle of rotation in half-turns).
 */

/**
 * Returns the default coefficient value (scalar 1) for each coefficient type.
 *
 * @retval {} (no_coeff_t)
 * @retval 0 (quarter_turns_t)
 * @retval 1. (Complex)
 * @retval 1 (Expr)
 */
template <typename CoeffType>
CoeffType default_coeff() = delete;
template <>
TKET_EXPORT no_coeff_t default_coeff<no_coeff_t>();
template <>
TKET_EXPORT quarter_turns_t default_coeff<quarter_turns_t>();
template <>
TKET_EXPORT Complex default_coeff<Complex>();
template <>
TKET_EXPORT Expr default_coeff<Expr>();

/**
 * Cast a coefficient to a different type.
 *
 * Casting to no_coeff_t just drops the coefficient to focus on the string.
 * Casting from no_coeff_t treats it as the scalar 1.
 *
 * Casting to quarter_turns_t throws an exception if value is not in the range
 * {1, i, -1, -i}.
 *
 * Casting from Expr throws an exception if the coefficient is symbolic.
 *
 * @param coeff The coefficient to cast to another type.
 */
template <typename OriginalCoeff, typename NewCoeff>
NewCoeff cast_coeff(const OriginalCoeff &coeff) = delete;
template <>
TKET_EXPORT no_coeff_t cast_coeff<no_coeff_t, no_coeff_t>(const no_coeff_t &);
template <>
TKET_EXPORT quarter_turns_t cast_coeff<no_coeff_t, quarter_turns_t>(const no_coeff_t &);
template <>
TKET_EXPORT Complex cast_coeff<no_coeff_t, Complex>(const no_coeff_t &);
template <>
TKET_EXPORT Expr cast_coeff<no_coeff_t, Expr>(const no_coeff_t &);
template <>
TKET_EXPORT no_coeff_t cast_coeff<quarter_turns_t, no_coeff_t>(const quarter_turns_t &);
template <>
TKET_EXPORT quarter_turns_t cast_coeff<quarter_turns_t, quarter_turns_t>(
    const quarter_turns_t &coeff);
template <>
TKET_EXPORT Complex cast_coeff<quarter_turns_t, Complex>(const quarter_turns_t &coeff);
template <>
TKET_EXPORT Expr cast_coeff<quarter_turns_t, Expr>(const quarter_turns_t &coeff);
template <>
TKET_EXPORT no_coeff_t cast_coeff<Complex, no_coeff_t>(const Complex &);
template <>
TKET_EXPORT quarter_turns_t cast_coeff<Complex, quarter_turns_t>(const Complex &coeff);
template <>
TKET_EXPORT Complex cast_coeff<Complex, Complex>(const Complex &coeff);
template <>
TKET_EXPORT Expr cast_coeff<Complex, Expr>(const Complex &coeff);
template <>
TKET_EXPORT no_coeff_t cast_coeff<Expr, no_coeff_t>(const Expr &);
template <>
TKET_EXPORT quarter_turns_t cast_coeff<Expr, quarter_turns_t>(const Expr &coeff);
template <>
TKET_EXPORT Complex cast_coeff<Expr, Complex>(const Expr &coeff);
template <>
TKET_EXPORT Expr cast_coeff<Expr, Expr>(const Expr &coeff);

/**
 * Compare two coefficients of the same type with respect to an ordering.
 *
 * All values of no_coeff_t are equal.
 * Values of quarter_turns_t are compared modulo 4.
 * Values of Complex are compared lexicographically by the real part and then
 * imaginary.
 * Non-symbolic values of Expr are compared as Complex, symbolic
 * values are compared by the canonical form of SymEngine expressions.
 *
 * @param first A coefficient.
 * @param second Another coefficient of the same type.
 * @retval -1 first < second
 * @retval 0 first == second
 * @retval 1 first > second
 */
template <typename CoeffType>
int compare_coeffs(const CoeffType &first, const CoeffType &second) = delete;
template <>
TKET_EXPORT int compare_coeffs<no_coeff_t>(const no_coeff_t &, const no_coeff_t &);
template <>
TKET_EXPORT int compare_coeffs<quarter_turns_t>(
    const quarter_turns_t &first, const quarter_turns_t &second);
template <>
TKET_EXPORT int compare_coeffs<Complex>(const Complex &first, const Complex &second);
template <>
TKET_EXPORT int compare_coeffs<Expr>(const Expr &first, const Expr &second);

template <typename CoeffType>
void print_coeff(std::ostream &os, const CoeffType &coeff) = delete;

/**
 * Generates the coefficient prefix for PauliTensor::to_str()
 */
template <>
TKET_EXPORT void print_coeff<no_coeff_t>(std::ostream &, const no_coeff_t &);
template <>
TKET_EXPORT void print_coeff<quarter_turns_t>(
    std::ostream &os, const quarter_turns_t &coeff);
template <>
TKET_EXPORT void print_coeff<Complex>(std::ostream &os, const Complex &coeff);
template <>
TKET_EXPORT void print_coeff<Expr>(std::ostream &os, const Expr &coeff);

/**
 * Hash a coefficient, combining it with an existing hash of another structure.
 */
template <typename CoeffType>
void hash_combine_coeff(std::size_t &seed, const CoeffType &coeff) = delete;
template <>
TKET_EXPORT void hash_combine_coeff<no_coeff_t>(std::size_t &, const no_coeff_t &);
template <>
TKET_EXPORT void hash_combine_coeff<quarter_turns_t>(
    std::size_t &seed, const quarter_turns_t &coeff);
template <>
TKET_EXPORT void hash_combine_coeff<Complex>(std::size_t &seed, const Complex &coeff);
template <>
TKET_EXPORT void hash_combine_coeff<Expr>(std::size_t &seed, const Expr &coeff);

/**
 * Multiply together two coefficients of the same type.
 *
 * Multiplication of no_coeff_t is trivial.
 * Multiplication of quarter_turns_t adds the unsigned values (e^{i*a*pi/2} *
 * e^{i*b*pi/2} = e^{i*(a+b)*pi/2}). Multiplication of Complex/Expr is standard
 * multiplication.
 */
template <typename CoeffType>
CoeffType multiply_coeffs(const CoeffType &first, const CoeffType &second) =
    delete;
template <>
TKET_EXPORT no_coeff_t multiply_coeffs<no_coeff_t>(const no_coeff_t &, const no_coeff_t &);
template <>
TKET_EXPORT quarter_turns_t multiply_coeffs<quarter_turns_t>(
    const quarter_turns_t &first, const quarter_turns_t &second);
template <>
TKET_EXPORT Complex multiply_coeffs<Complex>(const Complex &first, const Complex &second);
template <>
TKET_EXPORT Expr multiply_coeffs<Expr>(const Expr &first, const Expr &second);

/*******************************************************************************
 * PAULI CONTAINERS
 ******************************************************************************/

/**
 * A sparse, Qubit-indexed Pauli container.
 *
 * A QubitPauliMap is generally treated the same as if all Pauli::I entries were
 * removed.
 */
typedef std::map<Qubit, Pauli> QubitPauliMap;

/**
 * A dense, unsigned-indexed Pauli container.
 *
 * A DensePauliMap is generally treated the same regardless of any Pauli::Is
 * padded at the end. Each qubit index is treated as the corresponding Qubit id
 * from the default register.
 *
 * In future work, it may be interesting to consider
 * a symplectic representation (representing each Pauli by a pair of bits
 * representing X and Z) for both speed and memory efficiency.
 */
typedef std::vector<Pauli> DensePauliMap;

/**
 * Cast between two different Pauli container types.
 *
 * Casting into a type with restricted indices (e.g. DensePauliMap expecting
 * default register qubits) may throw an exception.
 *
 * @param cont The Pauli container to convert to another type.
 */
template <typename OriginalContainer, typename NewContainer>
NewContainer cast_container(const OriginalContainer &cont) = delete;
template <>
TKET_EXPORT QubitPauliMap cast_container<QubitPauliMap, QubitPauliMap>(
    const QubitPauliMap &cont);
template <>
TKET_EXPORT QubitPauliMap cast_container<DensePauliMap, QubitPauliMap>(
    const DensePauliMap &cont);
template <>
TKET_EXPORT DensePauliMap cast_container<QubitPauliMap, DensePauliMap>(
    const QubitPauliMap &cont);
template <>
TKET_EXPORT DensePauliMap cast_container<DensePauliMap, DensePauliMap>(
    const DensePauliMap &cont);

/**
 * Compare two Pauli containers of the same type for ordering.
 *
 * Individual Paulis are ordered alphabetically as I < X < Y < Z.
 * Strings are ordered lexicographically, IZ < ZI (Zq[1] < Zq[0]).
 *
 * @param first A Pauli container
 * @param second Another Pauli container of the same type.
 * @retval -1 first < second
 * @retval 0 first == second
 * @retval 1 first > second
 */
template <typename PauliContainer>
int compare_containers(
    const PauliContainer &first, const PauliContainer &second) = delete;
template <>
TKET_EXPORT int compare_containers<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second);
template <>
TKET_EXPORT int compare_containers<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Find the set of Qubits on which \p first and \p second have the same
 * non-trivial Pauli (X, Y, Z).
 */
std::set<Qubit> common_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second);

/**
 * Find the set of qubits (as unsigned integer indices) on which \p first and \p
 * second have the same non-trivial Pauli (X, Y, Z).
 */
std::set<unsigned> common_indices(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Find the set of Qubits on which \p first has a non-trivial Pauli (X, Y, Z)
 * but \p second either doesn't contain or maps to I.
 */
std::set<Qubit> own_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second);

/**
 * Find the set of qubits (as unsigned integer indices) on which \p first has a
 * non-trivial Pauli (X, Y, Z) but \p second either doesn't contain (>= size) or
 * maps to I.
 */
std::set<unsigned> own_indices(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Find the set of Qubits on which \p first and \p second have distinct
 * non-trivial Paulis (X, Y, Z).
 */
std::set<Qubit> conflicting_qubits(
    const QubitPauliMap &first, const QubitPauliMap &second);

/**
 * Find the set of qubits (as unsigned integer indices) on which \p first and \p
 * second have distinct non-trivial Paulis (X, Y, Z).
 */
std::set<unsigned> conflicting_indices(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Return whether two Pauli containers commute as Pauli strings (there are an
 * even number of qubits on which they have distinct non-trivial Paulis).
 */
template <typename PauliContainer>
bool commuting_containers(
    const PauliContainer &first, const PauliContainer &second) = delete;
template <>
TKET_EXPORT bool commuting_containers<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second);
template <>
TKET_EXPORT bool commuting_containers<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Generates the readable Pauli string portion of PauliTensor::to_str().
 *
 * Dense strings are printed as e.g. XIYZ.
 * Sparse strings are printed as e.g. (Xq[0], Yq[2], Zq[3]).
 */
template <typename PauliContainer>
void print_paulis(std::ostream &os, const PauliContainer &paulis) = delete;
template <>
TKET_EXPORT void print_paulis<QubitPauliMap>(std::ostream &os, const QubitPauliMap &paulis);
template <>
TKET_EXPORT void print_paulis<DensePauliMap>(std::ostream &os, const DensePauliMap &paulis);

/**
 * Hash a Pauli container, combining it with an existing hash of another
 * structure.
 */
template <typename PauliContainer>
void hash_combine_paulis(std::size_t &seed, const PauliContainer &paulis) =
    delete;
template <>
TKET_EXPORT void hash_combine_paulis<QubitPauliMap>(
    std::size_t &seed, const QubitPauliMap &paulis);
template <>
TKET_EXPORT void hash_combine_paulis<DensePauliMap>(
    std::size_t &seed, const DensePauliMap &paulis);

/**
 * Return the number of Pauli::Ys in the container. Used for
 * PauliTensor::transpose().
 */
template <typename PauliContainer>
unsigned n_ys(const PauliContainer &paulis) = delete;
template <>
unsigned n_ys<QubitPauliMap>(const QubitPauliMap &paulis);
template <>
unsigned n_ys<DensePauliMap>(const DensePauliMap &paulis);

/**
 * Returns a const reference to a lookup table for multiplying individual
 * Paulis.
 *
 * Maps {p0, p1} -> {k, p2} where p0*p1 = e^{i*k*pi/2}*p2, e.g. {Pauli::X,
 * Pauli::Y} -> {1, Pauli::Z} (X*Y = iZ).
 */
const std::map<std::pair<Pauli, Pauli>, std::pair<quarter_turns_t, Pauli>> &
get_mult_matrix();

/**
 * Multiplies two Pauli containers component-wise, returning both the resulting
 * phase and string.
 *
 * Maps {P0, P1} -> {k, P2} where P0*P1 = e^{i*k*pi/2}*P2, e.g. {XIY, YZY} ->
 * {1, ZZI} (XIY*YZY = iZZI).
 */
template <typename PauliContainer>
std::pair<quarter_turns_t, PauliContainer> multiply_strings(
    const PauliContainer &first, const PauliContainer &second) = delete;
template <>
TKET_EXPORT std::pair<quarter_turns_t, QubitPauliMap> multiply_strings<QubitPauliMap>(
    const QubitPauliMap &first, const QubitPauliMap &second);
template <>
TKET_EXPORT std::pair<quarter_turns_t, DensePauliMap> multiply_strings<DensePauliMap>(
    const DensePauliMap &first, const DensePauliMap &second);

/**
 * Evaluates a Pauli container to a sparse matrix describing the tensor product
 * of each Pauli in the string.
 *
 * The matrix gives the operator in ILO-BE format, e.g. (Zq[0], Xq[1]):
 * 0  1  0  0
 * 1  0  0  0
 * 0  0  0 -1
 * 0  0 -1  0
 *
 * For sparse Pauli containers, just those qubits present in the container will
 * be treated as part of the Pauli string (e.g. (Zq[1], Iq[2]) is treated as ZI
 * since q[0] is ignored but q[2] is present in the string), so it is often
 * preferred to use the other variants which explicitly provide the expected
 * qubits.
 */
template <typename PauliContainer>
CmplxSpMat to_sparse_matrix(const PauliContainer &paulis) = delete;
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<QubitPauliMap>(const QubitPauliMap &paulis);
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<DensePauliMap>(const DensePauliMap &paulis);

/**
 * Evaluates a Pauli container to a sparse matrix describing the tensor product
 * of each Pauli in the string.
 *
 * Qubits are restricted to the default register, from q[0] to q[ \p n_qubits -
 * 1], then presents the operator in ILO-BE format (if a sparse container
 * contains Qubits outside of this range or not in the default register, an
 * exception is thrown). E.g. (Zq[0]), 2:
 * 1  0  0  0
 * 0 -1  0  0
 * 0  0  1  0
 * 0  0  0 -1
 */
template <typename PauliContainer>
CmplxSpMat to_sparse_matrix(const PauliContainer &paulis, unsigned n_qubits) =
    delete;
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<QubitPauliMap>(
    const QubitPauliMap &paulis, unsigned n_qubits);
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<DensePauliMap>(
    const DensePauliMap &paulis, unsigned n_qubits);

/**
 * Evaluates a Pauli container to a sparse matrix describing the tensor product
 * of each Pauli in the string.
 *
 * \p qubits dictates the order of the qubits in the tensor product, with the
 * operator returned in Big Endian format. E.g. (Zq[0], Xq[1]), [q[1], q[0]]:
 * 0  0  1  0
 * 0  0  0 -1
 * 1  0  0  0
 * 0 -1  0  0
 */
template <typename PauliContainer>
CmplxSpMat to_sparse_matrix(
    const PauliContainer &paulis, const qubit_vector_t &qubits) = delete;
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<QubitPauliMap>(
    const QubitPauliMap &paulis, const qubit_vector_t &qubits);
template <>
TKET_EXPORT CmplxSpMat to_sparse_matrix<DensePauliMap>(
    const DensePauliMap &paulis, const qubit_vector_t &qubits);

/*******************************************************************************
 * PauliTensor TEMPLATE CLASS
 ******************************************************************************/

/**
 * PauliTensor<PauliContainer, CoeffType>
 *
 * A unified type for tensor products of Pauli operators, possibly with some
 * global scalar coefficient. It is parameterised in two ways:
 * - PauliContainer describes the data structure used to map qubits to Paulis.
 * This may be sparse or dense, and indexed by arbitrary Qubits or unsigneds
 * (referring to indices in the default register).
 * - CoeffType describes the kind of coefficient stored, ranging from no data to
 * restricted values, to symbolic expressions.
 *
 * Each implementation should be interoperable by casting. Some methods may only
 * be available for certain specialisations.
 */
template <typename PauliContainer, typename CoeffType>
class TKET_EXPORT PauliTensor {
  static_assert(
      std::is_same<PauliContainer, QubitPauliMap>::value ||
          std::is_same<PauliContainer, DensePauliMap>::value,
      "PauliTensor must be either dense or qubit-indexed.");
  static_assert(
      std::is_same<CoeffType, no_coeff_t>::value ||
          std::is_same<CoeffType, quarter_turns_t>::value ||
          std::is_same<CoeffType, Complex>::value ||
          std::is_same<CoeffType, Expr>::value,
      "PauliTensor must either support no coefficient, quarter turns, or "
      "arbitrary complex (floating point or symbolic) coefficients.");

 public:
  PauliContainer string;
  CoeffType coeff;

  /**
   * Default constructor, giving the empty Pauli string with unit scalar.
   */
  PauliTensor() : string(), coeff(default_coeff<CoeffType>()) {}

  /**
   * Constructor directly instantiating the Pauli string and coefficient.
   */
  PauliTensor(
      const PauliContainer &_string, const CoeffType &_coeff = default_coeff<CoeffType>())
      : string(_string), coeff(_coeff) {}

  /**
   * Convenience constructor for an individual Pauli in a sparse representation.
   */
  template <typename PC = PauliContainer>
  PauliTensor(
      const Qubit &q, Pauli p, const CoeffType &_coeff = default_coeff<CoeffType>(),
      typename std::enable_if<std::is_same<PC, QubitPauliMap>::value>::type * =
          0)
      : string({{q, p}}), coeff(_coeff) {}

  /**
   * Convenience constructor to immediately cast a dense Pauli string on the
   * default register to a sparse representation.
   */
  template <typename PC = PauliContainer>
  PauliTensor(
      const DensePauliMap &_string, const CoeffType &_coeff = default_coeff<CoeffType>(),
      typename std::enable_if<std::is_same<PC, QubitPauliMap>::value>::type * =
          0)
      : string(cast_container<DensePauliMap, QubitPauliMap>(_string)),
        coeff(_coeff) {}

  /**
   * Constructor for sparse representations which zips together an ordered list
   * of Qubits and Paulis.
   */
  template <typename PC = PauliContainer>
  PauliTensor(
      const std::list<Qubit> &qubits, const std::list<Pauli> &paulis,
      const CoeffType &_coeff = default_coeff<CoeffType>(),
      typename std::enable_if<std::is_same<PC, QubitPauliMap>::value>::type * =
          0)
      : string(), coeff(_coeff) {
    if (qubits.size() != paulis.size()) {
      throw std::logic_error(
          "Mismatch of Qubits and Paulis upon QubitPauliString "
          "construction");
    }
    std::list<Pauli>::const_iterator p_it = paulis.begin();
    for (const Qubit &qb : qubits) {
      Pauli p = *p_it;
      if (string.find(qb) != string.end()) {
        throw std::logic_error(
            "Non-unique Qubit inserted into QubitPauliString map");
      }
      string[qb] = p;
      ++p_it;
    }
  }

  /**
   * Casting operator between different specialisations of PauliTensor. Casts
   * the Pauli container and coefficient separately.
   */
  template <typename CastContainer, typename CastCoeffType>
  operator PauliTensor<CastContainer, CastCoeffType>() const {
    return PauliTensor<CastContainer, CastCoeffType>(
        cast_container<PauliContainer, CastContainer>(string),
        cast_coeff<CoeffType, CastCoeffType>(coeff));
  }

  /**
   * Compares two PauliTensors of the same type in lexicographical order by the
   * Paulis first, then coefficients.
   *
   * Ordering rules for each of the Pauli containers or coefficient types may
   * vary.
   *
   * @param other Another PauliTensor of the same type.
   * @retval -1 this < other
   * @retval 0 this == other
   * @retval 1 this > other
   */
  int compare(const PauliTensor<PauliContainer, CoeffType> &other) const {
    int cont_comp =
        compare_containers<PauliContainer>(this->string, other.string);
    if (cont_comp != 0) return cont_comp;
    return compare_coeffs<CoeffType>(this->coeff, other.coeff);
  }
  bool operator==(const PauliTensor<PauliContainer, CoeffType> &other) const {
    return (compare(other) == 0);
  }
  bool operator!=(const PauliTensor<PauliContainer, CoeffType> &other) const {
    return !(*this == other);
  }
  bool operator<(const PauliTensor<PauliContainer, CoeffType> &other) const {
    return compare(other) < 0;
  }

  /**
   * Checks for equivalence of PauliTensors, explicitly taking the coefficient
   * modulo \p n
   *
   * This is useful for treatment of Pauli exponentials, where the coefficient
   * becomes the rotation angle under exponentiation and so is unchanged under
   * some modulus.
   */
  template <typename CT = CoeffType>
  typename std::enable_if<std::is_same<CT, Expr>::value, bool>::type equiv_mod(
      const PauliTensor<PauliContainer, CoeffType> &other, unsigned n) const {
    int cont_comp =
        compare_containers<PauliContainer>(this->string, other.string);
    return (cont_comp == 0) && equiv_expr(this->coeff, other.coeff, n);
  }

  /**
   * Compress a sparse PauliTensor by removing identity terms.
   */
  template <typename PC = PauliContainer>
  typename std::enable_if<std::is_same<PC, QubitPauliMap>::value>::type
  compress() {
    QubitPauliMap::iterator it = string.begin();
    while (it != string.end()) {
      if (it->second == Pauli::I)
        it = string.erase(it);
      else
        ++it;
    }
  }

  /**
   * Checks commutation of two PauliTensors by evaluating how many Qubits have
   * anti-commuting Paulis in the string.
   */
  template <typename OtherCoeffType>
  bool commutes_with(
      const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return commuting_containers<PauliContainer>(string, other.string);
  }

  /**
   * Find the set of Qubits on which this and \p other have the same
   * non-trivial Pauli (X, Y, Z).
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, QubitPauliMap>::value, std::set<Qubit>>::type
  common_qubits(
      const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::common_qubits(string, other.string);
  }

  /**
   * Find the set of Qubits on which this has a non-trivial Pauli (X, Y, Z)
   * but \p other either doesn't contain or maps to I.
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, QubitPauliMap>::value, std::set<Qubit>>::type
  own_qubits(const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::own_qubits(string, other.string);
  }

  /**
   * Find the set of Qubits on which this and \p other have distinct
   * non-trivial Paulis (X, Y, Z).
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, QubitPauliMap>::value, std::set<Qubit>>::type
  conflicting_qubits(
      const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::conflicting_qubits(string, other.string);
  }

  /**
   * Find the set of qubits (as unsigned integer indices) on which this and
   * \p other have the same non-trivial Pauli (X, Y, Z).
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, DensePauliMap>::value, std::set<unsigned>>::type
  common_indices(
      const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::common_indices(string, other.string);
  }

  /**
   * Find the set of qubits (as unsigned integer indices) on which this has a
   * non-trivial Pauli (X, Y, Z) but \p other either doesn't contain (>= size)
   * or maps to I.
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, DensePauliMap>::value, std::set<unsigned>>::type
  own_indices(const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::own_indices(string, other.string);
  }

  /**
   * Find the set of qubits (as unsigned integer indices) on which this and
   * \p other have distinct non-trivial Paulis (X, Y, Z).
   */
  template <typename OtherCoeffType, typename PC = PauliContainer>
  typename std::enable_if<
      std::is_same<PC, DensePauliMap>::value, std::set<unsigned>>::type
  conflicting_indices(
      const PauliTensor<PauliContainer, OtherCoeffType> &other) const {
    return tket::conflicting_indices(string, other.string);
  }

  /**
   * Gets the Pauli at the given index within the string.
   *
   * For sparse representations, returns Pauli::I if index is not present.
   */
  template <typename PC = PauliContainer>
  typename std::enable_if<std::is_same<PC, QubitPauliMap>::value, Pauli>::type
  get(const Qubit &qb) const {
    QubitPauliMap::const_iterator i = string.find(qb);
    if (i == string.end())
      return Pauli::I;
    else
      return i->second;
  }
  template <typename PC = PauliContainer>
  typename std::enable_if<std::is_same<PC, DensePauliMap>::value, Pauli>::type
  get(unsigned qb) const {
    if (qb >= string.size())
      return Pauli::I;
    else
      return string.at(qb);
  }

  /**
   * Sets the Pauli at the given index within the string.
   */
  template <typename PC = PauliContainer>
  typename std::enable_if<std::is_same<PC, QubitPauliMap>::value>::type set(
      const Qubit &qb, Pauli p) {
    QubitPauliMap::iterator i = string.find(qb);
    if (i == string.end()) {
      if (p != Pauli::I) string.insert({qb, p});
    } else {
      if (p == Pauli::I)
        string.erase(i);
      else
        i->second = p;
    }
  }
  template <typename PC = PauliContainer>
  typename std::enable_if<std::is_same<PC, DensePauliMap>::value>::type set(
      unsigned qb, Pauli p) {
    if (qb >= string.size()) string.resize(qb + 1, Pauli::I);
    string.at(qb) = p;
  }

  /**
   * Asserts coefficient is real, and returns whether it is negative.
   *
   * quarter_turns_t is used as the coefficient to restrict to the Pauli group.
   * This is most commonly used for stabiliser methods, in which case valid
   * coefficients must be +-1. It is common in such representations for these to
   * be distinguished just by a single phase bit which is true if negative,
   * false if positive. This method immediately gives that phase bit.
   *
   * @retval true if coeff % 4 == 2 (coefficient is -1)
   * @retval false if coeff % 4 == 0 (coefficient is +1)
   */
  template <typename CT = CoeffType>
  typename std::enable_if<std::is_same<CT, quarter_turns_t>::value, bool>::type
  is_real_negative() const {
    switch (coeff % 4) {
      case 0: {
        return false;
      }
      case 2: {
        return true;
      }
      default: {
        throw std::logic_error(
            "is_real_negative() called on a PauliTensor with imaginary phase");
      }
    }
  }

  /**
   * A human-readable form of the PauliTensor, incorporating the coefficient and
   * Pauli string. Format may depend on the type specialisations.
   */
  std::string to_str() const {
    std::stringstream ss;
    print_coeff<CoeffType>(ss, coeff);
    print_paulis<PauliContainer>(ss, string);
    return ss.str();
  }

  /**
   * Hash the PauliTensor.
   */
  std::size_t hash_value() const {
    std::size_t seed = 0;
    hash_combine_paulis<PauliContainer>(seed, string);
    hash_combine_coeff<CoeffType>(seed, coeff);
    return seed;
  }

  /**
   * Update this to the transpose by negating the coefficient if the string
   * contains an odd number of Pauli::Ys.
   */
  void transpose() {
    if (n_ys<PauliContainer>(string) % 2 == 1)
      coeff = multiply_coeffs<CoeffType>(
          coeff, cast_coeff<quarter_turns_t, CoeffType>(2));
  }

  /**
   * Qubit-wise multiplication of two PauliTensors of the same type.
   */
  PauliTensor<PauliContainer, CoeffType> operator*(
      const PauliTensor<PauliContainer, CoeffType> &other) const {
    std::pair<quarter_turns_t, PauliContainer> prod =
        multiply_strings<PauliContainer>(this->string, other.string);
    CoeffType new_coeff = multiply_coeffs<CoeffType>(this->coeff, other.coeff);
    new_coeff = multiply_coeffs<CoeffType>(
        new_coeff, cast_coeff<quarter_turns_t, CoeffType>(prod.first));
    return PauliTensor<PauliContainer, CoeffType>(prod.second, new_coeff);
  }

  /**
   * Returns the set of free symbols in a symbolic PauliTensor.
   */
  template <typename CT = CoeffType>
  typename std::enable_if<std::is_same<CT, Expr>::value, SymSet>::type
  free_symbols() const {
    return expr_free_symbols(coeff);
  }
  /**
   * Replaces given symbols with values in a symbolic PauliTensor.
   */
  template <typename CT = CoeffType>
  typename std::enable_if<
      std::is_same<CT, Expr>::value,
      PauliTensor<PauliContainer, CoeffType>>::type
  symbol_substitution(const SymEngine::map_basic_basic &sub_map) const {
    return PauliTensor<PauliContainer, CoeffType>(string, coeff.subs(sub_map));
  }

  /**
   * Returns the size of the underlying Pauli string.
   *
   * This is taken directly from the container, so may include some qubits
   * mapped to Pauli::I and vary around calling PauliTensor::compress().
   */
  unsigned size() const { return string.size(); }

  /**
   * Evaluates a PauliTensor to a sparse matrix describing the tensor product
   * of each Pauli in the string. Throws an exception if the coefficient is
   * symbolic.
   *
   * The matrix gives the operator in ILO-BE format, e.g. (Zq[0], Xq[1]):
   * 0  1  0  0
   * 1  0  0  0
   * 0  0  0 -1
   * 0  0 -1  0
   *
   * For sparse Pauli containers, just those qubits present in the container
   * will be treated as part of the Pauli string (e.g. (Zq[1], Iq[2]) is treated
   * as ZI since q[0] is ignored but q[2] is present in the string), so it is
   * often preferred to use the other variants which explicitly provide the
   * expected qubits.
   */
  CmplxSpMat to_sparse_matrix() const {
    return cast_coeff<CoeffType, Complex>(coeff) *
           tket::to_sparse_matrix<PauliContainer>(string);
  }

  /**
   * Evaluates a PauliTensor to a sparse matrix describing the tensor product
   * of each Pauli in the string. Throws an exception if the coefficient is
   * symbolic.
   *
   * Qubits are restricted to the default register, from q[0] to q[ \p n_qubits
   * - 1], then presents the operator in ILO-BE format (if a sparse container
   * contains Qubits outside of this range or not in the default register, an
   * exception is thrown). E.g. (Zq[0]), 2:
   * 1  0  0  0
   * 0 -1  0  0
   * 0  0  1  0
   * 0  0  0 -1
   */
  CmplxSpMat to_sparse_matrix(const unsigned n_qubits) const {
    return cast_coeff<CoeffType, Complex>(coeff) *
           tket::to_sparse_matrix<PauliContainer>(string, n_qubits);
  }

  /**
   * Evaluates a PauliTensor to a sparse matrix describing the tensor product
   * of each Pauli in the string. Throws an exception if the coefficient is
   * symbolic.
   *
   * \p qubits dictates the order of the qubits in the tensor product, with the
   * operator returned in Big Endian format. E.g. (Zq[0], Xq[1]), [q[1], q[0]]:
   * 0  0  1  0
   * 0  0  0 -1
   * 1  0  0  0
   * 0 -1  0  0
   */
  CmplxSpMat to_sparse_matrix(const qubit_vector_t &qubits) const {
    return cast_coeff<CoeffType, Complex>(coeff) *
           tket::to_sparse_matrix<PauliContainer>(string, qubits);
  }

  /**
   * Applies the PauliTensor to a given statevector by matrix multiplication.
   *
   * Determines the number of qubits from the size of the statevector, and
   * assumes default register qubits in ILO-BE format.
   */
  Eigen::VectorXcd dot_state(const Eigen::VectorXcd &state) const {
    // allowing room for big states
    unsigned long long n = state.size();
    if (!(n && (!(n & (n - 1)))))
      throw std::logic_error("Statevector size is not a power of two.");
    unsigned n_qubits = 0;
    while (n) {
      n >>= 1;
      ++n_qubits;
    }
    --n_qubits;
    return (to_sparse_matrix(n_qubits) * state);
  }
  /**
   * Applies the PauliTensor to a given statevector by matrix multiplication.
   *
   * \p qubits dictates the order of Qubits in the state, assuming a Big Endian
   * format. An exception is thrown if the size of the state does not match up
   * with the number of Qubits given.
   */
  Eigen::VectorXcd dot_state(
      const Eigen::VectorXcd &state, const qubit_vector_t &qubits) const {
    if (state.size() != 1 << qubits.size())
      throw std::logic_error(
          "Size of statevector does not match number of qubits passed to "
          "dot_state");
    return (to_sparse_matrix(qubits) * state);
  }

  /**
   * Determines the expectation value of a given statevector with respect to the
   * PauliTensor by matrix multiplication.
   *
   * Determines the number of qubits from the size of the statevector, and
   * assumes default register qubits in ILO-BE format.
   */
  Complex state_expectation(const Eigen::VectorXcd &state) const {
    return state.dot(dot_state(state));
  }
  /**
   * Determines the expectation value of a given statevector with respect to the
   * PauliTensor by matrix multiplication.
   *
   * \p qubits dictates the order of Qubits in the state, assuming a Big Endian
   * format. An exception is thrown if the size of the state does not match up
   * with the number of Qubits given.
   */
  Complex state_expectation(
      const Eigen::VectorXcd &state, const qubit_vector_t &qubits) const {
    return state.dot(dot_state(state, qubits));
  }
};

template <typename PauliContainer, typename CoeffType>
void to_json(
    nlohmann::json &j, const PauliTensor<PauliContainer, CoeffType> &tensor) {
  j["string"] = tensor.string;
  j["coeff"] = tensor.coeff;
}

template <typename PauliContainer, typename CoeffType>
void from_json(
    const nlohmann::json &j, PauliTensor<PauliContainer, CoeffType> &tensor) {
  tensor = PauliTensor<PauliContainer, CoeffType>(
      j.at("string").get<PauliContainer>(), j.at("coeff").get<CoeffType>());
}

/*******************************************************************************
 * PauliTensor SPECIALISATION TYPEDEFS
 ******************************************************************************/

typedef PauliTensor<QubitPauliMap, no_coeff_t> SpPauliString;
typedef PauliTensor<DensePauliMap, no_coeff_t> PauliString;
typedef PauliTensor<QubitPauliMap, quarter_turns_t> SpPauliStabiliser;
typedef PauliTensor<DensePauliMap, quarter_turns_t> PauliStabiliser;
typedef PauliTensor<QubitPauliMap, Complex> SpCxPauliTensor;
typedef PauliTensor<DensePauliMap, Complex> CxPauliTensor;
typedef PauliTensor<QubitPauliMap, Expr> SpSymPauliTensor;
typedef PauliTensor<DensePauliMap, Expr> SymPauliTensor;

typedef std::vector<PauliStabiliser> PauliStabiliserVec;

}  // namespace tket
