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
import pickle

import numpy as np
import pytest
import strategies as st  # type: ignore
from hypothesis import given, settings
from sympy import Symbol, im, re

from pytket.circuit import Qubit
from pytket.pauli import Pauli, QubitPauliString
from pytket.utils import QubitPauliOperator


def test_QubitPauliOperator_addition() -> None:
    x = Symbol("x")
    qpo = QubitPauliOperator()
    qpo += QubitPauliOperator(
        {
            QubitPauliString(Qubit(0), Pauli.Z): x,
            QubitPauliString(Qubit(1), Pauli.Y): 2j,
        }
    )
    qpo += QubitPauliOperator({QubitPauliString(Qubit(2), Pauli.X): 1j})
    correct_qpo = QubitPauliOperator(
        {
            QubitPauliString(Qubit(0), Pauli.Z): x,
            QubitPauliString(Qubit(1), Pauli.Y): 2j,
            QubitPauliString(Qubit(2), Pauli.X): 1j,
        }
    )
    assert qpo == correct_qpo


def test_QubitPauliOperator_scalarmult() -> None:
    y = Symbol("y")
    qpo = QubitPauliOperator({QubitPauliString(Qubit("q"), Pauli.X): y})
    x = Symbol("x")
    qpo2 = x * qpo
    qpo3 = qpo * x
    assert qpo2 == qpo3
    assert qpo2[QubitPauliString(Qubit("q"), Pauli.X)] == x * y
    qpo2 *= x
    assert qpo2[QubitPauliString(Qubit("q"), Pauli.X)] == x**2 * y


def test_QubitPauliOperator_opmult() -> None:
    y = Symbol("y")
    qpo = QubitPauliOperator({QubitPauliString(Qubit(0), Pauli.Z): y})
    x = Symbol("x")
    qpo2 = QubitPauliOperator({QubitPauliString(Qubit(0), Pauli.X): x})
    qpo3 = qpo * qpo2  # order matters!
    assert qpo3._dict[QubitPauliString(Qubit(0), Pauli.Y)] == 1j * x * y  # noqa: SLF001
    qpo4 = qpo2 * qpo
    assert qpo4._dict[QubitPauliString(Qubit(0), Pauli.Y)] == -1j * x * y  # noqa: SLF001


def test_QubitPauliOperator_substitution() -> None:
    qps = QubitPauliString(Qubit(0), Pauli.X)
    e = Symbol("e")
    exp = e + 5
    qpo = QubitPauliOperator({qps: exp})
    qpo.subs({e: 1})
    assert qpo[QubitPauliString(Qubit(0), Pauli.X)] == 6


def test_QubitPauliOperator_io() -> None:
    q0 = Qubit(0)
    qubit_data = pickle.dumps(q0)
    q1 = pickle.loads(qubit_data)
    assert q0 == q1
    assert str(q1) == "q[0]"
    qps0 = QubitPauliString([q0, Qubit(1)], [Pauli.X, Pauli.Y])
    string_data = pickle.dumps(qps0)
    qps1 = pickle.loads(string_data)
    assert qps0 == qps1
    qps2 = QubitPauliString(Qubit(2), Pauli.Z)
    a = Symbol("a")
    op = QubitPauliOperator({qps1: a, qps2: 3.1})
    op_data = pickle.dumps(op)
    op2 = pickle.loads(op_data)
    op2.subs({a: 1})
    assert np.isclose(complex(op2[qps1]), 1)
    assert np.isclose(complex(op2[qps2]), 3.1)


