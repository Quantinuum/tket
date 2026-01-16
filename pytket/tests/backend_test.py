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
from collections import Counter
from typing import Any

import numpy as np
import pytest
from hypothesis import given, settings, strategies
from simulator import TketSimBackend, TketSimShotBackend  # type: ignore
from strategies import backendresults, outcomearrays  # type: ignore

from pytket.architecture import Architecture
from pytket.backends.backend import Backend, ResultHandleTypeError
from pytket.backends.backend_exceptions import CircuitNotRunError, InvalidResultType
from pytket.backends.backendresult import BackendResult
from pytket.backends.resulthandle import ResultHandle
from pytket.backends.status import CircuitStatus, StatusEnum
from pytket.circuit import BasisOrder, Bit, Circuit, Node, OpType, Qubit
from pytket.mapping import LexiLabellingMethod, LexiRouteRoutingMethod, MappingManager
from pytket.passes import CliffordSimp, PauliSimp
from pytket.predicates import CompilationUnit
from pytket.utils.outcomearray import OutcomeArray, readout_counts
from pytket.utils.prepare import prepare_circuit


def test_resulthandle() -> None:
    a = ResultHandle("sh", 3)
    assert str(a) == "('sh', 3)"

    assert repr(a) == "ResultHandle('sh', 3)"
    b = ResultHandle("sh", 3)
    assert ResultHandle("sh", 3) == a

    assert hash(a) == hash(b)
    assert a != ResultHandle(3.0, "sh", 1)

    sta = str(a)

    newa = ResultHandle.from_str(sta)
    assert a == newa
    assert newa != ResultHandle("sh", 3.1)

    def input_str_invalid(st: str) -> bool:
        with pytest.raises(ValueError) as errorinfo:
            ResultHandle.from_str(st)
        return "ResultHandle string format invalid." in str(errorinfo.value)

    assert input_str_invalid("2")
    assert input_str_invalid("[2]")
    assert input_str_invalid("sin(2)")
    assert input_str_invalid("({'a':2}, 4)")
    assert input_str_invalid("[3, 'asdf']")


def test_check_handle_single() -> None:
    b = TketSimBackend(ignore_measures=True)
    c = Circuit(2).measure_all()
    handles = b.process_circuits([c, c])
    assert b.get_results(handles)

    with pytest.raises(ResultHandleTypeError) as e:
        b.get_results(handles[0])
    assert "Possible use of single ResultHandle" in str(e.value)


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_bell() -> None:
    sv = [1 / np.sqrt(2), 0, 0, 1 / np.sqrt(2)]
    c = Circuit(2)
    c.H(0)
    c.CX(0, 1)
    assert np.allclose(c.get_statevector(), sv)
    b = TketSimBackend()
    r = b.run_circuit(c)
    assert np.allclose(r.get_state(), sv)
    # Check that the "get_compiled_circuit" result is still the same.
    # (The circuit could change, due to optimisations).
    c = b.get_compiled_circuit(c)
    assert np.allclose(c.get_statevector(), sv)
    with pytest.raises(CircuitNotRunError) as errorinfo:
        b.get_result(ResultHandle("test"))
        assert "test" in str(errorinfo.value)


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_basisorder() -> None:
    b = TketSimBackend()
    c = Circuit(2)
    c.X(1)
    r = b.run_circuit(c)
    assert (r.get_state() == np.asarray([0, 1, 0, 0])).all()
    assert (r.get_state(basis=BasisOrder.dlo) == np.asarray([0, 0, 1, 0])).all()
    assert (c.get_statevector() == np.asarray([0, 1, 0, 0])).all()
    b = TketSimShotBackend(ignore_measures=True)
    c.measure_all()
    r = b.run_circuit(c, n_shots=4, seed=4)
    assert r.get_shots().shape == (4, 2)
    assert r.get_counts() == {(0, 1): 4}


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_swaps() -> None:
    c = Circuit(2)
    c.CX(0, 1)
    c.Ry(0.1, 1)
    cu = CompilationUnit(c)
    PauliSimp().apply(cu)
    c1 = cu.circuit
    s_direct = c.get_statevector()
    s1_direct = c1.get_statevector()

    bs = TketSimBackend()
    c, c1 = bs.get_compiled_circuits((c, c1))
    [r, r1] = bs.run_circuits([c, c1])
    s = r.get_state()
    s1 = r1.get_state()
    assert np.allclose(s, s1)
    assert np.allclose(s_direct, s)
    assert np.allclose(s1_direct, s)


