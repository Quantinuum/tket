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
import math
import pickle
from math import sqrt
from pathlib import Path
from typing import TYPE_CHECKING, Any

import numpy as np
import pytest
import strategies as st  # type: ignore
from hypothesis import given, settings
from jsonschema import validate  # type: ignore
from scipy.linalg import block_diag
from sympy import Expr, Symbol, exp, pi, sympify

from pytket.circuit import (
    Bit,
    BitRegister,
    CircBox,
    Circuit,
    Command,
    ConjugationBox,
    CustomGate,
    CustomGateDef,
    CXConfigType,
    DiagonalBox,
    DummyBox,
    ExpBox,
    MultiplexedRotationBox,
    MultiplexedTensoredU2Box,
    MultiplexedU2Box,
    MultiplexorBox,
    Node,
    Op,
    OpType,
    PauliExpBox,
    PauliExpCommutingSetBox,
    PauliExpPairBox,
    QControlBox,
    Qubit,
    QubitRegister,
    ResourceBounds,
    ResourceData,
    StatePreparationBox,
    TermSequenceBox,
    ToffoliBox,
    ToffoliBoxSynthStrat,
    Unitary1qBox,
    Unitary2qBox,
    Unitary3qBox,
    fresh_symbol,
)
from pytket.circuit.display import get_circuit_renderer, render_circuit_as_html
from pytket.circuit_library import CnX_vchain_decomp
from pytket.passes import (
    CliffordSimp,
    DecomposeBoxes,
    RemoveRedundancies,
    SynthesiseTket,
)
from pytket.pauli import Pauli
from pytket.transform import PauliSynthStrat, Transform

if TYPE_CHECKING:
    from pytket.circuit.named_types import (
        BitstringToOpList,
        BitstringToOpMap,
        BitstringToTensoredOpList,
        BitstringToTensoredOpMap,
        ParamType,
        PermutationMap,
    )

curr_file_path = Path(__file__).resolve().parent

with open(curr_file_path.parent.parent / "schemas/circuit_v1.json") as f:
    schema = json.load(f)

_0 = False
_1 = True


def json_validate(circ: Circuit) -> bool:
    serializable_form = circ.to_dict()
    validate(instance=serializable_form, schema=schema)
    return circ == Circuit.from_dict(serializable_form)


def test_op_free_symbols() -> None:
    c = Circuit(2)
    c.add_barrier([0, 1])
    op = c.get_commands()[0].op
    assert op.free_symbols() == set()
    alpha = Symbol("alpha")
    c.Rx(alpha, 0)
    op = c.get_commands()[1].op
    assert op.free_symbols() == {alpha}


def test_circuit_transpose() -> None:
    c = Circuit(2)
    u = np.asarray([[0, 1, 0, 0], [0, 0, 0, 1], [0, 0, 1, 0], [1, 0, 0, 0]])
    ubox = Unitary2qBox(u)
    c.add_unitary2qbox(ubox, 1, 0)
    c.CX(0, 1)
    c_t = c.transpose()
    commands = c_t.get_commands()
    assert str(commands[0]) == "CX q[0], q[1];"
    assert str(commands[1]) == "Unitary2qBox q[1], q[0];"
    assert isinstance(commands[1].op, Unitary2qBox)
    assert commands[1].op.get_matrix().all() == u.transpose().all()


def test_circuit_dagger() -> None:
    c = Circuit(2)
    u = np.asarray([[0, 1, 0, 0], [0, 0, 0, 1], [0, 0, 1j, 0], [1, 0, 0, 0]])
    ubox = Unitary2qBox(u)
    c.add_unitary2qbox(ubox, 1, 0)
    c.add_gate(OpType.CnRy, 0.3, [0, 1])
    c_d = c.dagger()
    commands = c_d.get_commands()
    assert str(commands[0]) == "CnRy(3.7) q[0], q[1];"
    assert commands[0].qubits == [Qubit(0), Qubit(1)]
    assert str(commands[1]) == "Unitary2qBox q[1], q[0];"
    assert isinstance(commands[1].op, Unitary2qBox)
    assert commands[1].op.get_matrix().all() == u.conj().transpose().all()


def test_circuit_dagger_transpose_with_barriers() -> None:
    c = Circuit(2).S(0).add_barrier([0, 1]).CX(0, 1)
    c_d = c.dagger()
    assert c_d == Circuit(2).CX(0, 1).add_barrier([0, 1]).Sdg(0)
    c = Circuit(2).Ry(0.3, 0).add_barrier([0, 1]).CX(0, 1)
    c_t = c.transpose()
    assert c_t == Circuit(2).CX(0, 1).add_barrier([0, 1]).Ry(-0.3, 0)


# TKET-1365 bug
def test_cnx_dagger() -> None:
    c = Circuit(2)
    c.add_gate(OpType.CnX, [0, 1])
    for d in (c.dagger(), c.transpose()):
        d_dict = d.to_dict()

        args = d_dict["commands"][0]["args"]
        # before bugfix args would be None, due to empty signature
        assert args == [["q", [0]], ["q", [1]]]


def test_circuit_name() -> None:
    name = "test"
    a = Circuit(name)
    b = Circuit(2, name=name)
    c = Circuit(2, 2, name)

    assert a.name == b.name == c.name

    a.name = "new_test"
    assert a.name == "new_test"
    assert a.name != b.name

    c = Circuit(4, 4, name)
    c.X(0)
    c.H(1)
    c.S(1)
    c.CX(2, 0)
    c.CRz(0.5, 0, 3)
    c.Measure(3, 3)
    c.Measure(1, 1)

    SynthesiseTket().apply(c)
    assert c.name == name


def test_circuit_gen() -> None:  # noqa: PLR0915
    c = Circuit(4, 4)
    c.X(0)
    c.H(1)
    c.S(1)
    c.CX(2, 0)
    c.CRz(0.5, 0, 3)
    c.CRx(0.5, 0, 3)
    c.CRy(0.5, 0, 3)
    c.CV(0, 3)
    c.CVdg(1, 3)
    c.CSX(0, 2)
    c.CSXdg(1, 2)
    c.SX(3)
    c.SXdg(0)
    c.Measure(3, 3)
    c.Measure(1, 1)
    c.U1(0.25, 1)
    c.U2(0.25, 0.25, 3)
    c.U3(0.25, 0.25, 0.25, 2)
    c.TK1(0.3, 0.3, 0.3, 0)
    c.TK2(0.3, 0.3, 0.3, 0, 1)
    c.CU1(0.25, 0, 1)
    c.CU3(0.25, 0.25, 0.25, 0, 1)
    c.ISWAP(0.4, 1, 2)
    c.PhasedISWAP(0.5, 0.6, 2, 3)
    c.PhasedX(0.2, 0.3, 3)
    c.ESWAP(0.9, 3, 0)
    c.FSim(0.2, 0.4, 0, 1)
    c.Sycamore(1, 2)
    c.ISWAPMax(2, 3)
    c.CS(0, 2)
    c.CSdg(1, 2)

    assert c.n_qubits == 4
    assert c._n_vertices() == 47  # noqa: SLF001
    assert c.n_gates == 31

    commands = c.get_commands()
    assert len(commands) == 31
    assert str(commands[0]) == "X q[0];"
    assert str(commands[2]) == "CX q[2], q[0];"
    assert str(commands[4]) == "CRz(0.5) q[0], q[3];"
    assert str(commands[5]) == "CRx(0.5) q[0], q[3];"
    assert str(commands[6]) == "CRy(0.5) q[0], q[3];"
    assert str(commands[7]) == "CV q[0], q[3];"
    assert str(commands[8]) == "CSX q[0], q[2];"
    assert str(commands[9]) == "CVdg q[1], q[3];"
    assert str(commands[10]) == "SXdg q[0];"
    assert str(commands[11]) == "CSXdg q[1], q[2];"
    assert str(commands[12]) == "SX q[3];"
    assert str(commands[13]) == "Measure q[1] --> c[1];"
    assert str(commands[14]) == "Measure q[3] --> c[3];"
    assert str(commands[15]) == "TK1(0.3, 0.3, 0.3) q[0];"
    assert str(commands[16]) == "U3(0.25, 0.25, 0.25) q[2];"
    assert str(commands[17]) == "U1(0.25) q[1];"
    assert str(commands[18]) == "U2(0.25, 0.25) q[3];"
    assert str(commands[19]) == "TK2(0.3, 0.3, 0.3) q[0], q[1];"
    assert str(commands[20]) == "CU1(0.25) q[0], q[1];"
    assert str(commands[21]) == "CU3(0.25, 0.25, 0.25) q[0], q[1];"
    assert str(commands[22]) == "ISWAP(0.4) q[1], q[2];"
    assert str(commands[23]) == "PhasedISWAP(0.5, 0.6) q[2], q[3];"
    assert str(commands[24]) == "PhasedX(0.2, 0.3) q[3];"
    assert str(commands[25]) == "ESWAP(0.9) q[3], q[0];"
    assert str(commands[26]) == "FSim(0.2, 0.4) q[0], q[1];"
    assert str(commands[27]) == "Sycamore q[1], q[2];"
    assert str(commands[28]) == "ISWAPMax q[2], q[3];"
    assert str(commands[29]) == "CS q[0], q[2];"
    assert str(commands[30]) == "CSdg q[1], q[2];"

    assert commands[14].qubits == [Qubit(3)]
    assert commands[14].bits == [Bit(3)]
    assert c.depth_by_type({OpType.CX, OpType.CRz}) == 2
    assert commands[0] == Command(Op.create(OpType.X), [Qubit(0)])


