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

import json
import multiprocessing
import os
import shutil
import subprocess
import sys
from sysconfig import get_config_var

import setuptools  # type: ignore
from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext  # type: ignore
from wheel.bdist_wheel import bdist_wheel as _bdist_wheel


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


binders = [
    "logging",
    "utils_serialization",
    "circuit",
    "circuit_library",
    "unit_id",
    "passes",
    "predicates",
    "partition",
    "pauli",
    "mapping",
    "transform",
    "tailoring",
    "tableau",
    "zx",
    "placement",
    "architecture",
]

stable_abi = sys.version_info >= (3, 12)


class CMakeBuild(build_ext):
    def run(self):
        self.check_extensions_list(self.extensions)
        extdir = os.path.abspath(
            os.path.dirname(self.get_ext_fullpath(self.extensions[0].name))
        )
        extsource = self.extensions[0].sourcedir
        build_dir = os.path.join(extsource, "build")
        shutil.rmtree(build_dir, ignore_errors=True)
        os.mkdir(build_dir)
        install_dir = os.getenv("INSTALL_DIR") or os.path.join(build_dir, "install")
        subprocess.run(
            ["cmake", f"-DCMAKE_INSTALL_PREFIX={install_dir}", os.pardir],
            cwd=build_dir,
            check=True,
        )
        subprocess.run(
            [
                "cmake",
                "--build",
                os.curdir,
                f"-j{os.getenv('PYTKET_CMAKE_N_THREADS', multiprocessing.cpu_count())}",
            ],
            cwd=build_dir,
            check=True,
        )
        subprocess.run(["cmake", "--install", os.curdir], cwd=build_dir, check=True)
        lib_folder = os.path.join(install_dir, "lib")
        lib_names = ["libtklog.so", "libtket.so"]
        ext_suffix = ".abi3.so" if stable_abi else get_config_var("EXT_SUFFIX")
        lib_names.extend(f"{binder}{ext_suffix}" for binder in binders)
        # TODO make the above generic
        os.makedirs(extdir, exist_ok=True)
        for lib_name in lib_names:
            shutil.copy(os.path.join(lib_folder, lib_name), extdir)
        include_folder = os.path.join(install_dir, "include")
        shutil.copytree(include_folder, extdir + "/include", dirs_exist_ok=True)


class ConanBuild(build_ext):
    def run(self):
        self.check_extensions_list(self.extensions)
        extdir = os.path.abspath(
            os.path.dirname(self.get_ext_fullpath(self.extensions[0].name))
        )
        extsource = self.extensions[0].sourcedir

        jsonstr = subprocess.check_output(
            [
                "conan",
                "create",
                ".",
                "--build=missing",
                "-o",
                "boost/*:header_only=True",
                "-o",
                "tket/*:shared=True",
                "-o",
                "tklog/*:shared=True",
                "--format",
                "json",
            ],
            cwd=extsource,
        )

        # Collect the paths to the libraries to package together
        conaninfo = json.loads(jsonstr)
        nodes = conaninfo["graph"]["nodes"]
        os.makedirs(extdir, exist_ok=True)
        for comp in ["tklog", "tket", "pytket"]:
            compnodes = [
                node for _, node in nodes.items() if node["ref"].startswith(comp + "/")
            ]
            assert len(compnodes) == 1
            compnode = compnodes[0]
            lib_folder = os.path.join(compnode["package_folder"], "lib")
            for lib in os.listdir(lib_folder):
                libpath = os.path.join(lib_folder, lib)
                if not os.path.isdir(libpath):
                    shutil.copy(libpath, extdir)

            include_folder = os.path.join(compnode["package_folder"], "include")
            if os.path.exists(include_folder):
                shutil.copytree(include_folder, extdir + "/include", dirs_exist_ok=True)


plat_name = os.getenv("WHEEL_PLAT_NAME")


def get_build_ext():
    if os.getenv("NO_CONAN"):
        return CMakeBuild
    return ConanBuild


class bdist_wheel(_bdist_wheel):
    def finalize_options(self):
        _bdist_wheel.finalize_options(self)
        if plat_name is not None:
            print(f"Overriding plat_name to {plat_name}")  # noqa: T201
            self.plat_name = plat_name
            self.plat_name_supplied = True


setup(
    name="pytket",
    author="TKET development team",
    author_email="tket-support@quantinuum.com",
    python_requires=">=3.10",
    project_urls={
        "Documentation": "https://docs.quantinuum.com/tket/api-docs/",
        "Source": "https://github.com/quantinuum/tket",
        "Tracker": "https://github.com/quantinuum/tket/issues",
    },
    description="Quantum computing toolkit and interface to the TKET compiler",
    long_description=open("package.md").read(),  # noqa: SIM115
    long_description_content_type="text/markdown",
    license="Apache 2",
    packages=[*setuptools.find_packages(), "pytket.qasm.includes"],
    install_requires=[
        "sympy >= 1.12.1",
        "numpy >= 1.26.4",
        "lark >= 1.1.9",
        "scipy >= 1.13.1",
        "networkx >= 2.8.8",
        "graphviz >= 0.20.3",
        "jinja2 >= 3.1.4",
        "typing-extensions >= 4.12.2",
        "qwasm >= 1.0.1",
    ],
    extras_require={
        "ZX": [
            "numba >= 0.62.1",
            "quimb >= 1.8.2",
            "autoray >= 0.6.12",
        ],
    },
    ext_modules=[CMakeExtension(f"pytket._tket.{binder}") for binder in binders],
    cmdclass={
        "build_ext": get_build_ext(),
        "bdist_wheel": bdist_wheel,
    },
    classifiers=[
        "Environment :: Console",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: MacOS :: MacOS X",
        "Operating System :: POSIX :: Linux",
        "Operating System :: Microsoft :: Windows",
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering",
    ],
    include_package_data=True,
    package_data={"pytket": ["py.typed"]},
    zip_safe=False,
    options={"bdist_wheel": {"py_limited_api": "cp312"}} if stable_abi else None,
)
