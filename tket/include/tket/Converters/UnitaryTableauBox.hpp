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

#include "tket/Circuit/Boxes.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"

#include "tket_export.h"

namespace tket {

/**
 * Encapsulation of a unitary stabilizer tableau as a Box to use within a
 * circuit. Implementation and decomposition currently use the methods from
 * Aaronson & Gottesman, "Improved Simulation of Stabilizer Circuits",
 * https://arxiv.org/pdf/quant-ph/0406196.pdf which yield high gate counts in
 * practice.
 */
class TKET_EXPORT UnitaryTableauBox : public Box {
 public:
  /**
   * Constructor from a given tableau object.
   */
  explicit UnitaryTableauBox(const UnitaryTableau& tab);
  /**
   * Constructor from the matrix components of a tableau.
   * Qubits given default names.
   * @param xx The X component of the X rows
   * @param xz The Z component of the X rows
   * @param xph The phases of the X rows
   * @param zx The X component of the Z rows
   * @param zz The Z component of the Z rows
   * @param zph The phases of the Z rows
   */
  explicit UnitaryTableauBox(
      const MatrixXb& xx, const MatrixXb& xz, const VectorXb& xph,
      const MatrixXb& zx, const MatrixXb& zz, const VectorXb& zph);

  // Retrieve the underlying tableau object for inspection.
  const UnitaryTableau& get_tableau() const;

  /**
   * Op overloads
   */
  Op_ptr dagger() const override;
  Op_ptr transpose() const override;
  bool is_clifford() const override { return true; }
  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  SymSet free_symbols() const override;
  bool is_equal(const Op& op_other) const override;
  static Op_ptr from_json(const nlohmann::json& j);
  static nlohmann::json to_json(const Op_ptr& op);
  op_signature_t get_signature() const override;

 protected:
  void generate_circuit() const override;

 private:
  UnitaryTableau tab_;
};

}  // namespace tket
