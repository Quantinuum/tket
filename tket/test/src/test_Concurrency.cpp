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
#include <functional>
#include <nlohmann/json.hpp>
#include <thread>

#include "tket/Circuit/Circuit.hpp"
#include "tket/Transformations/OptimisationPass.hpp"

namespace tket {
namespace test_Concurrency {

// No test assertions here, as they are not thread-safe:
// https://catch2-temp.readthedocs.io/en/latest/limitations.html#thread-safe-assertions

SCENARIO("Concurrent transforms") {
#ifdef NDEBUG
  GIVEN("clifford_simp") {
    // https://github.com/quantinuum/tket/issues/1953
    const std::string json_str =
        R"({"bits": [], "commands": [{"args": [["q", [0]], ["q", [1]]], "op": {"type": "CX"}}, {"args": [["q", [0]], ["q", [1]]], "op": {"type": "CX"}}], "created_qubits": [], "discarded_qubits": [], "implicit_permutation": [[["q", [0]], ["q", [0]]], [["q", [1]], ["q", [1]]]], "phase": "0.0", "qubits": [["q", [0]], ["q", [1]]]})";
    Circuit circ = nlohmann::json::parse(json_str);
    std::function<void(Circuit)> func = [](Circuit circ) {
      Transforms::clifford_simp().apply(circ);
    };
    std::thread thread1(func, circ);
    std::thread thread2(func, circ);
    thread1.join();
    thread2.join();
  }
#endif
}
SCENARIO("Concurrent circuit operations") {
#ifdef NDEBUG
  GIVEN("Circuit construction and serialization") {
    // https://github.com/quantinuum/tket/issues/2009
    std::function<void(void)> func = []() {
      Circuit circ(2);
      circ.add_op<unsigned>(OpType::H, {0});
      nlohmann::json j = circ;
    };
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < 5; i++) {
      std::thread thread(func);
      threads.push_back(std::move(thread));
    }
    for (auto &thread : threads) {
      thread.join();
    }
  }
#endif
}

}  // namespace test_Concurrency
}  // namespace tket
