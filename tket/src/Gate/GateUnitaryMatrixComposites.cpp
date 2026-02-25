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

#include <utility>

#include "tket/Gate/GateUnitaryMatrixImplementations.hpp"
#include "tket/Gate/GateUnitaryMatrixUtils.hpp"
#include "tket/Utils/Constants.hpp"

// This file is for gates with unitary matrices
// computed using other ("primitive") gates.
// Of course, this choice is subjective and could change.

namespace tket {
namespace internal {

Eigen::Matrix4cd GateUnitaryMatrixImplementations::CU1(double lambda) {
  return GateUnitaryMatrixUtils::get_controlled_gate_unitary(U1(lambda));
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::CU3(
    double theta, double phi, double lambda) {
  return GateUnitaryMatrixUtils::get_controlled_gate_unitary(
      U3(theta, phi, lambda));
}

Eigen::Matrix2cd GateUnitaryMatrixImplementations::U2(
    double phi, double lambda) {
  return U3(0.5, phi, lambda);
}

Eigen::Matrix2cd GateUnitaryMatrixImplementations::U3(
    double theta, double phi, double lambda) {
  return std::polar(1.0, 0.5 * PI * (lambda + phi)) * Rz(phi) * Ry(theta) *
         Rz(lambda);
}

Eigen::Matrix2cd GateUnitaryMatrixImplementations::TK1(
    double alpha, double beta, double gamma) {
  return Rz(alpha) * Rx(beta) * Rz(gamma);
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::CRx(double alpha) {
  return GateUnitaryMatrixUtils::get_controlled_gate_unitary(Rx(alpha));
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::CRy(double alpha) {
  return GateUnitaryMatrixUtils::get_controlled_gate_unitary(Ry(alpha));
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::CRz(double alpha) {
  return GateUnitaryMatrixUtils::get_controlled_gate_unitary(Rz(alpha));
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::TK2(
    double alpha, double beta, double gamma) {
  return XXPhase(alpha) * YYPhase(beta) * ZZPhase(gamma);
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::PhasedISWAP(
    double p, double t) {
  auto matr = ISWAP(t);
  const auto exp_term = std::polar(1.0, -2 * PI * p);
  matr(2, 1) *= exp_term;
  matr(1, 2) *= std::conj(exp_term);
  return matr;
}

Eigen::Matrix<std::complex<double>, 1, 1>
GateUnitaryMatrixImplementations::Phase(double alpha) {
  return Eigen::Matrix<std::complex<double>, 1, 1>(std::exp(i_ * PI * alpha));
}

Eigen::Matrix2cd GateUnitaryMatrixImplementations::PhasedX(
    double alpha, double beta) {
  const auto rz_beta_matr = Rz(beta);
  return rz_beta_matr * Rx(alpha) * rz_beta_matr.conjugate();
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::NPhasedX(
    unsigned number_of_qubits, double alpha, double beta) {
  const auto phasedx_matr = PhasedX(alpha, beta);
  Eigen::MatrixXcd U = Eigen::MatrixXcd::Identity(1, 1);
  for (unsigned i = 0; i < number_of_qubits; i++) {
    Eigen::MatrixXcd V = Eigen::kroneckerProduct(phasedx_matr, U);
    U = std::move(V);
  }
  return U;
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::TwinPhasedX(
    double alpha, double beta) {
  const Eigen::Matrix2cd phased_x = PhasedX(alpha, beta);
  return Eigen::KroneckerProduct(phased_x, phased_x);
}

Eigen::Matrix4cd GateUnitaryMatrixImplementations::PhasedXX(
    double alpha, double beta) {
  const Eigen::Matrix2cd rz = Rz(beta);
  const Eigen::Matrix2cd rzc = rz.conjugate();
  return Eigen::KroneckerProduct(rz, rz) * XXPhase(alpha) *
         Eigen::KroneckerProduct(rzc, rzc);
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnRy(
    unsigned int number_of_qubits, double alpha) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      Ry(alpha), number_of_qubits);
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnRx(
    unsigned int number_of_qubits, double alpha) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      Rx(alpha), number_of_qubits);
}
Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnRz(
    unsigned int number_of_qubits, double alpha) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      Rz(alpha), number_of_qubits);
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnX(
    unsigned int number_of_qubits) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      X(), number_of_qubits);
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnZ(
    unsigned int number_of_qubits) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      Z(), number_of_qubits);
}

Eigen::MatrixXcd GateUnitaryMatrixImplementations::CnY(
    unsigned int number_of_qubits) {
  return GateUnitaryMatrixUtils::get_multi_controlled_gate_dense_unitary(
      Y(), number_of_qubits);
}

}  // namespace internal
}  // namespace tket