def assert_single_entry_approx_value(
    numbers: list[Any],
    index: int,
    value: float = 1.0,
    value_abs_eps: float = 1e-14,
    zero_abs_eps: float = 0.0,
) -> None:
    """The input numbers should all be zero except for a single entry,
    which should equal the expected value approximately at the given index.
    Maybe not exactly equal due to numerical roundoff.
    """
    assert index < len(numbers)
    for nn in range(len(numbers)):
        expected_value = 0.0
        eps = zero_abs_eps
        if nn == index:
            expected_value = value
            eps = value_abs_eps
        assert abs(numbers[nn] - expected_value) <= eps


@pytest.mark.filterwarnings("ignore::PendingDeprecationWarning")
def test_swaps_basisorder() -> None:
    # Check that implicit swaps can be corrected irrespective of BasisOrder
    c = Circuit(4)
    c.X(0)
    c.CX(0, 1)
    c.CX(1, 0)
    c.CX(1, 3)
    c.CX(3, 1)
    c.X(2)
    cu = CompilationUnit(c)
    CliffordSimp(True).apply(cu)
    c1 = cu.circuit
    assert c1.n_gates_of_type(OpType.CX) == 2

    s_ilo_direct = c1.get_statevector()
    correct_ilo_direct = c.get_statevector()

    b = TketSimBackend()
    c, c1 = b.get_compiled_circuits((c, c1))
    r, r1 = b.run_circuits((c, c1))
    s_ilo = r1.get_state(basis=BasisOrder.ilo)
    correct_ilo = r.get_state(basis=BasisOrder.ilo)

    assert np.allclose(s_ilo, correct_ilo)
    assert np.allclose(s_ilo_direct, s_ilo)
    assert np.allclose(correct_ilo_direct, s_ilo)

    s_dlo = r1.get_state(basis=BasisOrder.dlo)
    correct_dlo = r.get_state(basis=BasisOrder.dlo)
    assert np.allclose(s_dlo, correct_dlo)

    qbs = c.qubits

    for result in b.get_results(b.process_circuits([c, c1])):
        assert_single_entry_approx_value(
            np.abs(result.get_state([qbs[1], qbs[2], qbs[3], qbs[0]])).tolist(), 6
        )

        assert_single_entry_approx_value(
            np.abs(result.get_state([qbs[2], qbs[1], qbs[0], qbs[3]])).tolist(), 9
        )

        assert_single_entry_approx_value(
            np.abs(result.get_state([qbs[2], qbs[3], qbs[0], qbs[1]])).tolist(), 12
        )


def test_get_n_shots_as_list() -> None:
    convert = Backend._get_n_shots_as_list  # noqa: SLF001

    assert convert(1, 3) == [1, 1, 1]
    assert convert([1, 2, 3], 3) == [1, 2, 3]
    assert convert(None, 5) == 5 * [None]
    assert convert([1, None, 3], 3) == [1, None, 3]
    assert convert([1, None, 3], 3, set_zero=True) == [1, 0, 3]
    assert convert([None, None], 2, set_zero=True) == [0, 0]

    with pytest.raises(ValueError) as e:
        convert([1, None, 3], 3, optional=False)
    err_msg = "n_shots values are required for all circuits for this backend"
    assert err_msg in str(e.value)

    with pytest.raises(ValueError) as e:
        convert(None, 1, optional=False)
    err_msg = "Parameter n_shots is required for this backend"
    assert err_msg in str(e.value)

    with pytest.raises(ValueError) as e:
        convert([1, 2], 4, optional=False)
    err_msg = "The length of n_shots and circuits must match"
    assert err_msg in str(e.value)


