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

#include "tket/Transformations/RzPhasedXSquash.hpp"

#include <memory>

#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/PQPSquash.hpp"
#include "tket/Transformations/Transform.hpp"
#include "tket/Utils/Expression.hpp"

namespace tket {

namespace Transforms {

RzPhasedXSquasher::RzPhasedXSquasher(bool reversed)
    : PQPSquasher(OpType::Rz, OpType::Rx, false, reversed) {}

std::pair<Circuit, Gate_ptr> RzPhasedXSquasher::flush(
    std::optional<Pauli> commutation_colour) const {
  // For Quantinuum gate set, commutation_colour can only be Z or nullopt (e.g.
  // Measure)
  std::pair<Circuit, Gate_ptr> pair = PQPSquasher::flush(commutation_colour);

  // Setting smart_squash to false should guarantee that only Rzs can commute
  // through.
  TKET_ASSERT(pair.second == nullptr || pair.second->get_type() == OpType::Rz);
  Circuit replacement(1);
  replacement.add_phase(pair.first.get_phase());

  // 1. Recover the angles of RzRxRz and the leftover Rz from PQPSquasher
  std::vector<Command> coms = pair.first.get_commands();
  // pqp
  Expr rz1_angle = 0;
  Expr rx_angle = 0;
  Expr rz2_angle = 0;
  // left over Rz
  Expr rz3_angle = 0;
  if (coms.size() > 0) {
    if (coms[0].get_op_ptr()->get_type() == OpType::Rz) {
      rz1_angle = coms[0].get_op_ptr()->get_params()[0];
    } else {
      rx_angle = coms[0].get_op_ptr()->get_params()[0];
    }
  }
  if (coms.size() > 1) {
    if (coms[1].get_op_ptr()->get_type() == OpType::Rz) {
      rz2_angle = coms[1].get_op_ptr()->get_params()[0];
    } else {
      rx_angle = coms[1].get_op_ptr()->get_params()[0];
    }
  }
  if (coms.size() > 2) {
    rz2_angle = coms[2].get_op_ptr()->get_params()[0];
  }
  if (pair.second != nullptr) {
    rz3_angle = pair.second->get_params()[0];
  }

  // 2. Rebase (Rz Rx Rz | Rz) into (PhasedX Rz | Rz)
  if (commutation_colour == Pauli::Z || commutation_colour == Pauli::I) {
    if (!equiv_0(rx_angle, 4)) {
      replacement.add_op<unsigned>(
          OpType::PhasedX, {rx_angle, -rz1_angle}, {0});
    }
    rz3_angle += rz1_angle + rz2_angle;
  } else {
    if (!equiv_0(rx_angle, 4)) {
      replacement.add_op<unsigned>(
          OpType::PhasedX, {rx_angle, -rz1_angle}, {0});
    }
    if (!equiv_0(rz1_angle + rz2_angle, 4)) {
      replacement.add_op<unsigned>(OpType::Rz, {rz1_angle + rz2_angle}, {0});
    }
  }
  Gate_ptr rz3_gate =
      equiv_0(rz3_angle, 4)
          ? nullptr
          : std::make_shared<Gate>(OpType::Rz, std::vector<Expr>{rz3_angle}, 1);
  return {replacement, rz3_gate};
}

Transform squash_1qb_to_Rz_PhasedX(bool always_squash_symbols) {
  return Transform([always_squash_symbols](Circuit &circ) {
    bool reverse = false;
    bool success = decompose_ZX().apply(circ);
    auto squasher = std::make_unique<RzPhasedXSquasher>(reverse);
    return SingleQubitSquash(
               std::move(squasher), circ, reverse, always_squash_symbols)
               .squash() ||
           success;
  });
}

}  // namespace Transforms

}  // namespace tket
