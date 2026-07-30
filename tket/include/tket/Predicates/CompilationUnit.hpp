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

#include <memory>

#include "Predicates.hpp"
#include "tket_export.h"

namespace tket {

class CompilationUnit;
typedef std::map<std::type_index, PredicatePtr>
    PredicatePtrMap;  // this is map so i can look up by type later
typedef std::pair<const std::type_index, PredicatePtr> TypePredicatePair;
typedef std::map<std::type_index, std::pair<PredicatePtr, bool>> PredicateCache;

/* The CompilationUnit class encapsulates a Circuit and a PredicatePtrMap of the
   Predicates that Circuit is intended to satisfy by the end of its run through
   the compiler. It holds a cache of Predicates
    which are currently satisfied. */

class TKET_EXPORT CompilationUnit {
 public:
  explicit CompilationUnit(const Circuit& circ);
  CompilationUnit(const Circuit& circ, const PredicatePtrMap& preds);
  CompilationUnit(const Circuit& circ, const std::vector<PredicatePtr>& preds);

  bool calc_predicate(const Predicate& pred) const;
  bool check_all_predicates()
      const;  // returns false if any of the preds are unsatisfied

  /* getters to inspect the data members */
  const Circuit& get_circ_ref() const { return circ_; }
  const PredicateCache& get_cache_ref() const { return cache_; }
  const unit_bimap_t& get_initial_map_ref() const { return maps->initial; }
  const unit_bimap_t& get_final_map_ref() const { return maps->final; }
  std::string to_string() const;

  friend class Circuit;
  friend class BasePass;
  friend class StandardPass;

  static TypePredicatePair make_type_pair(const PredicatePtr& ptr);

 private:
  void empty_cache() const;
  void initialize_cache() const;
  void initialize_maps();
  Circuit circ_;  // modified continuously
  PredicatePtrMap
      target_preds;  // these are the predicates you WANT your circuit to
                     // satisfy by the end of your Compiler Passes
  mutable PredicateCache cache_;  // updated continuously

  // Maps from original logical qubits to corresponding current qubits
  std::shared_ptr<unit_bimaps_t> maps;
};

}  // namespace tket