def test_backendresult() -> None:  # noqa: PLR0915
    shots_list = [[1, 0, 1, 1] * 3, [1, 1, 0, 1] * 3]
    cbits = [Bit(i) for i in range(12)]
    qbits = [Qubit(i) for i in range(12)]

    def _invalid_bitlength(kwarg: Any) -> bool:
        with pytest.raises(ValueError) as errorinfo:
            BackendResult(**kwarg)
        return "does not match input data dimensions" in str(errorinfo.value)

    outcomeA = OutcomeArray.from_readouts(shots_list)
    assert _invalid_bitlength({"c_bits": cbits[:4], "shots": outcomeA})
    assert _invalid_bitlength({"c_bits": cbits[:5], "counts": outcomeA.counts()})
    assert _invalid_bitlength(
        {"q_bits": qbits[:6], "state": np.array([1 + 1j, 3 + 4j])}
    )
    assert _invalid_bitlength(
        {"q_bits": qbits[:6], "unitary": np.array([[1 + 1j, 3 + 4j], [1 + 1j, 3 + 4j]])}
    )

    with pytest.raises(ValueError) as errorinfo:
        BackendResult(c_bits=cbits, shots=outcomeA, counts=outcomeA.counts())
        assert "Provide either counts or shots, both is not valid" in str(
            errorinfo.value
        )

    backres_shots = BackendResult(shots=outcomeA)
    shots = backres_shots.get_result().shots
    assert shots is not None
    assert shots == outcomeA

    with pytest.raises(ValueError) as errorinfo:
        backres_shots.get_result([qbits[0], cbits[1]])
        assert "Request results for either only Bits or only Qubits." in str(
            errorinfo.value
        )
    with pytest.raises(InvalidResultType) as errorinfores:
        backres_shots.get_result(qbits)

    shots = backres_shots.get_result([cbits[1], cbits[2]]).shots
    assert shots is not None
    assert shots == OutcomeArray.from_readouts([[0, 1], [1, 0]])

    backres_counts = BackendResult(c_bits=cbits, counts=outcomeA.counts())
    assert backres_counts.get_result().counts == outcomeA.counts()
    counter = backres_counts.get_result([cbits[4], cbits[7]]).counts
    assert counter is not None
    assert counter[OutcomeArray.from_readouts([[1, 1]])] == 2
    testvec = [1 + 1j, 3 + 4j, 1 + 1j, 3 + 4j]
    teststate = np.array(testvec)
    teststate /= np.sqrt(teststate.conjugate().dot(teststate))
    testunitary = np.array([teststate] * 4).T
    testdensitymatrix = np.array([[0.5, 0.5], [0.5, 0.5]])

    backres_state = BackendResult(q_bits=qbits[:2], state=teststate)
    backres_unitary = BackendResult(q_bits=qbits[:2], unitary=testunitary)
    backres_density_matrix = BackendResult(
        q_bits=qbits[:1], density_matrix=testdensitymatrix
    )
    with pytest.raises(ValueError) as errorinfo:
        backres_state.get_result([qbits[1]])
        assert (
            "For state/unitary results only a permutation of all qubits can be requested."
            in str(errorinfo.value)
        )

    with pytest.raises(ValueError) as errorinfo:
        backres_unitary.get_result([qbits[1]])
        assert (
            "For state/unitary results only a permutation of all qubits can be requested."
            in str(errorinfo.value)
        )

    with pytest.raises(ValueError) as errorinfo:
        backres_density_matrix.get_result([qbits[1]])
        assert (
            "For density_matrix results only a permutation of all qubits can be requested."
            in str(errorinfo.value)
        )
    state = backres_state.get_result().state
    assert state is not None
    assert np.array_equal(state, teststate)
    unitary = backres_unitary.get_result().unitary
    assert unitary is not None
    assert np.array_equal(unitary, testunitary)
    density_matrix = backres_density_matrix.get_result().density_matrix
    assert density_matrix is not None
    assert np.array_equal(density_matrix, testdensitymatrix)

    assert backres_shots.get_counts() == readout_counts(outcomeA.counts())
    with pytest.raises(InvalidResultType) as errorinfores:
        backres_state.get_shots()
        assert "shots" in str(errorinfores.value)

    assert np.array_equal(backres_unitary.get_state(), teststate)

    with pytest.deprecated_call():
        shots_dist0 = backres_shots.get_distribution()
        assert len(shots_dist0) == 2
        assert shots_dist0[tuple(shots_list[0])] == 0.5
        assert shots_dist0[tuple(shots_list[1])] == 0.5

        state_dist0 = backres_state.get_distribution([qbits[1], qbits[0]])
        assert np.isclose(state_dist0[(0, 1)], abs(teststate[2]) ** 2)
        assert np.isclose(state_dist0[(1, 0)], abs(teststate[1]) ** 2)

    shots_dist = backres_shots.get_empirical_distribution()
    assert len(shots_dist.support) == 2
    assert shots_dist[tuple(shots_list[0])] == 1
    assert shots_dist[tuple(shots_list[1])] == 1

    with pytest.raises(InvalidResultType):
        backres_shots.get_probability_distribution()

    state_dist = backres_state.get_probability_distribution([qbits[1], qbits[0]])
    assert np.isclose(state_dist[(0, 1)], abs(teststate[2]) ** 2)
    assert np.isclose(state_dist[(1, 0)], abs(teststate[1]) ** 2)

    with pytest.raises(InvalidResultType):
        backres_state.get_empirical_distribution()


