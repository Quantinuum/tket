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

import re
from collections import Counter
from collections.abc import Callable
from typing import Any

import hypothesis.strategies as st
import numpy as np
from hypothesis.extra.numpy import arrays
from hypothesis.strategies._internal import SearchStrategy

from pytket import Bit, Circuit, Qubit
from pytket.architecture import Architecture
from pytket.backends.backendinfo import BackendInfo
from pytket.backends.backendresult import BackendResult
from pytket.circuit import Node, OpType
from pytket.pauli import Pauli, QubitPauliString
from pytket.utils import QubitPauliOperator
from pytket.utils.outcomearray import OutcomeArray

binary_digits = st.sampled_from((0, 1))
uint32 = st.integers(min_value=1, max_value=1 << 32 - 1)
uint64 = st.integers(min_value=1, max_value=1 << 64 - 1)
reg_name_regex = re.compile("[a-z][a-zA-Z0-9_]*")


@st.composite
def qubits(
    draw: Callable,
    name: SearchStrategy[str] = st.from_regex(reg_name_regex, fullmatch=True),  # noqa: B008
    index: SearchStrategy[int] = uint32,
) -> Qubit:
    return Qubit(draw(name), draw(index))


@st.composite
def nodes(
    draw: Callable,
    name: SearchStrategy[str] = st.from_regex(reg_name_regex, fullmatch=True),  # noqa: B008
    index: SearchStrategy[int] = uint32,
) -> Node:
    return Node(draw(name), draw(index))


@st.composite
def circuits(  # noqa: PLR0913 PLR0917
    draw: Callable[[SearchStrategy[Any]], Any],
    n_qubits: SearchStrategy[int] = st.integers(min_value=0, max_value=4),  # noqa: B008
    n_bits: SearchStrategy[int] = st.integers(min_value=0, max_value=4),  # noqa: B008
    depth: SearchStrategy[int] = st.integers(min_value=1, max_value=8),  # noqa: B008
    name: SearchStrategy[str] = st.text(min_size=0, max_size=6),  # noqa: B008
    phase: SearchStrategy[float] = st.floats(min_value=-2.0, max_value=2.0),  # noqa: B008
) -> Circuit:
    total_qubits = draw(n_qubits)
    total_bits = draw(n_bits)
    circuit = Circuit(total_qubits, total_bits, name=draw(name))
    circuit.add_phase(draw(phase))
    if total_qubits == 0:
        return circuit
    for _ in range(draw(depth)):
        gates = [circuit.Rx, circuit.H]
        if total_qubits > 1:
            gates.extend([circuit.CH, circuit.CX])
        gate = draw(st.sampled_from(gates))
        control = draw(st.integers(min_value=0, max_value=total_qubits - 1))
        if gate in (circuit.CH, circuit.CX):
            target = draw(
                st.integers(min_value=0, max_value=total_qubits - 1).filter(
                    lambda x: x != control  # noqa: B023
                )
            )
            gate(control, target)
        if gate == circuit.Rx:
            angle = draw(st.floats(min_value=-2.0, max_value=2.0))
            gate(angle, control)
        if gate == circuit.H:
            gate(control)

    return circuit


@st.composite
def outcomearrays(
    draw: Callable[[SearchStrategy[Any]], Any],
    rows: SearchStrategy[int] = st.integers(min_value=1, max_value=100),  # noqa: B008
    cols: SearchStrategy[int] = st.integers(min_value=0, max_value=10),  # noqa: B008
) -> OutcomeArray:
    row = draw(rows)
    col = draw(cols)
    array = draw(arrays(np.uint8, shape=(row, col)))
    width = (
        draw(st.integers(min_value=(col - 1) * 8 + 1, max_value=col * 8)) if col else 0
    )
    return OutcomeArray(array, width=width)


