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

#include "tket/Utils/MatrixAnalysis.hpp"

namespace tket {
namespace internal {

/** Functions to return the unitary matrix for a standard named gate.
 *  These know nothing about tket, OpTypes, etc.; only Eigen matrices.
 */
struct GateUnitaryMatrixImplementations {
  // NOTE: apart from gates taking a non-fixed number of qubits,
  // the unitaries are not sparse enough
  // to make sparse matrices worthwhile.
  // But for calculating the unitary of a circuit,
  // or applying the circuit unitary to another matrix,
  // it may be advisable to use sparse matrices.
  // (However, this would be done by basis permuting/expanding
  // the unitary matrices returned by these,
  // so dense->sparse routines would be needed).

  // Roughly follows the order in
  // https://tket.quantinuum.com/api-docs/optype.html

  // Note: the below functions do not check double parameter values,
  // the caller must do that.
  // Returns fixed size matrices where known.
  // Returns const references where the matrix is of fixed size,
  // taking no parameters, which may avoid some memory reallocation
  // and be slightly more efficient.

  // NOTE: the names of the functions should match OpType names exactly;
  // this enables macros to be used.

  static const Eigen::Matrix2cd& X();
  static const Eigen::Matrix2cd& Y();
  static const Eigen::Matrix2cd& Z();
  static const Eigen::Matrix2cd& S();
  static const Eigen::Matrix2cd& Sdg();
  static const Eigen::Matrix2cd& T();
  static const Eigen::Matrix2cd& Tdg();
  static const Eigen::Matrix2cd& V();
  static const Eigen::Matrix2cd& Vdg();
  static const Eigen::Matrix2cd& H();
  static const Eigen::Matrix2cd& SX();
  static const Eigen::Matrix2cd& SXdg();
  static const Eigen::Matrix4cd& CSX();
  static const Eigen::Matrix4cd& CSXdg();
  static const Eigen::Matrix4cd& CS();
  static const Eigen::Matrix4cd& CSdg();

  static Eigen::Matrix2cd Rx(double value);
  static Eigen::Matrix2cd Ry(double value);
  static Eigen::Matrix2cd Rz(double value);
  static Eigen::Matrix2cd U1(double value);

  static Eigen::Matrix2cd GPI(double value);
  static Eigen::Matrix2cd GPI2(double value);
  static Eigen::Matrix4cd AAMS(double theta, double phi0, double phi1);

  static Eigen::Matrix2cd U2(double phi, double lambda);

  static Eigen::Matrix2cd U3(double theta, double phi, double lambda);

  static Eigen::Matrix2cd TK1(double alpha, double beta, double gamma);

  static const Eigen::Matrix4cd& CX();
  static const Eigen::Matrix4cd& CY();
  static const Eigen::Matrix4cd& CZ();
  static const Eigen::Matrix4cd& CH();
  static const Eigen::Matrix4cd& CV();
  static const Eigen::Matrix4cd& CVdg();

  static Eigen::Matrix4cd CRx(double alpha);
  static Eigen::Matrix4cd CRy(double alpha);
  static Eigen::Matrix4cd CRz(double alpha);
  static Eigen::Matrix4cd CU1(double lambda);

  static Eigen::Matrix4cd CU3(double theta, double phi, double lambda);

  static const Matrix8cd& CCX();

  static const Eigen::Matrix4cd& SWAP();

  static const Matrix8cd& CSWAP();
  static const Matrix8cd& BRIDGE();
  static const Eigen::Matrix2cd& noop();
  static const Eigen::Matrix4cd& ECR();
  static Eigen::Matrix4cd ISWAP(double alpha);
  static Eigen::Matrix4cd PhasedISWAP(double p, double t);
  static Eigen::Matrix4cd XXPhase(double alpha);
  static Eigen::Matrix4cd YYPhase(double alpha);
  static Eigen::Matrix4cd ZZPhase(double alpha);
  static Eigen::Matrix4cd TK2(double alpha, double beta, double gamma);
  static Matrix8cd XXPhase3(double alpha);

  static Eigen::Matrix<std::complex<double>, 1, 1> Phase(double alpha);
  static Eigen::Matrix2cd PhasedX(double alpha, double beta);
  static Eigen::MatrixXcd NPhasedX(
      unsigned int number_of_qubits, double alpha, double beta);
  static Eigen::Matrix4cd TwinPhasedX(double alpha, double beta);
  static Eigen::Matrix4cd PhasedXX(double alpha, double beta);

  // Because number_of_qubits is unrestricted, it might also be useful
  // to have a sparse version.
  static Eigen::MatrixXcd CnRy(unsigned int number_of_qubits, double alpha);

  static Eigen::MatrixXcd CnRx(unsigned int number_of_qubits, double alpha);

  static Eigen::MatrixXcd CnRz(unsigned int number_of_qubits, double alpha);

  static Eigen::MatrixXcd CnX(unsigned int number_of_qubits);

  static Eigen::MatrixXcd CnZ(unsigned int number_of_qubits);

  static Eigen::MatrixXcd CnY(unsigned int number_of_qubits);

  static Eigen::MatrixXcd PhaseGadget(
      unsigned int number_of_qubits, double alpha);

  // It's a diagonal matrix, so return the entries.
  // (All nonzero, so more efficient than a sparse matrix).
  static Eigen::VectorXcd PhaseGadget_diagonal_entries(
      unsigned int number_of_qubits, double alpha);

  static const Eigen::Matrix4cd& ZZMax();

  static Eigen::Matrix4cd ESWAP(double alpha);

  static Eigen::Matrix4cd FSim(double alpha, double beta);

  static const Eigen::Matrix4cd& Sycamore();
  static const Eigen::Matrix4cd& ISWAPMax();

  // The 8x8 unitary matrix for BRIDGE has entry 1.0
  // at each position (i, column[i]), and zeros elsewhere
  static const std::array<unsigned, 8>& get_bridge_columns();

  // Similar to get_bridge_columns(): the CSWAP matrix
  // has ones exactly at (i, col[i]).
  static const std::array<unsigned, 8>& get_cswap_columns();
};

}  // namespace internal
}  // namespace tket
