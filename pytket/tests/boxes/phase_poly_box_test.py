import numpy as np
import pytest

from pytket.circuit import Bit, Circuit, OpType, PhasePolyBox, Qubit
from pytket.circuit.named_types import PhasePolynomialDict, PhasePolynomialSequence
from pytket.passes import ComposePhasePolyBoxes, DecomposeBoxes
from pytket.utils import compare_unitaries


def phase_polynomials_are_equal(
    phase_poly_0: PhasePolynomialDict | PhasePolynomialSequence,
    phase_poly_1: PhasePolynomialDict | PhasePolynomialSequence,
) -> bool:
    if isinstance(phase_poly_0, dict) and isinstance(phase_poly_1, dict):
        return phase_poly_0 == phase_poly_1
    # If lists are given the dicts created from them should compare equal.
    # Lists of pairs can contain duplicate "keys", this ensures the last defined value
    # for a key is what is compared, replicating a dict definition with duplicate keys.
    if isinstance(phase_poly_0, list) and isinstance(phase_poly_1, list):
        to_compare_0 = {tuple(pair[0]): pair[1] for pair in phase_poly_0}
        to_compare_1 = {tuple(pair[0]): pair[1] for pair in phase_poly_1}
        return to_compare_0 == to_compare_1
    raise NotImplementedError("comparison not implemented")