def test_backendresult_ppcirc() -> None:
    # TKET-1260

    # classical CX circuit:
    cx = Circuit(0, 2)
    cx.add_c_transform([0, 3, 2, 1], [0, 1], "ClCX")

    raw_readouts = [[0, 0]] * 1 + [[0, 1]] * 2 + [[1, 0]] * 3 + [[1, 1]] * 4
    outcomes = OutcomeArray.from_readouts(raw_readouts)

    backres_shots = BackendResult(shots=outcomes)
    shots = backres_shots.get_shots(ppcirc=cx)
    assert (shots == [[0, 0]] * 1 + [[0, 1]] * 2 + [[1, 1]] * 3 + [[1, 0]] * 4).all()

    backres_counts = BackendResult(counts=outcomes.counts())
    counts = backres_counts.get_counts(ppcirc=cx)
    assert counts == Counter({(0, 0): 1, (0, 1): 2, (1, 1): 3, (1, 0): 4})


def test_backendresult_ppcirc_init() -> None:
    # Provide ppcirc to BackendResult constructor.

    # classical CX circuit:
    cx = Circuit(0, 2)
    cx.add_c_transform([0, 3, 2, 1], [0, 1], "ClCX")

    raw_readouts = [[0, 0]] * 1 + [[0, 1]] * 2 + [[1, 0]] * 3 + [[1, 1]] * 4
    outcomes = OutcomeArray.from_readouts(raw_readouts)

    backres_shots = BackendResult(shots=outcomes, ppcirc=cx)
    shots = backres_shots.get_shots()
    assert (shots == [[0, 0]] * 1 + [[0, 1]] * 2 + [[1, 1]] * 3 + [[1, 0]] * 4).all()

    backres_counts = BackendResult(counts=outcomes.counts(), ppcirc=cx)
    counts = backres_counts.get_counts()
    assert counts == Counter({(0, 0): 1, (0, 1): 2, (1, 1): 3, (1, 0): 4})


@given(outcomearrays())
def test_outcomearray_serialization(outcome: OutcomeArray) -> None:
    serializable = outcome.to_dict()
    assert OutcomeArray.from_dict(serializable) == outcome
    assert json.loads(json.dumps(serializable)) == serializable


