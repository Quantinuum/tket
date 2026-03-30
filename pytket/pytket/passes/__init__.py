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

"""Module exposing various types of compiler pass."""

from pytket._tket.passes import *

from .combinecondpass import CombineCondPass
from .passselector import PassSelector
from .resizeregpass import scratch_reg_resize_pass
from .script import compilation_pass_from_script, compilation_pass_grammar
