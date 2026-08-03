import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load


class TketCAPIRecipe(ConanFile):
    name = "tket-c-api"
    package_type = "library"

    # Optional metadata
    license = "Apache 2"
    description = "C interface to a small subset of core TKET's C++ functionality"
    topics = ("quantum", "computation", "compiler", "c-interface")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

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

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

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

    def package_info(self):
        self.cpp_info.libs = ["tket-c-api"]

    def requirements(self):
        self.requires(f"tket/{self.version}@tket/stable")
