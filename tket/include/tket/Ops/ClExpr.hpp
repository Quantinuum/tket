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

/**
 * @file
 * @brief Classical expressions involving bits and registers
 */

#include <cstdint>
#include <map>
#include <nlohmann/detail/macro_scope.hpp>
#include <ostream>
#include <set>
#include <variant>
#include <vector>

#include "tket/Ops/Op.hpp"

#include "tket_export.h"

namespace tket {

// TODO Use X or list macros to reduce boilerplate.

/**
 * An function acting on bits or bit registers
 */
enum class ClOp {
  INVALID,  /// Invalid
  BitAnd,   /// Bitwise AND
  BitOr,    /// Bitwise OR
  BitXor,   /// Bitwise XOR
  BitEq,    /// Bitwise equality
  BitNeq,   /// Bitwise inequality
  BitNot,   /// Bitwise NOT
  BitZero,  /// Constant zero bit
  BitOne,   /// Constant one bit
  RegAnd,   /// Registerwise AND
  RegOr,    /// Registerwise OR
  RegXor,   /// Registerwise XOR
  RegEq,    /// Registerwise equality
  RegNeq,   /// Registerwise inequality
  RegNot,   /// Registerwise NOT
  RegZero,  /// Constant all-zeros register
  RegOne,   /// Constant all-ones register
  RegLt,    /// Integer less-than comparison
  RegGt,    /// Integer greater-than comparison
  RegLeq,   /// Integer less-than-or-equal comparison
  RegGeq,   /// Integer greater-than-or-equal comparison
  RegAdd,   /// Integer addition
  RegSub,   /// Integer subtraction
  RegMul,   /// Integer multiplication
  RegDiv,   /// Integer division
  RegPow,   /// Integer exponentiation
  RegLsh,   /// Left shift
  RegRsh,   /// Right shift
  RegNeg    /// Integer negation
};

std::ostream& operator<<(std::ostream& os, ClOp fn);

NLOHMANN_JSON_SERIALIZE_ENUM(
    ClOp, {
              {ClOp::INVALID, "INVALID"}, {ClOp::BitAnd, "BitAnd"},
              {ClOp::BitOr, "BitOr"},     {ClOp::BitXor, "BitXor"},
              {ClOp::BitEq, "BitEq"},     {ClOp::BitNeq, "BitNeq"},
              {ClOp::BitNot, "BitNot"},   {ClOp::BitZero, "BitZero"},
              {ClOp::BitOne, "BitOne"},   {ClOp::RegAnd, "RegAnd"},
              {ClOp::RegOr, "RegOr"},     {ClOp::RegXor, "RegXor"},
              {ClOp::RegEq, "RegEq"},     {ClOp::RegNeq, "RegNeq"},
              {ClOp::RegNot, "RegNot"},   {ClOp::RegZero, "RegZero"},
              {ClOp::RegOne, "RegOne"},   {ClOp::RegLt, "RegLt"},
              {ClOp::RegGt, "RegGt"},     {ClOp::RegLeq, "RegLeq"},
              {ClOp::RegGeq, "RegGeq"},   {ClOp::RegAdd, "RegAdd"},
              {ClOp::RegSub, "RegSub"},   {ClOp::RegMul, "RegMul"},
              {ClOp::RegDiv, "RegDiv"},   {ClOp::RegPow, "RegPow"},
              {ClOp::RegLsh, "RegLsh"},   {ClOp::RegRsh, "RegRsh"},
              {ClOp::RegNeg, "RegNeg"},
          })

/**
 * A bit variable within an expression
 */
typedef struct TKET_EXPORT ClBitVar {
  unsigned index;  /// Identifier for the variable within the expression
  bool operator==(const ClBitVar& other) const { return index == other.index; }
  friend std::ostream& operator<<(std::ostream& os, const ClBitVar& var);
} ClBitVar;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClBitVar, index)

/**
 * A register variable within an expression
 */
typedef struct TKET_EXPORT ClRegVar {
  unsigned index;  /// Identifier for the variable within the expression
  bool operator==(const ClRegVar& other) const { return index == other.index; }
  friend std::ostream& operator<<(std::ostream& os, const ClRegVar& var);
} ClRegVar;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClRegVar, index)

/**
 * A (bit or register) variable within an expression
 */
typedef std::variant<ClBitVar, ClRegVar> ClExprVar;

std::ostream& operator<<(std::ostream& os, const ClExprVar& var);

void to_json(nlohmann::json& j, const ClExprVar& var);

void from_json(const nlohmann::json& j, ClExprVar& var);

/**
 * A term in a classical expression (either a constant or a variable)
 */
