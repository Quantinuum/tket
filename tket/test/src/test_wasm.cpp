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

#include <Eigen/Core>
#include <catch2/catch_test_macros.hpp>

#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Ops/ClassicalOps.hpp"

namespace tket {
namespace test_Boxes {

SCENARIO("generating circ with wasm") {
  std::string wasm_file = "string/with/path/to/wasm/file";

  std::string wasm_func = "stringNameOfWASMFunc";

  std::vector<unsigned> uv = {2, 1};

  std::vector<unsigned> uv_2 = {1};

  std::vector<unsigned> uv_3 = {};

  GIVEN("wasmop creation") {
    WASMOp wop = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop.get_n_i32() == 3);
    REQUIRE(wop.get_func_name() == wasm_func);
    REQUIRE(wop.get_wasm_file_uid() == wasm_file);
  }
  GIVEN("wasmop to json") {
    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(4, 1, uv, uv_2, wasm_func, wasm_file);

    nlohmann::json j = wop_ptr->serialize();

    auto wopj = WASMOp::deserialize(j);

    REQUIRE(wop_ptr->is_equal(*wopj));
  }
  GIVEN("add wasmop to circ") {
    Circuit u(6, 6);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    std::vector<Bit> bits = {Bit(0)};

    std::vector<UnitID> new_args;

    new_args.push_back(Bit(0));

    WasmState wuid = WasmState(0);

    new_args.push_back(wuid);

    u.add_op<UnitID>(wop_ptr, new_args);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(6, 1, uv, uv, wasm_func, wasm_file);

    u.add_op<UnitID>(
        wop_ptr_2,
        {Bit(0),          //
         Bit(1),          //
         Bit(2),          //
         Bit(3),          //
         Bit(4),          //
         Bit(5),          //
         WasmState(0)});  // needs 6 bits + WasmState
  }
  GIVEN("compare wasmop") {
    WASMOp wop = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    REQUIRE(!wop.is_equal(wop_2));
  }
  GIVEN("compare wasmop II") {
    WASMOp wop = WASMOp(6, 1, uv, uv, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(!wop.is_equal(wop_2));
  }
  GIVEN("compare wasmop III") {
    WASMOp wop = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_func);

    REQUIRE(!wop.is_equal(wop_2));
  }
  GIVEN("compare wasmop IV") {
    WASMOp wop = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(4, 1, uv, uv_2, wasm_file, wasm_file);

    REQUIRE(!wop.is_equal(wop_2));
  }
  GIVEN("compare wasmop V") {
    WASMOp wop = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop.is_equal(wop_2));
  }
  GIVEN("compare wasmop 6") {
    WASMOp wop = WASMOp(4, 0, uv, uv_2, wasm_func, wasm_file);

    WASMOp wop_2 = WASMOp(4, 0, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop.is_equal(wop_2));
    REQUIRE(wop.get_ww_n() == 0);
    REQUIRE(wop_2.get_ww_n() == 0);
  }
  GIVEN("wasmop is_extern") {
    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop_ptr->is_extern());
  }
  GIVEN("wasmop add circuit") {
    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop_ptr->is_extern());
  }
  GIVEN("wasmop add circuit II") {
    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(4, 1, uv, uv_2, wasm_func, wasm_file);

    REQUIRE(wop_ptr->is_extern());
  }
  GIVEN("wasmop add circuit III") {
    Circuit u(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(wop_ptr_2, {Bit(0), WasmState(0)});

    u.assert_valid();

    REQUIRE(u.depth() == 2);
  }
  GIVEN("wasmop add circuit IV") {
    Circuit u(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    REQUIRE_THROWS(u.add_op<unsigned>(wop_ptr, {0}));
    REQUIRE_THROWS(u.add_op<unsigned>(wop_ptr_2, {0, 0, 0}));

    u.assert_valid();
    REQUIRE(u.depth() == 0);
  }
  GIVEN("wasmop add circuit V") {
    Circuit u(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 3, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(
        wop_ptr_2, {Bit(0), WasmState(0), WasmState(1), WasmState(2)});

    u.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.w_inputs().size() == 3);
    REQUIRE(u.w_outputs().size() == 3);
  }
  GIVEN("circuit get wasm uid") {
    Circuit u(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 3, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(
        wop_ptr_2, {Bit(0), WasmState(0), WasmState(1), WasmState(2)});

    u.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.get_wasm_file_uid() == wasm_file);
  }
  GIVEN("circuit get wasm uid 2") {
    Circuit u(0, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 3, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(
        wop_ptr_2, {Bit(0), WasmState(0), WasmState(1), WasmState(2)});

    u.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.get_wasm_file_uid() == wasm_file);

    CircBox circbox(u);
    Circuit major_circ(0, 1);
    unit_vector_t new_args;
    new_args.push_back(Bit(0));
    new_args.push_back(WasmState(0));
    new_args.push_back(WasmState(1));
    new_args.push_back(WasmState(2));
    major_circ.add_op(std::make_shared<CircBox>(circbox), new_args);

    REQUIRE(major_circ.depth() == 1);
    REQUIRE(major_circ.get_wasm_file_uid() == wasm_file);
  }
  GIVEN("circuit get wasm uid 3") {
    Circuit u(0, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 3, uv_2, uv_3, wasm_func, wasm_file);

    u.assert_valid();
    REQUIRE(u.depth() == 0);
    REQUIRE(u.get_wasm_file_uid() == std::nullopt);

    CircBox circbox(u);
    Circuit major_circ(0, 1);
    major_circ.add_box(circbox, {0});

    REQUIRE(major_circ.depth() == 1);
    REQUIRE(major_circ.get_wasm_file_uid() == std::nullopt);
  }
  GIVEN("circuit with wasm append") {
    Circuit u(1, 1);
    Circuit u2(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 3, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(
        wop_ptr_2, {Bit(0), WasmState(0), WasmState(1), WasmState(2)});

    u.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.w_inputs().size() == 3);
    REQUIRE(u.w_outputs().size() == 3);
    REQUIRE(u2.depth() == 0);
    REQUIRE(u2.w_inputs().size() == 0);
    REQUIRE(u2.w_outputs().size() == 0);
    u2.append(u);
    REQUIRE(u2.depth() == 2);
    REQUIRE(u2.w_inputs().size() == 3);
    REQUIRE(u2.w_outputs().size() == 3);
  }
  GIVEN("circuit with wasm append 2 ") {
    Circuit u(1, 1);
    Circuit u2(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(wop_ptr_2, {Bit(0), WasmState(0)});

    u.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.w_inputs().size() == 1);
    REQUIRE(u.w_outputs().size() == 1);
    REQUIRE(u2.depth() == 0);
    REQUIRE(u2.w_inputs().size() == 0);
    REQUIRE(u2.w_outputs().size() == 0);
    u2.append(u);
    REQUIRE(u2.depth() == 2);
    REQUIRE(u2.w_inputs().size() == 1);
    REQUIRE(u2.w_outputs().size() == 1);
  }
  GIVEN("circuit with wasm append 3 ") {
    Circuit u(1, 1);
    Circuit u2(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), WasmState(0)});

    u.assert_valid();
    REQUIRE(u.depth() == 1);
    REQUIRE(u.w_inputs().size() == 1);
    REQUIRE(u.w_outputs().size() == 1);
    REQUIRE(u2.depth() == 0);
    REQUIRE(u2.w_inputs().size() == 0);
    REQUIRE(u2.w_outputs().size() == 0);
    u2.append(u);
    REQUIRE(u2.depth() == 1);
    REQUIRE(u2.w_inputs().size() == 1);
    REQUIRE(u2.w_outputs().size() == 1);
  }
  GIVEN("circuit with wasm append 4") {
    Circuit u(1, 1);
    Circuit u2(1, 1);

    const std::shared_ptr<WASMOp> wop_ptr =
        std::make_shared<WASMOp>(2, 1, uv_2, uv_2, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_2 =
        std::make_shared<WASMOp>(1, 1, uv_2, uv_3, wasm_func, wasm_file);

    const std::shared_ptr<WASMOp> wop_ptr_3 =
        std::make_shared<WASMOp>(1, 3, uv_3, uv_2, wasm_func, wasm_file);

    u.add_op<UnitID>(wop_ptr, {Bit(0), Bit(0), WasmState(0)});
    u.add_op<UnitID>(wop_ptr_2, {Bit(0), WasmState(0)});
    u2.add_op<UnitID>(
        wop_ptr_3, {Bit(0), WasmState(0), WasmState(1), WasmState(2)});

    u.assert_valid();
    u2.assert_valid();
    REQUIRE(u.depth() == 2);
    REQUIRE(u.w_inputs().size() == 1);
    REQUIRE(u.w_outputs().size() == 1);
    REQUIRE(u2.depth() == 1);
    REQUIRE(u2.w_inputs().size() == 3);
    REQUIRE(u2.w_outputs().size() == 3);
    u2.append(u);
    REQUIRE(u2.depth() == 3);
    REQUIRE(u2.w_inputs().size() == 3);
    REQUIRE(u2.w_outputs().size() == 3);
  }
}

SCENARIO("test wasm uid") {
  GIVEN("wasm uid") { WasmState wuid = WasmState(); }
  GIVEN("wasm uid - compare") {
    WasmState wuid = WasmState();
    WasmState wuid_2 = WasmState();
    REQUIRE(wuid == wuid_2);
  }
  GIVEN("wasm uid - compare 2") {
    WasmState wuid = WasmState(1);
    WasmState wuid_2 = WasmState(3);
    WasmState wuid_3 = WasmState(3);

    REQUIRE(wuid != wuid_2);
    REQUIRE(wuid_3 == wuid_2);
    REQUIRE(wuid != wuid_3);
  }
  GIVEN("wasm uid - create bit from wasm") {
    WasmState wuid = WasmState();
    REQUIRE_THROWS(Bit(wuid));
  }
}

}  // namespace test_Boxes
}  // namespace tket
