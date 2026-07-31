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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <optional>

#include "tket/Circuit/Simulation/CircuitSimulator.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/OpType/OpTypeInfo.hpp"
#include "tket/Ops/Op.hpp"
#include "tket/Utils/Json.hpp"
#include "tket/Utils/MatrixAnalysis.hpp"
#include "tket/Utils/UnitID.hpp"

#include "tket_export.h"

namespace tket {

class Circuit;

/**
 * Abstract class for an operation from which a circuit can be extracted
 */
class TKET_EXPORT Box : public Op {
 public:
  explicit Box(const OpType &type, const op_signature_t &signature = {})
      : Op(type), signature_(signature), circ_(), id_(idgen()) {
    if (!is_box_type(type)) throw BadOpType(type);
  }

  Box(const Box &other)
      : Op(other.get_type()),
        signature_(other.signature_),
        circ_(other.circ_),
        id_(other.id_) {}

  /** Number of Quantum inputs */
  unsigned n_qubits() const override;

  /** Number of Boolean inputs */
  unsigned n_boolean() const;

  /** Number of Classical inputs */
  unsigned n_classical() const;

  op_signature_t get_signature() const override;

  nlohmann::json serialize() const override;

  static Op_ptr deserialize(const nlohmann::json &j);

  /** Circuit represented by box */
  std::shared_ptr<Circuit> to_circuit() const {
    if (circ_ == nullptr) generate_circuit();
    return circ_;
  };

  /**
   * If meaningful and implemented, return the numerical unitary matrix
   * (in ILO-BE convention) which this Box represents.
   *
   * @return unitary matrix (ILO-BE) which this Box represents
   */
  virtual std::optional<Eigen::MatrixXcd> get_box_unitary() const {
    return std::nullopt;
  }

  Eigen::MatrixXcd get_unitary() const override {
    std::optional<Eigen::MatrixXcd> u = get_box_unitary();
    if (u.has_value()) {
      return *u;
    }
    return tket_sim::get_unitary(*to_circuit());
  }

  /** Unique identifier (preserved on copy) */
  boost::uuids::uuid get_id() const { return id_; }

  template <typename BoxT>
  friend Op_ptr set_box_id(BoxT &b, boost::uuids::uuid newid);

 protected:
  static boost::uuids::uuid idgen() {
    static boost::uuids::random_generator gen = {};

    return gen();
  }
  op_signature_t signature_;
  mutable std::shared_ptr<Circuit> circ_;
  boost::uuids::uuid id_;

  virtual void generate_circuit() const = 0;
};

// json for base Box attributes
nlohmann::json core_box_json(const Box &box);

/**
 * @brief Set explicit ID on a box
 *
 * This is used for deserialization.
 *
 * @param b box
 * @param[in] newid new ID
 *
 * @tparam BoxT concrete box type
 *
 * @return box with desired ID
 */
template <typename BoxT>
Op_ptr set_box_id(BoxT &b, boost::uuids::uuid newid) {
  b.id_ = newid;
  return std::make_shared<BoxT>(b);
}

/**
 * Operation defined as a circuit.
 */
class TKET_EXPORT CircBox : public Box {
 public:
  /**
   * Construct from a given circuit.
   */
  explicit CircBox(const Circuit &circ);

  /**
   * Construct from the empty circuit
   */
  CircBox();

  /**
   * Copy constructor
   */
  CircBox(const CircBox &other);

  ~CircBox() override {}

  bool is_clifford() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  void symbol_substitution_in_place(const symbol_map_t &sub_map);

  SymSet free_symbols() const override;

  /**
   * Equality check between two CircBox instances
   */
  bool is_equal(const Op &op_other) const override;

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

  /**
   * Get the name of the inner circuit.
   *
   * @return name
   */
  std::optional<std::string> get_circuit_name() const;

  /**
   * Set the name of the inner circuit
   *
   * @param _name name string
   */
  void set_circuit_name(const std::string _name);

 protected:
  void generate_circuit() const override {}  // Already set by constructor
};

/**
 * One-qubit operation defined as a unitary matrix
 */
class TKET_EXPORT Unitary1qBox : public Box {
 public:
  /**
   * Construct from a given 2x2 unitary matrix
   *
   * @param m unitary matrix
   */
  explicit Unitary1qBox(const Eigen::Matrix2cd &m);

