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

#include "tket/Utils/Expression.hpp"
#include "tket_export.h"

namespace tket {

// Declare test namespace to grant it direct access to the symbol table
namespace test_Ops {
void clear_symbol_table();
}

/**
 * Utility class for accessing the global symbol table
 *
 * All members are static. There are no instances of this class.
 *
 * When an operation is created using \p get_op_ptr, any symbols in its
 * parameters are added to a global registry of symbols.
 */
struct TKET_EXPORT SymTable {
  /** Create a new symbol (not currently registered), and register it */
  static Sym fresh_symbol(const std::string &preferred = "a");

  static void register_symbol(const std::string &symbol);

  static void register_symbols(const SymSet &ss);

 private:
  friend void test_Ops::clear_symbol_table();
  static std::unordered_set<std::string> &get_registered_symbols();
};

}  // namespace tket