@given(backendresults())
def test_backendresult_serialization(backres: BackendResult) -> None:
    serializable = backres.to_dict()
    assert BackendResult.from_dict(serializable) == backres
    assert json.loads(json.dumps(serializable)) == serializable


@given(
    n_shots=strategies.integers(min_value=1, max_value=10),
    n_bits=strategies.integers(min_value=0, max_value=10),
)
def test_empty_result(n_shots: int, n_bits: int) -> None:
    circuit = Circuit(n_bits, n_bits)
    backend_results = Backend.empty_result(circuit, n_shots=n_shots)

    empty_shots = np.zeros((n_shots, n_bits), dtype=int)
    empty_shots_shape = (n_shots, n_bits)
    empty_counts = Counter({(0,) * n_bits: n_shots})
    assert np.array_equal(backend_results.get_shots(), empty_shots)
    assert backend_results.get_shots().shape == empty_shots_shape
    assert backend_results.get_counts() == empty_counts


@given(
    status=strategies.sampled_from(StatusEnum),
    message=strategies.text(),
)
@settings(deadline=None)
def test_status_serialization_basic(status: StatusEnum, message: str) -> None:
    c_stat = CircuitStatus(status, message)
    assert CircuitStatus.from_dict(c_stat.to_dict()) == c_stat
    with pytest.raises(ValueError) as errorinfo:
        c_stat = CircuitStatus.from_dict({"message": "asf", "status": "COMPETED"})
        assert "invalid format" in str(errorinfo.value)


@given(
    c_stat=strategies.builds(CircuitStatus),
)
@settings(deadline=None)
def test_status_serialization(c_stat: CircuitStatus) -> None:
    assert CircuitStatus.from_dict(c_stat.to_dict()) == c_stat


def test_shots_with_unmeasured() -> None:
    # TKET-1193
    b = TketSimShotBackend(ignore_measures=True)
    c = Circuit(4, 3).H(0).X(1).H(2).H(3)
    c.add_gate(OpType.Measure, [0, 1])
    c.add_gate(OpType.Measure, [1, 0])
    assert b.valid_circuit(c)
    h = b.process_circuit(c, n_shots=10)
    r = b.get_result(h)
    shots = r.get_shots()
    assert all(shots[i, 0] == 1 for i in range(10))
    assert all(shots[i, 2] == 0 for i in range(10))
    shots1 = r.get_shots(cbits=[Bit(1), Bit(2), Bit(0)])
    assert all(shots1[i, 2] == 1 for i in range(10))
    assert all(shots1[i, 1] == 0 for i in range(10))


def test_tket_sim_backend_equivalence_with_circuit_functions() -> None:
    circ = Circuit(3)
    circ.Ry(0.5, 0)
    circ.CZ(0, 2)
    circ.X(1)
    circ.V(0)
    circ.H(2)

    # paranoia: get_statevector(), get_unitary() should not alter the circuit!
    orig_circ_str = repr(circ)

    state = circ.get_statevector()
    unitary = circ.get_unitary()
    assert orig_circ_str == repr(circ)

    backend = TketSimBackend()
    compiled_circ = backend.get_compiled_circuit(circ)
    assert orig_circ_str == repr(circ)
    compiled_str = repr(compiled_circ)
    assert orig_circ_str != compiled_str

    states = [circ.get_statevector(), compiled_circ.get_statevector()]
    unitaries = [circ.get_unitary(), compiled_circ.get_unitary()]

    result = backend.run_circuit(compiled_circ)
    states.append(result.get_state())

    # paranoia: get_state should not alter the circuit
    states.append(compiled_circ.get_statevector())
    unitaries.append(compiled_circ.get_unitary())

    for other_state in states:
        assert np.allclose(state, other_state)

    for other_unitary in unitaries:
        assert np.allclose(unitary, other_unitary)

    assert compiled_str == repr(compiled_circ)