  /**
   * Construct from the identity matrix
   */
  Unitary1qBox();

  /**
   * Copy constructor
   */
  Unitary1qBox(const Unitary1qBox &other);

  ~Unitary1qBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<Unitary1qBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two Unitary1qBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the unitary matrix corresponding to this operation */
  Eigen::Matrix2cd get_matrix() const { return m_; }

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override {
    return m_;
  }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  bool is_clifford() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  const Eigen::Matrix2cd m_;
};

/**
 * Two-qubit operation defined as a unitary matrix (ILO-BE)
 */
class TKET_EXPORT Unitary2qBox : public Box {
 public:
  /**
   * Construct from a given 4x4 unitary matrix
   *
   * @param m unitary matrix
   * @param basis basis order convention for matrix
   */
  explicit Unitary2qBox(
      const Eigen::Matrix4cd &m, BasisOrder basis = BasisOrder::ilo);

  /**
   * Construct from the identity matrix
   */
  Unitary2qBox();

  /**
   * Copy constructor
   */
  Unitary2qBox(const Unitary2qBox &other);

  ~Unitary2qBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<Unitary2qBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two Unitary2qBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the unitary matrix corresponding to this operation */
  Eigen::Matrix4cd get_matrix() const { return m_; }

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override {
    return m_;
  }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  const Eigen::Matrix4cd m_;
};

/**
 * Three-qubit operation defined as a unitary matrix (ILO-BE)
 */
class TKET_EXPORT Unitary3qBox : public Box {
 public:
  /**
   * Construct from a given 8x8 unitary matrix
   *
   * @param m unitary matrix
   * @param basis basis order convention for matrix
   */
  explicit Unitary3qBox(const Matrix8cd &m, BasisOrder basis = BasisOrder::ilo);

  /**
   * Construct from the identity matrix
   */
  Unitary3qBox();

  /**
   * Copy constructor
   */
  Unitary3qBox(const Unitary3qBox &other);

  ~Unitary3qBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<Unitary3qBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two Unitary3qBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the unitary matrix corresponding to this operation */
  Matrix8cd get_matrix() const { return m_; }

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override {
    return m_;
  }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  const Matrix8cd m_;
};

/**
 * Two-qubit operation defined in terms of a hermitian matrix and a phase.
 *
 * The unitary corresponding to the matrix A and phase t is exp(i*t*A).
 * Matrix A is stored in ILO-BE form.
 */
class TKET_EXPORT ExpBox : public Box {
 public:
  /**
   * Construct from a given 4x4 hermitian matrix and optional phase.
   *
   * @param A hermitian matrix
   * @param t phase to apply
   * @param basis basis order convention for matrix
   */
  ExpBox(
      const Eigen::Matrix4cd &A, double t = 1.,
      BasisOrder basis = BasisOrder::ilo);

  /**
   * Construct from the zero matrix (resulting in the identity)
   */
  ExpBox();

  /**
   * Copy constructor
   */
  ExpBox(const ExpBox &other);

  ~ExpBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<ExpBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two ExpBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the hermitian matrix and phase parameter */
  std::pair<Eigen::Matrix4cd, double> get_matrix_and_phase() const {
    return std::make_pair(A_, t_);
  }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  const Eigen::Matrix4cd A_;
  double t_;
};

class TKET_EXPORT CompositeGateDef;
typedef std::shared_ptr<CompositeGateDef> composite_def_ptr_t;

// CompositeGateDef
JSON_DECL(composite_def_ptr_t)

class CompositeGateDef : public std::enable_shared_from_this<CompositeGateDef> {
 public:
  CompositeGateDef(
      const std::string &name, const Circuit &def,
      const std::vector<Sym> &args);

  static composite_def_ptr_t define_gate(
      const std::string &name, const Circuit &def,
      const std::vector<Sym> &args);

  Circuit instance(const std::vector<Expr> &params) const;

  std::string get_name() const { return name_; }
  std::vector<Sym> get_args() const { return args_; }
  std::shared_ptr<Circuit> get_def() const { return def_; }
  unsigned n_args() const { return args_.size(); }
  op_signature_t signature() const;

  bool operator==(const CompositeGateDef &other) const;

