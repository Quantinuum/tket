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

#include "tket/Gate/Rotation.hpp"

#include <symengine/constants.h>

#include <tkassert/Assert.hpp>

#include "tket/OpType/OpDesc.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Utils/Expression.hpp"

namespace tket {

static Expr atan2_bypi(const Expr &a, const Expr &b) {
  std::optional<double> va = eval_expr(a);
  std::optional<double> vb = eval_expr(b);
  if (va && vb) {
    double vva = va.value();
    double vvb = vb.value();
    if (std::abs(vva) < EPS && std::abs(vvb) < EPS) return Expr(0.);
    return atan2(vva, vvb) / PI;
  } else {
    // Convert symbolic zero to 0. This is a workaround for
    // https://github.com/symengine/symengine/issues/1875 .
    Expr a1 = a, b1 = b;
    if (a1 == SymEngine::zero) a1 = 0.;
    if (b1 == SymEngine::zero) b1 = 0.;
    return SymEngine::div(SymEngine::atan2(a1, b1), SymEngine::pi);
  }
}

static Expr acos_bypi(const Expr &a) {
  std::optional<double> va = eval_expr(a);
  if (va) {
    double vva = va.value();
    // avoid undefined values due to rounding
    if (vva >= 1.) return 0.;
    if (vva <= -1.) return 1.;
    return acos(vva) / PI;
  } else {
    return SymEngine::div(SymEngine::acos(a), SymEngine::pi);
  }
}

// SymEngine::div does not always spot when the numerator is a scalar multiple
// of the denominator, for example in expressions like (-a + b) / (a - b) where
// a and b are symbolic. This function picks out the common cases.
static Expr expr_div(const Expr &num, const Expr &den) {
  if (approx_0(SymEngine::expand(num - den))) return 1;
  if (approx_0(SymEngine::expand(num + den))) return -1;
  return SymEngine::div(num, den);
}

static std::tuple<Expr, Expr, Expr> xyx_angles_from_coeffs(
    const Expr &s, const Expr &i, const Expr &j, const Expr &k) {
  // Handle exceptional cases first.
  bool s_zero = approx_0(s);
  bool s_one = approx_0(s - 1);
  bool i_zero = approx_0(i);
  bool i_one = approx_0(i - 1);
  bool j_zero = approx_0(j);
  bool j_one = approx_0(j - 1);
  bool k_zero = approx_0(k);
  bool k_one = approx_0(k - 1);
  if (i_zero && j_zero && k_zero) {
    if (s_one)
      return {0, 0, 0};
    else
      return {2, 0, 0};
  }
  if (s_zero && j_zero && k_zero) {
    if (i_one)
      return {1, 0, 0};
    else
      return {3, 0, 0};
  }
  if (s_zero && i_zero && k_zero) {
    if (j_one)
      return {0, 1, 0};
    else
      return {0, 3, 0};
  }
  if (s_zero && i_zero && j_zero) {
    if (k_one)
      return {3, 1, 0};
    else
      return {1, 1, 0};
  }
  if (s_zero && i_zero) return {-2 * atan2_bypi(k, j), 1, 0};
  if (s_zero && j_zero) return {0, 2 * atan2_bypi(k, i), 1};
  if (s_zero && k_zero) return {0.5, 2 * atan2_bypi(j, i), 0.5};
  if (i_zero && j_zero) return {-0.5, 2 * atan2_bypi(k, s), 0.5};
  if (i_zero && k_zero) return {0, 2 * atan2_bypi(j, s), 0};
  if (j_zero && k_zero) return {2 * atan2_bypi(i, s), 0, 0};
  if (s_zero) {
    // We want to return (a, b, c) s.t.
    // Rx(c) Ry(b) Rx(a) = i*i_ + j*j_ + k*k_ (where i_, j_, k_ are the
    // quaternionic roots of -1. Multiplying out the LHS gives
    // (Cc + i_ Sc)(Cb + j_ Sb)(Ca + i_ Sa)
    // where for brevity Sa = sin(pi*a/2), Ca = cos(pi*a/2) etc.
    // Expanding and using the sin/cos addition identities gives
    // Cb cos(pi*l) = 0, Cb sin(pi*l) = i, Sb cos(pi*m) = j, Sb sin(pi*m) = k
    // where l = (c+a)/2 and m = (c-a)/2.
    // Since we have already handled the case s=i=0 we know that Cb != 0, so
    // cos(pi*l) = 0. A solution to these equations is given by
    // l = 1/2, m = atan2(k,j)/pi, b = (2/pi)*acos(i)
    // and finally substituting a=l-m, b=l+m gives the following solution:
    Expr m = atan2_bypi(k, j);
    return {0.5 - m, 2 * acos_bypi(i), 0.5 + m};
  }

  // This is a (partial) workaround for
  // https://github.com/symengine/symengine/issues/1806
  // (since it avoids the use of atan2 with proportional symbolics).
  // Explanation: When the quaternion is of the form
  //   A + uA i + B j -/+ uB k
  // where u is a pure number (with no free symbols) but A and B are symbolic,
  // symengine wrongly simplifies atan2(uA, A) and atan2(uB, B), ignoring the
  // symbols (whose sign ought to affect the result). So the general formula
  // below does not work. We therefore handle these cases differently, noting
  // that the quaternion factorizes as either
  //   (A + Bj) (1 + ui) or (1 + ui) (A + Bj)
  // -- that is, an Rx followed by an Ry or vice versa.
  // We will analyse the first case; the second is similar.
  // Let alpha = atan(u). Note that -pi/2 < alpha < pi/2, so cos(alpha) > 0.
  // The quaternion is then
  //   (A/cos(alpha) + B/cos(alpha) j) (cos(alpha) + sin(alpha) i)
  // So we can take the angle of the Rx as 2*alpha, and the angle of the Ry as
  // 2 * atan2(B, A).
  // Finally, note that u must be well-defined because we have already dealt
  // with all cases where s = 0.
  //
  // NOTE: Although the above symengine issue is fixed as of version 0.15.0, we
  // retain the "workaround" since it gives better simplifications: symengine is
  // not good at evaluating expressions combining sign and trig functions (e.g.
  // "sign(cos(pi/4))".
  TKET_ASSERT(!s_zero);
  if (approx_0(SymEngine::expand(i * j + s * k))) {
    Expr u = expr_div(i, s);
    if (SymEngine::free_symbols(u).empty()) {
      Expr a = SymEngine::atan(u);
      Expr q = 2 * atan2_bypi(j, s);
      Expr two_a_by_pi = SymEngine::div(2 * a, SymEngine::pi);
      return std::tuple<Expr, Expr, Expr>(two_a_by_pi, q, 0);
    }
  } else if (approx_0(SymEngine::expand(i * j - s * k))) {
    Expr u = expr_div(i, s);
    if (SymEngine::free_symbols(u).empty()) {
      Expr a = SymEngine::atan(u);
      Expr q = 2 * atan2_bypi(j, s);
      Expr two_a_by_pi = SymEngine::div(2 * a, SymEngine::pi);
      return std::tuple<Expr, Expr, Expr>(0, q, two_a_by_pi);
    }
  }

  // Now the general case.
  Expr a = atan2_bypi(i, s);
  Expr b = atan2_bypi(k, j);
  Expr q = acos_bypi(SymEngine::expand(s * s + i * i - j * j - k * k));
  return std::tuple<Expr, Expr, Expr>(a - b, q, a + b);
}

Rotation::Rotation(OpType optype, Expr a)
    : i_(0), j_(0), k_(0), optype_(optype), a_(a) {
  if (equiv_0(a, 4)) {
    rep_ = Rep::id;
    s_ = 1;
    i_ = j_ = k_ = 0;
  } else if (equiv_0(a - 2, 4)) {
    rep_ = Rep::minus_id;
    s_ = -1;
    i_ = j_ = k_ = 0;
  } else {
    rep_ = Rep::orth_rot;
    s_ = cos_halfpi_times(a);
    Expr t = sin_halfpi_times(a);
    switch (optype) {
      case OpType::Rx:
        i_ = t;
        break;
      case OpType::Ry:
        j_ = t;
        break;
      case OpType::Rz:
        k_ = t;
        break;
      default:
        throw std::logic_error(
            "Quaternions can only be constructed "
            "from Rx, Ry or Rz rotations");
    }
  }
}

std::optional<Expr> Rotation::angle(OpType optype) const {
  if (rep_ == Rep::id) {
    return Expr(0);
  } else if (rep_ == Rep::minus_id) {
    return Expr(2);
  } else if (rep_ == Rep::orth_rot && optype_ == optype) {
    return a_;
  } else {
    return std::nullopt;
  }
}

std::tuple<Expr, Expr, Expr> Rotation::to_pqp(OpType p, OpType q) const {
  if (rep_ == Rep::id) {
    return {Expr(0), Expr(0), Expr(0)};
  } else if (rep_ == Rep::minus_id) {
    return {Expr(2), Expr(0), Expr(0)};
  } else if (rep_ == Rep::orth_rot) {
    if (optype_ == p) {
      return {a_, Expr(0), Expr(0)};
    } else if (optype_ == q) {
      return {Expr(0), a_, Expr(0)};
    }
  }
  if (p == OpType::Rx && q == OpType::Ry) {
    return xyx_angles_from_coeffs(s_, i_, j_, k_);
  } else if (p == OpType::Ry && q == OpType::Rx) {
    return xyx_angles_from_coeffs(s_, j_, i_, -k_);
  } else if (p == OpType::Ry && q == OpType::Rz) {
    return xyx_angles_from_coeffs(s_, j_, k_, i_);
  } else if (p == OpType::Rz && q == OpType::Ry) {
    return xyx_angles_from_coeffs(s_, k_, j_, -i_);
  } else if (p == OpType::Rz && q == OpType::Rx) {
    return xyx_angles_from_coeffs(s_, k_, i_, j_);
  } else if (p == OpType::Rx && q == OpType::Rz) {
    return xyx_angles_from_coeffs(s_, i_, k_, -j_);
  } else {
    throw std::logic_error("Axes must be a pair of X, Y, Z.");
  }
}

// Table of compositions
static const std::map<
    std::tuple<OpType, int, OpType, int>, std::pair<OpType, int>>
    product = {
        {{OpType::Rx, +1, OpType::Ry, +1}, {OpType::Rz, -1}},
        {{OpType::Rx, +1, OpType::Rz, +1}, {OpType::Ry, +1}},
        {{OpType::Rx, +1, OpType::Ry, -1}, {OpType::Rz, +1}},
        {{OpType::Rx, +1, OpType::Rz, -1}, {OpType::Ry, -1}},
        {{OpType::Rx, -1, OpType::Ry, +1}, {OpType::Rz, +1}},
        {{OpType::Rx, -1, OpType::Rz, +1}, {OpType::Ry, -1}},
        {{OpType::Rx, -1, OpType::Ry, -1}, {OpType::Rz, -1}},
        {{OpType::Rx, -1, OpType::Rz, -1}, {OpType::Ry, +1}},
        {{OpType::Ry, +1, OpType::Rz, +1}, {OpType::Rx, -1}},
        {{OpType::Ry, +1, OpType::Rx, +1}, {OpType::Rz, +1}},
        {{OpType::Ry, +1, OpType::Rz, -1}, {OpType::Rx, +1}},
        {{OpType::Ry, +1, OpType::Rx, -1}, {OpType::Rz, -1}},
        {{OpType::Ry, -1, OpType::Rz, +1}, {OpType::Rx, +1}},
        {{OpType::Ry, -1, OpType::Rx, +1}, {OpType::Rz, -1}},
        {{OpType::Ry, -1, OpType::Rz, -1}, {OpType::Rx, -1}},
        {{OpType::Ry, -1, OpType::Rx, -1}, {OpType::Rz, +1}},
        {{OpType::Rz, +1, OpType::Rx, +1}, {OpType::Ry, -1}},
        {{OpType::Rz, +1, OpType::Ry, +1}, {OpType::Rx, +1}},
        {{OpType::Rz, +1, OpType::Rx, -1}, {OpType::Ry, +1}},
        {{OpType::Rz, +1, OpType::Ry, -1}, {OpType::Rx, -1}},
        {{OpType::Rz, -1, OpType::Rx, +1}, {OpType::Ry, +1}},
        {{OpType::Rz, -1, OpType::Ry, +1}, {OpType::Rx, -1}},
        {{OpType::Rz, -1, OpType::Rx, -1}, {OpType::Ry, -1}},
        {{OpType::Rz, -1, OpType::Ry, -1}, {OpType::Rx, +1}},
};

void Rotation::apply(const Rotation &other) {
  if (other.rep_ == Rep::id) return;

  if (rep_ == Rep::id) {
    rep_ = other.rep_;
    s_ = other.s_;
    i_ = other.i_;
    j_ = other.j_;
    k_ = other.k_;
    optype_ = other.optype_;
    a_ = other.a_;
    return;
  }

  if (rep_ == Rep::minus_id) {
    if (other.rep_ == Rep::minus_id) {
      rep_ = Rep::id;
      s_ = 1;
      i_ = j_ = k_ = 0;
    } else if (other.rep_ == Rep::orth_rot) {
      rep_ = Rep::orth_rot;
      optype_ = other.optype_;
      a_ = other.a_ + 2;
      s_ = -other.s_;
      i_ = -other.i_;
      j_ = -other.j_, k_ = -other.k_;
    } else {
      rep_ = Rep::quat;
      s_ = -other.s_;
      i_ = -other.i_;
      j_ = -other.j_, k_ = -other.k_;
    }
    return;
  }

  if (rep_ == Rep::orth_rot && other.rep_ == Rep::orth_rot) {
    if (optype_ == other.optype_) {
      a_ += other.a_;
      if (equiv_0(a_, 4)) {
        rep_ = Rep::id;
      } else if (equiv_0(a_, 2)) {
        rep_ = Rep::minus_id;
      }
    } else if (
        (equiv_val(a_, 1., 4) || equiv_val(a_, -1., 4)) &&
        (equiv_val(other.a_, 1., 4) || equiv_val(other.a_, -1., 4))) {
      // We are in a subgroup of order 8
      int m0 = equiv_val(a_, 1., 4) ? 1 : -1;
      int m1 = equiv_val(other.a_, 1., 4) ? 1 : -1;
      std::tie(optype_, a_) = product.at({optype_, m0, other.optype_, m1});
    } else
      rep_ = Rep::quat;
  } else
    rep_ = Rep::quat;

  Expr s1 = other.s_ * s_ - other.i_ * i_ - other.j_ * j_ - other.k_ * k_;
  Expr i1 = other.s_ * i_ + other.i_ * s_ + other.j_ * k_ - other.k_ * j_;
  Expr j1 = other.s_ * j_ - other.i_ * k_ + other.j_ * s_ + other.k_ * i_;
  Expr k1 = other.s_ * k_ + other.i_ * j_ - other.j_ * i_ + other.k_ * s_;
  s_ = SymEngine::expand(s1);
  i_ = SymEngine::expand(i1);
  j_ = SymEngine::expand(j1);
  k_ = SymEngine::expand(k1);

  if (rep_ == Rep::quat) {
    // See if we can simplify the representation.
    bool i_zero = approx_0(i_);
    bool j_zero = approx_0(j_);
    bool k_zero = approx_0(k_);
    if (i_zero && j_zero && k_zero) {
      if (approx_0(s_ - 1)) {
        rep_ = Rep::id;
        s_ = 1;
        i_ = j_ = k_ = 0;
      } else {
        rep_ = Rep::minus_id;
        s_ = -1;
        i_ = j_ = k_ = 0;
      }
    } else if (j_zero && k_zero) {
      rep_ = Rep::orth_rot;
      optype_ = OpType::Rx;
      a_ = 2 * atan2_bypi(i_, s_);
      j_ = k_ = 0;
    } else if (k_zero && i_zero) {
      rep_ = Rep::orth_rot;
      optype_ = OpType::Ry;
      a_ = 2 * atan2_bypi(j_, s_);
      k_ = i_ = 0;
    } else if (i_zero && j_zero) {
      rep_ = Rep::orth_rot;
      optype_ = OpType::Rz;
      a_ = 2 * atan2_bypi(k_, s_);
      i_ = j_ = 0;
    }
  }
}

std::ostream &operator<<(std::ostream &os, const Rotation &q) {
  if (q.rep_ == Rotation::Rep::id) {
    return os << "I";
  } else if (q.rep_ == Rotation::Rep::minus_id) {
    return os << "-I";
  } else if (q.rep_ == Rotation::Rep::orth_rot) {
    return os << OpDesc(q.optype_).name() << "(" << q.a_ << ")";
  } else {
    return os << q.s_ << " + " << q.i_ << " i + " << q.j_ << " j + " << q.k_
              << " k";
  }
}

std::vector<double> tk1_angles_from_unitary(const Eigen::Matrix2cd &U) {
  // clang-format off
    //
    // Assume U = e^{i pi p} TK1(a,b,c)
    //          = e^{i pi p} Rz(a) Rx(b) Rz(c)
    //                       |    e^{-i pi (a+c)/2} cos(pi b/2)  -i e^{-i pi (a-c)/2} sin(pi b/2)  |
    //          = e^{i pi p} |                                                                     |
    //                       | -i e^{ i pi (a-c)/2} sin(pi b/2)     e^{ i pi (a+c)/2} cos(pi b/2)  |
    //
  // clang-format on

  double a, b, c, p;  // to be found satisfying the above equation

  std::complex s = 0.5 * (U(0, 0) + U(1, 1));
  std::complex x = 0.5 * i_ * (U(1, 0) + U(0, 1));
  std::complex y = 0.5 * (U(1, 0) - U(0, 1));
  std::complex z = 0.5 * i_ * (U(0, 0) - U(1, 1));

  // s = e^{i pi p} cos(pi b/2) cos(pi (a+c)/2)
  // x = e^{i pi p} sin(pi b/2) cos(pi (a-c)/2)
  // y = e^{i pi p} sin(pi b/2) sin(pi (a-c)/2)
  // z = e^{i pi p} cos(pi b/2) sin(pi (a+c)/2)

  // s, x, y and z all have phase e^{i pi p}. Extract it from the one with
  // largest absolute value, to minimize numerical instability. Note that
  // there are two possible values (p and p+1), but the choice is w.l.o.g.
  // because (p,b) --> (p+1,b+2) does not change the value of the unitary.

  std::complex w = s;
  if (std::abs(x) > std::abs(w)) w = x;
  if (std::abs(y) > std::abs(w)) w = y;
  if (std::abs(z) > std::abs(w)) w = z;
  std::complex eip = w / std::abs(w);

  // eip = e^{i pi p}
  p = std::arg(eip) / PI;

  // Now we've got the phase, factor it out.

  std::complex emip = std::conj(eip);
  double s0 = std::real(emip * s);
  double x0 = std::real(emip * x);
  double y0 = std::real(emip * y);
  double z0 = std::real(emip * z);

  // s0 = cos(pi b/2) cos(pi (a+c)/2)
  // x0 = sin(pi b/2) cos(pi (a-c)/2)
  // y0 = sin(pi b/2) sin(pi (a-c)/2)
  // z0 = cos(pi b/2) sin(pi (a+c)/2)

  std::complex u = {s0, z0}, v = {x0, y0};

  // u = cos(pi b/2) e^{i pi (a+c)/2}
  // v = sin(pi b/2) e^{i pi (a-c)/2}
  // |u|^2 + |v|^2 = 1

  // Note that (a,b) --> (a+2, b+2) does not change the value of the unitary, so
  // we are free to choose either solution.
  //
  // We treat the two special cases u=0 and v=0 separately, then the general
  // case.

  if (std::abs(u) < EPS) {  // special case, b = 1 or 3
    // v = +/- e^{i pi (a-c)/2}

    // We may as well choose c = 0.
    c = 0;

    // Assume b=1 and compute a.
    // (b'=3, a'=a+2) is the other possibility but the unitary is the same.
    b = 1;
    a = 2 * std::arg(v) / PI;
  } else if (std::abs(v) < EPS) {  // special case, b = 0 or 2
    // u = e^{i pi (a+c)/2}

    // We may as well choose c = 0.
    c = 0;

    // Assume b=0 and compute a.
    // (b'=2, a'=a+2) is the other possibility but the unitary is the same.
    b = 0;
    a = 2 * std::arg(u) / PI;
  } else {  // general case
    // s0^2 + z0^2 - x0^2 - y0^2 = cos(pi b)
    double t = s0 * s0 + z0 * z0 - x0 * x0 - y0 * y0;
    // Rounding errors may mean t is outside the domain of acos. Fix this.
    if (t > +1.) t = +1.;
    if (t < -1.) t = -1.;
    b = std::acos(t) / PI;

    // w.l.o.g. b is in the range (-1,+1).
    double ac0 = std::arg(u), ac1 = std::arg(v);
    a = (ac0 + ac1) / PI;
    c = (ac0 - ac1) / PI;
  }

  return {a, b, c, p};
}

std::tuple<double, double, double> get_bloch_coordinate_from_state(
    const Complex &a, const Complex &b) {
  // We require the state to be normalised
  double norm = a.real() * a.real() + a.imag() * a.imag() +
                b.real() * b.real() + b.imag() * b.imag();
  if (std::abs(norm - 1) > EPS) {
    throw std::invalid_argument(
        "Attempt to find the Bloch sphere coordinate for an unnormalised "
        "state. Only unit length vectors have coordinates on the Bloch "
        "sphere.");
  }
  double r_a = std::abs(a);
  double theta_a = std::arg(a) / PI;
  double theta_b = std::arg(b) / PI;
  double theta = (std::acos(r_a) * 2) / PI;
  double phi = theta_b - theta_a;
  double phase = theta_a;
  return {theta, phi, phase};
}

Eigen::Matrix2cd get_matrix_from_tk1_angles(std::vector<Expr> params) {
  double alpha = eval_expr(params[0]).value();
  double beta = eval_expr(params[1]).value();
  double gamma = eval_expr(params[2]).value();
  double t = eval_expr(params[3]).value();
  Eigen::Matrix2cd m;
  alpha *= PI;
  beta *= PI;
  gamma *= PI;
  t *= PI;
  double c = cos(0.5 * beta);
  double s = sin(0.5 * beta);
  m << exp(-0.5 * i_ * (alpha + gamma)) * c,
      -i_ * exp(0.5 * i_ * (gamma - alpha)) * s,
      -i_ * exp(0.5 * i_ * (alpha - gamma)) * s,
      exp(0.5 * i_ * (alpha + gamma)) * c;
  return exp(i_ * t) * m;
}

}  // namespace tket
