# Copyright Quantinuum
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.errors import ConanInvalidConfiguration


class test_tkrngRecipe(ConanFile):
    name = "test-tkrng"
    version = "0.3.3"
    package_type = "application"
    license = "Apache 2"
    url = "https://github.com/quantinuum/tket"
    description = "Unit tests for tkrng"
    settings = "os", "compiler", "build_type", "arch"
    options = {"with_coverage": [True, False]}
    default_options = {"with_coverage": False}
    exports_sources = "CMakeLists.txt", "src/*"

    def configure(self):
        if self.options.with_coverage:
            self.options["tkrng"].profile_coverage = True

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["WITH_COVERAGE"] = self.options.with_coverage
        tc.generate()

    def validate(self):
        if self.options.with_coverage and self.settings.compiler != "gcc":
            raise ConanInvalidConfiguration(
                "`with_coverage` option only available with gcc"
            )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def requirements(self):
        self.requires("tkrng/0.3.3")
        self.requires("catch2/3.13.0@tket/stable")
