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
 * @brief Named registers of arrays of (quantum or classical) nodes
 */

#include <boost/bimap.hpp>
#include <boost/functional/hash.hpp>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <tklog/TketLog.hpp>

#include "Json.hpp"

#include "tket_export.h"

namespace tket {

/** Type of information held */
enum class UnitType { Qubit, Bit, WasmState, RngState };

/** The type and dimension of a register */
typedef std::pair<UnitType, unsigned> register_info_t;

typedef std::optional<register_info_t> opt_reg_info_t;

TKET_EXPORT const std::string &q_default_reg();
TKET_EXPORT const std::string &q_routing_ancilla_reg();
TKET_EXPORT const std::string &c_default_reg();
TKET_EXPORT const std::string &w_default_reg();
TKET_EXPORT const std::string &r_default_reg();
TKET_EXPORT const std::string &node_default_reg();
TKET_EXPORT const std::string &c_debug_zero_prefix();
TKET_EXPORT const std::string &c_debug_one_prefix();
TKET_EXPORT const std::string &c_debug_default_name();
TKET_EXPORT const std::string &c_permutation_scratch_name();

/** Conversion invalid */
class InvalidUnitConversion : public std::logic_error {
 public:
  InvalidUnitConversion(const std::string &name, const std::string &new_type)
      : std::logic_error("Cannot convert " + name + " to " + new_type) {}
};

/**
 * Location holding a bit or qubit of information
 *
 * Each location has a name (signifying the 'register' to which it belongs) and
 * an index within that register (which may be multi-dimensional).
 */
class TKET_EXPORT UnitID {
 public:
  UnitID() : data_(std::make_shared<UnitData>()) {}

  /** String representation including name and index */
  std::string repr() const;

  /** Register name */
  std::string reg_name() const { return data_->name_; }

  /** Index dimension */
  unsigned reg_dim() const { return data_->index_.size(); }

  /** Index */
  std::vector<unsigned> index() const { return data_->index_; }

  /** Unit type */
  UnitType type() const { return data_->type_; }

  /** Register dimension and type */
  register_info_t reg_info() const { return {type(), reg_dim()}; }

  bool operator<(const UnitID &other) const {
    int n = data_->name_.compare(other.data_->name_);
    if (n > 0) return false;
    if (n < 0) return true;
    return data_->index_ < other.data_->index_;
  }
  bool operator>(const UnitID &other) const { return other < *this; }
  bool operator==(const UnitID &other) const {
    return (this->data_->name_ == other.data_->name_) &&
           (this->data_->index_ == other.data_->index_);
  }
  bool operator!=(const UnitID &other) const { return !(*this == other); }

  friend std::size_t hash_value(UnitID const &unitid) {
    std::size_t seed = 0;
    boost::hash_combine(seed, unitid.data_->name_);
    boost::hash_combine(seed, unitid.data_->index_);
    boost::hash_combine(seed, unitid.data_->type_);
    return seed;
  }

 protected:
  UnitID(
      const std::string &name, const std::vector<unsigned> &index,
      UnitType type)
      : data_(std::make_shared<UnitData>(name, index, type)) {}

 private:
  struct UnitData {
    std::string name_;
    std::vector<unsigned> index_;
    UnitType type_;

    UnitData() : name_(), index_(), type_(UnitType::Qubit) {}
    UnitData(
        const std::string &name, const std::vector<unsigned> &index,
        UnitType type)
        : name_(name), index_(index), type_(type) {
      static const std::string id_regex_str = "[a-z][A-Za-z0-9_]*";
      static const std::regex id_regex(id_regex_str);
      if (!name.empty() && !std::regex_match(name, id_regex)) {
        std::stringstream msg;
        msg << "UnitID name '" << name << "' does not match '" << id_regex_str
            << "', as required for QASM conversion.";
        tket_log()->warn(msg.str());
      }
    }
  };
  std::shared_ptr<UnitData> data_;
};

template <class Unit_T>
void unitid_to_json(nlohmann::json &j, const Unit_T &unit) {
  static_assert(std::is_base_of<UnitID, Unit_T>::value);
  j.push_back(unit.reg_name());
  j.push_back(unit.index());
}

template <class T>
void json_to_unitid(const nlohmann::json &j, T &unit) {
  unit = T(j.at(0).get<std::string>(), j.at(1).get<std::vector<unsigned>>());
}

/** Location holding a qubit */
class TKET_EXPORT Qubit : public UnitID {
 public:
  Qubit() : UnitID("", {}, UnitType::Qubit) {}

  /** Qubit in default register */
  explicit Qubit(unsigned index)
      : UnitID(q_default_reg(), {index}, UnitType::Qubit) {}