def test_circuit_gen_ids() -> None:
    c = Circuit()
    a = [Qubit("a", i) for i in range(4)]
    b = [Bit("b", i) for i in range(4)]
    for q in a:
        c.add_qubit(q)
    for cb in b:
        c.add_bit(cb)
    c.add_gate(OpType.X, [a[0]])
    c.H(a[1])
    c.add_gate(OpType.Rz, 0.5, [a[1]])
    c.CX(a[2], a[0])
    c.CRz(0.5, a[0], a[3])
    c.Measure(a[3], b[3])
    c.Measure(a[1], b[1])

    assert c.n_qubits == 4
    assert c._n_vertices() == 23  # noqa: SLF001
    assert c.n_gates == 7

    commands = c.get_commands()
    assert len(commands) == 7
    assert str(commands[0]) == "X a[0];"
    assert str(commands[2]) == "CX a[2], a[0];"
    assert str(commands[3]) == "Rz(0.5) a[1];"
    assert str(commands[6]) == "Measure a[3] --> b[3];"


def test_symbolic_ops() -> None:
    c = Circuit(2)
    alpha = Symbol("alpha")
    c.Rx(alpha, 0)
    beta = fresh_symbol("alpha")
    c.CRz(beta * 2, 1, 0)
    gamma = Symbol("gamma")
    # https://github.com/quantinuum/tket/issues/1068
    c.Rz(exp(gamma), 1)
    s_map = {alpha: 0.5, beta: 3.2, gamma: 1}
    assert c.is_symbolic()
    assert c.free_symbols() == {alpha, beta, gamma}
    c.symbol_substitution(s_map)
    assert not c.is_symbolic()

    commands = c.get_commands()
    assert beta.__str__() == "alpha_1"
    assert np.allclose(np.asarray(commands[0].op.params), [0.5], atol=1e-10)
    assert np.allclose(np.asarray(commands[1].op.params), [2.4], atol=1e-10)
    assert np.allclose(np.asarray(commands[2].op.params), [math.e], atol=1e-10)


def test_symbolic_circbox() -> None:
    c = Circuit(2)
    c_outer = Circuit(2)
    alpha = Symbol("alpha")
    c.Rx(alpha, 0)
    beta = fresh_symbol("alpha")
    c.CRz(beta * 2, 1, 0)
    s_map: dict[Symbol, ParamType] = {alpha: 0.5, beta: 3.2}
    circ_box = CircBox(c)
    assert circ_box.free_symbols() == {alpha, beta}
    assert circ_box.get_circuit().is_symbolic()
    c_outer.add_circbox(circ_box, [0, 1])
    assert c_outer.is_symbolic()
    assert c_outer.free_symbols() == {alpha, beta}
    circ_box.symbol_substitution(s_map)
    assert len(circ_box.free_symbols()) == 0
    assert not circ_box.get_circuit().is_symbolic()
    assert not c_outer.is_symbolic()


def test_renaming_circbox_circuit() -> None:
    c = Circuit(2).CX(0, 1)
    cbox = CircBox(c)
    d = Circuit(2).add_circbox(cbox, [0, 1])
    cbox.circuit_name = "test_name"
    assert cbox.circuit_name == "test_name"
    assert d.get_commands()[0].op.circuit_name == "test_name"  # type: ignore


def test_subst_4() -> None:
    # https://github.com/quantinuum/tket/issues/219
    m = fresh_symbol("m")
    c = Circuit(1)
    a = m / 4
    c.add_gate(OpType.Rx, a, [0])
    c.symbol_substitution({m: 4})
    angle = float(c.get_commands()[0].op.params[0])
    assert np.isclose(angle, 1.0)


def test_sympy_conversion() -> None:
    def get_type_tree(expr: Expr) -> str:
        # Format e.g. "<class 'sympy.core.numbers.Pi'>" to "Pi"
        tree_str = str(type(expr)).rsplit(".", 1)[-1].split("'")[0]
        if len(expr.args) != 0:
            tree_str += " ("
            tree_str += ", ".join([get_type_tree(a) for a in expr.args])
            tree_str += ")"
        return tree_str

    test_dict = {
        "Abs(x**2)": "Abs (Pow (Symbol, Integer))",
        "atan2(0.436, pi*I + b)": "atan2 (Float, Add (Symbol, Mul (Pi, ImaginaryUnit)))",
        "cos(a)": "cos (Symbol)",
        "a*oo": "Mul (Infinity, Symbol)",
        "a*-oo": "Mul (NegativeInfinity, Symbol)",
        "a*zoo": "Mul (ComplexInfinity, Symbol)",
        "a*E": "Mul (Exp1, Symbol)",
        "atan2(a, nan)": "atan2 (Symbol, NaN)",
    }
    for expr_string, type_tree in test_dict.items():
        c = Circuit(1)
        c.Rz(sympify(expr_string), 0)
        com = c.get_commands()[0]
        param0 = com.op.params[0]
        assert isinstance(param0, Expr)
        assert get_type_tree(param0) == type_tree


def test_4x4_matrix_to_circ() -> None:
    u = np.asarray([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, -1]])
    ubox = Unitary2qBox(u)
    c = Circuit(2)
    c.add_unitary2qbox(ubox, 0, 1)
    Transform.DecomposeBoxes().apply(c)
    Transform.OptimiseCliffords().apply(c)
    mat = c.get_unitary()
    assert np.allclose(mat, u)
    assert c.n_gates_of_type(OpType.CX) == 1


def test_8x8_matrix_to_circ() -> None:
    u = np.asarray(
        [
            [1, 0, 0, 0, 0, 0, 0, 0],
            [0, 1, 0, 0, 0, 0, 0, 0],
            [0, 0, 1, 0, 0, 0, 0, 0],
            [0, 0, 0, 1, 0, 0, 0, 0],
            [0, 0, 0, 0, 1, 0, 0, 0],
            [0, 0, 0, 0, 0, 1, 0, 0],
            [0, 0, 0, 0, 0, 0, 1, 0],
            [0, 0, 0, 0, 0, 0, 0, -1],
        ]
    )
    ubox = Unitary3qBox(u)
    c = Circuit(3)
    c.add_unitary3qbox(ubox, 0, 1, 2)
    Transform.DecomposeBoxes().apply(c)
    Transform.OptimiseCliffords().apply(c)
    mat = c.get_unitary()
    assert np.allclose(mat, u)
    assert c.n_gates_of_type(OpType.CX) <= 10


def test_exp_to_circ() -> None:
    PI = float(pi.evalf())
    u = (
        np.asarray(
            [[1, 0, 0, 0], [0, -1, 0, 0], [0, 0, -1, 0], [0, 0, 0, 1]],
            dtype=np.complex128,
        )
        * -PI
        / 4
    )
    ebox = ExpBox(u, 1.0)
    c = Circuit(2)
    c.add_expbox(ebox, 0, 1)
    Transform.DecomposeBoxes().apply(c)
    mat = c.get_unitary()
    correct = np.asarray(
        [[1 - 1j, 0, 0, 0], [0, 1 + 1j, 0, 0], [0, 0, 1 + 1j, 0], [0, 0, 0, 1 - 1j]]
    ) / np.sqrt(2)
    assert np.allclose(mat, correct)


def test_implicit_swaps() -> None:
    c = Circuit(2)
    c.CX(0, 1)
    c.CX(1, 0)
    perm = c.implicit_qubit_permutation()
    assert all(a == b for (a, b) in perm.items())
    CliffordSimp().apply(c)
    perm1 = c.implicit_qubit_permutation()
    assert all(a != b for (a, b) in perm1.items())


