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

#include <catch2/catch_test_macros.hpp>

#include "tket/Circuit/Circuit.hpp"

namespace tket {
namespace test_ClassicalOps {

SCENARIO("Circuit iteration") {
  GIVEN("Successive conditionals on the same unit") {
    // https://github.com/quantinuum/tket/issues/1673
    Circuit circ(2, 2);
    circ.add_conditional_gate<unsigned>(OpType::X, {}, {1}, {0, 1}, 3);
    circ.add_conditional_gate<unsigned>(OpType::Y, {}, {1}, {0}, 1);
    circ.add_measure(0, 0);
    unsigned d = circ.depth();
    REQUIRE(d == 3);
  }
}

}  // namespace test_ClassicalOps
}  // namespace tket