@st.composite
def outcome_counts(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> Counter:
    width = draw(st.integers(min_value=0, max_value=20))
    cols = -(-width // 8)
    ar_strat = arrays(np.uint8, shape=(1, cols))
    drawn_arrays = draw(st.lists(ar_strat, min_size=1, max_size=20))
    outcomes = (OutcomeArray(ar, width) for ar in drawn_arrays)
    countstrat = st.integers(min_value=1, max_value=100)
    count_vals = [draw(countstrat) for i in range(len(drawn_arrays))]

    return Counter(dict(zip(outcomes, count_vals, strict=False)))


def _gen_unitid(uidtype, index):  # type: ignore
    return uidtype(name="uid", index=index)


@st.composite
def architecture(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> Architecture:
    n_nodes = draw(st.integers(min_value=4, max_value=15))
    vertex = st.integers(min_value=0, max_value=n_nodes - 1)
    edge = st.lists(vertex, min_size=2, max_size=2, unique=True)
    edges = st.lists(edge, min_size=1, max_size=n_nodes)
    return Architecture(draw(edges))


@st.composite
def optypes(draw: Callable[[SearchStrategy[Any]], Any]) -> OpType:
    # the min value excludes the in and out optypes for quantum, classical and wasm
    return OpType(draw(st.integers(min_value=8, max_value=54)))


@st.composite
def errors(draw: Callable[[SearchStrategy[Any]], Any]) -> Any:
    return draw(st.floats(min_value=0.0, max_value=1.0))


@st.composite
def optype_errors(draw: Callable[[SearchStrategy[Any]], Any]) -> Any:
    return draw(st.dictionaries(optypes(), errors()))


@st.composite
def edges(draw: Callable[[SearchStrategy[Any]], Any]) -> Any:
    return draw(st.tuples(nodes(), nodes()))


@st.composite
def backendinfo(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> BackendInfo:
    name = draw(st.text(min_size=1, max_size=30))
    device_name = draw(st.text(min_size=1, max_size=30))
    version = draw(st.text(min_size=1, max_size=5))
    # hardware constraints
    arc = draw(st.one_of(st.none(), architecture()))
    gate_set = draw(st.sets(optypes()))
    supports_fast_feedforward = draw(st.booleans())
    supports_reset = draw(st.booleans())
    supports_midcircuit_measurement = draw(st.booleans())
    all_node_gate_errors = draw(st.dictionaries(nodes(), optype_errors()))
    all_edge_gate_errors = draw(st.dictionaries(edges(), optype_errors()))
    all_readout_errors = draw(st.dictionaries(nodes(), st.lists(st.lists(errors()))))
    averaged_node_gate_errors = draw(st.dictionaries(nodes(), errors()))
    averaged_edge_gate_errors = draw(st.dictionaries(edges(), errors()))
    averaged_readout_errors = draw(st.dictionaries(nodes(), errors()))
    misc = draw(st.dictionaries(st.text(), st.text()))

    return BackendInfo(
        name=name,
        device_name=device_name,
        version=version,
        architecture=arc,
        gate_set=gate_set,
        supports_fast_feedforward=supports_fast_feedforward,
        supports_reset=supports_reset,
        supports_midcircuit_measurement=supports_midcircuit_measurement,
        all_node_gate_errors=all_node_gate_errors,
        all_edge_gate_errors=all_edge_gate_errors,
        all_readout_errors=all_readout_errors,
        averaged_node_gate_errors=averaged_node_gate_errors,
        averaged_edge_gate_errors=averaged_edge_gate_errors,
        averaged_readout_errors=averaged_readout_errors,
        misc=misc,
    )


@st.composite
def backendresults(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> BackendResult:
    uid_indexes = st.lists(st.integers(min_value=0, max_value=3))
    measured = draw(st.booleans())
    if measured:
        bit_strat = uid_indexes.map(lambda x: _gen_unitid(Bit, x))  # type: ignore
        shots = draw(st.booleans())
        if shots:
            ar_strat = outcomearrays()
            shots = draw(ar_strat)
            cbits = draw(
                st.lists(
                    bit_strat, min_size=shots.width, max_size=shots.width, unique=True
                )
            )
            return BackendResult(c_bits=cbits, shots=shots)
        counts = draw(outcome_counts())
        width = next(counts.elements()).width
        cbits = draw(st.lists(bit_strat, unique=True, min_size=width, max_size=width))
        return BackendResult(c_bits=cbits, counts=counts)

    qbstrat = uid_indexes.map(lambda x: _gen_unitid(Qubit, x))  # type: ignore
    qubits_list = draw(st.lists(qbstrat, unique=True, min_size=1, max_size=6))
    n_qb = len(qubits_list)
    dims = 1 << n_qb
    state = draw(st.booleans())
    if state:
        state_ar = draw(
            arrays(
                complex,
                shape=(dims, 1),
                elements=st.complex_numbers(allow_nan=False),
            )
        )
        return BackendResult(q_bits=qubits_list, state=state_ar)

    density_matrix = draw(st.booleans())
    if density_matrix:
        density_matrix_ar = draw(
            arrays(
                complex,
                shape=(dims, dims),
                elements=st.complex_numbers(allow_nan=False),
            )
        )
        return BackendResult(q_bits=qubits_list, density_matrix=density_matrix_ar)

    unitary_ar = draw(
        arrays(
            complex, shape=(dims, dims), elements=st.complex_numbers(allow_nan=False)
        )
    )
    return BackendResult(q_bits=qubits_list, unitary=unitary_ar)


@st.composite
def qubitpaulistrings(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> QubitPauliString:
    qubit_lists = st.lists(qubits(), unique=True)
    pauli = st.sampled_from([Pauli.I, Pauli.X, Pauli.Y, Pauli.Z])

    qbs = draw(qubit_lists)
    pauli_str = draw(st.lists(pauli, min_size=len(qbs), max_size=len(qbs)))
    return QubitPauliString(qbs, pauli_str)


@st.composite
def qubitpaulioperators(
    draw: Callable[[SearchStrategy[Any]], Any],
) -> QubitPauliOperator:
    ops = st.dictionaries(
        qubitpaulistrings(),
        st.complex_numbers(min_magnitude=0.5, max_magnitude=3),
        max_size=10,
    )
    return QubitPauliOperator(draw(ops))
