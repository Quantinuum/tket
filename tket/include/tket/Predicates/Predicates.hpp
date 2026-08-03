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
#include <string>
#include <typeindex>

#include "tket/Architecture/Architecture.hpp"
#include "tket/Circuit/Circuit.hpp"

#include "tket_export.h"

namespace tket {
class Predicate;

typedef std::shared_ptr<Predicate> PredicatePtr;

JSON_DECL_EXPORT(PredicatePtr)

class IncorrectPredicate : public std::logic_error {
 public:
  explicit IncorrectPredicate(const std::string& exception_string)
      : std::logic_error(exception_string) {}
};

class UnsatisfiedPredicate : public std::logic_error {
 public:
  explicit UnsatisfiedPredicate(const std::string& pred_name)
      : std::logic_error(
            "Predicate requirements are not satisfied: " + pred_name) {}
};

class PredicateNotSerializable : public std::logic_error {
 public:
  explicit PredicateNotSerializable(const std::string& pred_name)
      : std::logic_error("Predicate not serializable: " + pred_name) {}
};

const std::string& predicate_name(std::type_index idx);

/////////////////////
// PREDICATE CLASSES//
/////////////////////

// abstract interface class
class TKET_EXPORT Predicate {
 public:
  virtual bool verify(const Circuit& circ) const = 0;

  // implication currently only works between predicates of the same subclass
  virtual bool implies(const Predicate& other) const = 0;
  virtual PredicatePtr meet(const Predicate& other) const = 0;
  virtual std::string to_string() const = 0;
  virtual ~Predicate() {};  // satisfy compiler
};

// all Predicate subclasses must inherit from `Predicate`

/**
 * Asserts that all operations are in the specified set of types.
 *
 * Note that the following are always permitted and do not need to be included
 * in the permitted set:
 * - "meta" operations (inputs, outputs, barriers);
 * - OpType::Phase gates (which have no input or output wires).
 *
 * Classically conditioned operations are permitted provided that the
 * conditional operation is of a permitted type.
 */
class TKET_EXPORT GateSetPredicate : public Predicate {
 public:
  explicit GateSetPredicate(const OpTypeSet& allowed_types)
      : allowed_types_(allowed_types) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;

  std::string to_string() const override;
  const OpTypeSet& get_allowed_types() const { return allowed_types_; }

 private:
  const OpTypeSet allowed_types_;
};

/**
 * Asserts that there are no conditional gates in the circuit.
 */
class TKET_EXPORT NoClassicalControlPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT NoFastFeedforwardPredicate : public Predicate {
  // verifies Circuit has no classical bits which are written from quantum gates
  // and then read in later in the Circuit
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT NoClassicalBitsPredicate : public Predicate {
  // this verifies that the Circuit uses no classical bits
  // (read or write -- so no measures and no classical controls)
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT NoWireSwapsPredicate : public Predicate {
  /**
   * Verifies that you can follow the paths of each qubit/bit in the circuit
   * and finish on the same qubit/bit you started with
   */
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT MaxTwoQubitGatesPredicate : public Predicate {
  // this verifies that the Circuit uses no gates with greater than 2 qubits
  // Barriers are ignored
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT PlacementPredicate : public Predicate {
 public:
  explicit PlacementPredicate(const Architecture& arch)
      : nodes_(arch.nodes()) {}
  explicit PlacementPredicate(const node_set_t& nodes) : nodes_(nodes) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
  const node_set_t& get_nodes() const { return nodes_; }

 private:
  const node_set_t nodes_;
};

class TKET_EXPORT ConnectivityPredicate : public Predicate {
 public:
  explicit ConnectivityPredicate(const Architecture& arch) : arch_(arch) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
  const Architecture& get_arch() const { return arch_; }

 private:
  const Architecture arch_;
};

class TKET_EXPORT DirectednessPredicate : public Predicate {
 public:
  explicit DirectednessPredicate(const Architecture& arch) : arch_(arch) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
  const Architecture& get_arch() const { return arch_; }

 private:
  const Architecture arch_;
};

class TKET_EXPORT CliffordCircuitPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT UserDefinedPredicate : public Predicate {
 public:
  explicit UserDefinedPredicate(const std::function<bool(const Circuit&)>& func)
      : func_(func) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate&) const override;
  PredicatePtr meet(const Predicate&) const override;
  std::string to_string() const override;

 private:
  const std::function<bool(const Circuit&)> func_;
};

class TKET_EXPORT DefaultRegisterPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

class TKET_EXPORT MaxNQubitsPredicate : public Predicate {
 public:
  explicit MaxNQubitsPredicate(unsigned n_qubits) : n_qubits_(n_qubits) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
  unsigned get_n_qubits() const { return n_qubits_; }

 private:
  const unsigned n_qubits_;
};

/**
 * Asserts that the circuit only contains N classical registers or less
 */
class TKET_EXPORT MaxNClRegPredicate : public Predicate {
 public:
  explicit MaxNClRegPredicate(unsigned _n_cl_reg) : n_cl_reg_(_n_cl_reg) {}
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
  unsigned get_n_cl_reg() const { return n_cl_reg_; }

 private:
  const unsigned n_cl_reg_;
};

/**
 * Asserts that the circuit contains no \ref OpType::Barrier
 */
class TKET_EXPORT NoBarriersPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

/**
 * Asserts that any internal measurements can be commuted to the end of the
 * circuit
 */
class TKET_EXPORT CommutableMeasuresPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

/**
 * Asserts that any measurements occur at the end of the circuit
 */
class TKET_EXPORT NoMidMeasurePredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

/**
 * Asserts that no gates in the circuit have symbolic parameters
 */
class TKET_EXPORT NoSymbolsPredicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

/**
 * Asserts that all TK2 gates are normalised
 *
 * A gate TK2(a, b, c) is considered normalised if
 *
 *  - If all expressions are non symbolic, then it must hold
 *    `0.5 ≥ a ≥ b ≥ |c|`.
 *  - In the ordering (a, b, c), any symbolic expression must appear before
 *    non-symbolic ones. The remaining non-symbolic expressions must still be
 *    ordered in non-increasing order and must be in the interval [0, 1/2],
 *    with the exception of the last one that may be in [-1/2, 1/2].
 */
class TKET_EXPORT NormalisedTK2Predicate : public Predicate {
 public:
  bool verify(const Circuit& circ) const override;
  bool implies(const Predicate& other) const override;
  PredicatePtr meet(const Predicate& other) const override;
  std::string to_string() const override;
};

}  // namespace tket