  /** Named register with no index */
  explicit Qubit(const std::string &name) : UnitID(name, {}, UnitType::Qubit) {}

  /** Named register with a one-dimensional index */
  Qubit(const std::string &name, unsigned index)
      : UnitID(name, {index}, UnitType::Qubit) {}

  /** Named register with a two-dimensional index */
  Qubit(const std::string &name, unsigned row, unsigned col)
      : UnitID(name, {row, col}, UnitType::Qubit) {}

  /** Named register with a three-dimensional index */
  Qubit(const std::string &name, unsigned row, unsigned col, unsigned layer)
      : UnitID(name, {row, col, layer}, UnitType::Qubit) {}

  /** Named register with a multi-dimensional index */
  Qubit(const std::string &name, std::vector<unsigned> index)
      : UnitID(name, index, UnitType::Qubit) {}

  /** Copy constructor */
  explicit Qubit(const UnitID &other) : UnitID(other) {
    if (other.type() != UnitType::Qubit) {
      throw InvalidUnitConversion(other.repr(), "Qubit");
    }
  }
};

JSON_DECL(Qubit)

/** Location holding a bit */
class TKET_EXPORT Bit : public UnitID {
 public:
  Bit() : UnitID("", {}, UnitType::Bit) {}

  /** Bit in default register */
  explicit Bit(unsigned index)
      : UnitID(c_default_reg(), {index}, UnitType::Bit) {}

  /** Named register with no index */
  explicit Bit(const std::string &name) : UnitID(name, {}, UnitType::Bit) {}

  /** Named register with a one-dimensional index */
  Bit(const std::string &name, unsigned index)
      : UnitID(name, {index}, UnitType::Bit) {}

  /** Named register with a two-dimensional index */
  Bit(const std::string &name, unsigned row, unsigned col)
      : UnitID(name, {row, col}, UnitType::Bit) {}

  /** Named register with a three-dimensional index */
  Bit(const std::string &name, unsigned row, unsigned col, unsigned layer)
      : UnitID(name, {row, col, layer}, UnitType::Bit) {}

  /** Named register with a multi-dimensional index */
  Bit(const std::string &name, std::vector<unsigned> index)
      : UnitID(name, index, UnitType::Bit) {}

  explicit Bit(const UnitID &other) : UnitID(other) {
    if (other.type() != UnitType::Bit) {
      throw InvalidUnitConversion(other.repr(), "Bit");
    }
  }
};

JSON_DECL(Bit)

/** Location holding a wasm UID */
class WasmState : public UnitID {
 public:
  WasmState() : UnitID(w_default_reg(), {}, UnitType::WasmState) {}

  /** Bit in default register */
  explicit WasmState(unsigned index)
      : UnitID(w_default_reg(), {index}, UnitType::WasmState) {}

  /** Named register with no index */
  explicit WasmState(const std::string &name)
      : UnitID(name, {}, UnitType::WasmState) {}

  /** Named register with a one-dimensional index */
  WasmState(const std::string &name, unsigned index)
      : UnitID(name, {index}, UnitType::WasmState) {}

  /** Named register with a two-dimensional index */
  WasmState(const std::string &name, unsigned row, unsigned col)
      : UnitID(name, {row, col}, UnitType::WasmState) {}

  /** Named register with a three-dimensional index */
  WasmState(const std::string &name, unsigned row, unsigned col, unsigned layer)
      : UnitID(name, {row, col, layer}, UnitType::WasmState) {}

  /** Named register with a multi-dimensional index */
  WasmState(const std::string &name, std::vector<unsigned> index)
      : UnitID(name, index, UnitType::WasmState) {}

  explicit WasmState(const UnitID &other) : UnitID(other) {
    if (other.type() != UnitType::WasmState) {
      throw InvalidUnitConversion(other.repr(), "WasmState");
    }
  }
};

JSON_DECL(WasmState)

/** Location holding an RNG UID */
class TKET_EXPORT RngState : public UnitID {
 public:
  RngState() : UnitID(r_default_reg(), {}, UnitType::RngState) {}

  /** Bit in default register */
  explicit RngState(unsigned index)
      : UnitID(r_default_reg(), {index}, UnitType::RngState) {}

  /** Named register with no index */
  explicit RngState(const std::string &name)
      : UnitID(name, {}, UnitType::RngState) {}

  /** Named register with a one-dimensional index */
  RngState(const std::string &name, unsigned index)
      : UnitID(name, {index}, UnitType::RngState) {}

  /** Named register with a two-dimensional index */
  RngState(const std::string &name, unsigned row, unsigned col)
      : UnitID(name, {row, col}, UnitType::RngState) {}

