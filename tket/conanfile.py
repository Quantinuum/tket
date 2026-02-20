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

import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy, load
from conan.errors import ConanInvalidConfiguration


class TketConan(ConanFile):
    name = "tket"
    package_type = "library"
    license = "Apache 2"
    homepage = "https://github.com/quantinuum/tket"
    url = "https://github.com/conan-io/conan-center-index"
    description = "Quantum SDK"
    topics = ("quantum", "computation", "compiler")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "profile_coverage": [True, False],
        "with_test": [True, False],
        "with_proptest": [True, False],
        "with_all_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "profile_coverage": False,
        "with_test": False,
        "with_proptest": False,
        "with_all_tests": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "include/*",
        "test/*",
        "proptest/*",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        # Only need header libraries from boost.
        self.options["boost"].header_only = True

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["PROFILE_COVERAGE"] = self.options.profile_coverage
        if self.build_test():
            tc.variables["BUILD_TKET_TEST"] = True
            architectures_dir = os.path.join(
                self.source_folder, "test/src/test_architectures"
            )
            copy(self, "*.json", architectures_dir, self.build_folder)
            circuits_dir = os.path.join(self.source_folder, "test/src/test_circuits")
            copy(self, "*.json", circuits_dir, self.build_folder)
        if self.build_proptest():
            tc.variables["BUILD_TKET_PROPTEST"] = True
        tc.generate()

    def set_version(self):
        """Set the version of this package from the TKET_VERSION file."""

        # TKET_VERSION is in the parent directory in the repo, but will be in
        # the current directory once the package has been exported to conan cache
        for try_dir in [os.curdir, os.pardir]:
            path = os.path.join(try_dir, "TKET_VERSION")
            if os.path.exists(path):
                self.version = load(self, path).strip()
                return

        raise FileNotFoundError("TKET_VERSION not found")

    def export(self):
        # Copy the TKET_VERSION file to the export folder
        copy(
            self,
            "TKET_VERSION",
            os.path.join(self.recipe_folder, os.pardir),
            self.export_folder,
        )

    def validate(self):
        if self.options.profile_coverage and self.settings.compiler != "gcc":
            raise ConanInvalidConfiguration(
                "`profile_coverage` option only available with gcc"
            )

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.build_test():
            self.run(
                os.path.join(
                    "test",
                    "test-tket" + (".exe" if self.settings.os == "Windows" else ""),
                )
            )
        if self.build_proptest():
            self.run(
                os.path.join(
                    "proptest",
                    "proptest-tket" + (".exe" if self.settings.os == "Windows" else ""),
                )
            )

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["tket"]

    def requirements(self):
        # libraries installed from remote:
        # https://quantinuumsw.jfrog.io/artifactory/api/conan/tket1-libs
        self.requires("boost/tci-1.90.0@tket/stable", transitive_headers=True)
        self.requires("eigen/5.0.1", transitive_headers=True)
        self.requires("nlohmann_json/3.12.0", transitive_headers=True)
        self.requires("symengine/tci-0.14.0.2@tket/stable", transitive_headers=True)
        self.requires("tkassert/0.3.4@tket/stable", transitive_headers=True)
        self.requires("tklog/0.3.3@tket/stable")
        self.requires("tkrng/0.3.3@tket/stable")
        self.requires("tktokenswap/0.3.13@tket/stable")
        self.requires("tkwsm/0.3.13@tket/stable")
        if self.build_test():
            self.test_requires("catch2/3.13.0@tket/stable")
        if self.build_proptest():
            self.test_requires("rapidcheck/tci-20231215@tket/stable")

    def build_test(self):
        return self.options.with_test or self.options.with_all_tests

    def build_proptest(self):
        return self.options.with_proptest or self.options.with_all_tests