def test_postprocess_1() -> None:
    b = TketSimShotBackend(ignore_measures=True)
    n_shots = 100
    seed = 7

    c = Circuit(2).H(0).CX(0, 1)
    c.measure_all()

    h = b.process_circuit(c, n_shots=n_shots, seed=seed, postprocess=False)
    r = b.get_result(h)
    orig_shots = r.get_shots()

    h = b.process_circuit(c, n_shots=n_shots, seed=seed, postprocess=True)
    r = b.get_result(h)
    shots = r.get_shots()
    counts = r.get_counts()

    assert (shots == orig_shots).all()

    assert all(readout[0] == readout[1] for readout in counts.keys())  # noqa: SIM118
    assert all(
        counts[(j, j)] == len([i for i in range(n_shots) if shots[i, 0] == j])
        for j in range(2)
    )


def test_postprocess_2() -> None:
    b = TketSimShotBackend(ignore_measures=True)
    c = Circuit(3).CX(0, 1).Y(0).H(1).CX(1, 2)
    c.measure_all()
    for xcirc in [None, Circuit(1).Y(0).Z(0)]:
        c0, _ppcirc = prepare_circuit(c, xcirc=xcirc)
        c0 = b.get_compiled_circuit(c0)
        # c0 should act trivially on qubits 0 and 2
        assert (
            len(
                {
                    arg
                    for cmd in c0.get_commands()
                    for arg in cmd.args
                    if cmd.op.type != OpType.Measure
                }
            )
            == 1
        )
        h = b.process_circuit(c, n_shots=100, postprocess=True)
        r = b.get_result(h)
        counts = r.get_counts()
        assert all(
            readout[0] == 1 and readout[1] == readout[2]
            for readout in counts.keys()  # noqa: SIM118
        )
        counts1 = r.get_counts(cbits=[Bit(1), Bit(0)])
        assert counts1 == Counter(
            {(readout[1], readout[0]): count for readout, count in counts.items()}
        )


def test_postprocess_3() -> None:
    b = TketSimShotBackend(ignore_measures=True)
    qbs = [Node("qn", i) for i in range(4)]
    arc = Architecture([(qbs[i], qbs[i + 1]) for i in range(3)])
    c = Circuit(3, 3).H(0).CX(0, 2).measure_all()

    mm = MappingManager(arc)
    rc = c.copy()
    mm.route_circuit(rc, [LexiLabellingMethod(), LexiRouteRoutingMethod()])
    n_shots = 100
    h = b.process_circuit(b.get_compiled_circuit(c), n_shots=n_shots, postprocess=True)
    r = b.get_result(h)
    counts = r.get_counts()
    assert all(
        readout[1] == 0 and readout[0] == readout[2]
        for readout in counts.keys()  # noqa: SIM118
    )


def test_postprocess_4() -> None:
    b = TketSimShotBackend(ignore_measures=True)
    c = Circuit(3, 2)
    c.X(0).H(1).H(2).CY(1, 2)
    c.measure_all()
    c.S(0).CX(0, 1)
    n_shots = 100
    h = b.process_circuit(b.get_compiled_circuit(c), n_shots=n_shots, postprocess=True)
    r = b.get_result(h)
    counts = r.get_counts()
    assert all(readout[0] == 1 for readout in counts.keys())  # noqa: SIM118


def test_empty_backenresult() -> None:
    # https://github.com/quantinuum/tket/issues/961
    r = BackendResult(counts=Counter(), c_bits=[Bit(name="A", index=0)])
    assert r.get_counts() == Counter()


def test_int_type() -> None:
    # https://github.com/quantinuum/tket/issues/1677
    b = TketSimShotBackend(ignore_measures=True)
    c = Circuit(2).H(0).H(1).measure_all()
    c1 = b.get_compiled_circuit(c)
    h = b.process_circuit(c1, n_shots=10)
    r = b.get_result(h)
    counts = r.get_counts()
    for k, v in counts.items():
        assert all(isinstance(i, int) for i in k)
        assert isinstance(v, int)