  /** Named register with a three-dimensional index */
  RngState(const std::string &name, unsigned row, unsigned col, unsigned layer)
      : UnitID(name, {row, col, layer}, UnitType::RngState) {}

  /** Named register with a multi-dimensional index */
  RngState(const std::string &name, std::vector<unsigned> index)
      : UnitID(name, index, UnitType::RngState) {}

  explicit RngState(const UnitID &other) : UnitID(other) {
    if (other.type() != UnitType::RngState) {
      throw InvalidUnitConversion(other.repr(), "RngState");
    }
  }
};

JSON_DECL(RngState)

/** Architectural qubit location */
class Node : public Qubit {
 public:
  Node() : Qubit() {}

  /** Qubit in default register */
  explicit Node(unsigned index) : Qubit(node_default_reg(), index) {}

  /** Named register with a one-dimensional index */
  Node(const std::string &name, unsigned index) : Qubit(name, index) {}

  /** Named register with a two-dimensional index */
  Node(const std::string &name, unsigned row, unsigned col)
      : Qubit(name, row, col) {}

  /** Named register with a three-dimensional index */
  Node(const std::string &name, unsigned row, unsigned col, unsigned layer)
      : Qubit(name, row, col, layer) {}

  /** Named register with a multi-dimensional index */
  Node(const std::string &name, std::vector<unsigned> index)
      : Qubit(name, index) {}

  explicit Node(const UnitID &other) : Qubit(other) {}
};

JSON_DECL(Node)

/** WASM UID */
class WasmNode : public WasmState {
 public:
  WasmNode() : WasmState() {}
};

JSON_DECL(WasmNode)

/** RNG UID */
class RngNode : public RngState {
 public:
  RngNode() : RngState() {}
};

JSON_DECL(WasmNode)

/** A correspondence between two sets of unit IDs */
typedef boost::bimap<UnitID, UnitID> unit_bimap_t;

/** A pair of ("initial" and "final") correspondences between unit IDs */
typedef struct {
  unit_bimap_t initial;
  unit_bimap_t final;
} unit_bimaps_t;

typedef std::vector<UnitID> unit_vector_t;
typedef std::map<UnitID, UnitID> unit_map_t;
typedef std::set<UnitID> unit_set_t;

typedef std::vector<Qubit> qubit_vector_t;
typedef std::map<Qubit, Qubit> qubit_map_t;
JSON_DECL(qubit_map_t)

typedef std::vector<Bit> bit_vector_t;
typedef std::map<Bit, Bit> bit_map_t;

typedef std::set<Node> node_set_t;
typedef std::vector<Node> node_vector_t;

/** A register of locations sharing the same name */
typedef std::map<unsigned, UnitID> register_t;

template <typename UnitA, typename UnitB>
static bool update_map(unit_bimap_t &m, const std::map<UnitA, UnitB> &um) {
  unit_map_t new_m;
  bool changed = false;
  for (const std::pair<const UnitA, UnitB> &pair : um) {
    const auto &it = m.right.find(pair.first);
    if (it == m.right.end()) {
      continue;
    }
    new_m.insert({it->second, pair.second});
    changed |= (m.right.erase(pair.first) > 0);
  }
  for (const std::pair<const UnitID, UnitID> &pair : new_m) {
    changed |= m.left.insert(pair).second;
  }
  return changed;
}

/**
 * Update a pair of "initial" and "final" correspondences.
 *
 * If \p maps is null then the function does nothing and returns false.
 *
 * @param[in,out] maps maps to be updated
 * @param[in] um_initial new correspondences added to initial map
 * @param[in] um_final new correspondences added to final map
 *
 * @tparam UnitA first unit type
 * @tparam UnitB second unit type
 *
 * @return whether any changes were made to the maps
 */
template <typename UnitA, typename UnitB>
bool update_maps(
    std::shared_ptr<unit_bimaps_t> maps,
    const std::map<UnitA, UnitB> &um_initial,
    const std::map<UnitA, UnitB> &um_final) {
  if (!maps) return false;
  // Can only work for Unit classes
  static_assert(std::is_base_of<UnitID, UnitA>::value);
  static_assert(std::is_base_of<UnitID, UnitB>::value);
  // Unit types must be related, so cannot rename e.g. Bits to Qubits
  static_assert(
      std::is_base_of<UnitA, UnitB>::value ||
      std::is_base_of<UnitB, UnitA>::value);

  bool changed = false;
  changed |= update_map(maps->initial, um_initial);
  changed |= update_map(maps->final, um_final);
  return changed;
}

}  // namespace tket