def test_boxes() -> None:  # noqa: PLR0915
    c = Circuit(4, 4)
    c.X(0)
    c.H(1)
    c.add_gate(OpType.ESWAP, 0.4, [0, 1])
    c.add_gate(OpType.FSim, [0.7, 0.1], [2, 3])
    c.S(1)
    c.CX(2, 0)
    c.CRz(0.5, 0, 3)
    cbox = CircBox(c)
    assert cbox.type == OpType.CircBox
    c1 = cbox.get_circuit()
    assert len(c1.get_commands()) == 7
    d = Circuit(4, 4)
    d.add_circbox(cbox, [0, 2, 1, 3, 3, 2, 1, 0])

    m = np.asarray([[1 / 2, sqrt(3) / 2], [sqrt(3) / 2, -1 / 2]])
    mbox = Unitary1qBox(m)
    m1 = mbox.get_matrix()
    assert np.allclose(m, m1)
    assert mbox.type == OpType.Unitary1qBox
    d.add_unitary1qbox(mbox, 3)

    u2q = np.asarray([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, -1]])
    u2qbox = Unitary2qBox(u2q)
    u2q_ = u2qbox.get_matrix()
    assert np.allclose(u2q, u2q_)
    assert u2qbox.type == OpType.Unitary2qBox
    d.add_unitary2qbox(u2qbox, 1, 2)

    u3q = np.asarray(
        [
            [1, 0, 0, 0, 0, 0, 0, 0],
            [0, 1, 0, 0, 0, 0, 0, 0],
            [0, 0, 1, 0, 0, 0, 0, 0],
            [0, 0, 0, 1, 0, 0, 0, 0],
            [0, 0, 0, 0, 1, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 1, 0],
            [0, 0, 0, 0, 0, 1, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 1],
        ]
    )
    u3qbox = Unitary3qBox(u3q)
    u3q_ = u3qbox.get_matrix()
    assert np.allclose(u3q, u3q_)
    assert u3qbox.type == OpType.Unitary3qBox
    d.add_unitary3qbox(u3qbox, 1, 2, 0)

    A = np.asarray(
        [[1, 2, 3, 4 + 1j], [2, 0, 1j, -1], [3, -1j, 2, 1j], [4 - 1j, -1, -1j, 1]]
    )
    ebox = ExpBox(A, 0.5)
    assert ebox.type == OpType.ExpBox
    d.add_expbox(ebox, 3, 2)

    paulis = [Pauli.X, Pauli.Z, Pauli.X]
    pbox = PauliExpBox(paulis, Symbol("alpha"))
    assert pbox.type == OpType.PauliExpBox
    d.add_pauliexpbox(pbox, [3, 2, 1])

    ppairbox = PauliExpPairBox(
        [Pauli.I, Pauli.X, Pauli.Y, Pauli.Z],
        Symbol("alpha"),
        [Pauli.Y, Pauli.I, Pauli.I, Pauli.X],
        Symbol("beta"),
    )
    assert ppairbox.type == OpType.PauliExpPairBox
    d.add_pauliexppairbox(ppairbox, [3, 2, 1, 0])

    psetbox = PauliExpCommutingSetBox(
        [
            ([Pauli.X, Pauli.X, Pauli.X, Pauli.Y], Symbol("alpha")),
            ([Pauli.X, Pauli.X, Pauli.Y, Pauli.X], Symbol("beta")),
            ([Pauli.X, Pauli.Y, Pauli.X, Pauli.X], Symbol("gamma")),
        ]
    )
    assert psetbox.type == OpType.PauliExpCommutingSetBox
    d.add_pauliexpcommutingsetbox(psetbox, [0, 1, 2, 3])

    tseqbox = TermSequenceBox(
        [
            ([Pauli.X, Pauli.X, Pauli.X, Pauli.Y], Symbol("alpha")),
            ([Pauli.X, Pauli.X, Pauli.Y, Pauli.X], Symbol("beta")),
            ([Pauli.X, Pauli.Y, Pauli.X, Pauli.X], Symbol("gamma")),
        ]
    )
    assert tseqbox.type == OpType.TermSequenceBox
    d.add_termsequencebox(tseqbox, [0, 1, 2, 3])

    qcbox = QControlBox(Op.create(OpType.S), 2)
    assert qcbox.type == OpType.QControlBox
    assert qcbox.get_op().type == OpType.S
    assert qcbox.get_n_controls() == 2
    d.add_qcontrolbox(qcbox, [1, 2, 3])
    assert d.n_gates == 10

    pauli_exps = [cmd.op for cmd in d if cmd.op.type == OpType.PauliExpBox]
    assert len(pauli_exps) == 1
    pauli_exp = pauli_exps[0]
    assert isinstance(pauli_exp, PauliExpBox)
    assert pauli_exp.get_paulis() == paulis
    assert pauli_exp.get_phase() == Symbol("alpha")

    boxes = (cbox, mbox, u2qbox, u3qbox, ebox, pbox, qcbox)
    assert all(box == box for box in boxes)  # noqa: PLR0124
    assert all(isinstance(box, Op) for box in boxes)
    permutation: PermutationMap = {(_0, _0): (_1, _1), (_1, _1): (_0, _0)}
    tb = ToffoliBox(permutation)
    assert tb.type == OpType.ToffoliBox
    unitary = tb.get_circuit().get_unitary()
    comparison = np.asarray([[0, 0, 0, 1], [0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, 0]])
    assert np.allclose(unitary, comparison)
    d.add_toffolibox(tb, [0, 1])
    assert d.n_gates == 11

    # MultiplexorBox, MultiplexedU2Box
    op_map: BitstringToOpMap = {
        (_0, _0): Op.create(OpType.Rz, 0.3),
        (_1, _1): Op.create(OpType.H),
    }
    op_map_alt: BitstringToOpList = [
        ([_0, _0], Op.create(OpType.Rz, 0.3)),
        ([_1, _1], Op.create(OpType.H)),
    ]
    multiplexor = MultiplexorBox(op_map)
    multiplexor_alt = MultiplexorBox(op_map_alt)
    assert multiplexor.get_op_map() == op_map
    assert multiplexor_alt.get_op_map() == op_map
    assert multiplexor.get_bitstring_op_pair_list() == op_map_alt
    assert multiplexor_alt.get_bitstring_op_pair_list() == op_map_alt

    ucu2_box = MultiplexedU2Box(op_map)
    ucu2_box_alt = MultiplexedU2Box(op_map_alt)
    assert ucu2_box.get_op_map() == op_map
    assert ucu2_box_alt.get_op_map() == op_map
    assert ucu2_box.get_bitstring_op_pair_list() == op_map_alt
    assert ucu2_box_alt.get_bitstring_op_pair_list() == op_map_alt
    c0 = multiplexor.get_circuit()
    DecomposeBoxes().apply(c0)
    unitary0 = c0.get_unitary()
    c1 = ucu2_box.get_circuit()
    DecomposeBoxes().apply(c1)
    unitary1 = c1.get_unitary()
    comparison = block_diag(
        Circuit(1).Rz(0.3, 0).get_unitary(),
        np.eye(2),
        np.eye(2),
        Circuit(1).H(0).get_unitary(),
    )
    assert np.allclose(unitary0, comparison)
    assert np.allclose(unitary1, comparison)
    d.add_multiplexor(multiplexor, [Qubit(0), Qubit(1), Qubit(2)])
    d.add_multiplexedu2(ucu2_box, [Qubit(0), Qubit(1), Qubit(2)])
    # constructor taking qubit indices
    d.add_multiplexor(multiplexor, [0, 1, 2])
    d.add_multiplexedu2(ucu2_box, [0, 1, 2])
    assert d.n_gates == 15
    # MultiplexedRotationBox
    op_map = {
        (_0, _0): Op.create(OpType.Rz, 0.3),
        (_1, _1): Op.create(OpType.Rz, 1.7),
    }
    op_map_alt = [
        ([_0, _0], Op.create(OpType.Rz, 0.3)),
        ([_1, _1], Op.create(OpType.Rz, 1.7)),
    ]
    multiplexed_rot = MultiplexedRotationBox(op_map)
    multiplexed_rot_alt = MultiplexedRotationBox(op_map_alt)
    assert multiplexed_rot.get_op_map() == op_map
    assert multiplexed_rot_alt.get_op_map() == op_map
    assert multiplexed_rot.get_bitstring_op_pair_list() == op_map_alt
    assert multiplexed_rot_alt.get_bitstring_op_pair_list() == op_map_alt
    c0 = multiplexed_rot.get_circuit()
    unitary = c0.get_unitary()
    comparison = block_diag(
        Circuit(1).Rz(0.3, 0).get_unitary(),
        np.eye(2),
        np.eye(2),
        Circuit(1).Rz(1.7, 0).get_unitary(),
    )
    assert np.allclose(unitary, comparison)
    d.add_multiplexedrotation(multiplexed_rot, [Qubit(0), Qubit(1), Qubit(2)])
    d.add_multiplexedrotation(multiplexed_rot, [1, 2, 0])
    assert d.n_gates == 17
    multiplexed_rot = MultiplexedRotationBox([0.3, 0, 0, 1.7], OpType.Rz)
    unitary = multiplexed_rot.get_circuit().get_unitary()
    assert np.allclose(unitary, comparison)
    d.add_multiplexedrotation(multiplexed_rot, [Qubit(0), Qubit(1), Qubit(2)])
    assert d.n_gates == 18
    # StatePreparationBox
    state = np.array([np.sqrt(0.125)] * 8)
    prep_box = StatePreparationBox(state)
    prep_state = prep_box.get_circuit().get_statevector()
    assert np.allclose(state, prep_state)
    prep_box = StatePreparationBox(state, True)
    prep_u = prep_box.get_circuit().get_unitary()
    zero_state = np.zeros(8)
    zero_state[0] = 1
    assert np.allclose(prep_u.dot(state), zero_state)
    d.add_state_preparation_box(prep_box, [Qubit(0), Qubit(1), Qubit(2)])
    d.add_state_preparation_box(prep_box, [2, 1, 0])
    assert d.n_gates == 20
    # DiagonalBox
    diag_vect = np.array([1j] * 8)
    diag_box = DiagonalBox(diag_vect)
    u = diag_box.get_circuit().get_unitary()
    assert np.allclose(np.diag(diag_vect), u)
    d.add_diagonal_box(diag_box, [Qubit(0), Qubit(1), Qubit(2)])
    d.add_diagonal_box(diag_box, [0, 1, 2])
    assert d.n_gates == 22
    # MultiplexedTensoredU2Box
    rz_op = Op.create(OpType.Rz, 0.3)
    pauli_x_op = Op.create(OpType.X)
    pauli_z_op = Op.create(OpType.Z)
    op_map_tensored: BitstringToTensoredOpMap = {
        (_0, _0): [rz_op, pauli_x_op],
        (_1, _1): [pauli_x_op, pauli_z_op],
    }
    op_map_tensored_alt: BitstringToTensoredOpList = [
        ([_0, _0], [rz_op, pauli_x_op]),
        ([_1, _1], [pauli_x_op, pauli_z_op]),
    ]
    multiplexU2 = MultiplexedTensoredU2Box(op_map_tensored)
    multiplexU2_alt = MultiplexedTensoredU2Box(op_map_tensored_alt)
    assert multiplexU2.get_op_map() == op_map_tensored
    assert multiplexU2_alt.get_op_map() == op_map_tensored
    assert multiplexU2.get_bitstring_op_pair_list() == op_map_tensored_alt
    assert multiplexU2_alt.get_bitstring_op_pair_list() == op_map_tensored_alt
    c0 = multiplexU2.get_circuit()
    unitary = c0.get_unitary()
    comparison = block_diag(
        np.kron(rz_op.get_unitary(), pauli_x_op.get_unitary()),
        np.eye(4),
        np.eye(4),
        np.kron(pauli_x_op.get_unitary(), pauli_z_op.get_unitary()),
    )
    d.add_multiplexed_tensored_u2(multiplexU2, [Qubit(0), Qubit(1), Qubit(2), Qubit(3)])
    d.add_multiplexed_tensored_u2(multiplexU2, [3, 2, 1, 0])
    assert np.allclose(unitary, comparison)
    assert d.n_gates == 24
    # ConjugationBox
    compute = CircBox(Circuit(3).CX(0, 1).CX(1, 2))
    action = CircBox(Circuit(3).H(2))
    conj_box1 = ConjugationBox(compute, action)
    assert conj_box1.get_compute() == compute
    assert conj_box1.get_action() == action
    assert conj_box1.get_uncompute() is None
    uncompute = CircBox(Circuit(3).CX(1, 2).CX(0, 1))
    conj_box2 = ConjugationBox(compute, action, uncompute)
    assert conj_box2.get_uncompute() == uncompute
    d.add_conjugation_box(conj_box1, [0, 1, 2])
    d.add_conjugation_box(conj_box2, [Qubit(0), Qubit(1), Qubit(2)])
    assert d.n_gates == 26
    assert json_validate(d)
    # test op.get_unitary doesn't throw
    for command in d.get_commands():
        if len(command.op.free_symbols()) == 0:
            command.op.get_unitary()


