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

"""Backends for connecting to devices and simulators directly from pytket"""

__path__ = __import__("pkgutil").extend_path(__path__, __name__)

from .backend import Backend
from .backend_exceptions import CircuitNotRunError, CircuitNotValidError
from .resulthandle import ResultHandle
from .status import CircuitStatus, StatusEnum

__all__ = [
    "Backend",
    "CircuitNotRunError",
    "CircuitNotValidError",
    "CircuitStatus",
    "ResultHandle",
    "StatusEnum",
]
