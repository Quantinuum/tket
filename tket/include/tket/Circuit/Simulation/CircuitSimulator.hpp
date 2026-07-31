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

#include "tket_export.h"

namespace tket {
class Circuit;
typedef Eigen::VectorXcd StateVector;

namespace tket_sim {

/** Calculate the unitary matrix of the circuit applied to the state
 *  |00...0>, using ILO-BE convention.
 *  (Note: if any OpType::Barrier occurs,
 *  it is simply ignored - the same as a noop).
 *  @param circ The circuit to simulate.
 *  @param abs_epsilon Used to decide if an entry of a sparse matrix is
 *              too small, i.e. if std::abs(z) <= abs_epsilon then we treat
 *              z as zero exactly in any intermediate sparse matrices
 *              (although not in the final dense result).
 *  @param max_number_of_qubits Throw an exception if this limit is exceeded.
 */
TKET_EXPORT StateVector get_statevector(
    const Circuit& circ, double abs_epsilon = EPS,
    unsigned max_number_of_qubits = 11);

/** Calculates the unitary matrix of the circuit,
 *  using ILO-BE convention.
 *  @param circ The circuit to simulate.
 *  @param abs_epsilon Used to decide if an entry of a sparse matrix is
 *              too small, i.e. if std::abs(z) <= abs_epsilon then we treat
 *              z as zero exactly in any intermediate sparse matrices
 *              (although not in the final dense result).
 *  @param max_number_of_qubits Throw an exception if the circuit has too
 *              many qubits, to prevent users accidentally passing in
 *              huge circuits.
 */
TKET_EXPORT Eigen::MatrixXcd get_unitary(
    const Circuit& circ, double abs_epsilon = EPS,
    unsigned max_number_of_qubits = 11);

/** Let U be the unitary matrix which represents the given circuit
 *  using ILO-BE convention. Replace the given M with UM.
 *  Note that U is not calculated explicitly and sparse matrices are used,
 *  so it is quicker than calling calc_unitary if M is, e.g., a column vector.
 *  @param circ The circuit to simulate.
 *  @param matr The matrix M which will be premultiplied by the unitary matrix.
 *  @param abs_epsilon Used to decide if an entry of a sparse matrix is
 *              too small, i.e. if std::abs(z) <= abs_epsilon then we treat
 *              z as zero exactly in any intermediate sparse matrices
 *              (although not in the final dense result).
 * @param max_number_of_qubits Throw an exception if this limit is exceeded.
 */
TKET_EXPORT void apply_unitary(
    const Circuit& circ, Eigen::MatrixXcd& matr, double abs_epsilon = EPS,
    unsigned max_number_of_qubits = 11);

}  // namespace tket_sim
}  // namespace tket