def test_pauliexp_pair_box_serialisation() -> None:
    # https://github.com/quantinuum/tket/issues/1084
    p = PauliExpPairBox(
        [Pauli.Z, Pauli.X], 0.5, [Pauli.X, Pauli.Z], 0.2, CXConfigType.MultiQGate
    )
    c = Circuit(2).add_pauliexppairbox(p, [0, 1])
    assert json_validate(c)


def test_tofollibox_strats() -> None:
    permutation = [
        ([_0, _0, _0, _0], [_1, _1, _1, _1]),
        ([_1, _1, _1, _1], [_0, _0, _0, _0]),
    ]
    tb = ToffoliBox(permutation, ToffoliBoxSynthStrat.Cycle)
    assert tb.type == OpType.ToffoliBox
    assert tb.get_strat() == ToffoliBoxSynthStrat.Cycle
    circ = tb.get_circuit()
    unitary = circ.get_unitary()
    comparison = np.eye(16)
    comparison[[0, 15]] = comparison[[15, 0]]
    assert np.allclose(unitary, comparison)
    assert circ.n_gates == circ.n_gates_of_type(OpType.CnX) + circ.n_gates_of_type(
        OpType.X
    )


def test_state_prep_mid_circuit() -> None:
    c = Circuit(3).H(0).H(1).H(2)
    state = np.array([np.sqrt(0.125)] * 8)
    prep_box = StatePreparationBox(state, with_initial_reset=True)
    c.add_state_preparation_box(prep_box, [0, 1, 2])
    assert c.n_gates == 4
    Transform.DecomposeBoxes().apply(c)
    assert c.n_gates_of_type(OpType.Reset) == 3


def test_u1q_stability() -> None:
    # https://github.com/quantinuum/tket/issues/222
    u = np.array(
        [
            [
                -1.0000000000000000e00 + 0.0000000000000000e00j,
                -4.7624091282918654e-10 + 2.0295010872500105e-16j,
            ],
            [
                4.5447577055178555e-10 - 1.4232772405184710e-10j,
                -9.5429791447115209e-01 + 2.9885697320961047e-01j,
            ],
        ]
    )
    ubox = Unitary1qBox(u)
    op = ubox.get_circuit().get_commands()[0].op
    assert op.type == OpType.TK1
    a, b, c = op.params
    assert np.isfinite(float(a))
    assert np.isfinite(float(b))
    assert np.isfinite(float(c))


def test_custom_gates() -> None:
    a = Symbol("a")
    b = Symbol("b")
    setup = Circuit(3)
    setup.CX(0, 1)
    setup.Rz(a + 0.5, 2)
    setup.CRz(b, 0, 2)
    gatedef = CustomGateDef.define("g", setup, [a, b])
    c = Circuit(4)
    c.add_custom_gate(gatedef, [0.2, 1.3], [0, 3, 1])
    assert json_validate(c)
    coms = c.get_commands()
    assert len(coms) == 1
    cmd0 = coms[0]
    assert str(cmd0) == "g(0.2,1.3) q[0], q[3], q[1];"
    gate = CustomGate(gatedef, [0.2, 1.3])
    op0 = cmd0.op
    assert gate.type == op0.type
    assert gate.params == op0.params
    c_d = c.dagger()
    c_t = c.transpose()
    Transform.DecomposeBoxes().apply(c)
    coms = c.get_commands()
    assert str(coms[0]) == "CX q[0], q[3];"
    assert str(coms[1]) == "Rz(0.7) q[1];"
    assert str(coms[2]) == "CRz(1.3) q[0], q[1];"
    Transform.DecomposeBoxes().apply(c_d)
    coms_d = c_d.get_commands()
    assert str(coms_d[0]) == "CRz(2.7) q[0], q[1];"
    assert str(coms_d[1]) == "CX q[0], q[3];"
    assert str(coms_d[2]) == "Rz(3.3) q[1];"
    Transform.DecomposeBoxes().apply(c_t)
    coms_t = c_t.get_commands()
    assert str(coms_t[0]) == "CRz(1.3) q[0], q[1];"
    assert str(coms_t[1]) == "CX q[0], q[3];"
    assert str(coms_t[2]) == "Rz(0.7) q[1];"


def test_errors() -> None:
    # TKET-289
    c = Circuit(1)
    a = Symbol("a")
    c.Rz(a, 0)
    c.Rz(0.5, 0)
    c.Rz(0, 0)
    with pytest.raises(TypeError):
        c.Rz(0, "a")  # type: ignore
    assert c.get_commands()[0].free_symbols() == {a}


