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

#include <optional>
#include <unordered_set>

#include "tket/Utils/Expression.hpp"
#include "tket/ZX/Types.hpp"

namespace tket {

namespace zx {

enum class ZXType {
  /**
   * Boundary vertices
   */
  // Input boundary
  Input,

  // Output boundary
  Output,

  // Open boundary (not specified as input or output)
  Open,

  /**
   * Symmetric generators
   */
  // Z (green) spider
  // Equivalently, a (postselected) XY qubit (with negative phase) in MBQC
  ZSpider,

  // X (red) spider
  XSpider,

  // Hbox
  Hbox,

  // A (postselected) XY qubit in MBQC
  // Corresponds to a Z spider with negative phase
  XY,

  // A (postselected) XZ qubit in MBQC
  // Corresponds to a 0.5-phase (n+1)-ary Z spider connected to a phaseful 1-ary
  // X spider
  XZ,

  // A (postselected) YZ qubit in MBQC
  // Corresponds to a 0-phase (n+1)-ary Z spider connected to a phaseful 1-ary X
  // spider
  YZ,

  // A (postselected) Pauli X qubit in MBQC
  // Corresponds to a Z spider with phase either 0 (param=False) or 1
  // (param=True)
  // Also used for unmeasured vertices in MBQC diagrams (connected to an Output
  // via a Basic wire)
  PX,

  // A (postselected) Pauli Y qubit in MBQC
  // Corresponds to a Z spider with phase either -0.5 (param=False) or +0.5
  // (param=True)
  PY,

  // A (postselected) Pauli Z qubit in MBQC
  // Corresponds to a 0-phase (n+1)-ary Z spider connected to a 1-ary X spider
  // with phase either 0 (param=False) or 1 (param=True)
  PZ,

  /**
   * Directed (non-commutative) generators
   */
  // Triangle [[1, 1], [0, 1]]
  // Port 0 is the apex/input of the triangle, port 1 is the flat/output of the
  // triangle
  Triangle,

  /**
   * Boxes
   */
  // Abstraction of an inner ZX diagram
  ZXBox
};

typedef std::unordered_set<ZXType> ZXTypeSet;

/**
 * Helper functions to identify various groups of ZXTypes
 */
bool is_boundary_type(ZXType type);
bool is_basic_gen_type(ZXType type);
bool is_spider_type(ZXType type);
bool is_directed_type(ZXType type);
bool is_MBQC_type(ZXType type);
bool is_phase_type(ZXType type);
bool is_Clifford_gen_type(ZXType type);

// Forward declaration so we can use ZXGen_ptr in the interface of ZXGen
class ZXGen;
typedef std::shared_ptr<const ZXGen> ZXGen_ptr;

/**
 * Abstract class for a ZX generator.
 * Each ZXType has a single possible subclass that can realise it, allowing us
 * to statically cast to a subclass once that is determined. Treatment of ports
 * and QuantumType is handled by each subclass.
 */
class ZXGen {
 public:
  ZXType get_type() const;

  /**
   * Return the quantum type of the generator, if it is definable.
   * It may not be definable for directed generators that mix types of different
   * ports, such as ZXBox.
   *
   * What this means might be context dependent. Generally, we say that this is
   * the expected quantum type of every incident edge. However, Classical
   * BasicGen objects (spiders, HBox) can accept Quantum edges which are treated
   * as a pair of edges.
   */
  virtual std::optional<QuantumType> get_qtype() const = 0;

  /**
   * Returns whether or not an edge of a given QuantumType can validly be placed
   * on the given port.
   */
  virtual bool valid_edge(
      std::optional<unsigned> port, QuantumType qtype) const = 0;

  // Set of all free symbols occurring in operation parameters
  virtual SymSet free_symbols() const = 0;

  /**
   * Operation with values substituted for symbols
   *
   * @param sub_map map from symbols to values
   *
   * @return New operation with symbols substituted, or a null pointer if it is
   * unchanged
   */
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const = 0;

  /**
   * Outputs a string-based description for the generator.
   * This should be enough to specify the generator exactly.
   *
   * @param latex whether the string should be formatted for LaTeX math mode or
   * plaintext
   */
  virtual std::string get_name(bool latex = false) const = 0;

  /**
   * Implementation of equivalence checking to eliminate ambiguous warnings
   * for operator==.
   *
   * Assumes other matches the ZXType of this, so it is safe to cast.
   */
  virtual bool is_equal(const ZXGen& other) const;

  bool operator==(const ZXGen& other) const;

  virtual ~ZXGen();

  /**
   * Generic constructors for obtaining generators with more generality than
   * going via subtype constructors.
   */
  static ZXGen_ptr create_gen(
      ZXType type, QuantumType qtype = QuantumType::Quantum);
  static ZXGen_ptr create_gen(
      ZXType type, Expr param, QuantumType qtype = QuantumType::Quantum);
  static ZXGen_ptr create_gen(
      ZXType type, bool param, QuantumType qtype = QuantumType::Quantum);

 protected:
  ZXGen(ZXType type);
  const ZXType type_;
};

/**
 * Implementation of ZXGen for boundary vertices.
 * Each vertex must have degree 1.
 * `std::nullopt` is used for ports as there is no need to distinguish.
 * The adjacent wire must have the same QuantumType as the boundary.
 * The only variation between boundaries is the ZXType and QuantumType.
 */
class BoundaryGen : public ZXGen {
 public:
  BoundaryGen(ZXType type, QuantumType qtype);

