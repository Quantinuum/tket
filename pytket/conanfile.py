import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class pytketRecipe(ConanFile):
    name = "pytket"
    version = "1.0.0"
    package_type = "application"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "binders/*"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def _get_tket_version(self) -> str:
        tket_ver = os.getenv("TKET_VERSION")
        if tket_ver is not None:
            return tket_ver
        # TKET_VERSION is in the parent directory in the repo, but will be in
        # the current directory once the package has been exported to conan cache
        for try_dir in [os.curdir, os.pardir]:
            path = os.path.join(try_dir, "TKET_VERSION")
            if os.path.exists(path):
                return load(self, path).strip()

        raise FileNotFoundError("TKET_VERSION not found")

    def requirements(self):
        tket_version = self._get_tket_version()

        self.requires("gmp/tci-6.3.0@tket/stable")
        self.requires("nlohmann_json/3.12.0")
        self.requires("nanobind/tci-2.10.2@tket/stable")
        self.requires("symengine/tci-0.14.0.2@tket/stable")
        self.requires("tkassert/0.3.4@tket/stable")
        self.requires(f"tket/{tket_version}@tket/stable")
        self.requires("tklog/0.3.3@tket/stable")
        self.requires("tkrng/0.3.3@tket/stable")
        self.requires("tktokenswap/0.3.13@tket/stable")
        self.requires("tkwsm/0.3.13@tket/stable")

    def export(self):
        # Copy the TKET_VERSION file to the export folder
        copy(
            self,
            "TKET_VERSION",
            os.path.join(self.recipe_folder, os.pardir),
            self.export_folder,
        )