def test_str() -> None:
    # TKET-437
    c = Circuit(2).CRz(0.5, 0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CRz(0.5)"
    # TKET-871
    c = Circuit(2).CRx(0.5, 0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CRx(0.5)"
    c = Circuit(2).CRy(0.5, 0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CRy(0.5)"
    # TKET-957
    c = Circuit(2).CV(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CV"
    c = Circuit(2).CVdg(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CVdg"
    c = Circuit(2).CSX(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CSX"
    c = Circuit(2).CSXdg(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CSXdg"
    c = Circuit(2).CS(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CS"
    c = Circuit(2).CSdg(0, 1)
    op = c.get_commands()[0].op
    assert op.__str__() == "CSdg"
    c = Circuit(1).SX(0)
    op = c.get_commands()[0].op
    assert op.__str__() == "SX"
    c = Circuit(1).SXdg(0)
    op = c.get_commands()[0].op
    assert op.__str__() == "SXdg"


def test_repr() -> None:
    c = Circuit(2).Rx(0.3, 0).CX(0, 1)
    c.qubit_create(Qubit(1))
    c.qubit_discard(Qubit(0))
    assert c.__repr__() == "[Create q[1]; Rx(0.3) q[0]; CX q[0], q[1]; Discard q[0]; ]"


def test_qubit_to_bit_map() -> None:
    c = Circuit()
    a = [Qubit("a", i) for i in range(4)]
    b = [Bit("b", i) for i in range(4)]
    for q in a:
        c.add_qubit(q)
    for cb in b:
        c.add_bit(cb)
    c.add_gate(OpType.X, [a[0]])
    c.H(a[1])
    c.add_gate(OpType.Rz, 0.5, [a[1]])
    c.CX(a[2], a[0])
    c.CRz(0.5, a[0], a[3])
    c.Measure(a[3], b[3])
    c.Measure(a[1], b[1])

    qbmap = c.qubit_to_bit_map
    assert len(qbmap) == 2
    assert qbmap[a[3]] == b[3]
    assert qbmap[a[1]] == b[1]


def test_barrier_errors() -> None:
    c = Circuit(2)
    c.add_barrier([0, 1])
    with pytest.raises(RuntimeError) as err:
        c.add_gate(OpType.Barrier, [], [0, 1])
    assert "Please use `add_barrier`" in str(err.value)


def test_phase() -> None:
    c = Circuit(2).H(0).CX(0, 1)
    c1 = c.copy()
    c1.add_phase(0.125)
    assert c1.phase - c.phase == 0.125
    c1.add_phase(1.875)
    assert c == c1


def test_phase_return_circ() -> None:
    c = Circuit(2).H(0).CX(0, 1)
    c1 = c.copy()
    c1.add_phase(0.125).H(0)
    assert c1.phase - c.phase == 0.125
    c1.add_phase(1.875)
    c.H(0)
    assert c == c1


@given(st.circuits())
@settings(deadline=None)
def test_circuit_to_serializable_json_roundtrip(circuit: Circuit) -> None:
    serializable_form = circuit.to_dict()
    assert json.loads(json.dumps(serializable_form)) == serializable_form


@given(st.circuits())
@settings(deadline=None)
def test_circuit_to_serialized_json_roundtrip(circuit: Circuit) -> None:
    serialized_form = circuit.to_json()
    assert Circuit.from_json(serialized_form).to_json() == serialized_form


@given(st.circuits())
@settings(deadline=None)
def test_circuit_pickle_roundtrip(circuit: Circuit) -> None:
    assert pickle.loads(pickle.dumps(circuit)) == circuit


@given(st.circuits())
@settings(deadline=None)
def test_circuit_from_to_serializable(circuit: Circuit) -> None:
    assert json_validate(circuit)


@given(st.circuits())
@settings(deadline=None)
def test_circuit_display(circuit: Circuit) -> None:
    html_str_circ = render_circuit_as_html(circuit, jupyter=False)
    html_str_dict = render_circuit_as_html(circuit.to_dict(), jupyter=False)
    assert isinstance(html_str_circ, str)
    assert isinstance(html_str_dict, str)


@given(st.circuits())
@settings(deadline=None)
def test_circuit_display_multiple(circuit: Circuit) -> None:
    html_str_circ = render_circuit_as_html([circuit, circuit], jupyter=False)
    html_str_dict = render_circuit_as_html(
        [circuit.to_dict(), circuit.to_dict()], jupyter=False
    )
    assert isinstance(html_str_circ, str)
    assert isinstance(html_str_dict, str)


@given(st.circuits())
@settings(deadline=None)
def test_circuit_display_with_options(circuit: Circuit) -> None:
    circuit_renderer = get_circuit_renderer()
    circuit_renderer.set_render_options(zx_style=False)
    html_str_circ = circuit_renderer.render_circuit_as_html(circuit, jupyter=False)
    html_str_dict = circuit_renderer.render_circuit_as_html(
        circuit.to_dict(), jupyter=False
    )
    assert isinstance(html_str_circ, str)
    assert isinstance(html_str_dict, str)


def test_circuit_display_with_barrier() -> None:
    # TKET-1434
    c = Circuit(2).X(0).X(1).add_barrier([0, 1]).X(0).X(1)
    html_str_circ = render_circuit_as_html(c, jupyter=False)
    html_str_dict = render_circuit_as_html(c.to_dict(), jupyter=False)
    assert isinstance(html_str_circ, str)
    assert isinstance(html_str_dict, str)


def test_ops_of_type() -> None:
    c = Circuit(2).H(0).Rz(0.5, 1).CX(0, 1).Rz(0.5, 1).Rz(-0.5, 0).CRy(0.3, 0, 1)
    ops_H = c.ops_of_type(OpType.H)
    ops_CX = c.ops_of_type(OpType.CX)
    ops_Rz = c.ops_of_type(OpType.Rz)
    ops_Rx = c.ops_of_type(OpType.Rx)
    ops_CRy = c.ops_of_type(OpType.CRy)
    assert len(ops_H) == 1
    assert ops_H[0].type == OpType.H
    assert len(ops_CX) == 1
    assert ops_CX[0].type == OpType.CX
    assert len(ops_Rz) == 3
    assert all(op.type == OpType.Rz for op in ops_Rz)
    assert len(ops_Rx) == 0
    assert len(ops_CRy) == 1
    assert ops_CRy[0].type == OpType.CRy


def test_commands_of_type() -> None:
    c = Circuit(2).H(0).Rz(0.5, 1).CX(0, 1).Rz(0.5, 1).CZ(0, 1).Rz(-0.5, 0)
    cmds_H = c.commands_of_type(OpType.H)
    cmds_CX = c.commands_of_type(OpType.CX)
    cmds_Rz = c.commands_of_type(OpType.Rz)
    cmds_Rx = c.commands_of_type(OpType.Rx)
    assert len(cmds_H) == 1
    assert cmds_H[0].op.type == OpType.H
    assert cmds_H[0].qubits == [c.qubits[0]]
    assert len(cmds_CX) == 1
    assert cmds_CX[0].op.type == OpType.CX
    assert cmds_CX[0].qubits == c.qubits
    assert len(cmds_Rz) == 3
    assert all(cmd.op.type == OpType.Rz for cmd in cmds_Rz)
    assert [cmd.qubits[0] for cmd in cmds_Rz] == [c.qubits[i] for i in [1, 1, 0]]
    assert len(cmds_Rx) == 0


def test_empty_circuit() -> None:
    circ = Circuit(0)
    circt_dict = circ.to_dict()
    assert isinstance(circt_dict, dict)
    assert len(circt_dict) > 0
    assert Circuit(0) == Circuit(0)


def test_circuit_with_qubit_creations_and_discards() -> None:
    circ = Circuit(2)
    circt_dict = circ.to_dict()
    assert len(circt_dict["created_qubits"]) == 0
    assert len(circt_dict["discarded_qubits"]) == 0
    circ2 = circ.copy()
    circ2.qubit_create(Qubit(0))
    circ2.qubit_discard(Qubit(0))
    circ2.qubit_discard(Qubit(1))
    circt_dict2 = circ2.to_dict()
    assert circt_dict2["created_qubits"] == [Qubit(0).to_list()]
    assert circt_dict2["discarded_qubits"] == [Qubit(0).to_list(), Qubit(1).to_list()]
    assert circ != circ2
    assert len(circ.created_qubits) == 0
    assert len(circ.discarded_qubits) == 0
    assert circ2.created_qubits == [Qubit(0)]
    assert circ2.discarded_qubits == [Qubit(0), Qubit(1)]


def with_empty_qubit(op: Op) -> CircBox:
    n_qb = op.n_qubits
    return CircBox(Circuit(n_qb + 1).add_gate(op, list(range(1, n_qb + 1))))


def with_control_qubit(op: Op) -> QControlBox:
    return QControlBox(op, 1)


def test_opgroups() -> None:
    # Replace with op
    c = Circuit(1)
    c.Rz(0.2, 0, opgroup="Z rotations")
    c.H(0)
    c.Rz(0.3, 0, opgroup="Z rotations")
    newop = Op.create(OpType.T)
    assert c.substitute_named(newop, "Z rotations")

    # Replace with circuit
    newcirc = Circuit(1).H(0)
    assert c.substitute_named(newcirc, "Z rotations")

    # Add a control to a bunch of H and CX ops:
    c = Circuit(3)
    h_op = Op.create(OpType.H)
    cx_op = Op.create(OpType.CX)
    h_0_cbox = with_empty_qubit(h_op)
    h_q_qbox = with_control_qubit(h_op)
    cx_0_cbox = with_empty_qubit(cx_op)
    cx_q_qbox = with_control_qubit(cx_op)
    c.X(0).Y(1)
    c.add_circbox(h_0_cbox, [2, 0], opgroup="hgroup")
    c.add_circbox(cx_0_cbox, [2, 0, 1], opgroup="cxgroup")
    c.Y(0).X(1)
    c.add_circbox(h_0_cbox, [2, 1], opgroup="hgroup")
    c.add_circbox(cx_0_cbox, [2, 1, 0], opgroup="cxgroup")
    c.X(0).Y(1)
    assert c.substitute_named(h_q_qbox, "hgroup")
    assert c.substitute_named(cx_q_qbox, "cxgroup")

    # Add a pi/4 rotation to an Rz op:
    c = Circuit(2).H(0).H(1)
    c.Rz(0.5, 0, opgroup="rot")
    c.CX(0, 1)
    cbox = CircBox(Circuit(1).Rz(0.5, 0, opgroup="rot").Rz(0.25, 0, opgroup="rot"))
    assert c.substitute_named(cbox, "rot")
    assert DecomposeBoxes().apply(c)
    assert RemoveRedundancies().apply(c)
    assert c.n_gates_of_type(OpType.Rz) == 1
    # When all squashed vertices have the same name, the squashed vertex will have that
    # name too.
    # Swap the Rz for an Ry
    ry_op = Op.create(OpType.Ry, 0.75)
    assert c.substitute_named(ry_op, "rot")
    assert c.n_gates_of_type(OpType.Rz) == 0
    assert c.n_gates_of_type(OpType.Ry) == 1

    # Remove a redundant gate
    c = Circuit(3).H(0)
    assert len(c.opgroups) == 0
    c.CX(0, 1, opgroup="cx0")
    c.CX(1, 2, opgroup="cx1")
    c.CX(2, 0, opgroup="cx2")
    c.CX(0, 1, opgroup="cx3")
    assert c.opgroups == {"cx0", "cx1", "cx2", "cx3"}
    c.substitute_named(Circuit(2), "cx3")
    assert c.n_gates == 4
    assert c.n_gates_of_type(OpType.CX) == 3
    assert c.opgroups == {"cx0", "cx1", "cx2"}


def test_depth() -> None:
    c = Circuit(3)
    c.H(0).H(1).CX(1, 2).CZ(0, 1).H(1).CZ(1, 2)
    c.add_barrier([0, 1, 2])
    c.CX(1, 2).CZ(0, 1).H(1).CX(1, 2)
    assert c.depth() == 9
    assert c.depth_by_type(OpType.H) == 3
    assert c.depth_by_type(OpType.CX) == 3
    assert c.depth_by_type(OpType.CZ) == 3
    assert c.depth_by_type({OpType.CX, OpType.CZ}) == 6
    assert c.depth_by_type({OpType.CX, OpType.H}) == 6
    assert c.depth_by_type({OpType.CZ, OpType.H}) == 6
    assert c.depth_by_type(set()) == 0
    assert c.depth_2q() == 6


def test_op_dagger_transpose() -> None:
    sx = Op.create(OpType.SX)
    sxdg = Op.create(OpType.SXdg)
    assert sx.dagger == sxdg
    assert sx.transpose == sx
    cs = Op.create(OpType.CS)
    csdg = Op.create(OpType.CSdg)
    assert cs.dagger == csdg
    assert cs.transpose == cs
    assert csdg.transpose == csdg


def test_clifford_checking() -> None:
    c = Circuit(2, 1)
    c.H(0).CX(0, 1).T(1).Rz(0.5, 1).Rz(0.3, 1).Measure(1, 0)
    h = c.get_commands()[0].op
    assert h.is_clifford_type()
    cx = c.get_commands()[1].op
    assert cx.is_clifford_type()
    t = c.get_commands()[2].op
    assert not t.is_clifford_type()
    rz1 = c.get_commands()[3].op
    assert not rz1.is_clifford_type()
    rz2 = c.get_commands()[4].op
    assert not rz2.is_clifford_type()
    m = c.get_commands()[5].op
    assert not m.is_clifford_type()


def test_clifford_evaluation() -> None:
    c = Circuit(2, 1)
    c.Rx(0, 0).ISWAP(1, 0, 1).Rz(0.3, 0)
    rx = c.get_commands()[0].op
    assert rx.is_clifford()
    iswap = c.get_commands()[1].op
    assert iswap.is_clifford()
    rz = c.get_commands()[2].op
    assert not rz.is_clifford()


def test_getting_registers() -> None:
    c = Circuit(2, 1)
    c_regs = c.c_registers
    assert len(c_regs) == 1
    assert c_regs[0] == BitRegister("c", 1)
    q_regs = c.q_registers
    assert len(q_regs) == 1
    assert q_regs[0] == QubitRegister("q", 2)
    q_err_msg = "Cannot find quantum register with name"
    c_err_msg = "Cannot find classical register with name"
    with pytest.raises(RuntimeError) as e:
        c.get_c_register("q")
    assert c_err_msg in str(e.value)
    with pytest.raises(RuntimeError) as e:
        c.get_q_register("c")
    assert q_err_msg in str(e.value)
    assert c.get_c_register("c").name == "c"
    assert c.get_c_register("c").size == 1
    assert c.get_q_register("q").name == "q"
    assert c.get_q_register("q").size == 2
    c.add_q_register("test_qr", 10)
    c.add_c_register("test_cr", 8)
    assert c.get_c_register("test_cr").name == "test_cr"
    assert c.get_c_register("test_cr").size == 8
    assert c.get_q_register("test_qr").name == "test_qr"
    assert c.get_q_register("test_qr").size == 10

    c_regs = c.c_registers
    c_regs.sort()
    assert len(c_regs) == 2
    assert c_regs[0] == BitRegister("c", 1)
    assert c_regs[1] == BitRegister("test_cr", 8)
    q_regs = c.q_registers
    q_regs.sort()
    assert len(q_regs) == 2
    assert q_regs[0] == QubitRegister("q", 2)
    assert q_regs[1] == QubitRegister("test_qr", 10)


def test_getting_registers_with_non_consecutive_indices() -> None:
    # https://github.com/quantinuum/tket/issues/1160
    c = Circuit()
    c.add_qubit(Qubit(3))
    c.add_qubit(Qubit(2))
    c.add_bit(Bit(3))
    c.add_qubit(Qubit("a", 0))
    c.add_qubit(Qubit("a", 1))
    c.add_qubit(Qubit("a", 2))
    c.add_bit(Bit("b", 0))
    c.add_bit(Bit("b", 1))
    c_regs = c.c_registers
    assert len(c_regs) == 1
    assert c_regs[0] == BitRegister("b", 2)
    q_regs = c.q_registers
    assert len(q_regs) == 1
    assert q_regs[0] == QubitRegister("a", 3)


def test_getting_registers_with_unindexed_bits() -> None:
    # https://github.com/quantinuum/tket/issues/1785
    c = Circuit()
    qubit = Qubit("q", [])
    bit = Bit("a", [])
    c.add_qubit(qubit)
    c.add_bit(bit)
    c.PhasedX(0.5, 0.5, qubit).Measure(qubit, bit)
    assert c.q_registers == []
    assert c.c_registers == []


def test_measuring_registers() -> None:
    c = Circuit()
    with pytest.raises(RuntimeError) as e:
        qreg = QubitRegister("qr", 2)
        c.measure_register(qreg, "cr")
    assert "The given QubitRegister is not in use" in str(e.value)
    qreg = c.add_q_register("qr", 2)
    c.measure_register(qreg, "cr")
    qreg2 = c.add_q_register("qr2", 2)
    c.measure_register(qreg2, "cr")

    assert len(c.bits) == 2
    assert c.n_qubits == 4
    commands = c.get_commands()
    assert len(commands) == 4
    assert str(commands[0]) == "Measure qr[0] --> cr[0];"
    assert str(commands[1]) == "Measure qr[1] --> cr[1];"
    assert str(commands[2]) == "Measure qr2[0] --> cr[0];"
    assert str(commands[3]) == "Measure qr2[1] --> cr[1];"
    qreg3 = c.add_q_register("qr3", 3)
    with pytest.raises(RuntimeError) as e:
        c.measure_register(qreg3, "cr")
    assert "size doesn't match the given QubitRegister" in str(e.value)


def test_zzmax() -> None:
    c = Circuit(5)
    c.ZZMax(0, 1)
    assert c.depth() == 1


def test_multi_controlled_gates() -> None:
    c = Circuit(5)
    c.add_gate(OpType.CnX, [0, 1, 2])
    c.add_gate(OpType.CnY, [0, 1, 2])
    c.add_gate(OpType.CnZ, [0, 1, 2])
    assert c.depth() == 3


def test_counting_n_qubit_gates() -> None:
    c = Circuit(5).X(0).H(1).Y(2).Z(3).S(4).CX(0, 1).CX(1, 2).CX(2, 3).CX(3, 4)
    c.add_gate(OpType.CnX, [0, 1, 2])
    c.add_gate(OpType.CnX, [0, 1, 2, 3])
    c.add_gate(OpType.CnX, [0, 1, 2, 3, 4])
    c.add_barrier([0, 1, 2, 3, 4])
    assert c.n_1qb_gates() == 5
    assert c.n_nqb_gates(1) == 5
    assert c.n_2qb_gates() == 4
    assert c.n_nqb_gates(2) == 4
    assert c.n_nqb_gates(3) == 1
    assert c.n_nqb_gates(4) == 1
    assert c.n_nqb_gates(5) == 1


def test_counting_conditional_gates() -> None:
    c = Circuit(5, 2).X(0).H(1).Y(2).Z(3).S(4).CX(0, 1).CX(1, 2).CX(2, 3).CX(3, 4)
    c.add_gate(OpType.H, [Qubit(0)], condition=Bit(0))
    c.add_gate(OpType.H, [Qubit(1)], condition=(Bit(0) & Bit(1)))
    c.add_gate(OpType.CX, [Qubit(0), Qubit(1)], condition=Bit(1))
    assert c.n_gates_of_type(OpType.H, include_conditional=True) == 3
    assert c.n_gates_of_type(OpType.H, include_conditional=False) == 1
    assert c.n_gates_of_type(OpType.H) == 1
    assert c.n_gates_of_type(OpType.X, include_conditional=True) == 1
    assert c.n_gates_of_type(OpType.X, include_conditional=False) == 1
    assert c.n_gates_of_type(OpType.X) == 1
    assert c.n_gates_of_type(OpType.CX, include_conditional=True) == 5
    assert c.n_gates_of_type(OpType.CX, include_conditional=False) == 4
    assert c.n_gates_of_type(OpType.CX) == 4


def test_qcontrol_box_constructors() -> None:
    # only one argument
    qcbox1 = QControlBox(Op.create(OpType.S))
    # two arguments
    qcbox2 = QControlBox(Op.create(OpType.S), 1)
    # all arguments. state expressed as an integer
    qcbox3 = QControlBox(Op.create(OpType.S), 2, 1)
    # all arguments. state expressed as a bit vector
    qcbox4 = QControlBox(Op.create(OpType.S), 2, [_0, _1])
    assert qcbox1 == qcbox2
    assert qcbox3 == qcbox4
    assert qcbox1.get_control_state() == 1
    assert qcbox3.get_control_state() == 1
    assert qcbox3.get_control_state_bits() == [0, 1]


def test_error_wrong_parameters() -> None:
    circ = Circuit(1, 1)
    with pytest.raises(RuntimeError):
        circ.add_gate(OpType.H, [Bit(0)])


def test_symbol_subst() -> None:
    # https://github.com/quantinuum/tket/issues/999
    d = Circuit(4)
    rz_op = Op.create(OpType.Rz, 0.3)
    pauli_x_op = Op.create(OpType.X)
    pauli_z_op = Op.create(OpType.Z)
    u = np.asarray([[1.0, 0.0], [0.0, -1.0]])
    ubox = Unitary1qBox(u)
    op_map_new: BitstringToTensoredOpMap = {
        (_0, _0): [rz_op, pauli_x_op],
        (_1, _1): [ubox, pauli_z_op],
    }
    multiplexU2 = MultiplexedTensoredU2Box(op_map_new)
    d.add_multiplexed_tensored_u2(multiplexU2, [0, 1, 2, 3])
    d.symbol_substitution({})
    assert len(d.get_commands()) == 1


def test_phase_order() -> None:
    # https://github.com/quantinuum/tket/issues/1073
    c = Circuit(2)
    c.Ry(0.0, 1)
    c.add_gate(OpType.Phase, [0.0], [])
    c.add_gate(OpType.Phase, [0.5], [])
    c.add_gate(OpType.Phase, [0.0], [])
    c.Ry(0.0, 0)
    c.X(0)
    c.ISWAP(0.0, 1, 0)
    c.CVdg(0, 1)
    for _ in range(100):
        c1 = c.copy()
        assert c == c1


def test_empty_multiplexed_rz() -> None:
    # https://github.com/quantinuum/tket/issues/1928
    ubox = Unitary1qBox(np.eye(2, dtype=complex))

    op0 = Op.create(OpType.noop)
    op1 = Op.create(OpType.Z)
    op_map: BitstringToTensoredOpMap = {(_0,): [ubox, op0], (_1,): [ubox, op1]}
    sbox = MultiplexedTensoredU2Box(op_map)
    assert sbox.get_circuit()


def test_dummy_box() -> None:
    resource_data = ResourceData(
        op_type_count={OpType.T: ResourceBounds(10, 20)},
        gate_depth=ResourceBounds(5, 8),
        op_type_depth={OpType.CZ: ResourceBounds(3, 6), OpType.T: ResourceBounds(4, 6)},
        two_qubit_gate_depth=ResourceBounds(4, 5),
    )
    dbox = DummyBox(n_qubits=3, n_bits=1, resource_data=resource_data)
    c = Circuit(4, 2)
    c.add_dummybox(dbox, [0, 2, 3], [1])
    cmds = c.get_commands()
    assert len(cmds) == 1
    op = cmds[0].op
    assert type(op) is DummyBox
    resource_data1 = op.get_resource_data()
    op_type_count = resource_data1.get_op_type_count()
    assert op_type_count[OpType.T].get_min() == 10
    assert op_type_count[OpType.T].get_max() == 20
    assert json_validate(c)


def test_resources() -> None:
    resource_data0 = ResourceData(
        op_type_count={
            OpType.T: ResourceBounds(1, 2),
            OpType.H: ResourceBounds(0, 1),
            OpType.CX: ResourceBounds(1, 2),
            OpType.CZ: ResourceBounds(3, 3),
        },
        gate_depth=ResourceBounds(5, 8),
        op_type_depth={
            OpType.T: ResourceBounds(0, 10),
            OpType.H: ResourceBounds(0, 10),
            OpType.CX: ResourceBounds(1, 2),
            OpType.CZ: ResourceBounds(3, 3),
        },
        two_qubit_gate_depth=ResourceBounds(4, 5),
    )
    dbox0 = DummyBox(n_qubits=2, n_bits=0, resource_data=resource_data0)
    resource_data1 = ResourceData(
        op_type_count={
            OpType.T: ResourceBounds(2, 2),
            OpType.H: ResourceBounds(1, 1),
            OpType.CX: ResourceBounds(2, 3),
            OpType.CZ: ResourceBounds(3, 5),
        },
        gate_depth=ResourceBounds(5, 10),
        op_type_depth={
            OpType.T: ResourceBounds(1, 2),
            OpType.H: ResourceBounds(2, 4),
            OpType.CX: ResourceBounds(1, 1),
            OpType.CZ: ResourceBounds(3, 4),
        },
        two_qubit_gate_depth=ResourceBounds(3, 5),
    )
    dbox1 = DummyBox(n_qubits=3, n_bits=0, resource_data=resource_data1)
    c = Circuit(3)
    c.H(0)
    c.CX(1, 2)
    c.CX(0, 1)
    c.T(2)
    c.H(1)
    c.add_dummybox(dbox0, [0, 1], [])
    c.CZ(1, 2)
    c.add_dummybox(dbox1, [0, 1, 2], [])
    c.H(2)
    resource_data = c.get_resources()
    op_type_count = resource_data.get_op_type_count()
    assert op_type_count[OpType.T].get_min() == 4
    assert op_type_count[OpType.T].get_max() == 5
    assert op_type_count[OpType.H].get_min() == 4
    assert op_type_count[OpType.H].get_max() == 5
    assert op_type_count[OpType.CX].get_min() == 5
    assert op_type_count[OpType.CX].get_max() == 7
    assert op_type_count[OpType.CZ].get_min() == 7
    assert op_type_count[OpType.CZ].get_max() == 9
    gate_depth = resource_data.get_gate_depth()
    assert gate_depth.get_min() == 15
    assert gate_depth.get_max() == 23
    op_type_depth = resource_data.get_op_type_depth()
    assert op_type_depth[OpType.T].get_min() == 2
    assert op_type_depth[OpType.T].get_max() == 12
    assert op_type_depth[OpType.H].get_min() == 5
    assert op_type_depth[OpType.H].get_max() == 17
    assert op_type_depth[OpType.CX].get_min() == 4
    assert op_type_depth[OpType.CX].get_max() == 5
    assert op_type_depth[OpType.CZ].get_min() == 7
    assert op_type_depth[OpType.CZ].get_max() == 8
    two_qubit_gate_depth = resource_data.get_two_qubit_gate_depth()
    assert two_qubit_gate_depth.get_min() == 10
    assert two_qubit_gate_depth.get_max() == 13


def test_add_circbox_with_registers() -> None:
    c0 = Circuit()
    areg = c0.add_q_register("a", 2)
    breg = c0.add_q_register("b", 3)
    c0.CZ(areg[0], areg[1])
    c0.CZ(areg[1], breg[0])
    c0.CCX(breg[0], breg[1], breg[2])
    cbox = CircBox(c0)
    c = Circuit()
    c.add_q_register("x", 3)
    c.add_q_register("y", 2)
    zreg = c.add_q_register("z", 3)
    wreg = c.add_q_register("w", 2)
    for qb in c.qubits:
        c.H(qb)
    c1 = c.copy()
    c1.add_circbox_regwise(cbox, [wreg, zreg], [])
    assert c1.n_gates == 11
    DecomposeBoxes().apply(c1)
    assert c1.n_gates == 13
    c2 = c.copy()
    c2.add_circbox_with_regmap(cbox, {"a": "w", "b": "z"}, {})
    DecomposeBoxes().apply(c2)
    assert c1 == c2


def test_add_circbox_with_mixed_registers() -> None:
    c0 = Circuit()
    c0.add_q_register("q1", 2)
    c0.add_q_register("q2", 3)
    c0.add_c_register("c1", 4)
    c0.add_c_register("c2", 5)
    cbox = CircBox(c0)
    c = Circuit()
    c.add_q_register("q1", 2)
    c.add_q_register("q2", 3)
    c.add_c_register("c1", 4)
    c.add_c_register("c2", 5)

    c.add_circbox_with_regmap(
        cbox, qregmap={"q1": "q1", "q2": "q2"}, cregmap={"c1": "c1", "c2": "c2"}
    )

    # Incomplete map:
    with pytest.raises(IndexError):
        c.add_circbox_with_regmap(
            cbox, qregmap={"q1": "q1", "q2": "q2"}, cregmap={"c1": "c1"}
        )

    # Mismatched register sizes:
    with pytest.raises(RuntimeError):
        c.add_circbox_with_regmap(
            cbox, qregmap={"q1": "q2", "q2": "q1"}, cregmap={"c1": "c2", "c2": "c1"}
        )

    # Non-register qubit in box:
    c0.add_qubit(Qubit("q3", 1))
    cbox = CircBox(c0)
    c.add_qubit(Qubit("q3", 0))
    with pytest.raises(RuntimeError):
        c.add_circbox_with_regmap(
            cbox,
            qregmap={"q1": "q1", "q2": "q2", "q3": "q3"},
            cregmap={"c1": "c1", "c2": "c2"},
        )


def test_deserialization_from_junk() -> None:
    # https://github.com/quantinuum/tket/issues/1243
    with pytest.raises(RuntimeError):
        Circuit.from_dict(
            {
                "phase": "1.9999999999999998",
                "qubits": [("q", (20057, 24021, 112, 9628, 79))],
                "bits": [("c", (128, 3, 384))],
                "implicit_permutation": [
                    (("c", (1174437931,)), ("q", (0,))),
                    (("c", (25199232,)), ("c", (29697, 126852352))),
                ],
                "commands": [
                    {
                        "op": {
                            "type": "CCX",
                            "conditional": {
                                "op": {"type": "CCX"},
                                "width": 3,
                                "value": 2,
                            },
                        },
                        "args": [],
                    }
                ],
                "name": "ú\x19",
                "created_qubits": [("c", (0,))],
                "discarded_qubits": [("c", (0,))],
            }
        )


def test_wasm_serialization() -> None:
    c = Circuit(2, 2).H(0).H(1).measure_all()
    c._add_wasm("some_name", "some_uid", [1, 1], [], [Bit(0), Bit(1)], [0])  # noqa: SLF001
    c.CZ(0, 1).measure_all()
    assert json_validate(c)


def test_pickle_bit() -> None:
    # https://github.com/quantinuum/tket/issues/1293
    for b in [Bit(1), Bit("z", 0), Bit("z", (2, 0, 3))]:
        assert b == pickle.loads(pickle.dumps(b))


def test_cnrx_cnrz() -> None:
    c1rx = Circuit(2)
    c1rx.add_gate(OpType.CnRx, 0.3, [0, 1])
    crx = Circuit(2)
    crx.add_gate(OpType.CRx, 0.3, [0, 1])

    c1rz = Circuit(2)
    c1rz.add_gate(OpType.CnRz, 0.3, [0, 1])
    crz = Circuit(2)
    crz.add_gate(OpType.CRz, 0.3, [0, 1])

    assert np.allclose(c1rz.get_unitary(), crz.get_unitary())
    assert np.allclose(c1rx.get_unitary(), crx.get_unitary())


def test_greedy_TermSequenceBox() -> None:
    tseqbox = TermSequenceBox(
        [
            ([Pauli.X, Pauli.I, Pauli.I], 0.3),
            ([Pauli.I, Pauli.Y, Pauli.I], 0.2),
            ([Pauli.I, Pauli.I, Pauli.Z], 1.1),
            ([Pauli.X, Pauli.Z, Pauli.I], 1.8),
        ],
        synthesis_strategy=PauliSynthStrat.Greedy,
        depth_weight=0.28,
    )
    c = tseqbox.get_circuit()
    cmds = c.get_commands()
    assert cmds[0].op.type == OpType.TK1
    assert cmds[1].op.type == OpType.TK1
    assert cmds[2].op.type == OpType.TK1
    assert c.n_2qb_gates() <= 2


def test_controlled_TermSequenceBox_logging(capfd: Any) -> None:
    tseqbox = TermSequenceBox(
        [
            ([Pauli.X, Pauli.I, Pauli.I], 0.3),
            ([Pauli.I, Pauli.Y, Pauli.I], 0.2),
            ([Pauli.I, Pauli.I, Pauli.Z], 1.1),
            ([Pauli.X, Pauli.Z, Pauli.I], 1.8),
        ],
        synthesis_strategy=PauliSynthStrat.Greedy,
        depth_weight=0.28,
    )
    QControlBox(tseqbox)
    out = capfd.readouterr().out
    assert "does not preserve global phase" in out


def test_cnx_vchain_zeroed_ancillas() -> None:
    for n in range(3, 7):
        circ = CnX_vchain_decomp(n, True)

        n_ancillas = (n - 1) // 2
        n_qubits = n + n_ancillas + 1

        # Compare with CnX, considering only the the subspace
        # where all ancilla qubits are in state |0>
        m = circ.get_unitary()
        dim = 1 << n_qubits
        ancillas_one = (1 << n_ancillas) - 1
        idx = np.array([ii for ii in range(dim) if (ii & ancillas_one) == 0])

        m_sub = m[idx][:, idx]
        m_cnx = np.eye(1 << (n + 1), dtype=np.complex128)
        m_cnx[-2:, -2:] = np.array([[0, 1], [1, 0]])

        assert np.allclose(m_sub, m_cnx)


def test_cnx_vchain_arbitrary_ancillas() -> None:
    for n in range(3, 7):
        circ = CnX_vchain_decomp(n, False)

        n_ancillas = (n - 1) // 2

        # Compare with CnX acting on the first n + 1 qubits
        m = circ.get_unitary()

        m_cnx = np.eye(1 << (n + 1), dtype=np.complex128)
        m_cnx[-2:, -2:] = np.array([[0, 1], [1, 0]])
        m_cnx_with_ancillas = np.kron(m_cnx, np.eye(1 << n_ancillas))

        assert np.allclose(m, m_cnx_with_ancillas)


def test_c0z() -> None:
    # https://github.com/quantinuum/tket/issues/1873
    circ = Circuit(1).add_gate(OpType.CnZ, [0])
    cmds = circ.get_commands()
    assert len(cmds) == 1
    cmd = cmds[0]
    assert cmd.op.type == OpType.Z


def test_pickle() -> None:
    n = Node("abc", [1, 2, 3])
    s = pickle.dumps(n)
    n1 = pickle.loads(s)
    assert n == n1


def test_serialized_params() -> None:
    c = Circuit(1).Rz(7.5, 0)
    assert c.to_dict()["commands"][0]["op"]["params"][0] == "3.5"


def test_replace_swaps() -> None:
    c = Circuit(2).SWAP(0, 1)
    assert c.replace_SWAPs()
    assert c.n_gates == 0
    c = Circuit(2).TK2(0.5, 0.5, 0.5, 0, 1)
    assert not c.replace_SWAPs(replace_tk2_equivalents=False)
    assert c.n_gates == 1
    assert c.replace_SWAPs(replace_tk2_equivalents=True)
    assert c.n_gates == 0
