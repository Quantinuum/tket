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

// This must be the first include since it includes Python.h.
// https://docs.python.org/3/c-api/intro.html#include-files
// clang-format off
#include <nanobind/nanobind.h>
// clang-format on

#include <nanobind/stl/complex.h>

#include <complex>

#include "nanobind_json/nanobind_json.hpp"
#include "tket/Utils/Json.hpp"

namespace nb = nanobind;
using json = nlohmann::json;

namespace tket {

NB_MODULE(utils_serialization, m) {
  nb::set_leak_warnings(false);
  m.def(
      "complex_to_list",
      [](std::complex<double> c) { return nb::object(json(c)); },
      "Convert complex number to serializable list [real, imag].");
  m.def(
      "list_to_complex",
      [](const nb::object& py_object) {
        return json(py_object).get<std::complex<double>>();
      },
      "Convert serializable list as output by `complex_to_list` to complex "
      "number.");
}

}  // namespace tket