typedef std::variant<uint64_t, ClExprVar> ClExprTerm;

std::ostream& operator<<(std::ostream& os, const ClExprTerm& term);

void to_json(nlohmann::json& j, const ClExprTerm& term);

void from_json(const nlohmann::json& j, ClExprTerm& term);

class ClExpr;

/**
 * An argument to a classical operation in an expression
 */
typedef std::variant<ClExprTerm, ClExpr> ClExprArg;

std::ostream& operator<<(std::ostream& os, const ClExprArg& arg);

void to_json(nlohmann::json& j, const ClExprArg& arg);

void from_json(const nlohmann::json& j, ClExprArg& arg);

/**
 * A classical expression
 *
 * It may be composed of subexpressions.
 */
class TKET_EXPORT ClExpr {
 public:
  /**
   * Default constructor
   */
  ClExpr();

  /**
   * Construct a classical expression from an operation and its arguments
   *
   * @param op Operation
   * @param args Arguments
   */
  ClExpr(ClOp op, std::vector<ClExprArg> args);

  bool operator==(const ClExpr& other) const;

  friend std::ostream& operator<<(std::ostream& os, const ClExpr& expr);

  /**
   * Main operation
   */
  ClOp get_op() const;

  /**
   * Arguments
   */
  std::vector<ClExprArg> get_args() const;

  /**
   * All bit variables occurring within the expression
   */
  std::set<unsigned> all_bit_variables() const;

  /**
   * All register variables occurring within the expression
   */
  std::set<unsigned> all_reg_variables() const;

 private:
  ClOp op;
  std::vector<ClExprArg> args;
  std::set<unsigned> all_bit_vars;
  std::set<unsigned> all_reg_vars;
};

void to_json(nlohmann::json& j, const ClExpr& expr);

void from_json(const nlohmann::json& j, ClExpr& expr);

/**
 * A classical expression defined over a sequence of bits
 *
 * This defines an operation on a finite number of bits. Bit variables within
 * the expression are mapped to specific bit indices and register variables are
 * mapped to specific (disjoint) sequences of bit indices. The output of the
 * expression is also mapped to a specific bit index or sequence of bit indices.
 * If the output is a register, it must either be disjoint from all of the input
 * registers or exactly match one of them.
 */
class TKET_EXPORT WiredClExpr {
 public:
  /**
   * Default constructor
   */
  WiredClExpr();

  /**
   * Construct by specifying the bit, register and output positions
   *
   * @param expr Expression
   * @param bit_posn Map from identifiers of bit variables to bit positions
   * @param reg_posn Map from identifiers of register variables to sequences of
   *     bit positions.
   * @param output_posn Sequence of bit positions for the output
   * @throws ClExprWiringError if wiring is not valid
   */
  WiredClExpr(
      const ClExpr& expr, const std::map<unsigned, unsigned>& bit_posn,
      const std::map<unsigned, std::vector<unsigned>>& reg_posn,
      const std::vector<unsigned> output_posn);

  bool operator==(const WiredClExpr& other) const;

  friend std::ostream& operator<<(std::ostream& os, const WiredClExpr& expr);

  /**
   * Expression
   */
  ClExpr get_expr() const;

  /**
   * Bit positions
   */
  std::map<unsigned, unsigned> get_bit_posn() const;

  /**
   * Register positions
   */
  std::map<unsigned, std::vector<unsigned>> get_reg_posn() const;

  /**
   * Output positions
   */
  std::vector<unsigned> get_output_posn() const;

  /**
   * Total number of bits including bit and register inputs and output
   */
  unsigned get_total_n_bits() const;

 private:
  ClExpr expr;
  std::map<unsigned, unsigned> bit_posn;
  std::map<unsigned, std::vector<unsigned>> reg_posn;
  std::set<unsigned> all_bit_posns;
  std::set<std::vector<unsigned>> all_reg_posns;
  std::vector<unsigned> output_posn;
  unsigned total_n_bits;
};

void to_json(nlohmann::json& j, const WiredClExpr& expr);

void from_json(const nlohmann::json& j, WiredClExpr& expr);

class ClExprWiringError : public std::logic_error {
 public:
  explicit ClExprWiringError(const std::string& message)
      : std::logic_error(message) {}
};

class TKET_EXPORT ClExprOp : public Op {
 public:
  ClExprOp(const WiredClExpr& expr);

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  SymSet free_symbols() const override;
  op_signature_t get_signature() const override;

  /**
   * Wired classical expression
   */
  WiredClExpr get_wired_expr() const;

  nlohmann::json serialize() const override;

  static Op_ptr deserialize(const nlohmann::json& j);

 private:
  WiredClExpr expr;
};

}  // namespace tket