  // Overrides from ZXGen
  virtual std::optional<QuantumType> get_qtype() const override;
  virtual bool valid_edge(
      std::optional<unsigned> port, QuantumType qtype) const override;
  virtual SymSet free_symbols() const override;
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  virtual std::string get_name(bool latex = false) const override;
  virtual bool is_equal(const ZXGen& other) const override;

 protected:
  const QuantumType qtype_;
};

/**
 * Virtual subclass of ZXGen for undirected (commutative) generators.
 * `std::nullopt` is used for ports as there is no need to distinguish.
 * If the generator is Quantum, all adjacent wires must also be Quantum.
 * If the generator is Classical, adjacent wires can be either Quantum or
 * Classical.
 * Implementations include PhasedGen for generators with 1 Expr parameter or
 * CliffordGen for Clifford generators with 1 bool parameter.
 */
class BasicGen : public ZXGen {
 public:
  BasicGen(ZXType type, QuantumType qtype = QuantumType::Quantum);

  // Overrides from ZXGen
  virtual std::optional<QuantumType> get_qtype() const override;
  virtual bool valid_edge(
      std::optional<unsigned> port, QuantumType qtype) const override;
  virtual bool is_equal(const ZXGen& other) const override;

 protected:
  const QuantumType qtype_;
};

/**
 * Implementation of BasicGen for phased generators, e.g. spiders, Hbox.
 * Each generator has a single Expr parameter which is:
 * - A complex number for Hbox
 * - A real-valued phase in half-turns otherwise
 */
class PhasedGen : public BasicGen {
 public:
  PhasedGen(
      ZXType type, const Expr& param, QuantumType qtype = QuantumType::Quantum);

  Expr get_param() const;

  // Overrides from ZXGen
  virtual SymSet free_symbols() const override;
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  virtual std::string get_name(bool latex = false) const override;
  virtual bool is_equal(const ZXGen& other) const override;

 protected:
  const Expr param_;
};

/**
 * Implementation of BasicGen for Clifford generators.
 * The basis is determined by the ZX type, and the boolean parameter determines
 * the discrete phase (false = 0 versus true = 1 half-turn).
 */
class CliffordGen : public BasicGen {
 public:
  CliffordGen(
      ZXType type, bool param, QuantumType qtype = QuantumType::Quantum);

  bool get_param() const;

  // Overrides from ZXGen
  virtual SymSet free_symbols() const override;
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  virtual std::string get_name(bool latex = false) const override;
  virtual bool is_equal(const ZXGen& other) const override;

 protected:
  const bool param_;
};

/**
 * Virtual subclass of ZXGen for directed (non-commutative) generators.
 * The generator has a pre-determined number of ports labelled from 0 to
 * n_ports-1. Each port has a pre-determined QuantumType, captured by the
 * signature. There must be exactly one incident edge for each port and it must
 * match the corresponding QuantumType. Implementations include both actual
 * generators (e.g. Triangle) and Box types
 */
class ZXDirected : public ZXGen {
 public:
  virtual unsigned n_ports() const = 0;
  virtual std::vector<QuantumType> get_signature() const = 0;

 protected:
  ZXDirected(ZXType type);
};

/**
 * Implementation of ZXDirected for actual generators in the prop setting (e.g.
 * Triangle). The number of ports is dictated by the ZXType. Generators can be
 * constructed as either QuantumType with all ports having the same type.
 */
class DirectedGen : public ZXDirected {
 public:
  DirectedGen(ZXType type, QuantumType qtype);

  // Overrides from ZXGen
  virtual std::optional<QuantumType> get_qtype() const override;
  virtual bool valid_edge(
      std::optional<unsigned> port, QuantumType qtype) const override;
  virtual SymSet free_symbols() const override;
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  virtual std::string get_name(bool latex = false) const override;
  virtual bool is_equal(const ZXGen& other) const override;

  // Overrides from ZXDirected
  virtual unsigned n_ports() const override;
  virtual std::vector<QuantumType> get_signature() const override;

 protected:
  const QuantumType qtype_;
};

/**
 * Implementation of ZXDirected for Box abstractions.
 * The number of ports is dictated by the inner ZXDiagram.
 * Ports iterate through inputs first, then outputs [i0, ..., in, o0, ..., om].
 */
// Forward declaration of ZXDiagram
class ZXDiagram;
class ZXBox : public ZXDirected {
 public:
  ZXBox(const ZXDiagram& diag);

  std::shared_ptr<const ZXDiagram> get_diagram() const;

  // Overrides from ZXGen
  virtual std::optional<QuantumType> get_qtype() const override;
  virtual bool valid_edge(
      std::optional<unsigned> port, QuantumType qtype) const override;
  virtual SymSet free_symbols() const override;
  virtual ZXGen_ptr symbol_substitution(
      const SymEngine::map_basic_basic& sub_map) const override;
  virtual std::string get_name(bool latex = false) const override;
  virtual bool is_equal(const ZXGen& other) const override;

  // Overrides from ZXDirected
  virtual unsigned n_ports() const override;
  virtual std::vector<QuantumType> get_signature() const override;

 protected:
  const std::shared_ptr<const ZXDiagram> diag_;
};

}  // namespace zx

}  // namespace tket
