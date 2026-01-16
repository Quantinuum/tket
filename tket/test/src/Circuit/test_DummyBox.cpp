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
#include <tket/OpType/OpType.hpp>

#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/DummyBox.hpp"
#include "tket/Circuit/ResourceData.hpp"
namespace tket {

SCENARIO("DummyBox") {
  GIVEN("ResourceData") {
    DummyBox dbox0(
        1, 0,
        ResourceData{
            {{OpType::H, ResourceBounds<unsigned>(3, 4)}},
            ResourceBounds<unsigned>(2, 3),
            {{OpType::H, ResourceBounds<unsigned>(3)}},
            ResourceBounds<unsigned>()});
    DummyBox dbox1(
        2, 0,
        ResourceData{
            {{OpType::H, ResourceBounds<unsigned>(3, 4)},
             {OpType::CX, ResourceBounds<unsigned>(2, 8)}},
            ResourceBounds<unsigned>(2, 3),
            {{OpType::CX, ResourceBounds<unsigned>(2, 8)}},
            ResourceBounds<unsigned>(4, 8)});
    Circuit c(2);
    c.add_op<unsigned>(OpType::CX, {0, 1});
    c.add_box(dbox0, {0});
    c.add_box(dbox1, {0, 1});
    ResourceData data = c.get_resources();
    ResourceData expected{
        {{OpType::H, ResourceBounds<unsigned>(6, 8)},
         {OpType::CX, ResourceBounds<unsigned>(3, 9)}},
        ResourceBounds<unsigned>(5, 7),
        {{OpType::H, ResourceBounds<unsigned>(3)},
         {OpType::CX, ResourceBounds<unsigned>(3, 9)}},
        ResourceBounds<unsigned>(5, 9)};
    CHECK(data == expected);
  }

  GIVEN("Phase op") {
    // https://github.com/quantinuum/tket/issues/2066
    Circuit c(1);
    c.add_op<unsigned>(OpType::Phase, 0.0, {});
    ResourceData data = c.get_resources();
    ResourceData expected{};
    CHECK(data == expected);
  }
}

}  // namespace tket
