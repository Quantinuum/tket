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

#include "MeasurementSetup.hpp"
#include "tket/Diagonalisation/PauliPartition.hpp"

#include "tket_export.h"

namespace tket {

/**
 * A tool for reducing the number of measurements required for
 * variational quantum algorithms by partitioning Pauli strings into
 * mutually commuting sets.
 * See: https://arxiv.org/abs/1907.07859, https://arxiv.org/abs/1908.11857,
 * https://arxiv.org/abs/1907.13623, https://arxiv.org/abs/1908.08067,
 * https://arxiv.org/abs/1908.06942, https://arxiv.org/abs/1907.03358
 */
TKET_EXPORT MeasurementSetup measurement_reduction(
    const std::list<SpPauliString>& strings, PauliPartitionStrat strat,
    GraphColourMethod method = GraphColourMethod::Lazy,
    CXConfigType cx_config = CXConfigType::Snake);

}  // namespace tket
