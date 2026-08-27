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
#include <catch2/matchers/catch_matchers_string.hpp>
#include <map>
#include <set>
#include <sstream>

#include "../testutil.hpp"
#include "GatesData.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/Gate/Gate.hpp"
#include "tket/Gate/GateUnitaryMatrix.hpp"
#include "tket/Gate/GateUnitaryMatrixError.hpp"
#include "tket/Gate/GateUnitaryMatrixImplementations.hpp"
#include "tket/Gate/Rotation.hpp"
#include "tket/Utils/MatrixAnalysis.hpp"

using Catch::Matchers::ContainsSubstring;

namespace tket {
namespace test_GateUnitaryMatrix {

// For easily cycling through parameter values.
// Might seem a bit naughty to use doubles, but actually perfectly fine.
//
// KEY: current parameter value
// VALUE: the next value to jump to; the last one will jump back to the start.
//
typedef std::map<double, double> ValuesMap;

static ValuesMap get_values_map(const std::set<double>& values) {
  REQUIRE(values.size() > 1);
  ValuesMap values_map;

  auto citer = values.cbegin();
  const double first_double = *citer;
  double previous_double = first_double;
  ++citer;
  for (; citer != values.cend(); ++citer) {
    values_map[previous_double] = *citer;
    previous_double = *citer;
  }
  values_map[previous_double] = first_double;
  return values_map;
}

static Eigen::MatrixXcd get_tket_sim_unitary(
    OpType op_type, const std::vector<Expr>& current_values_expr,
    const std::vector<unsigned>& qubits) {
  Circuit circ(qubits.size());
  circ.add_op<unsigned>(op_type, current_values_expr, qubits);
  const auto tket_sim_unitary = tket_sim::get_unitary(circ);

  REQUIRE(tket_sim_unitary.cols() == tket_sim_unitary.rows());
  return tket_sim_unitary;
}

static void write_error_information(
    const std::string& name, const std::vector<double>& current_values,
    std::size_t number_of_qubits, const Eigen::MatrixXcd& unitary,
    std::stringstream& ss) {
  ss << "\nOp type " << name << " acting on " << number_of_qubits
     << " qubits, with " << current_values.size() << " parameters: [";
  for (double x : current_values) {
    ss << x << ", ";
  }
  ss << "], we calculated U=\n" << unitary << "\n";
}

// Compare the unitary matrix with that calculated by tket-sim,
// returning false and writing an error message if they differ.
//
// Of course, since tket-sim now uses the single gate code
// rather than 3rd party code, this test is not so powerful as before,
// although still worth doing.
static bool calculate_and_compare_unitaries(
    OpType op_type, const std::string& name,
    const std::vector<double>& current_values,
    const std::vector<Expr>& current_values_expr,
    const std::vector<unsigned>& qubits, const Eigen::MatrixXcd& unitary,
    std::stringstream& ss, bool no_previous_errors) {
  // Let's actually calculate using tket-sim and compare.
  const auto tket_sim_unitary =
      get_tket_sim_unitary(op_type, current_values_expr, qubits);
  if (tket_sim_unitary.isApprox(unitary)) {
    return true;
  }
  if (no_previous_errors) {
    // Don't print the error information twice.
    write_error_information(name, current_values, qubits.size(), unitary, ss);
  }
  // Errors specific to tket-sim result
  ss << "\nU is not close to tket-sim calculated V=\n" << tket_sim_unitary;
  return false;
}

// Returns false if the calculated matrix U is not almost unitary.
static bool check_is_unitary(
    const std::string& name, const std::vector<double>& current_values,
    std::size_t number_of_qubits, const Eigen::MatrixXcd& unitary,
    std::stringstream& ss) {
  if (is_unitary(unitary)) {
    return true;
  }
  write_error_information(name, current_values, number_of_qubits, unitary, ss);

  ss << "\nU is not almost unitary! UU* is not approximately I\n";
  return false;
}

static bool compare_dense_unitary_with_triplets(
    const Gate& gate, const Eigen::MatrixXcd& unitary, const std::string& name,
    std::stringstream& ss) {
  const auto triplets = GateUnitaryMatrix::get_unitary_triplets(gate, 0.0);
  const auto recalc_sparse_matrix =
      get_sparse_matrix(triplets, unitary.rows(), unitary.cols());
  const Eigen::MatrixXcd recalc_dense_matrix = recalc_sparse_matrix;
  if (!unitary.isApprox(recalc_dense_matrix)) {
    ss << "\nGate " << name
       << " gives different dense matrix and sparse "
          "matrix (get triplets) results";
    return false;
  }
  return true;
}

// If the test failed, writes to the stream and returns false.
static bool test_op_with_parameters(
    OpType op_type, const std::string& name,
    const std::vector<double>& current_values,
    const std::vector<Expr>& current_values_expr,
    const std::vector<unsigned>& qubits, std::stringstream& ss) {
  const Gate gate(op_type, current_values_expr, qubits.size());
  const auto unitary = GateUnitaryMatrix::get_unitary(gate);
  bool success = compare_dense_unitary_with_triplets(gate, unitary, name, ss);

  // Gate functions should just be wrappers around
  // GateUnitaryMatrix functions, so we demand equality.
  CHECK(matrices_are_equal(unitary, gate.get_unitary()));
  REQUIRE(unitary.cols() >= 2);
  REQUIRE(unitary.cols() == (1L << qubits.size()));

  success &= check_is_unitary(name, current_values, qubits.size(), unitary, ss);

  success &= calculate_and_compare_unitaries(
      op_type, name, current_values, current_values_expr, qubits, unitary, ss,
      success);
  return success;
}

// Move the parameter values onto the next vector,
// returning false if it's back to the start.
static bool update_parameter_values(
    const ValuesMap& values_map, std::vector<double>& current_values) {
  if (current_values.empty()) {
    return false;
  }
  for (std::size_t nn = current_values.size() - 1;;) {
    double& value = current_values[nn];
    value = values_map.at(value);
    if (value != values_map.crbegin()->first) {
      // No "carry digit"
      return true;
    }
    // We have a carry digit, will be updated.
    if (nn == 0) {
      // It's looped back to the start
      return false;
    }
    --nn;
  }
}

static void test_op(
    OpType op_type, std::size_t number_of_parameters,
    std::size_t number_of_qubits, const ValuesMap& values_map) {
  const OpDesc desc(op_type);
  const auto name = desc.name();
  INFO(
      "Testing op " << name << " with " << number_of_qubits << " qubits, "
                    << number_of_parameters << " parameters");
  REQUIRE(number_of_qubits > 0);
  REQUIRE(number_of_qubits <= 4);
  REQUIRE(number_of_parameters <= 3);
  REQUIRE(!values_map.empty());
  const double first_value = values_map.begin()->first;

  // The test values for a single parameter are {a0, a1, a2, ..., an}.
  // We will systematically test all m-tuples of values:
  // (a0,..,a0,a0), (a0,...,a0,a1), (a0,...,a0,a2), ... (an,...,an).
  std::vector<double> current_values(number_of_parameters);
  fill(current_values.begin(), current_values.end(), first_value);

  std::vector<Expr> current_values_expr(number_of_parameters);
  int remaining_messages = 3;

  std::vector<unsigned> qubits(number_of_qubits);
  std::iota(qubits.begin(), qubits.end(), 0);

  std::stringstream ss;

  try {
    for (;;) {
      // Convert the doubles to expressions.
      for (std::size_t nn = 0; nn < current_values.size(); ++nn) {
        current_values_expr[nn] = Expr(current_values[nn]);
      }
      if (!test_op_with_parameters(
              op_type, name, current_values, current_values_expr, qubits, ss)) {
        --remaining_messages;
        if (remaining_messages <= 0) {
          break;
        }
      }
      if (!update_parameter_values(values_map, current_values)) {
        break;
      }
    }
  } catch (const std::exception& e) {
    ss << "\nGate " << name << " gave exception: " << e.what();
  }
  const auto message = ss.str();
  if (message.empty()) {
    // no error
    return;
  }
  INFO("Error: " << message);
  CHECK(false);
}

SCENARIO("Single fixed size gates") {
  const auto& gates_data = internal::GatesData::get();

  // Many parameter values are reduced (mod 2) or (mod 4),
  // so we want to test values inside and a bit outside this range.
  const std::set<double> param_values{0.1, 1.1, -3.3, 5.0, -6.1};
  const auto values_map = get_values_map(param_values);

  for (const auto& outer_entry : gates_data.input_data) {
    const auto& number_of_qubits = outer_entry.first;
    for (const auto& inner_entry : outer_entry.second) {
      const auto& number_of_parameters = inner_entry.first;
      const auto& ops = inner_entry.second;

      for (OpType type : ops) {
        test_op(type, number_of_parameters, number_of_qubits, values_map);
      }
    }
  }
}

SCENARIO("Test 1 qubit gates against TK1 angles") {
  const auto& gates_data = internal::GatesData::get();
  const auto& one_qubit_data = gates_data.input_data.at(1);
  std::vector<double> current_values;
  std::vector<Expr> current_values_expr;
  std::vector<Expr> tk1_angles;

  for (const auto& entry : one_qubit_data) {
    const auto& number_of_parameters = entry.first;
    current_values.resize(number_of_parameters);
    current_values_expr.resize(number_of_parameters);
    for (unsigned nn = 0; nn < number_of_parameters; ++nn) {
      const double value = 0.123456789 + nn * 0.2222233333;
      current_values[nn] = value;
      current_values_expr[nn] = Expr(value);
    }
    const auto& ops = entry.second;
    for (auto op_type : ops) {
      INFO("for op " << OpDesc(op_type).name());
      const auto simulation_unitary =
          GateUnitaryMatrix::get_unitary(op_type, 1, current_values);

      const Gate gate(op_type, current_values_expr, 1);
      // These should be implemented just as wrappers around
      // the GateUnitaryMatrix function, and so we demand EXACT numerical
      // matches.
      CHECK(matrices_are_equal(
          simulation_unitary, GateUnitaryMatrix::get_unitary(gate)));
      CHECK(matrices_are_equal(simulation_unitary, gate.get_unitary()));

      tk1_angles = gate.get_tk1_angles();
      REQUIRE(tk1_angles.size() == 4);
      const Eigen::Matrix2cd tk1_angles_unitary =
          get_matrix_from_tk1_angles(tk1_angles);
      CHECK(simulation_unitary.isApprox(tk1_angles_unitary));
    }
  }
}

SCENARIO("Invalid numbers of arguments cause exceptions") {
  const unsigned max_number_of_qubits = 5;
  const unsigned max_number_of_parameters = 5;
  const auto& gates_data = internal::GatesData::get();

  for (std::vector<double> parameters;
       parameters.size() <= max_number_of_parameters;
       parameters.push_back(0.0)) {
    for (unsigned number_of_qubits = 0;
         number_of_qubits <= max_number_of_qubits; ++number_of_qubits) {
      for (const auto& outer_entry : gates_data.input_data) {
        const auto correct_number_of_qubits = outer_entry.first;
        for (const auto& inner_entry : outer_entry.second) {
          const auto correct_number_of_parameters = inner_entry.first;
          for (auto type : inner_entry.second) {
            bool expect_throw =
                parameters.size() != correct_number_of_parameters;

            const auto citer =
                gates_data.min_number_of_qubits_for_variable_qubit_type.find(
                    type);

            if (citer == gates_data.min_number_of_qubits_for_variable_qubit_type
                             .cend()) {
              expect_throw |= number_of_qubits == 0;
              expect_throw |= number_of_qubits != correct_number_of_qubits;
            } else {
              expect_throw |= number_of_qubits < citer->second;
            }
            const auto name = OpDesc(type).name();
            INFO(
                "op " << name << " expects " << correct_number_of_qubits
                      << " qubits, but given " << number_of_qubits
                      << "; expects " << correct_number_of_parameters
                      << " parameters, but given " << parameters.size()
                      << "; should throw? " << expect_throw);
            bool did_throw = false;
            try {
              GateUnitaryMatrix::get_unitary(
                  type, number_of_qubits, parameters);
            } catch (const GateUnitaryMatrixError& e) {
              CHECK(e.cause == GateUnitaryMatrixError::Cause::INPUT_ERROR);
              if (type != OpType::CnX && type != OpType::CnRy &&
                  type != OpType::CnY && type != OpType::CnZ) {
                CHECK_THAT(e.what(), ContainsSubstring(name));
              }
              CHECK_THAT(
                  e.what(),
                  ContainsSubstring(std::to_string(number_of_qubits)));
              CHECK_THAT(
                  e.what(),
                  ContainsSubstring(std::to_string(parameters.size())));
              did_throw = true;
            }
            CHECK(expect_throw == did_throw);
          }
        }
      }
    }
  }
}

SCENARIO("Non-unitary op types cause not implemented exceptions") {
  const std::vector<OpType> non_unitary_types{
      OpType::Input,
      OpType::Output,
      OpType::ClInput,
      OpType::ClOutput,
      OpType::WASMInput,
      OpType::WASMOutput,
      OpType::Barrier,
      OpType::Label,
      OpType::Branch,
      OpType::Goto,
      OpType::Stop,
      OpType::ClassicalTransform,
      OpType::SetBits,
      OpType::CopyBits,
      OpType::RangePredicate,
      OpType::ExplicitPredicate,
      OpType::ExplicitModifier,
      OpType::MultiBit,
      OpType::Measure,
      OpType::Collapse,
      OpType::Reset,
      OpType::Conditional,
      OpType::RNGInput,
      OpType::RNGOutput,
      OpType::RNGSeed,
      OpType::RNGBound,
      OpType::RNGIndex,
      OpType::RNGNum,
      OpType::JobShotNum};
  const std::vector<double> no_parameters;
  for (auto type : non_unitary_types) {
    const auto name = OpDesc(type).name();
    INFO("op type " << name);
    try {
      GateUnitaryMatrix::get_unitary(type, 1, no_parameters);
      CHECK(false);
    } catch (const BadOpType& e) {
      CHECK_THAT(e.what(), ContainsSubstring(name));
    }
  }
}

SCENARIO("Phase Gadget test") {
  const double alpha = 0.111222333;
  const std::vector<double> parameters{alpha};
  Eigen::VectorXcd expected_vector(1);
  // The initial seed entry; we will build the vector iteratively.
  expected_vector(0) = std::polar(1.0, -0.5 * PI * alpha);

  // I'm unsure from the Eigen docs if resize
  // preserves the lower elements, the way std::vector resize does,
  // so something like this is necessary.
  // (Otherwise we could be fancy and use just one vector, filling
  // from the end rather than the beginning).
  Eigen::VectorXcd previous_entries_copy;

  for (unsigned qubits = 0; qubits <= 5; ++qubits) {
    if (qubits != 0) {
      const unsigned old_size = expected_vector.rows();
      previous_entries_copy = expected_vector;
      expected_vector.resize(2 * old_size);

      for (unsigned nn = 0; nn < old_size; ++nn) {
        expected_vector[2 * nn] = previous_entries_copy[nn];
        expected_vector[2 * nn + 1] = std::conj(previous_entries_copy[nn]);
      }
    }
    const auto calc_entries = internal::GateUnitaryMatrixImplementations::
        PhaseGadget_diagonal_entries(qubits, alpha);
    INFO("for " << qubits << " qubits, alpha=" << alpha);
    REQUIRE(calc_entries.isApprox(expected_vector));

    const auto calc_unitary =
        GateUnitaryMatrix::get_unitary(OpType::PhaseGadget, qubits, parameters);
    // The diagonal unitary matrix must be a wrapper around the vector function
    // in any sensible implementation, so should have exact equality.
    REQUIRE(matrices_are_equal(
        Eigen::MatrixXcd(calc_entries.asDiagonal()), calc_unitary));
  }
}

SCENARIO("Dagger pairs of gates without parameters") {
  // Test the known (U, U dagger) pairs.
  const std::vector<std::pair<OpType, OpType>> dagger_pairs{
      {OpType::X, OpType::X},           {OpType::Y, OpType::Y},
      {OpType::Z, OpType::Z},           {OpType::S, OpType::Sdg},
      {OpType::SX, OpType::SXdg},       {OpType::H, OpType::H},
      {OpType::T, OpType::Tdg},         {OpType::V, OpType::Vdg},
      {OpType::BRIDGE, OpType::BRIDGE},
  };

  // No parameters
  const std::vector<Expr> current_values_expr;

  for (const auto& entry : dagger_pairs) {
    const Gate gate1(entry.first, current_values_expr, 1);
    const auto unitary1 = GateUnitaryMatrix::get_unitary(gate1);
    CHECK(matrices_are_equal(unitary1, gate1.get_unitary()));

    const Gate gate2(entry.second, current_values_expr, 1);
    const auto unitary2 = GateUnitaryMatrix::get_unitary(gate2);
    CHECK(matrices_are_equal(unitary2, gate2.get_unitary()));

    const Eigen::MatrixXcd product = unitary1 * unitary2;
    if (!product.isApprox(
            Eigen::MatrixXcd::Identity(product.cols(), product.cols()))) {
      INFO(
          "Multiplying unitaries for ops "
          << OpDesc(entry.first).name() << ", " << OpDesc(entry.second).name()
          << " gave\n"
          << product << "\nwhich was NOT almost the identity!");
      CHECK(false);
    }
  }
}

namespace {
// For trivial equivalence tests.
struct EquivalenceData {
  OpType other_type;
  std::vector<double> params;

