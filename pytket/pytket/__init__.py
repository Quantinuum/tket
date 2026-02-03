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

"""Python Interface to tket"""

import pytket.architecture
import pytket.mapping
import pytket.placement
import pytket.transform
from pytket._version import __version__
from pytket.circuit import (
    Circuit,
    OpType,
)
from pytket.config import PytketConfig, get_config_file_path
from pytket.unit_id import (
    Bit,
    Qubit,
)

# Create pytket config file if it does not exist:
pytket_config_file = get_config_file_path()
if not pytket_config_file.exists():
    config = PytketConfig.default()
    config.write_file(pytket_config_file)

__path__ = __import__("pkgutil").extend_path(__path__, __name__)

"""Flag indicating 64-bit support.

If True, classical transforms and predicates, and QASM registers, with up to 64
bits are supported."""
bit_width_64 = True

__all__ = [
    "Bit",
    "Circuit",
    "OpType",
    "PytketConfig",
    "Qubit",
    "__path__",
    "__version__",
    "bit_width_64",
    "get_config_file_path",
]
