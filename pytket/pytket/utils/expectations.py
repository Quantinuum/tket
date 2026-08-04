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

from typing import TYPE_CHECKING

import numpy as np

from pytket.circuit import Circuit, Qubit
from pytket.partition import (
    GraphColourMethod,
    PauliPartitionStrat,
    measurement_reduction,
)
from pytket.pauli import QubitPauliString

from .measurements import _all_pauli_measurements, append_pauli_measurement
from .operators import QubitPauliOperator
from .results import KwargTypes

if TYPE_CHECKING:
    from pytket.backends.backend import Backend


def expectation_from_shots(shot_table: np.ndarray) -> float:
    """Estimates the expectation value of a circuit from its shots.
    Computes the parity of '1's across all bits to determine a +1 or -1 contribution
    from each row, and returns the average.

    :param shot_table: The table of shots to interpret.
    :return: The expectation value in the range [-1, 1].
    """
    aritysum = 0.0
    for row in shot_table:
        aritysum += np.sum(row) % 2
    return -2 * aritysum / len(shot_table) + 1


def expectation_from_counts(counts: dict[tuple[int, ...], int]) -> float:
    """Estimates the expectation value of a circuit from shot counts.
    Computes the parity of '1's across all bits to determine a +1 or -1 contribution
    from each readout, and returns the weighted average.

    :param counts: Counts of each measurement outcome observed.
    :return: The expectation value in the range [-1, 1].
    """
    aritysum = 0.0
    total_shots = 0
    for row, count in counts.items():
        aritysum += count * (sum(row) % 2)
        total_shots += count
    return -2 * aritysum / total_shots + 1


def _default_index(q: Qubit) -> int:
    if q.reg_name != "q" or len(q.index) != 1:
        raise ValueError("Non-default qubit register")
    return int(q.index[0])


def get_pauli_expectation_value(
    state_circuit: Circuit,
    pauli: QubitPauliString,
    backend: "Backend",
    n_shots: int | None = None,
) -> complex:
    """Estimates the expectation value of the given circuit with respect to the Pauli
    term by preparing measurements in the appropriate basis, running on the backend and
    interpreting the counts/statevector

    :param state_circuit: Circuit that generates the desired state
        :math:`\\left|\\psi\\right>`.
    :param pauli: Pauli operator
    :param backend: pytket backend to run circuit on.
    :param n_shots: Number of shots to run if backend supports shots/counts. Set to None
        to calculate using statevector if supported by the backend. Defaults to None
    :return: :math:`\\left<\\psi | P | \\psi \\right>`
    """
    if not n_shots:
        if not backend.valid_circuit(state_circuit):
            state_circuit = backend.get_compiled_circuit(state_circuit)
        if backend.supports_expectation:
            return backend.get_pauli_expectation_value(state_circuit, pauli)
        state = backend.run_circuit(state_circuit).get_state()
        return complex(pauli.state_expectation(state))

    measured_circ = state_circuit.copy()
    append_pauli_measurement(pauli, measured_circ)
    measured_circ = backend.get_compiled_circuit(measured_circ)
    if backend.supports_counts:
        counts = backend.run_circuit(measured_circ, n_shots=n_shots).get_counts()
        return expectation_from_counts(counts)
    if backend.supports_shots:
        shot_table = backend.run_circuit(measured_circ, n_shots=n_shots).get_shots()
        return expectation_from_shots(shot_table)
    raise ValueError("Backend does not support counts or shots")


