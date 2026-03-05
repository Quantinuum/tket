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

#include "tket/Utils/MatrixAnalysis.hpp"

#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <tkassert/Assert.hpp>
#include <utility>
#include <vector>

namespace tket {

bool is_unitary(const Eigen::MatrixXcd &U, double tol) {
  unsigned m = U.rows(), n = U.cols();
  if (m != n) return false;
  return Eigen::MatrixXcd::Identity(n, n).isApprox(U.adjoint() * U, tol);
}

bool is_projector(const Eigen::MatrixXcd &P, double tol) {
  unsigned m = P.rows(), n = P.cols();
  if (m != n) return false;
  return P.isApprox(P * P, tol) && P.isApprox(P.adjoint());
}

std::pair<MatrixXb, MatrixXb> binary_LLT_decomposition(const MatrixXb &a) {
  /*
   * Aaronson-Gottesman: Improved Simulation of Stabilizer Circuits, Lemma 7
   * For any symmetric, binary matrix A, there exists a diagonal matrix D
   * and an invertible lower-triangular matrix L such that A + D = LL^T.
   * Given A, returns the pair <L, D>
   */
  unsigned n = a.rows();
  MatrixXb lo = MatrixXb::Identity(n, n);
  for (unsigned j = 0; j < n; j++) {
    for (unsigned i = j + 1; i < n; i++) {
      bool sum = a(i, j);
      for (unsigned k = 0; k < j; k++) {
        sum ^= lo(i, k) && lo(j, k);
      }
      lo(i, j) = sum;
    }
  }
  MatrixXb d = MatrixXb::Zero(n, n);
  for (unsigned i = 0; i < n; i++) {
    bool sum = a(i, i);
    for (unsigned k = 0; k < n; k++) {
      sum ^= lo(i, k);
    }  // diagonal element of LL^T is just parity of row of L
    d(i, i) = sum;
  }
  return {lo, d};
}

std::vector<std::pair<unsigned, unsigned>> gaussian_elimination_col_ops(
    const MatrixXb &a, unsigned blocksize) {
  return gaussian_elimination_row_ops(a.transpose(), blocksize);
}

struct MatrixXbBlockCompare {
  bool operator()(
      const MatrixXb::BlockXpr &lhs, const MatrixXb::BlockXpr &rhs) const {
    TKET_ASSERT(lhs.rows() == rhs.rows());
    TKET_ASSERT(lhs.cols() == rhs.cols());
    for (unsigned r = 0; r < lhs.rows(); ++r) {
      for (unsigned c = 0; c < lhs.cols(); ++c) {
        if (lhs(r, c) < rhs(r, c)) return true;
        if (lhs(r, c) > rhs(r, c)) return false;
      }
    }
    return false;
  }
};

/* see https://web.eecs.umich.edu/~imarkov/pubs/jour/qic08-cnot.pdf for a full
 * explanation of this technique */
/* K. Patel, I. Markov, J. Hayes. Optimal Synthesis of Linear Reversible
        Circuits. QIC 2008 */
std::vector<std::pair<unsigned, unsigned>> gaussian_elimination_row_ops(
    const MatrixXb &a, unsigned blocksize) {
  std::vector<std::pair<unsigned, unsigned>> ops;
  MatrixXb m = a;
  unsigned rows = m.rows();
  unsigned cols = m.cols();
  std::vector<unsigned> pcols;  // we know which columns have non-zero entries
                                // (to save actually transposing the matrix)
  unsigned pivot_row = 0;
  unsigned ceiling =
      (cols + blocksize - 1) / blocksize;  // ceil(cols/blocksize)

  // Get to upper echelon form
  for (unsigned sec = 0; sec < ceiling; ++sec) {
    // determine column range for this section
    // if it hits cols don't go any higher
    unsigned i0 = sec * blocksize;
    unsigned i1 = std::min(cols, (sec + 1) * blocksize);

    /* first, try to eliminate sub-rows (ie chunks), for greater speed than
     * naively doing gaussian elim. */
    std::map<MatrixXb::BlockXpr, unsigned, MatrixXbBlockCompare> chunks;
    for (unsigned r = pivot_row; r < rows; ++r) {
      MatrixXb::BlockXpr t = m.block(r, i0, 1, i1 - i0);
      if (t == MatrixXb::Zero(1, i1 - i0)) continue;
      /* if first copy of pattern, save. If duplicate then remove by adding
       * rows*/
      auto chunk_it = chunks.find(t);
      if (chunk_it != chunks.end()) {
        for (unsigned k = 0; k < cols; ++k) {
          m(r, k) ^= m(chunk_it->second, k);
        }
        ops.push_back({chunk_it->second, r});
      } else {
        chunks.insert({t, r});
      }
    }
    /* do gaussian elim. on remaining entries */
    for (unsigned col = i0; col < i1; ++col) {
      // find first 1 element in column after pivot_row
      unsigned first_1 = pivot_row;
      while (first_1 < rows && !m(first_1, col)) {
        ++first_1;
      }
      if (first_1 == rows) continue;

      // pull back to pivot
      if (first_1 != pivot_row) {
        for (unsigned k = 0; k < cols; ++k) {
          m(pivot_row, k) ^= m(first_1, k);
        }
        ops.push_back({first_1, pivot_row});
      }

      // clear all entries below pivot
      for (unsigned r = std::max(pivot_row + 1, first_1); r < rows; ++r) {
        if (m(r, col)) {
          for (unsigned k = 0; k < cols; ++k) {
            m(r, k) ^= m(pivot_row, k);
          }
          ops.push_back({pivot_row, r});
        }
      }

      // record that we pivoted for this column
      pcols.push_back(col);
      ++pivot_row;
    }
  }

  /* matrix is now in upper triangular form; matrix is reduced to diagonal */

  --pivot_row;

  for (unsigned sec = ceiling; sec-- > 0;) {
    unsigned i0 = sec * blocksize;
    unsigned i1 = std::min(cols, (sec + 1) * blocksize);

    std::map<MatrixXb::BlockXpr, unsigned, MatrixXbBlockCompare> chunks;
    for (unsigned r = pivot_row + 1; r-- > 0;) {
      MatrixXb::BlockXpr t = m.block(r, i0, 1, i1 - i0);
      if (t == MatrixXb::Zero(1, i1 - i0)) continue;

      auto chunk_it = chunks.find(t);
      if (chunk_it != chunks.end()) {
        for (unsigned k = 0; k < cols; ++k) {
          m(r, k) ^= m(chunk_it->second, k);
        }
        ops.push_back({chunk_it->second, r});
      } else {
        chunks.insert({t, r});
      }
    }
    while (!pcols.empty() && i0 <= pcols.back() && pcols.back() < i1) {
      unsigned pcol = pcols.back();
      pcols.pop_back();
      for (unsigned r = 0; r < pivot_row; ++r) {
        if (m(r, pcol)) {
          for (unsigned k = 0; k < cols; ++k) {
            m(r, k) ^= m(pivot_row, k);
          }
          ops.push_back({pivot_row, r});
        }
      }
      --pivot_row;
    }
  }

  return ops;
}

static Eigen::PermutationMatrix<Eigen::Dynamic> qubit_permutation(
    unsigned n_qubits) {
  Eigen::PermutationMatrix<Eigen::Dynamic> perm(1u << n_qubits);
  for (unsigned i = 0; i < (1u << n_qubits); i++) {
    unsigned rev = 0;
    unsigned forwards = i;
    for (unsigned q = 0; q < n_qubits; q++) {
      rev <<= 1;
      rev += forwards % 2;
      forwards >>= 1;
    }
    perm.indices()[i] = rev;
  }
  return perm;
}

Eigen::PermutationMatrix<Eigen::Dynamic> lift_perm(
    const std::map<unsigned, unsigned> &p) {
  unsigned n = p.size();
  Eigen::PermutationMatrix<Eigen::Dynamic> pm(1u << n);
  for (unsigned i = 0; i < (1u << n); ++i) {
    unsigned target = 0;
    unsigned mask = 1u << n;
    for (unsigned q = 0; q < n; ++q) {
      mask /= 2;
      if (i & mask) {
        target |= 1u << (n - 1 - p.at(q));
      }
    }
    pm.indices()[i] = target;
  }
  return pm;
}

static Eigen::PermutationMatrix<Eigen::Dynamic> qubit_permutation(
    const qubit_map_t &qmap) {
  std::map<Qubit, unsigned> q_indices;
  std::map<unsigned, unsigned> uq_map;
  unsigned qi = 0;
  for (const std::pair<const Qubit, Qubit> &pair : qmap) {
    q_indices.insert({pair.first, qi});
    ++qi;
  }
  for (const std::pair<const Qubit, Qubit> &pair : qmap) {
    unsigned in = q_indices[pair.first];
    unsigned out = q_indices[pair.second];
    uq_map.insert({in, out});
  }
  return lift_perm(uq_map);
}

static const Eigen::PermutationMatrix<4, 4> SWAP = qubit_permutation(2);

Eigen::Matrix4cd reverse_indexing(const Eigen::Matrix4cd &m) {
  return SWAP * m * SWAP;
}

Matrix8cd reverse_indexing(const Matrix8cd &m) {
  return static_cast<const Matrix8cd &>(
      reverse_indexing(static_cast<const Eigen::MatrixXcd &>(m)));
}

Eigen::MatrixXcd reverse_indexing(const Eigen::MatrixXcd &m) {
  unsigned dim = m.rows();
  unsigned n_qubits = get_number_of_qubits(dim);
  Eigen::PermutationMatrix<Eigen::Dynamic> perm = qubit_permutation(n_qubits);
  return perm * m * perm;
}

Eigen::VectorXcd reverse_indexing(const Eigen::VectorXcd &v) {
  unsigned dim = v.size();
  unsigned n_qubits = get_number_of_qubits(dim);
  Eigen::PermutationMatrix<Eigen::Dynamic> perm = qubit_permutation(n_qubits);
  return perm * v;
}

Eigen::MatrixXcd apply_qubit_permutation(
    const Eigen::MatrixXcd &m, const qubit_map_t &perm) {
  Eigen::PermutationMatrix<Eigen::Dynamic> perm_m = qubit_permutation(perm);
  return perm_m * m;
}

Eigen::VectorXcd apply_qubit_permutation(
    const Eigen::VectorXcd &v, const qubit_map_t &perm) {
  Eigen::PermutationMatrix<Eigen::Dynamic> perm_m = qubit_permutation(perm);
  return perm_m * v;
}

double trace_fidelity(double a, double b, double c) {
  constexpr double g = PI / 2;
  a *= g;
  b *= g;
  c *= g;
  double trace_sq = 16. * (pow(cos(a) * cos(b) * cos(c), 2) +
                           pow(sin(a) * sin(b) * sin(c), 2));
  return (4. + trace_sq) / 20.;
}

inline double mod(double d, double max) { return d - max * floor(d / max); }

std::tuple<Eigen::Matrix4cd, std::array<double, 3>, Eigen::Matrix4cd>
get_information_content(const Eigen::Matrix4cd &X) {
  using ExpGate = std::array<double, 3>;
  using Mat4 = Eigen::Matrix4cd;
  using Vec4 = Eigen::Vector4cd;

  if (!is_unitary(X)) {
    throw std::invalid_argument(
        "Non-unitary matrix passed to get_information_content");
  }

  // change of basis for SU(2) x SU(2) -> SO(4)
  Mat4 MagicM;
  MagicM << 1, 0, 0, i_, 0, i_, 1, 0, 0, i_, -1, 0, 1, 0, 0, -i_;
  MagicM /= sqrt(2.);  // unitary

  // change to magic basis and make sure U in SU(4)
  const auto norm_X = pow(X.determinant(), 0.25);
  const Mat4 Xprime = MagicM.adjoint() * X * MagicM / norm_X;

  // Find a common eigendecomposition of Re( X'.adjoint * X' ) and Im(
  // X'.adjoint * X' ) Use pseudorandom linear comb to avoid issues with
  // multiplicities > 1
  Mat4 X2 = Xprime.transpose() * Xprime;
  // For Clifford matrix, SelfAdjointEigenSolver seems to have a higher chance
  // to produce eigenvectors that eventually lead to non-clifford angles when
  // there are rounding errors. In this case, the matrix X2 should consist of
  // 0, 1, -1, i and -i entries only, so clamp to these values.
  X2 = X2.unaryExpr([](Complex x) -> Complex {
    if (std::abs(x) < 1e-14) {
      return 0.;
    } else if (std::abs(x - 1.) < 1e-14) {
      return 1.;
    } else if (std::abs(x + 1.) < 1e-14) {
      return -1.;
    } else if (std::abs(x - i_) < 1e-14) {
      return i_;
    } else if (std::abs(x + i_) < 1e-14) {
      return -i_;
    } else {
      return x;
    }
  });
  const Eigen::Matrix4d X2real = X2.real();
  const Eigen::Matrix4d X2imag = X2.imag();
  Mat4 eigv;
  Vec4 eigs;
  double r = 0.;
  while (true) {
    r += 1 / PI;
    if (r >= 1) r -= 1;
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix4d> ces(
        r * X2real + (1 - r) * X2imag);
    eigv = ces.eigenvectors().cast<Complex>();
    eigs = (eigv.transpose() * X2 * eigv).diagonal();

    if (std::abs((X2 - eigv * eigs.asDiagonal() * eigv.adjoint()).sum()) <
        EPS) {
      // success!
      break;
    }
  }

  Eigen::Vector4d thetas =
      eigs.unaryExpr([](const Complex lambda) { return std::arg(lambda) / 2.; })
          .real();
  // make sure exp(i thetas) is in SU(4) ie Σ thetas = 0
  thetas(3) = -thetas.head<3>().sum();

  const Mat4 tmp = (-i_ * thetas).asDiagonal();
  const auto eigs_sqrt_inv = tmp.exp();

  // transform to Pauli basis exp(i k_i σ_ii)
  Eigen::Matrix4d basis_change;
  basis_change << 1, 1, -1, -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, 1, 1,
      1;  // k3 = 0 always
  Eigen::Vector3d k =
      (-.5 * basis_change * thetas / PI).head<3>().unaryExpr([](double d) {
        return mod(d, 4);
      });

  Mat4 Q2 = eigv.transpose();
  // we need to ensure that det(Q) == 1 so that K1,K2 \in SU(2) x SU(2)
  if (Q2.determinant().real() < 0) {
    Q2.row(3) = -Q2.row(3);
  }
  Mat4 Q1 = Xprime * Q2.transpose() * eigs_sqrt_inv;

  // we now have Xprime = Q1 * exp(i thetas) * Q2
  // with Q1,Q2 in SO(4), exp(i thetas) in SU(4)

  // these are our local operations (left and right)
  Mat4 K1 = MagicM * Q1 * MagicM.adjoint();
  Mat4 K2 = MagicM * Q2 * MagicM.adjoint();

  // fix phase
  K1 *= norm_X;

  // Finally, we got our ks
  ExpGate A{k(0), k(1), k(2)};

  // K1,K2 in SU(2)xSU(2), A = Exp(i aσ_XX + i bσ_YY + i cσ_ZZ)
  return std::tuple<Mat4, ExpGate, Mat4>{K1, A, K2};
}

std::pair<Eigen::Matrix2cd, Eigen::Matrix2cd> kronecker_decomposition(
    Eigen::Matrix4cd &U) {
  using Mat4 = Eigen::Matrix4cd;
  Mat4 Up = U / pow(U.determinant(), 0.25);
  Mat4 U_mod;
  U_mod << Up(0, 0), Up(1, 0), Up(0, 1), Up(1, 1), Up(2, 0), Up(3, 0), Up(2, 1),
      Up(3, 1), Up(0, 2), Up(1, 2), Up(0, 3), Up(1, 3), Up(2, 2), Up(3, 2),
      Up(2, 3), Up(3, 3);
  Eigen::JacobiSVD<Mat4, Eigen::NoQRPreconditioner> svd(
      U_mod, Eigen::DecompositionOptions::ComputeFullU |
                 Eigen::DecompositionOptions::ComputeFullV);
  Eigen::Vector4cd sings = svd.singularValues();
  Mat4 l = svd.matrixU();
  Mat4 r = svd.matrixV().conjugate();
  Eigen::Matrix2cd u0, u1;
  u0 << l(0, 0), l(2, 0), l(1, 0), l(3, 0);
  u1 << r(0, 0), r(2, 0), r(1, 0), r(3, 0);
  u0 *= sqrt(sings[0]);
  u1 *= sqrt(sings[0]);
  return {u0, u1};
}

unsigned get_matrix_size(unsigned number_of_qubits) {
  // Left-shifting by the number of bits stored by the int type,
  // or more, is UNDEFINED by the C++ standard!
  // It does NOT necessarily give zero!
  if (number_of_qubits < std::numeric_limits<unsigned>::digits) {
    unsigned size = 1;
    size <<= number_of_qubits;
    return size;
  }
  std::stringstream ss;
  ss << "get_matrix_size for " << number_of_qubits << " qubits; overflow!";
  throw std::runtime_error(ss.str());
}

unsigned get_number_of_qubits(unsigned matrix_size) {
  unsigned n_qubits = (unsigned)log2(matrix_size);
  if (get_matrix_size(n_qubits) == matrix_size) {
    return n_qubits;
  }
  std::stringstream ss;
  ss << "get_number_of_qubits: matrix size " << matrix_size
     << " is not a power of two";
  throw std::runtime_error(ss.str());
}

SparseMatrixXcd get_sparse_matrix(
    const std::vector<TripletCd> &triplets, unsigned rows, unsigned cols) {
  SparseMatrixXcd matr(rows, cols);
  matr.setFromTriplets(triplets.cbegin(), triplets.cend());
  return matr;
}

SparseMatrixXcd get_sparse_square_matrix(
    const std::vector<TripletCd> &triplets, unsigned rows) {
  return get_sparse_matrix(triplets, rows, rows);
}

std::vector<TripletCd> get_triplets(
    const SparseMatrixXcd &matr, double abs_epsilon) {
  std::vector<TripletCd> result;
  for (unsigned kk = 0; kk < matr.outerSize(); ++kk) {
    for (SparseMatrixXcd::InnerIterator it(matr, kk); it; ++it) {
      if (std::abs(it.value()) > abs_epsilon) {
        const unsigned row = it.row();
        const unsigned col = it.col();
        result.emplace_back(row, col, it.value());
      }
    }
  }
  return result;
}

std::vector<TripletCd> get_triplets(
    const Eigen::MatrixXcd &matr, double abs_epsilon) {
  std::vector<TripletCd> triplets;
  for (unsigned jj = 0; jj < matr.cols(); ++jj) {
    for (unsigned ii = 0; ii < matr.rows(); ++ii) {
      if (!(std::abs(matr(ii, jj)) <= abs_epsilon)) {
        triplets.emplace_back(ii, jj, matr(ii, jj));
      }
    }
  }
  return triplets;
}

bool in_weyl_chamber(const std::array<Expr, 3> &k) {
  bool is_symbolic = true;
  double last_val = .5;
  for (unsigned i = 0; i < k.size(); ++i) {
    std::optional<double> eval = eval_expr_mod(k[i], 4);
    if (eval) {
      is_symbolic = false;
      if (i + 1 == k.size()) {
        double abs_eval = std::min(*eval, -(*eval) + 4);
        if (abs_eval - last_val > EPS) {
          return false;
        }
      } else {
        if (*eval - last_val > EPS) {
          return false;
        }
      }
      last_val = *eval;
    } else if (!is_symbolic) {
      return false;
    }
  }
  return true;
}

Eigen::Matrix2cd nth_root(const Eigen::Matrix2cd &u, unsigned long long n) {
  if (u.isApprox(Eigen::Matrix2cd::Identity(), EPS)) {
    return Eigen::Matrix2cd::Identity();
  } else if (n == 0) {
    throw std::invalid_argument("Non-identity matrix does not have a 0th root");
  }
  Eigen::ComplexEigenSolver<Eigen::Matrix2cd> eigen_solver(u);
  return std::pow(eigen_solver.eigenvalues()[0], 1. / n) *
             eigen_solver.eigenvectors().col(0) *
             eigen_solver.eigenvectors().col(0).adjoint() +
         std::pow(eigen_solver.eigenvalues()[1], 1. / n) *
             eigen_solver.eigenvectors().col(1) *
             eigen_solver.eigenvectors().col(1).adjoint();
}

}  // namespace tket
