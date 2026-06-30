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

"""
NOTE: The methods defined in this file are copied from
https://github.com/wjakob/nanobind/blob/7e3958086241ec579adfb4d9683bba24e2df80bd/src/stubgen.py
with a few small changes. Please keep modifications, including formatting, to a minimum,
so as to keep the diff simple.
The nanobind project is licensed under the BSD 3-Clause license, reproduced below:
============
Copyright (c) 2022 Wenzel Jakob <wenzel.jakob@epfl.ch>, All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============
"""

import re
import types
import typing
from inspect import ismodule, signature
from pathlib import Path
from typing import Any, Callable, List, Optional, Sequence, cast

from nanobind.stubgen import (
    create_subdirectory_for_module,
    NbFunction,
    NbStaticProperty,
    NbType,
    ReplacePattern,
    SKIP_LIST,
    StubGen,
    typing_extensions,
)

class PytketStubGen(StubGen):
    def __init__(
        self,
        module: types.ModuleType,
        recursive: bool = False,
        include_docstrings: bool = True,
        include_private: bool = False,
        private_exceptions: set[str] = set(),
        include_internal_imports: bool = True,
        include_external_imports: bool = False,
        private_submodule: Optional[str] = None,
        max_expr_length: int = 50,
        patterns: List[ReplacePattern] = [],
        quiet: bool = True,
        output_file: Optional[Path] = None,
    ) -> None:
        super().__init__(
            module,
            recursive=recursive,
            include_docstrings=include_docstrings,
            include_private=include_private,
            include_external_imports=include_external_imports,
            max_expr_length=max_expr_length,
            patterns=patterns,
            quiet=quiet,
            output_file=output_file,
        )

        # Private members that should be included regardless of `include_private`.
        self.private_exceptions = private_exceptions

        # Name of private submodule, if any.
        self.private_submodule = private_submodule

    def public_module_name(self, name: str):
        if self.private_submodule is None:
            return name
        return re.sub(f".{self.private_submodule}", "", name)

    def put(self, value: object, name: Optional[str] = None, parent: Optional[object] = None) -> None:
        old_prefix = self.prefix

        if value in self.stack:
            # Avoid infinite recursion due to cycles
            return

        try:
            self.stack.append(value)
            self.prefix = self.prefix + (("." + name) if name else "")

            # Check if an entry in a provided pattern file matches
            if self.apply_pattern(self.prefix, value):
                return

            # Exclude various standard elements found in modules, classes, etc.
            if name in SKIP_LIST:
                return

            is_type_alias = typing.get_origin(value) or (
                isinstance(value, type)
                and (value.__name__ != name or value.__module__ != self.module.__name__)
            )

            # Ignore private members unless the user requests their inclusion
            if (
                not self.include_private
                and name
                and not is_type_alias
                and len(name) > 2
                and (
                    (name[0] == "_" and name[1] != "_")
                    or (name[-1] == "_" and name[-2] != "_")
                )
                and name not in self.private_exceptions
            ):
                return

            tp = type(value)
            tp_mod, tp_name = tp.__module__, tp.__name__

            if ismodule(value):
                if len(self.stack) != 1:
                    value_name_s = value.__name__.split(".")
                    module_name_s = self.module.__name__.split(".")
                    is_external = value_name_s[0] != module_name_s[0]
                    if not self.include_external_imports and is_external:
                        return

                    # Do not include submodules in the same stub, but include a directive to import them
                    self.import_object(value.__name__, name=None, as_name=name)

                    # If the user requested this, generate recursive stub files as well
                    if self.recursive and value_name_s[:-1] == module_name_s and self.output_file:
                        if create_subdirectory_for_module(value):
                            # Create a new subdirectory and start with an __init__.pyi file there
                            dir_name = self.output_file.parents[0] / value_name_s[-1]
                            dir_name.mkdir(parents=False, exist_ok=True)
                            output_file = dir_name / '__init__.pyi'
                        else:
                            output_file = self.output_file.parents[0] / (value_name_s[-1] + '.pyi')

                        sg = PytketStubGen(
                            module=value,
                            recursive=self.recursive,
                            include_docstrings=self.include_docstrings,
                            include_private=self.include_private,
                            private_exceptions=self.private_exceptions,
                            include_external_imports=self.include_external_imports,
                            include_internal_imports=self.include_internal_imports,
                            private_submodule=self.private_submodule,
                            max_expr_length=self.max_expr_length,
                            patterns=self.patterns,
                            output_file=output_file,
                            quiet=self.quiet
                        )

                        sg.put(value)
                        output_file = output_file.resolve()

                        if not self.quiet:
                            print(f'  - writing stub "{output_file}" ..')

                        with open(output_file, "w", encoding='utf-8') as f:
                            f.write(sg.get())
                    return
                else:
                    self.apply_pattern(self.prefix + ".__prefix__", None)
                    # using value.__dict__ rather than inspect.getmembers
                    # to preserve insertion order
                    for name, child in value.__dict__.items():
                        self.put(child, name=name, parent=value)
                    self.apply_pattern(self.prefix + ".__suffix__", None)
            elif self.is_function(tp):
                value = cast(NbFunction, value)
                self.put_function(value, name, parent)
            elif issubclass(tp, type):
                value = cast(NbType, value)
                self.put_type(value, name)
            elif tp_mod == "nanobind":
                if tp_name == "nb_method":
                    value = cast(NbFunction, value)
                    self.put_function(value, name)
                elif tp_name == "nb_static_property":
                    value = cast(NbStaticProperty, value)
                    self.put_nb_static_property(name, value)
            elif tp_mod == "builtins":
                if tp is property:
                    value = cast(property, value)
                    self.put_property(value, name)
                else:
                    assert name is not None
                    abbrev = name != "__all__"
                    self.put_value(value, name, parent, abbrev=abbrev)
            else:
                assert name is not None
                self.put_value(value, name, parent)
        finally:
            self.stack.pop()
            self.prefix = old_prefix

    def put_function(self, fn: Callable[..., Any], name: Optional[str] = None, parent: Optional[object] = None):
        """Append a function of an arbitrary type to the stub"""
        # Don't generate a constructor for nanobind classes that aren't constructible
        if name == "__init__" and type(parent).__name__.startswith("nb_type"):
            return

        fn_module = getattr(fn, "__module__", None)
        fn_name = getattr(fn, "__name__", None)

        # Check if this function is an alias from *another* module
        if name and fn_module and self.public_module_name(fn_module) != self.public_module_name(self.module.__name__):
            self.put_value(fn, name)
            return

        # Check if this function is an alias from the *same* module
        if name and fn_name and name != fn_name:
            self.write_ln(f"{name} = {fn_name}\n")
            return

        # Unwrap staticmethod/classmethod descriptors.
        if is_staticmethod := isinstance(fn, staticmethod):
            fn = fn.__func__
        if is_classmethod := isinstance(fn, classmethod):
            self.write_ln("@classmethod")
            fn = fn.__func__

        # Special handling for nanobind functions with overloads.
        if type(fn).__module__ == "nanobind":
            fn = cast(NbFunction, fn)
            self.put_nb_func(fn, name, is_classmethod=is_classmethod)
            return

        if is_staticmethod:
            self.write_ln("@staticmethod")

        if name is None:
            name = fn.__name__
            assert name

        overloads: Sequence[Callable[..., Any]] = []
        if hasattr(fn, "__module__"):
            if typing_extensions:
                overloads = typing_extensions.get_overloads(fn)
            else:
                overloads = typing.get_overloads(fn)

        if not overloads:
            overloads = [fn]

        for i, fno in enumerate(overloads):
            if len(overloads) > 1:
                overload = self.import_object("typing", "overload")
                self.write_ln(f"@{overload}")

            try:
                sig = signature(fno)
            except ValueError:
                sig = None

            if sig is not None:
                sig_str = f"{name}{self.signature_str(sig)}"
            else:
                # If inspect.signature fails, use a maximally permissive type.
                any_type = self.import_object("typing", "Any")
                sig_str = f"{name}(*args, **kwargs) -> {any_type}"

            # Potentially copy docstring from the implementation function
            docstr = fno.__doc__
            if i == 0 and not docstr and fn.__doc__:
                docstr = fn.__doc__

            if not docstr or not self.include_docstrings:
                self.write_ln("def " + sig_str + ": ...")
            else:
                self.write_ln("def " + sig_str + ":")
                self.depth += 1
                self.put_docstr(docstr)
                self.depth -= 1
            self.write("\n")
