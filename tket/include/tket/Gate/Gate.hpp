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

#include "tket/Ops/Op.hpp"

#include "tket_export.h"

namespace tket {

class SubstitutionFailure : public std::logic_error {
 public:
  explicit SubstitutionFailure(const std::string &message)
      : std::logic_error(message) {}
};

class TKET_EXPORT Gate : public Op {
 public:
  // return hermitian conjugate
  Op_ptr dagger() const override;

  // return transpose
  Op_ptr transpose() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  std::string get_name(bool latex = false) const override;

  std::string get_command_str(const unit_vector_t &args) const override;

  /**
   * Return the gate decomposition in terms of Rz(a)Rx(b)Rz(c).
   *
   * This decomposition is in matrix-multiplication order, i.e. the reverse of
   * circuit order.
   *
   * @return a, b, c and a global phase
   */
  std::vector<Expr> get_tk1_angles() const;
  std::vector<Expr> get_params() const override;
  std::vector<Expr> get_params_reduced() const override;
  SymSet free_symbols() const override;

  unsigned n_qubits() const override;

  std::optional<Pauli> commuting_basis(port_t i) const override;

  op_signature_t get_signature() const override;

  nlohmann::json serialize() const override;

  static Op_ptr deserialize(const nlohmann::json &j);

  std::optional<double> is_identity() const override;
  bool is_clifford() const override;
  bool has_symmetry(unsigned port1, unsigned port2) const override;

  Eigen::MatrixXcd get_unitary() const override;

  ~Gate() override {}

  /**
   * Equality check between two Gate instances
   */
  bool is_equal(const Op &other) const override;

  Gate(
      OpType type, const std::vector<Expr> &params = {}, unsigned n_qubits = 0);

  Gate();

 private:
  // vector of symbolic params
  const std::vector<Expr> params_;
  unsigned n_qubits_; /**< Number of qubits, when not deducible from type */
};

}  // namespace tket