def get_operator_expectation_value(  # noqa: PLR0912 PLR0913 PLR0915 PLR0917
    state_circuit: Circuit,
    operator: QubitPauliOperator,
    backend: "Backend",
    n_shots: int | None = None,
    partition_strat: PauliPartitionStrat | None = None,
    colour_method: GraphColourMethod = GraphColourMethod.LargestFirst,
    **kwargs: KwargTypes,
) -> complex:
    """Estimates the expectation value of the given circuit with respect to the operator
    based on its individual Pauli terms. If the QubitPauliOperator has symbolic values
    the expectation value will also be symbolic. The input circuit must belong to the
    default qubit register and have contiguous qubit ordering.

    :param state_circuit: Circuit that generates the desired state
        :math:`\\left|\\psi\\right>`
    :param operator: Operator :math:`H`. Currently does not support free symbols for the
        purpose of obtaining expectation values.
    :param backend: pytket backend to run circuit on.
    :param n_shots: Number of shots to run if backend supports shots/counts. None will
        force the backend to give the full state if available. Defaults to None
    :param partition_strat: If retrieving shots, can perform measurement reduction using
        a chosen strategy
    :return: :math:`\\left<\\psi | H | \\psi \\right>`
    """
    if not n_shots:
        if not backend.valid_circuit(state_circuit):
            state_circuit = backend.get_compiled_circuit(state_circuit)
        try:
            coeffs: list[complex] = [complex(v) for v in operator.get_dict().values()]
        except TypeError:
            raise ValueError(  # noqa: B904
                "QubitPauliOperator contains unevaluated symbols."
            )
        if backend.supports_expectation and (
            backend.expectation_allows_nonhermitian or all(z.imag == 0 for z in coeffs)
        ):
            return backend.get_operator_expectation_value(state_circuit, operator)
        result = backend.run_circuit(state_circuit)
        state = result.get_state()
        return operator.state_expectation(state)
    energy: complex
    id_string = QubitPauliString()
    energy = complex(operator[id_string]) if id_string in operator.get_dict() else 0
    if not partition_strat:
        operator_without_id = QubitPauliOperator(
            {p: c for p, c in operator.get_dict().items() if (p != id_string)}
        )
        coeffs = [complex(c) for c in operator_without_id.get_dict().values()]
        pauli_circuits = list(
            _all_pauli_measurements(operator_without_id, state_circuit)
        )

        handles = backend.process_circuits(
            backend.get_compiled_circuits(pauli_circuits),
            n_shots,
            valid_check=True,
            **kwargs,
        )
        results = backend.get_results(handles)
        if backend.supports_counts:
            for result, coeff in zip(results, coeffs, strict=False):
                counts = result.get_counts()
                energy += coeff * expectation_from_counts(counts)
            for handle in handles:
                backend.pop_result(handle)
            return energy
        if backend.supports_shots:
            for result, coeff in zip(results, coeffs, strict=False):
                shots = result.get_shots()
                energy += coeff * expectation_from_shots(shots)
            for handle in handles:
                backend.pop_result(handle)
            return energy
        raise ValueError("Backend does not support counts or shots")
    qubit_pauli_string_list = [p for p in operator.get_dict() if (p != id_string)]
    measurement_expectation = measurement_reduction(
        qubit_pauli_string_list, partition_strat, colour_method
    )
    # note: this implementation requires storing all the results
    # in memory simultaneously to filter through them.
    measure_circs = []
    for pauli_circ in measurement_expectation.measurement_circs:
        circ = state_circuit.copy()
        circ.append(pauli_circ)
        measure_circs.append(circ)
    handles = backend.process_circuits(
        backend.get_compiled_circuits(measure_circs),
        n_shots=n_shots,
        valid_check=True,
        **kwargs,
    )
    results = backend.get_results(handles)
    for pauli_string in measurement_expectation.results:
        bitmaps = measurement_expectation.results[pauli_string]
        string_coeff = operator[pauli_string]
        for bm in bitmaps:
            index = bm.circ_index
            aritysum = 0.0
            if backend.supports_counts:
                counts = results[index].get_counts()
                total_shots = 0
                for row, count in counts.items():
                    aritysum += count * (sum(row[i] for i in bm.bits) % 2)
                    total_shots += count
                e = (
                    ((-1) ** bm.invert)
                    * string_coeff
                    * (-2 * aritysum / total_shots + 1)
                )
                energy += complex(e)
            elif backend.supports_shots:
                shots = results[index].get_shots()
                for row in shots:
                    aritysum += sum(row[i] for i in bm.bits) % 2
                e = (
                    ((-1) ** bm.invert)
                    * string_coeff
                    * (-2 * aritysum / len(shots) + 1)
                )
                energy += complex(e)
            else:
                raise ValueError("Backend does not support counts or shots")
    for handle in handles:
        backend.pop_result(handle)
    return energy