  EquivalenceData& other(OpType op) {
    other_type = op;
    return *this;
  }
  EquivalenceData& param(double p) {
    params.push_back(p);
    return *this;
  }
};
}  // namespace

SCENARIO("Trivial unitary matrix identities") {
  GIVEN("Fixed ops, special cases of ops with parameters") {
    // KEY: number of qubits, for an op taking no parameters
    // VALUE: (this op) -> (data about another op taking parameters,
    //                      which should be equivalent)
    std::map<unsigned, std::map<OpType, EquivalenceData>> data;

    data[1][OpType::S].other(OpType::U1).param(0.5);
    data[1][OpType::Sdg].other(OpType::U1).param(-0.5);
    data[1][OpType::T].other(OpType::U1).param(0.25);
    data[1][OpType::Tdg].other(OpType::U1).param(-0.25);
    data[1][OpType::V].other(OpType::Rx).param(0.5);
    data[1][OpType::Vdg].other(OpType::Rx).param(-0.5);
    data[2][OpType::ISWAPMax].other(OpType::ISWAP).param(1.0);
    data[2][OpType::Sycamore].other(OpType::FSim).param(0.5).param(1.0 / 6);
    data[2][OpType::ZZMax].other(OpType::ZZPhase).param(0.5);

    const std::vector<double> no_params;

    for (const auto& outer_entry : data) {
      const unsigned number_of_qubits = outer_entry.first;
      for (const auto& inner_entry : outer_entry.second) {
        const auto op_with_no_params = inner_entry.first;
        const auto other_op = inner_entry.second.other_type;
        const auto& params = inner_entry.second.params;

        CHECK(
            GateUnitaryMatrix::get_unitary(
                op_with_no_params, number_of_qubits, no_params)
                .isApprox(
                    GateUnitaryMatrix::get_unitary(
                        other_op, number_of_qubits, params)));
      }
    }
  }
  GIVEN("Single parameter ops given by exponentials") {
    // KEY: number of qubits
    // VALUE: list of ops M taking a single parameter t,
    //          such that M(t) = exp(tA) for some fixed A.
    std::map<unsigned, std::vector<OpType>> data;
    data[1] = {OpType::Rx, OpType::Ry, OpType::Rz};
    data[2] = {
        OpType::XXPhase,
        OpType::YYPhase,
        OpType::ZZPhase,
        OpType::ESWAP,
    };
    data[3] = {
        OpType::XXPhase3,
    };

    // Most ops are of the form exp(itB)
    // for a real matrix B. If so, we can test with conjugation also.
    const std::set<OpType> ops_not_of_form_exp_itb_for_real_b{
        OpType::Ry,
    };
    std::vector<double> params(1);

    for (const auto& entry : data) {
      const unsigned number_of_qubits = entry.first;
      for (auto type : entry.second) {
        for (double start_tt = -0.812345; start_tt < 2.0; start_tt += 0.7) {
          params[0] = start_tt;
          const auto start_matr =
              GateUnitaryMatrix::get_unitary(type, number_of_qubits, params);
          auto new_matr = start_matr;
          double new_tt = start_tt;
          for (int nn = 0; nn < 5; ++nn) {
            new_tt += start_tt;
            new_matr *= start_matr;
            params[0] = new_tt;
            const auto recalc_exp_ta =
                GateUnitaryMatrix::get_unitary(type, number_of_qubits, params);

            const OpDesc desc(type);
            INFO(
                "for op " << desc.name() << ", start t =" << start_tt
                          << ", n=" << nn << ", q=" << number_of_qubits
                          << "\nM=" << new_matr
                          << "\nrecalc exp(tA)=" << recalc_exp_ta);
            CHECK(new_matr.isApprox(recalc_exp_ta));

            if (ops_not_of_form_exp_itb_for_real_b.count(type) != 0) {
              continue;
            }
            params[0] = -new_tt;
            const auto recalc_exp_minus_ta =
                GateUnitaryMatrix::get_unitary(type, number_of_qubits, params);

            const auto conj_matr = recalc_exp_minus_ta.conjugate();
            CHECK(recalc_exp_ta.isApprox(conj_matr));
          }
        }
      }
    }
  }
}

}  // namespace test_GateUnitaryMatrix
}  // namespace tket
