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

#include <complex>
#include <nlohmann/json.hpp>
#include <optional>

// macro for type T serialization and deserialization declarations
#define JSON_DECL(T)                              \
  void to_json(nlohmann::json& j, const T& type); \
  void from_json(const nlohmann::json& j, T& type);

namespace tket {

class JsonError : public std::logic_error {
 public:
  explicit JsonError(const std::string& message) : std::logic_error(message) {}
};

}  // namespace tket

// no default serialization for complex types
namespace std {

template <class T>
void to_json(nlohmann::json& j, const std::complex<T>& p) {
  j = nlohmann::json{p.real(), p.imag()};
}

template <class T>
void from_json(const nlohmann::json& j, std::complex<T>& p) {
  p.real(j.at(0));
  p.imag(j.at(1));
}

// no default serialization for std::optional types
template <class T>
void to_json(nlohmann::json& j, const std::optional<T>& v) {
  if (v.has_value())
    j = *v;
  else
    j = nullptr;
}

template <class T>
void from_json(const nlohmann::json& j, std::optional<T>& v) {
  if (j.is_null())
    v = std::nullopt;
  else
    v = j.get<T>();
}

}  // namespace std