def test_QubitPauliOperator_matrices() -> None:
    qbs = [Qubit(i) for i in range(2)]
    qpsXY = QubitPauliString(qbs, [Pauli.X, Pauli.Y])
    qpsZI = QubitPauliString(qbs, [Pauli.Z, Pauli.I])
    op = QubitPauliOperator({qpsXY: 2, qpsZI: 1j})

    op_mat = np.array(
        [
            [0.0 + 1.0j, 0.0 + 0.0j, 0.0 + 0.0j, 0.0 - 2.0j],
            [0.0 + 0.0j, 0.0 + 1.0j, 0.0 + 2.0j, 0.0 + 0.0j],
            [0.0 + 0.0j, 0.0 - 2.0j, 0.0 - 1.0j, 0.0 + 0.0j],
            [0.0 + 2.0j, 0.0 + 0.0j, 0.0 + 0.0j, 0.0 - 1.0j],
        ]
    )
    assert np.array_equal(op_mat, op.to_sparse_matrix(2).toarray())

    state = 0.5 * np.array([1, 1j, 1, 1j])
    final_state = np.array([1 + 1j / 2, -1 / 2 + 1j, 1 - 1j / 2, 1 / 2 + 1j])
    assert np.array_equal(op.dot_state(state), final_state)
    assert op.state_expectation(state) == 2

    named_qbs = [Qubit("a", 0), Qubit("b")]
    aXbY = QubitPauliString(named_qbs, [Pauli.X, Pauli.Y])
    aZbI = QubitPauliString(named_qbs, [Pauli.Z, Pauli.I])
    named_op = QubitPauliOperator({aXbY: 2, aZbI: 1j})

    assert np.array_equal(op_mat, named_op.to_sparse_matrix().toarray())
    assert np.array_equal(named_op.dot_state(state, named_qbs), final_state)
    assert named_op.state_expectation(state, named_qbs) == 2

    assert np.array_equal(
        np.kron(op_mat, np.eye(2)),
        named_op.to_sparse_matrix([*named_qbs, Qubit("a", 1)]).toarray(),
    )

    # https://github.com/quantinuum/tket/issues/294
    P = QubitPauliString({Qubit(0): Pauli.Z, Qubit(1): Pauli.I})
    H = QubitPauliOperator({P: 1})
    assert np.allclose(P.to_sparse_matrix().todense(), H.to_sparse_matrix().todense())


def test_QubitPauliOperator_compression() -> None:
    qbs = [Qubit(i) for i in range(2)]
    qpsXY = QubitPauliString(qbs, [Pauli.X, Pauli.Y])
    qpsZI = QubitPauliString(qbs, [Pauli.Z, Pauli.I])
    qpsYY = QubitPauliString(qbs, [Pauli.Y, Pauli.Y])
    x = Symbol("x")
    op = QubitPauliOperator({qpsXY: 2, qpsZI: 1e-11j * x, qpsYY: 1e-11 * x + 1j})
    op.compress()
    with pytest.raises(KeyError) as errorinfo:
        _ = op[qpsZI]
    assert "(Zq[0], Iq[1])" in str(errorinfo.value)
    assert op[qpsXY] == 2
    assert re(op[qpsYY]) == 0
    assert im(op[qpsYY])
    assert op[qpsYY].subs({x: 0.001}).equals(1.0j)
    assert op[qpsYY].subs({x: 10}).equals(1.0j)


def test_QubitPauliString_serialization() -> None:
    qps0 = QubitPauliString()
    qps1 = QubitPauliString(
        [Qubit(i) for i in range(4)], [Pauli.Y, Pauli.I, Pauli.X, Pauli.Z]
    )
    assert qps0.to_list() == []
    for qps in [qps0, qps1]:
        serializable = qps.to_list()
        assert QubitPauliString.from_list(serializable) == qps
        assert json.loads(json.dumps(serializable)) == serializable


def test_QubitPauliOperator_serialization() -> None:
    qps = [Qubit(i) for i in range(2)]
    qpsXY = QubitPauliString(qps, [Pauli.X, Pauli.Y])
    qpsZI = QubitPauliString(qps, [Pauli.Z, Pauli.I])
    op = QubitPauliOperator({qpsXY: 2.0, qpsZI: 1j})

    serializable = op.to_list()
    assert QubitPauliOperator.from_list(serializable) == op
    assert json.loads(json.dumps(serializable)) == serializable


def test_QubitPauliOperator_serialization_symbolic() -> None:
    # https://github.com/quantinuum/tket/issues/614
    qps = [Qubit(i) for i in range(2)]
    qpsXY = QubitPauliString(qps, [Pauli.X, Pauli.Y])
    qpsZI = QubitPauliString(qps, [Pauli.Z, Pauli.I])
    x = Symbol("x")
    op = QubitPauliOperator({qpsXY: 2 * x, qpsZI: 1j})

    serializable = op.to_list()
    assert QubitPauliOperator.from_list(serializable) == op
    assert json.loads(json.dumps(serializable)) == serializable


@given(st.qubitpaulistrings())
def test_QubitPauliString_serialization_hypothesis(qps: QubitPauliString) -> None:
    serializable = qps.to_list()
    assert QubitPauliString.from_list(serializable) == qps
    assert json.loads(json.dumps(serializable)) == serializable


@given(st.qubitpaulioperators())
@settings(deadline=None)
def test_QubitPauliOperator_serialization_hypothesis(ops: QubitPauliOperator) -> None:
    serializable = ops.to_list()
    assert QubitPauliOperator.from_list(serializable) == ops
    assert json.loads(json.dumps(serializable)) == serializable