def test_phase_polybox() -> None:
    c = Circuit(1, 1)
    n_qb = 1
    qubit_indices = {Qubit(0): 0}
    phase_polynomial: PhasePolynomialDict = {(True,): 0.1}
    phase_polynomial_alt: PhasePolynomialSequence = [([True], 0.1)]
    linear_transformation = np.array([[1]])
    p_box = PhasePolyBox(n_qb, qubit_indices, phase_polynomial, linear_transformation)
    p_box_alt = PhasePolyBox(
        n_qb, qubit_indices, phase_polynomial_alt, linear_transformation
    )

    b = p_box.get_circuit()

    p_box_ii = PhasePolyBox(b)

    c.add_phasepolybox(p_box, [0])
    c.add_phasepolybox(p_box, [Qubit(0)])
    c.add_phasepolybox(p_box_alt, [0])

    c.add_phasepolybox(p_box_ii, [0])

    assert p_box.n_qubits == n_qb
    assert p_box_ii.n_qubits == n_qb
    assert p_box.qubit_indices == qubit_indices
    assert p_box_ii.qubit_indices == qubit_indices
    assert phase_polynomials_are_equal(
        p_box.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(
        p_box_alt.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box_alt.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(p_box_ii.phase_polynomial, phase_polynomial)
    assert np.array_equal(p_box.linear_transformation, linear_transformation)
    assert np.array_equal(p_box_ii.linear_transformation, linear_transformation)
    assert DecomposeBoxes().apply(c)


def test_phase_polybox_II() -> None:
    c = Circuit(1, 1)
    n_qb = 1
    qubit_indices = {Qubit(0): 0}
    phase_polynomial: PhasePolynomialDict = {(True,): 0.3}
    phase_polynomial_alt: PhasePolynomialSequence = [([True], 0.3)]
    linear_transformation = np.array([[1]])
    p_box = PhasePolyBox(n_qb, qubit_indices, phase_polynomial, linear_transformation)
    p_box_alt = PhasePolyBox(
        n_qb, qubit_indices, phase_polynomial_alt, linear_transformation
    )

    b = p_box.get_circuit()

    p_box_ii = PhasePolyBox(b)

    c.add_phasepolybox(p_box, [0])
    c.add_phasepolybox(p_box, [Qubit(0)])

    c.add_phasepolybox(p_box_ii, [0])

    assert p_box.n_qubits == n_qb
    assert p_box_ii.n_qubits == n_qb
    assert p_box.qubit_indices == qubit_indices
    assert p_box_ii.qubit_indices == qubit_indices
    assert phase_polynomials_are_equal(p_box.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(
        p_box.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box_alt.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(
        p_box_alt.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box_ii.phase_polynomial, phase_polynomial)
    assert np.array_equal(p_box.linear_transformation, linear_transformation)
    assert np.array_equal(p_box_ii.linear_transformation, linear_transformation)
    assert DecomposeBoxes().apply(c)


def test_phase_polybox_big() -> None:
    c = Circuit(3, 3)
    n_qb = 3
    qubit_indices = {Qubit(0): 0, Qubit(1): 1, Qubit(2): 2}
    phase_polynomial: PhasePolynomialDict = {
        (True, False, True): 0.333,
        (False, False, True): 0.05,
        (False, True, False): 1.05,
    }
    phase_polynomial_alt: PhasePolynomialSequence = [
        ([True, False, True], 0.333),
        ([False, False, True], 0.05),
        ([False, True, False], 1.05),
    ]
    linear_transformation = np.array([[1, 1, 0], [0, 1, 0], [0, 0, 1]])
    p_box = PhasePolyBox(n_qb, qubit_indices, phase_polynomial, linear_transformation)
    p_box_alt = PhasePolyBox(
        n_qb, qubit_indices, phase_polynomial_alt, linear_transformation
    )

    b = p_box.get_circuit()

    p_box_ii = PhasePolyBox(b)

    c.add_phasepolybox(p_box, [0, 1, 2])
    c.add_phasepolybox(p_box_alt, [0, 1, 2])
    c.add_phasepolybox(p_box_ii, [0, 1, 2])
    assert p_box.n_qubits == n_qb
    assert p_box_ii.n_qubits == n_qb
    assert p_box.qubit_indices == qubit_indices
    assert p_box_ii.qubit_indices == qubit_indices
    assert phase_polynomials_are_equal(p_box.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(
        p_box.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box_alt.phase_polynomial, phase_polynomial)
    assert phase_polynomials_are_equal(
        p_box_alt.phase_polynomial_as_list, phase_polynomial_alt
    )
    assert phase_polynomials_are_equal(p_box_ii.phase_polynomial, phase_polynomial)
    assert np.array_equal(p_box.linear_transformation, linear_transformation)
    assert np.array_equal(p_box_ii.linear_transformation, linear_transformation)
    assert DecomposeBoxes().apply(c)


def test_compose_phase_polybox_default_registers() -> None:
    c = Circuit(3)
    c.CX(0, 2).CX(1, 0)
    d = c.copy()
    ComposePhasePolyBoxes().apply(c)
    assert compare_unitaries(d.get_unitary(), c.get_unitary())


def test_compose_phase_polybox_one_registers() -> None:
    c = Circuit(3)
    areg = c.add_q_register("a", 3)
    c.CX(areg[0], areg[2]).CX(areg[1], areg[0])
    d = c.copy()
    ComposePhasePolyBoxes().apply(c)
    DecomposeBoxes().apply(c)
    assert compare_unitaries(d.get_unitary(), c.get_unitary())


def test_compose_phase_polybox_custom_registers() -> None:
    c = Circuit()
    areg = c.add_q_register("a", 2)
    breg = c.add_q_register("b", 1)
    c.CX(areg[0], breg[0]).CX(areg[1], areg[0])
    ComposePhasePolyBoxes().apply(c)


@pytest.mark.parametrize("min_size", [0, 2, 10])
@pytest.mark.parametrize(
    "qubits",
    [
        [Qubit("p", i) for i in range(3)],
        [Qubit("a", 1), Qubit("b", 4), Qubit("z", [2, 3])],
        [Qubit(i) for i in (1, 3, 7)],
        [Qubit(0), Qubit("a", 2), Qubit("b", 4)],
    ],
)
def test_compose_phase_polybox_custom_register_boundaries(
    qubits: list[Qubit], min_size: int
) -> None:
    c = Circuit()
    for qubit in qubits:
        c.add_qubit(qubit)
    a, b, d = qubits
    c.CX(a, b).T(b).CX(a, b).H(a).CX(a, d)
    c.T(d).CX(a, d).CX(a, b).T(b).H(a).CX(a, b)
    c.add_barrier(qubits)
    c.Rz(0.3, b).CX(b, d)
    c.add_phase(0.37)
    expected_unitary = c.get_unitary()
    expected_qubits = c.qubits

    assert ComposePhasePolyBoxes(min_size=min_size).apply(c)
    assert c.qubits == expected_qubits
    if min_size == 0:
        assert c.n_gates_of_type(OpType.PhasePolyBox) > 0
    if min_size == 10:
        assert c.n_gates_of_type(OpType.PhasePolyBox) == 0
    DecomposeBoxes().apply(c)
    np.testing.assert_allclose(c.get_unitary(), expected_unitary, atol=1e-10)


@pytest.mark.parametrize("min_size", [0, 10])
def test_compose_phase_polybox_custom_classical_registers(min_size: int) -> None:
    a, b = Qubit("p", 3), Qubit("p", 7)
    first, second = Bit("readout", 2), Bit("flag", 5)
    c = Circuit()
    for qubit in (a, b):
        c.add_qubit(qubit)
    for bit in (first, second):
        c.add_bit(bit)
    c.H(a).Measure(a, first).CX(a, b)
    c.Reset(a).add_gate(OpType.Collapse, [b])
    c.Measure(b, second).H(b).CX(a, b).Measure(a, first)
    expected = c.copy()
    assert ComposePhasePolyBoxes(min_size=min_size).apply(c)
    assert c.qubits == expected.qubits
    assert c.bits == expected.bits
    DecomposeBoxes().apply(c)
    assert c == expected


def test_add_and_decompose_phase_polybox_custom_registers() -> None:
    c = Circuit()
    areg = c.add_q_register("a", 2)
    breg = c.add_q_register("b", 1)
    c.CX(areg[0], breg[0]).CX(areg[1], areg[0])
    ppbox = PhasePolyBox(c)
    d = Circuit(3)
    d.add_phasepolybox(ppbox, [0, 1, 2])
    DecomposeBoxes().apply(d)
    assert compare_unitaries(d.get_unitary(), c.get_unitary())