 private:
  std::string name_;
  std::shared_ptr<Circuit> def_;
  std::vector<Sym> args_;

  CompositeGateDef() {}
};

class TKET_EXPORT CustomGate : public Box {
 public:
  CustomGate(const composite_def_ptr_t &gate, const std::vector<Expr> &params);
  CustomGate(const CustomGate &other);

  SymSet free_symbols() const override;

  composite_def_ptr_t get_gate() const { return gate_; }
  std::vector<Expr> get_params() const override { return params_; }
  std::string get_name(bool latex = false) const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  /**
   * Equality check between two CustomGate instances.
   * This does more than simply checking id_.
   */
  bool is_equal(const Op &op_other) const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

  bool is_clifford() const override;

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

 protected:
  void generate_circuit() const override;
  CustomGate() : Box(OpType::CustomGate), gate_(), params_() {}

 private:
  composite_def_ptr_t gate_;
  const std::vector<Expr> params_;
};

/**
 * Wraps another quantum op, adding control qubits
 */
class TKET_EXPORT QControlBox : public Box {
 public:
  /**
   * Construct from a given op and number of controls
   *
   * @param op op to control
   * @param n_controls number of qubit controls to add
   * @param control_state control state expressed as a bit vector.
   * If control_state is non-empty, its size should match n_controls.
   * An empty vector is converted to an all-1s vector of length n_controls.
   */
  explicit QControlBox(
      const Op_ptr &op, unsigned n_controls = 1,
      const std::vector<bool> &control_state = {});

  /**
   * Copy constructor
   */
  QControlBox(const QControlBox &other);

  ~QControlBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two QControlBox instances
   */
  bool is_equal(const Op &op_other) const override;

  std::string get_command_str(const unit_vector_t &args) const override;

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override;

  Op_ptr get_op() const { return op_; }
  unsigned get_n_controls() const { return n_controls_; }
  std::vector<bool> get_control_state() const { return control_state_; }

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;
  QControlBox()
      : Box(OpType::QControlBox),
        op_(),
        n_controls_(0),
        n_inner_qubits_(0),
        control_state_() {}

 private:
  const Op_ptr op_;
  const unsigned n_controls_;
  unsigned n_inner_qubits_;
  const std::vector<bool> control_state_;
};

class TKET_EXPORT ProjectorAssertionBox : public Box {
 public:
  /**
   * Construct from a given 2x2, 4x4 or 8x8 projector matrix
   *
   * @param m projector matrix
   * @param basis basis order convention for matrix
   */
  explicit ProjectorAssertionBox(
      const Eigen::MatrixXcd &m, BasisOrder basis = BasisOrder::ilo);

  /**
   * Copy constructor
   */
  ProjectorAssertionBox(const ProjectorAssertionBox &other);

  ~ProjectorAssertionBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<ProjectorAssertionBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two ProjectorAssertionBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the unitary matrix corresponding to this operation */
  Eigen::MatrixXcd get_matrix() const { return m_; }
  std::vector<bool> get_expected_readouts() const { return expected_readouts_; }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  op_signature_t get_signature() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  const Eigen::MatrixXcd m_;
  // expected readouts the debug bits
  // false -> 0
  // true -> 1
  mutable std::vector<bool> expected_readouts_;
};

class TKET_EXPORT StabiliserAssertionBox : public Box {
 public:
  /**
   * Construct from a set of stabiliser Pauli strings
   *
   * @param paulis a set of stabiliser Pauli strings
   */
  explicit StabiliserAssertionBox(const PauliStabiliserVec &paulis);

  /**
   * Copy constructor
   */
  StabiliserAssertionBox(const StabiliserAssertionBox &other);

  ~StabiliserAssertionBox() override {}
  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<StabiliserAssertionBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two StabiliserAssertionBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the pauli stabilisers */
  PauliStabiliserVec get_stabilisers() const { return paulis_; }
  std::vector<bool> get_expected_readouts() const { return expected_readouts_; }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

  op_signature_t get_signature() const override;

 protected:
  void generate_circuit() const override;

 private:
  const PauliStabiliserVec paulis_;
  // expected readouts the debug bits
  // false -> 0
  // true -> 1
  mutable std::vector<bool> expected_readouts_;
};
}  // namespace tket
