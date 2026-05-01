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

"""Utility functions for performing high-level procedures in pytket"""

from .distribution import (
    EmpiricalDistribution,
    ProbabilityDistribution,
    convex_combination,
)
from .expectations import (
    expectation_from_counts,
    expectation_from_shots,
    get_operator_expectation_value,
    get_pauli_expectation_value,
)
from .graph import Graph
from .measurements import append_pauli_measurement
from .operators import QubitPauliOperator
from .outcomearray import OutcomeArray, readout_counts
from .prepare import prepare_circuit
from .results import (
    compare_statevectors,
    compare_unitaries,
    counts_from_shot_table,
    permute_basis_indexing,
    permute_qubits_in_statevector,
    permute_rows_cols_in_unitary,
    probs_from_counts,
    probs_from_state,
)
from .stats import gate_counts
from .symbolic import circuit_apply_symbolic_statevector, circuit_to_symbolic_unitary
from .term_sequence import gen_term_sequence_circuit

__all__ = [
    "EmpiricalDistribution",
    "Graph",
    "OutcomeArray",
    "ProbabilityDistribution",
    "QubitPauliOperator",
    "append_pauli_measurement",
    "circuit_apply_symbolic_statevector",
    "circuit_to_symbolic_unitary",
    "compare_statevectors",
    "compare_unitaries",
    "convex_combination",
    "counts_from_shot_table",
    "expectation_from_counts",
    "expectation_from_shots",
    "gate_counts",
    "gen_term_sequence_circuit",
    "get_operator_expectation_value",
    "get_pauli_expectation_value",
    "permute_basis_indexing",
    "permute_qubits_in_statevector",
    "permute_rows_cols_in_unitary",
    "prepare_circuit",
    "probs_from_counts",
    "probs_from_state",
    "readout_counts",
]
