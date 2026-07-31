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
 * @brief Functions related to (possibly symbolic) phase values
 */

#include <symengine/eval.h>
#include <symengine/expression.h>
#include <symengine/functions.h>
#include <symengine/integer.h>
#include <symengine/visitor.h>

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "Constants.hpp"
#include "Json.hpp"

#include "tket_export.h"

/** Helper struct for use with std::visit */
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace tket {

/** Representation of a phase as a multiple of \f$ \pi \f$ */
typedef SymEngine::Expression Expr;

JSON_DECL(Expr)

/** Shared pointer to an \p Expr */
typedef SymEngine::RCP<const SymEngine::Basic> ExprPtr;

/** Shared pointer to a free symbol */
typedef SymEngine::RCP<const SymEngine::Symbol> Sym;

}  // namespace tket

namespace nlohmann {

template <>
struct adl_serializer<tket::Expr> {
  static void to_json(json& j, const tket::Expr& exp) {
    tket::ExprPtr e_ = exp;
    j = e_->__str__();
  }

  static void from_json(const json& j, tket::Expr& exp) {
    exp = j.get<std::string>();
  }
};

template <>
struct adl_serializer<tket::Sym> {
  static void to_json(json& j, const tket::Sym& exp) {
    tket::ExprPtr e_ = exp;
    j = e_->__str__();
  }

  static void from_json(const json& j, tket::Sym& exp) {
    exp = SymEngine::symbol(j.get<std::string>());
  }
};

}  // namespace nlohmann

namespace tket {

struct SymCompareLess {
  bool operator()(const Sym& a, const Sym& b) const {
    return a->compare(*b) < 0;
  }
};

typedef std::set<Sym, SymCompareLess> SymSet;

/** Map from symbols to expressions */
typedef std::map<Sym, Expr, SymEngine::RCPBasicKeyLess> symbol_map_t;

/** Set of all free symbols contained in the expression */
SymSet expr_free_symbols(const Expr& e);

/** Set of all free symbols contained in the expressions in the vector */
SymSet expr_free_symbols(const std::vector<Expr>& es);

TKET_EXPORT std::optional<double> eval_expr(const Expr& e);

std::optional<Complex> eval_expr_c(const Expr& e);

/**
 * Evaluate an expression modulo n
 *
 * The result will be in the half-interval [0,n). If it is within EPS of a
 * multiple of 0.25 the result is clamped to an exact multiple.
 *
 * @param e expression to evaluate
 * @param n modulus
 * @return value of expression modulo n, iff it has no free symbols
 */
std::optional<double> eval_expr_mod(const Expr& e, unsigned n = 2);

/**
 * Return cos(e*pi/2)
 *
 * If e is within @p EPS of an integer then it is rounded so that the result can
 * be evaluated.
 */
Expr cos_halfpi_times(const Expr& e);

/**
 * Return sin(e*pi/2)
 *
 * If e is within @p EPS of an integer then it is rounded so that the result can
 * be evaluated.
 */
Expr sin_halfpi_times(const Expr& e);

/**
 * Return -e
 *
 * Expanding e after multiplying by -1 may reduce its size, especially when
 * `minus_times` is applied repeatedly and should cancel out.
 *
 * @param e expression
 * @return Expr -e
 */
Expr minus_times(const Expr& e);

/**
 * Test if an expression is approximately zero
 *
 * @param e expression
 * @param tol tolerance
 *
 * @return whether \p e is within \p tol of zero
 */
bool approx_0(const Expr& e, double tol = EPS);

/**
 * Evaluate modulo n in the range [0,n)
 *
 * @param x value to be reduced
 * @param n modulus
 *
 * @return y s.t. y == x (mod n) and 0 <= y < n
 */
double fmodn(double x, unsigned n);

/**
 * Test approximate equality of two values modulo n
 *
 * @param x first value
 * @param y second value
 * @param mod modulus
 * @param tol tolerance
 *
 * @return whether \p x is within \p tol of \p y modulo \p mod
 */
bool approx_eq(double x, double y, unsigned mod = 2, double tol = EPS);

/**
 * Test approximate equality of expressions modulo n
 *
 * @param e0 expression
 * @param e1 expression
 * @param n modulus
 * @param tol tolerance
 *
 * @retval true \p e0 and \p e1 are within \p tol of each other modulo n
 * @retval false expressions are not within tolerance or could not ve evaluated
 */
bool equiv_expr(
    const Expr& e0, const Expr& e1, unsigned n = 2, double tol = EPS);

/**
 * Test approximate value of an expression modulo n
 *
 * @param e expression
 * @param x test value
 * @param n modulus
 * @param tol tolerance
 *
 * @retval true \p e is within \p tol of \p x modulo n
 * @retval false expression is not within tolerance or could not be evaluated
 */
bool equiv_val(const Expr& e, double x, unsigned n = 2, double tol = EPS);

/**
 * Test whether a expression is approximately 0 modulo n
 *
 * @param e expression
 * @param n modulus
 * @param tol tolerance
 *
 * @retval true \p e is within \p tol of 0 modulo n
 * @retval false expression is not within tolerance or could not be evaluated
 */
bool equiv_0(const Expr& e, unsigned n = 2, double tol = EPS);

/**
 * Test whether an expression is approximately a Clifford angle (some multiple
 * of 0.5 modulo n)
 *
 * @param e expression
 * @param n modulus
 * @param tol tolerance
 *
 * @retval nullopt expression is not within tolerance or could not be evaluated
 * @retval u \f$ e \approx \frac12 u \pmod n \f$ where \f$ 0 \leq u < 2n \} \f$.
 */
std::optional<unsigned> equiv_Clifford(
    const Expr& e, unsigned n = 2, double tol = EPS);

/**
 * Exception indicating that symbolic values are not supported.
 */
class SymbolsNotSupported : public std::logic_error {
 public:
  explicit SymbolsNotSupported(const std::string& message)
      : std::logic_error(message) {}
  SymbolsNotSupported()
      : SymbolsNotSupported("Symbolic values not supported") {}
};

}  // namespace tket
