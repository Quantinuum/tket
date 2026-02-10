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

from importlib.util import find_spec
from math import isclose

import numpy as np
import pytest
from sympy import sympify

from pytket import Circuit, OpType, Qubit
from pytket.passes import AutoRebase
from pytket.pauli import Pauli, QubitPauliString
from pytket.utils.results import compare_unitaries
from pytket.zx import (
    CliffordGen,
    DirectedGen,
    PhasedGen,
    QuantumType,
    Rewrite,
    ZXBox,
    ZXDiagram,
    ZXGen,
    ZXType,
    ZXWireType,
    circuit_to_zx,
)

have_quimb: bool = find_spec("quimb") is not None
if have_quimb:
    from pytket.zx.tensor_eval import (
        density_matrix_from_cptp_diagram,
        fix_boundaries_to_binary_states,
        fix_inputs_to_binary_state,
        fix_outputs_to_binary_state,
        tensor_from_mixed_diagram,
        tensor_from_quantum_diagram,
        unitary_from_classical_diagram,
        unitary_from_quantum_diagram,
    )


def test_generator_creation() -> None:
    diag = ZXDiagram(1, 0, 0, 0)
    in_v = diag.get_boundary()[0]
    gen = diag.get_vertex_ZXGen(in_v)
    assert repr(gen) == "Q-Input"

    z_spid = diag.add_vertex(ZXType.ZSpider, 0.3)
    spid_gen = diag.get_vertex_ZXGen(z_spid)
    assert repr(spid_gen) == "Q-Z(0.3)"

    with pytest.raises(RuntimeError):
        diag.add_vertex(ZXType.Triangle, 0.3)

    tri = diag.add_vertex(ZXType.Triangle, QuantumType.Classical)
    tri_gen = diag.get_vertex_ZXGen(tri)
    assert repr(tri_gen) == "C-Tri"


def test_diagram_creation() -> None:  # noqa: PLR0915
    diag = ZXDiagram(1, 1, 0, 0)
    z_spid = diag.add_vertex(ZXType.ZSpider, 0.1)
    x_spid = diag.add_vertex(ZXType.XSpider, 3.4)
    z_spid2 = diag.add_vertex(ZXType.ZSpider, 6.7, QuantumType.Classical)

    with pytest.raises(RuntimeError):
        diag.add_vertex(ZXType.ZXBox, 3.0)

    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Boundary vertex does not have degree 1" in str(errorinfo.value)

    diag.add_wire(diag.get_boundary()[0], z_spid)
    diag.add_wire(diag.get_boundary()[1], x_spid)
    diag.add_wire(z_spid, x_spid)
    diag.add_wire(x_spid, z_spid, ZXWireType.H)
    extra = diag.add_wire(diag.get_boundary()[1], z_spid)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Boundary vertex does not have degree 1" in str(errorinfo.value)

    diag.remove_wire(extra)
    diag.check_validity()

    wrong_port = diag.add_wire(u=z_spid2, v=x_spid, u_port=0)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Wire at a named port of an undirected vertex" in str(errorinfo.value)
    diag.remove_wire(wrong_port)

    tri = diag.add_vertex(ZXType.Triangle)
    diag.add_wire(u=tri, v=z_spid, u_port=0)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Not all ports of a directed vertex have wires connected" in str(
        errorinfo.value
    )

    no_port = diag.add_wire(z_spid, tri)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Wire at an unnamed port of a directed vertex" in str(errorinfo.value)
    diag.remove_wire(no_port)
    diag.add_wire(u=z_spid, v=tri, v_port=1)
    diag.check_validity()

    extra_port = diag.add_wire(u=tri, v=z_spid, u_port=1)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "Multiple wires on the same port of a vertex" in str(errorinfo.value)
    diag.remove_wire(extra_port)

    inner = ZXDiagram(1, 2, 1, 0)
    inner_spid = inner.add_vertex(ZXType.ZSpider, 0.6, QuantumType.Classical)
    inner.add_wire(inner_spid, inner.get_boundary()[0])
    inner.add_wire(inner_spid, inner.get_boundary()[1])
    inner.add_wire(inner_spid, inner.get_boundary()[2], ZXWireType.H)
    inner.add_wire(inner_spid, inner.get_boundary()[3], qtype=QuantumType.Classical)
    box = diag.add_zxbox(inner)
    diag.add_wire(u=box, v=z_spid2, u_port=0)
    diag.add_wire(u=box, v=z_spid2, u_port=1)
    diag.add_wire(u=box, v=x_spid, u_port=2)
    wrong_qtype = diag.add_wire(u=box, v=z_spid2, u_port=3)
    with pytest.raises(RuntimeError) as errorinfo:
        diag.check_validity()
    assert "QuantumType of wire is incompatible with the given port" in str(
        errorinfo.value
    )

    diag.set_wire_qtype(wrong_qtype, QuantumType.Classical)
    diag.check_validity()


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_known_tensors() -> None:  # noqa: PLR0915
    # A single basic edge
    diag = ZXDiagram(1, 1, 0, 0)
    w = diag.add_wire(diag.get_boundary()[0], diag.get_boundary()[1])
    correct = np.asarray([[1, 0], [0, 1]])
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)

    # A single H edge
    diag.set_wire_type(w, ZXWireType.H)
    diag.multiply_scalar(0.5)
    correct = np.asarray([[1, 1], [1, -1]]) * np.sqrt(0.5)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)

    # A triangle
    diag.remove_wire(w)
    diag.multiply_scalar(2.0)
    tri = diag.add_vertex(ZXType.Triangle)
    diag.add_wire(u=diag.get_boundary()[0], v=tri, v_port=0)
    diag.add_wire(u=diag.get_boundary()[1], v=tri, v_port=1)
    correct = np.asarray([[1, 0], [1, 1]], dtype=complex)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)

    # A pair of edges to test endianness
    diag = ZXDiagram(2, 2, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    diag.add_wire(ins[0], outs[0])
    diag.add_wire(ins[1], outs[1], type=ZXWireType.H)
    correct = np.asarray([[1, 1, 0, 0], [1, -1, 0, 0], [0, 0, 1, 1], [0, 0, 1, -1]])
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    initialised = fix_inputs_to_binary_state(diag, [0, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    assert np.allclose(simulated.T, correct[:, 1])

    # A Bell effect
    diag = ZXDiagram(2, 0, 0, 0)
    ins = diag.get_boundary()
    diag.add_wire(ins[0], ins[1])
    correct = np.asarray([[1, 0, 0, 1]])
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    initialised = fix_inputs_to_binary_state(diag, [0, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    assert np.allclose(simulated, correct[0, 1])
    initialised = fix_inputs_to_binary_state(diag, [1, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    assert np.allclose(simulated, correct[0, 3])

    # A single Z spider
    diag = ZXDiagram(2, 3, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    spid = diag.add_vertex(ZXType.ZSpider, 0.3)
    diag.add_wire(spid, ins[0])
    diag.add_wire(spid, ins[1])
    diag.add_wire(spid, outs[0])
    diag.add_wire(spid, outs[1])
    diag.add_wire(spid, outs[2])
    correct = np.zeros((8, 4), dtype=complex)
    correct[0, 0] = 1
    correct[7, 3] = np.exp(1j * np.pi * 0.3)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    initialised = fix_inputs_to_binary_state(diag, [1, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    assert np.allclose(simulated.T, correct[:, 3])
    # Adding a self-loop
    self_loop = diag.add_wire(spid, spid)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    diag.set_wire_type(self_loop, ZXWireType.H)
    correct[7, 3] = np.exp(1j * np.pi * 1.3)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    # A single X spider
    diag.remove_wire(self_loop)
    diag.set_vertex_ZXGen(spid, ZXGen.create(ZXType.XSpider, 0.3))
    phase = np.exp(1j * np.pi * 0.3)
    p = 1.0 + phase
    m = 1.0 - phase
    correct = np.asarray(
        [
            [p, m, m, p],
            [m, p, p, m],
            [m, p, p, m],
            [p, m, m, p],
            [m, p, p, m],
            [p, m, m, p],
            [p, m, m, p],
            [m, p, p, m],
        ]
    )
    correct = correct * pow(0.5, 2.5)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    diag.set_vertex_ZXGen(spid, ZXGen.create(ZXType.ZSpider, 0.3))
    for w in diag.adj_wires(spid):
        diag.set_wire_type(w, ZXWireType.H)
    evaluated = unitary_from_quantum_diagram(diag)
    correct = correct * pow(2.0, 2.5)
    assert np.allclose(evaluated, correct)
    initialised = fix_inputs_to_binary_state(diag, [0, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    assert np.allclose(simulated.T, correct[:, 1])

    # Bialgebra example
    diag = ZXDiagram(2, 2, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    z = diag.add_vertex(ZXType.ZSpider)
    x = diag.add_vertex(ZXType.XSpider)
    diag.add_wire(z, ins[0])
    diag.add_wire(z, ins[1])
    diag.add_wire(x, outs[0])
    diag.add_wire(x, outs[1])
    diag.add_wire(z, x)
    correct = np.asarray([[1, 0, 0, 0], [0, 0, 0, 1], [0, 0, 0, 1], [1, 0, 0, 0]])
    correct = correct * pow(0.5, 0.5)
    evaluated = unitary_from_quantum_diagram(diag)
    assert np.allclose(evaluated, correct)
    other = ZXDiagram(2, 2, 0, 0)
    oth_ins = other.get_boundary(ZXType.Input)
    oth_outs = other.get_boundary(ZXType.Output)
    x0 = other.add_vertex(ZXType.XSpider)
    x1 = other.add_vertex(ZXType.XSpider)
    z0 = other.add_vertex(ZXType.ZSpider)
    z1 = other.add_vertex(ZXType.ZSpider)
    other.add_wire(x0, oth_ins[0])
    other.add_wire(x1, oth_ins[1])
    other.add_wire(z0, oth_outs[0])
    other.add_wire(z1, oth_outs[1])
    other.add_wire(x0, z0)
    other.add_wire(x0, z1)
    other.add_wire(x1, z0)
    other.add_wire(x1, z1)
    evaluated = unitary_from_quantum_diagram(other)
    evaluated = evaluated * pow(2.0, 0.5)
    assert np.allclose(evaluated, correct)

    # A CX gate
    diag = ZXDiagram(2, 2, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    c = diag.add_vertex(ZXType.ZSpider)
    t = diag.add_vertex(ZXType.XSpider)
    diag.add_wire(ins[0], c)
    diag.add_wire(c, outs[0])
    diag.add_wire(ins[1], t)
    diag.add_wire(t, outs[1])
    diag.add_wire(c, t)
    correct = np.asarray([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1], [0, 0, 1, 0]])
    evaluated = unitary_from_quantum_diagram(diag)
    evaluated = evaluated * pow(2.0, 0.5)
    assert np.allclose(evaluated, correct)

    # A ZXBox containing a CX gate
    diag2 = ZXDiagram(2, 2, 0, 0)
    ins = diag2.get_boundary(ZXType.Input)
    outs = diag2.get_boundary(ZXType.Output)
    b = diag2.add_zxbox(diag)
    diag2.add_wire(u=ins[0], v=b, v_port=0)
    diag2.add_wire(u=ins[1], v=b, v_port=1)
    diag2.add_wire(u=outs[0], v=b, v_port=2)
    diag2.add_wire(u=outs[1], v=b, v_port=3)
    evaluated = tensor_from_mixed_diagram(diag2)
    evaluated *= 2.0
    correct = correct.reshape((2, 2, 2, 2))
    correct = np.kron(correct, correct).reshape((2, 2, 2, 2, 2, 2, 2, 2))
    assert np.allclose(evaluated, correct)

    # A Pauli gadget
    diag = ZXDiagram(4, 4, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    v = diag.add_vertex(ZXType.XSpider, 0.5)
    vdg = diag.add_vertex(ZXType.XSpider, -0.5)
    z = diag.add_vertex(ZXType.ZSpider)
    x = diag.add_vertex(ZXType.ZSpider)
    y = diag.add_vertex(ZXType.ZSpider)
    axis = diag.add_vertex(ZXType.XSpider)
    phase = diag.add_vertex(ZXType.ZSpider, 0.3)
    diag.add_wire(ins[0], v)
    diag.add_wire(v, y)
    diag.add_wire(y, vdg)
    diag.add_wire(vdg, outs[0])
    diag.add_wire(ins[1], outs[1])
    diag.add_wire(ins[2], z)
    diag.add_wire(z, outs[2])
    diag.add_wire(ins[3], x, ZXWireType.H)
    diag.add_wire(x, outs[3], ZXWireType.H)
    diag.add_wire(x, axis)
    diag.add_wire(y, axis)
    diag.add_wire(z, axis)
    diag.add_wire(phase, axis)
    correct = (
        np.cos(0.15 * np.pi) * np.eye(16)
        - 1j
        * np.sin(0.15 * np.pi)
        * QubitPauliString(
            [Qubit(i) for i in range(4)], [Pauli.Y, Pauli.I, Pauli.Z, Pauli.X]
        ).to_sparse_matrix()
    )
    evaluated = unitary_from_quantum_diagram(diag)
    evaluated = evaluated * np.exp(-1j * 0.15 * np.pi)
    assert np.allclose(evaluated, correct)
    initialised = fix_inputs_to_binary_state(diag, [0, 1, 0, 1])
    simulated = unitary_from_quantum_diagram(initialised)
    simulated = simulated * np.exp(-1j * 0.15 * np.pi)
    assert np.allclose(simulated, correct[:, 5])
    postselected = fix_outputs_to_binary_state(diag, [1, 1, 0, 0])
    postsimulated = unitary_from_quantum_diagram(postselected)
    postsimulated = postsimulated * np.exp(-1j * 0.15 * np.pi)
    assert np.allclose(postsimulated, correct[12, :])

    # A scalar
    diag = ZXDiagram(0, 0, 0, 0)
    red_one = diag.add_vertex(ZXType.XSpider, 1.0)
    green_one = diag.add_vertex(ZXType.ZSpider, 0.3)
    diag.add_wire(red_one, green_one)
    red_three = diag.add_vertex(ZXType.XSpider)
    green_three = diag.add_vertex(ZXType.ZSpider)
    diag.add_wire(red_three, green_three)
    diag.add_wire(red_three, green_three)
    diag.add_wire(red_three, green_three)
    evaluated = tensor_from_quantum_diagram(diag)
    assert np.allclose(evaluated, np.exp(1j * 0.3 * np.pi))


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_classical_and_cptp() -> None:  # noqa: PLR0915
    # A single classical spider in a classical diagram
    diag = ZXDiagram(0, 0, 1, 2)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    spid = diag.add_vertex(ZXType.ZSpider, qtype=QuantumType.Classical)
    diag.add_wire(ins[0], spid, qtype=QuantumType.Classical)
    diag.add_wire(outs[0], spid, qtype=QuantumType.Classical)
    diag.add_wire(outs[1], spid, qtype=QuantumType.Classical)
    correct = np.asarray([[1, 0], [0, 0], [0, 0], [0, 1]])
    evaluated = unitary_from_classical_diagram(diag)
    assert np.allclose(evaluated, correct)

    # Compare a classical spider to a quantum spider in CPTP
    diag = ZXDiagram(1, 2, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    spid = diag.add_vertex(ZXType.ZSpider, 1.0, QuantumType.Classical)
    diag.add_wire(ins[0], spid)
    diag.add_wire(outs[0], spid)
    diag.add_wire(outs[1], spid)
    correct = np.zeros((8, 8))
    correct[0, 0] = 1.0
    correct[7, 7] = -1.0
    evaluated = density_matrix_from_cptp_diagram(diag)
    assert np.allclose(evaluated, correct)
    # Change classical spider to quantum spider
    diag.set_vertex_ZXGen(spid, ZXGen.create(ZXType.ZSpider, 1.0, QuantumType.Quantum))
    correct[0, 7] = -1.0
    correct[7, 0] = -1.0
    correct[7, 7] = 1.0
    evaluated = density_matrix_from_cptp_diagram(diag)
    assert np.allclose(evaluated, correct)
    # Add discard by connecting to a classical spider
    disc = diag.add_vertex(ZXType.ZSpider, 0.0, QuantumType.Classical)
    diag.add_wire(spid, disc)
    correct[0, 7] = 0.0
    correct[7, 0] = 0.0
    evaluated = density_matrix_from_cptp_diagram(diag)
    assert np.allclose(evaluated, correct)

    # Quantum teleportation
    diag = ZXDiagram(3, 1, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    bell_cx_ctrl = diag.add_vertex(ZXType.ZSpider)
    bell_cx_trgt = diag.add_vertex(ZXType.XSpider)
    meas_cx_ctrl = diag.add_vertex(ZXType.ZSpider)
    meas_cx_trgt = diag.add_vertex(ZXType.XSpider)
    meas_z = diag.add_vertex(ZXType.ZSpider, qtype=QuantumType.Classical)
    meas_x = diag.add_vertex(ZXType.XSpider, qtype=QuantumType.Classical)
    init_z = diag.add_vertex(ZXType.ZSpider, qtype=QuantumType.Classical)
    init_x = diag.add_vertex(ZXType.XSpider, qtype=QuantumType.Classical)
    corr_z = diag.add_vertex(ZXType.ZSpider)
    corr_x = diag.add_vertex(ZXType.XSpider)
    # Prepare Bell state
    diag.add_wire(ins[1], bell_cx_ctrl, ZXWireType.H)
    diag.add_wire(ins[2], bell_cx_trgt)
    diag.add_wire(bell_cx_ctrl, bell_cx_trgt)
    # Perform Bell measurement
    diag.add_wire(ins[0], meas_cx_ctrl)
    diag.add_wire(bell_cx_ctrl, meas_cx_trgt)
    diag.add_wire(meas_cx_ctrl, meas_cx_trgt)
    diag.add_wire(meas_cx_ctrl, meas_x)
    diag.add_wire(meas_cx_trgt, meas_z)
    # Apply corrections
    diag.add_wire(meas_x, init_x, qtype=QuantumType.Classical)
    diag.add_wire(meas_z, init_z, qtype=QuantumType.Classical)
    diag.add_wire(init_x, corr_z)
    diag.add_wire(init_z, corr_x)
    diag.add_wire(bell_cx_trgt, corr_x)
    diag.add_wire(corr_x, corr_z)
    diag.add_wire(corr_z, outs[0])
    correct = np.zeros((16, 16))
    # Correct [0,0] initialisation of Bell state
    correct[0, 0] = 1.0
    correct[0, 9] = 1.0
    correct[9, 0] = 1.0
    correct[9, 9] = 1.0
    # [0,1] initialisation of Bell state applies X
    correct[10, 10] = 1.0
    correct[10, 3] = 1.0
    correct[3, 10] = 1.0
    correct[3, 3] = 1.0
    # [1,0] initialisation of Bell state applies Z
    correct[4, 4] = 1.0
    correct[4, 13] = -1.0
    correct[13, 4] = -1.0
    correct[13, 13] = 1.0
    # [1,1] initialisation of Bell state applies Y
    correct[14, 14] = 1.0
    correct[14, 7] = -1.0
    correct[7, 14] = -1.0
    correct[7, 7] = 1.0
    evaluated = density_matrix_from_cptp_diagram(diag)
    evaluated = evaluated * 8.0
    assert np.allclose(evaluated, correct)
    # Simulate for different input states
    initialised = fix_inputs_to_binary_state(diag, [0, 0, 0])
    simulated = density_matrix_from_cptp_diagram(initialised)
    assert np.allclose(simulated, np.asarray([[1, 0], [0, 0]]))
    initialised = fix_inputs_to_binary_state(diag, [1, 0, 0])
    simulated = density_matrix_from_cptp_diagram(initialised)
    assert np.allclose(simulated, np.asarray([[0, 0], [0, 1]]))


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_tensor_errors() -> None:
    # A symbolic generator
    diag = ZXDiagram(0, 1, 0, 0)
    v = diag.add_vertex(ZXType.XSpider, sympify("2*a"))
    diag.add_wire(v, diag.get_boundary()[0])
    with pytest.raises(ValueError) as exc_info:
        tensor_from_quantum_diagram(diag)
    assert "symbolic expression" in exc_info.value.args[0]

    # A symbolic scalar
    diag.set_vertex_ZXGen(v, PhasedGen(ZXType.XSpider, 0.5))
    diag.multiply_scalar(sympify("2*a"))
    with pytest.raises(ValueError) as exc_info:
        tensor_from_quantum_diagram(diag)
    assert "symbolic scalar" in exc_info.value.args[0]

    # Non-quantum components in tensor_from_quantum_diagram
    diag = ZXDiagram(0, 1, 0, 0)
    v = diag.add_vertex(ZXType.XSpider, 0.3, QuantumType.Classical)
    diag.add_wire(v, diag.get_boundary()[0], qtype=QuantumType.Classical)
    with pytest.raises(ValueError) as exc_info:
        tensor_from_quantum_diagram(diag)
    assert "Non-quantum vertex" in exc_info.value.args[0]
    diag.set_vertex_ZXGen(v, PhasedGen(ZXType.XSpider, 0.3))
    with pytest.raises(ValueError) as exc_info:
        tensor_from_quantum_diagram(diag)
    assert "Non-quantum wire" in exc_info.value.args[0]

    # Mixed boundaries when expecting just one qtype
    diag = ZXDiagram(0, 1, 0, 1)
    v = diag.add_vertex(ZXType.XSpider, 0.0, QuantumType.Classical)
    diag.add_wire(v, diag.get_boundary()[0])
    diag.add_wire(v, diag.get_boundary()[1], qtype=QuantumType.Classical)
    with pytest.raises(ValueError) as exc_info:
        unitary_from_classical_diagram(diag)
    assert "Non-classical boundary vertex" in exc_info.value.args[0]
    with pytest.raises(ValueError) as exc_info:
        density_matrix_from_cptp_diagram(diag)
    assert "Non-quantum boundary vertex" in exc_info.value.args[0]

    # Errors in fixing boundaries
    with pytest.raises(ValueError) as exc_info:
        fix_boundaries_to_binary_states(diag, {v: 0})
    assert "boundary vertices" in exc_info.value.args[0]
    with pytest.raises(ValueError) as exc_info:
        fix_boundaries_to_binary_states(diag, {diag.get_boundary()[0]: 2})
    assert "|0> and |1>" in exc_info.value.args[0]

    # Wrong length of vector to fix inputs/outputs
    with pytest.raises(ValueError) as exc_info:
        fix_inputs_to_binary_state(diag, [0, 1, 0])
    assert "3 values for 0 inputs" in exc_info.value.args[0]
    with pytest.raises(ValueError) as exc_info:
        fix_outputs_to_binary_state(diag, [])
    assert "0 values for 2 outputs" in exc_info.value.args[0]


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_graph_like_reduction() -> None:
    # Diagram on https://arxiv.org/pdf/1902.03178.pdf, Figure 2
    # We have added an extra input/output pair for testing purposes
    diag = ZXDiagram(5, 5, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    z1 = diag.add_vertex(ZXType.ZSpider)
    z2 = diag.add_vertex(ZXType.ZSpider)
    z3 = diag.add_vertex(ZXType.ZSpider)
    ph1 = diag.add_vertex(ZXType.ZSpider, 0.5)
    ph2 = diag.add_vertex(ZXType.ZSpider, 1.0)
    x1 = diag.add_vertex(ZXType.XSpider)
    x2 = diag.add_vertex(ZXType.XSpider)
    x3 = diag.add_vertex(ZXType.XSpider)
    diag.add_wire(ins[0], z1)
    diag.add_wire(z1, ph1)
    diag.add_wire(ph1, z2)
    diag.add_wire(z2, outs[0], ZXWireType.H)
    diag.add_wire(z1, x1)
    diag.add_wire(z2, x2)
    diag.add_wire(ins[1], x1, ZXWireType.H)
    diag.add_wire(x1, z3)
    diag.add_wire(z3, x2)
    diag.add_wire(x2, ph2)
    diag.add_wire(ph2, outs[1])
    diag.add_wire(z3, x3)
    diag.add_wire(ins[2], x3, ZXWireType.H)
    diag.add_wire(x3, outs[2])
    diag.add_wire(ins[3], outs[3], ZXWireType.H)
    diag.add_wire(ins[4], outs[4])
    diag.check_validity()

    original = unitary_from_quantum_diagram(diag)

    # Replace X with Z spiders
    assert Rewrite.red_to_green().apply(diag)
    assert diag.count_vertices(ZXType.XSpider) == 0
    assert diag.count_vertices(ZXType.ZSpider) == 8

    # Spider fusion
    assert Rewrite.spider_fusion().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 6

    # Parallel edge pair removal
    assert not Rewrite.parallel_h_removal().apply(diag)

    # Remove hadamard edges connected directly to the boundaries
    assert Rewrite.io_extension().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 10

    # Boundary vertices sharing spiders
    # Deal with directly connected in/outputs
    assert Rewrite.separate_boundaries().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 13

    diag.check_validity()
    final = unitary_from_quantum_diagram(diag)
    final = final * pow(2.0, -3.5)
    assert np.allclose(original, final)


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_spider_fusion() -> None:  # noqa: PLR0915
    diag = ZXDiagram(2, 1, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    s1 = diag.add_vertex(ZXType.ZSpider, 0.1)
    s2 = diag.add_vertex(ZXType.ZSpider, 0.3)
    s3 = diag.add_vertex(ZXType.ZSpider)
    s4 = diag.add_vertex(ZXType.ZSpider, 0.5)
    s5 = diag.add_vertex(ZXType.ZSpider)
    diag.add_wire(ins[0], s1)
    diag.add_wire(ins[1], s5, ZXWireType.H)
    diag.add_wire(s1, s2, ZXWireType.H)
    diag.add_wire(s2, s3)
    diag.add_wire(s3, s2, ZXWireType.H)
    diag.add_wire(s3, s4, ZXWireType.H)
    diag.add_wire(s4, s5)
    diag.add_wire(s5, s1)
    diag.add_wire(s3, outs[0])
    diag.add_wire(s3, s3)
    diag.add_wire(s3, s3, ZXWireType.H)
    diag.check_validity()
    original = unitary_from_quantum_diagram(diag)
    assert Rewrite.self_loop_removal().apply(diag)
    assert Rewrite.spider_fusion().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 2
    assert Rewrite.self_loop_removal().apply(diag)
    assert Rewrite.parallel_h_removal().apply(diag)
    assert Rewrite.io_extension().apply(diag)
    diag.check_validity()
    final = unitary_from_quantum_diagram(diag)
    assert np.allclose(original, final)

    # Now with a scalar diagram
    diag = ZXDiagram(0, 0, 0, 0)
    v1 = diag.add_vertex(ZXType.ZSpider)
    v2 = diag.add_vertex(ZXType.ZSpider)
    v3 = diag.add_vertex(ZXType.ZSpider, 3.22)
    v4 = diag.add_vertex(ZXType.ZSpider)
    v5 = diag.add_vertex(ZXType.ZSpider)
    v6 = diag.add_vertex(ZXType.ZSpider)
    diag.add_wire(v1, v4, ZXWireType.H)
    diag.add_wire(v4, v5)
    diag.add_wire(v5, v4, ZXWireType.H)
    diag.add_wire(v5, v6)
    diag.add_wire(v6, v3, ZXWireType.H)
    diag.add_wire(v3, v2)
    diag.add_wire(v2, v3, ZXWireType.H)
    diag.add_wire(v2, v1)
    diag.check_validity()
    assert not Rewrite.self_loop_removal().apply(diag)
    assert Rewrite.spider_fusion().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 2
    assert Rewrite.self_loop_removal().apply(diag)
    assert Rewrite.parallel_h_removal().apply(diag)
    assert not Rewrite.io_extension().apply(diag)
    assert not Rewrite.separate_boundaries().apply(diag)
    assert diag.count_vertices(ZXType.ZSpider) == 2
    diag.check_validity()


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")(
    not have_quimb, reason="quimb not installed"
)
def test_simplification() -> None:  # noqa: PLR0915
    # This diagram follows from section A of https://arxiv.org/pdf/1902.03178.pdf
    diag = ZXDiagram(4, 4, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    v11 = diag.add_vertex(ZXType.ZSpider, 1.5)
    v12 = diag.add_vertex(ZXType.ZSpider, 0.5)
    v13 = diag.add_vertex(ZXType.ZSpider)
    v14 = diag.add_vertex(ZXType.XSpider)
    v15 = diag.add_vertex(ZXType.ZSpider, 0.25)
    v21 = diag.add_vertex(ZXType.ZSpider, 0.5)
    v22 = diag.add_vertex(ZXType.ZSpider)
    v23 = diag.add_vertex(ZXType.ZSpider)
    v24 = diag.add_vertex(ZXType.ZSpider, 0.25)
    v25 = diag.add_vertex(ZXType.ZSpider)
    v31 = diag.add_vertex(ZXType.XSpider)
    v32 = diag.add_vertex(ZXType.XSpider)
    v33 = diag.add_vertex(ZXType.ZSpider, 0.5)
    v34 = diag.add_vertex(ZXType.ZSpider, 0.5)
    v35 = diag.add_vertex(ZXType.XSpider)
    v41 = diag.add_vertex(ZXType.ZSpider)
    v42 = diag.add_vertex(ZXType.ZSpider)
    v43 = diag.add_vertex(ZXType.ZSpider, 1.5)
    v44 = diag.add_vertex(ZXType.XSpider, 1.0)
    v45 = diag.add_vertex(ZXType.ZSpider, 0.5)
    v46 = diag.add_vertex(ZXType.XSpider, 1.0)

    diag.add_wire(ins[0], v11)
    diag.add_wire(v11, v12, ZXWireType.H)
    diag.add_wire(v12, v13)
    diag.add_wire(v13, v41, ZXWireType.H)
    diag.add_wire(v13, v14)
    diag.add_wire(v14, v42)
    diag.add_wire(v14, v15, ZXWireType.H)
    diag.add_wire(v15, outs[0], ZXWireType.H)

    diag.add_wire(ins[1], v21)
    diag.add_wire(v21, v22)
    diag.add_wire(v22, v31)
    diag.add_wire(v22, v23, ZXWireType.H)
    diag.add_wire(v23, v32)
    diag.add_wire(v23, v24)
    diag.add_wire(v24, v25, ZXWireType.H)
    diag.add_wire(v25, v35)
    diag.add_wire(outs[1], v25)

    diag.add_wire(ins[2], v31)
    diag.add_wire(v31, v32)
    diag.add_wire(v32, v33)
    diag.add_wire(v33, v34, ZXWireType.H)
    diag.add_wire(v34, v35)
    diag.add_wire(v35, outs[2])

    diag.add_wire(ins[3], v41, ZXWireType.H)
    diag.add_wire(v41, v42)
    diag.add_wire(v42, v43, ZXWireType.H)
    diag.add_wire(v43, v44)
    diag.add_wire(v44, v45)
    diag.add_wire(v45, v46)
    diag.add_wire(v46, outs[3])

    diag.check_validity()
    original = unitary_from_quantum_diagram(diag)
    # Transform into graph-like form
    Rewrite.red_to_green().apply(diag)
    Rewrite.spider_fusion().apply(diag)
    Rewrite.parallel_h_removal().apply(diag)
    Rewrite.io_extension().apply(diag)
    Rewrite.separate_boundaries().apply(diag)
    # Graph simplification via Pauli & Clifford removal
    Rewrite.remove_interior_cliffords().apply(diag)
    Rewrite.extend_at_boundary_paulis().apply(diag)
    Rewrite.remove_interior_paulis().apply(diag)
    diag.check_validity()
    final = unitary_from_quantum_diagram(diag)
    final = final * 0.5 * 1j
    assert np.allclose(original, final)


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_converting_from_circuit() -> None:
    c = Circuit(4)
    c.CZ(0, 1)
    c.CX(1, 2)
    c.H(1)
    c.X(0)
    c.Rx(0.7, 0)
    c.Rz(0.2, 1)
    c.X(3)
    c.H(2)
    diag, _ = circuit_to_zx(c)
    # Check the unitaries are equal up to a global phase
    v = unitary_from_quantum_diagram(diag)
    u = c.get_unitary()
    m = v.dot(u.conj().T)
    phase = m[0][0]
    assert isclose(abs(phase), 1)
    assert np.allclose(m * (1 / phase), np.eye(16))


def test_constructors() -> None:
    phased_gen = PhasedGen(ZXType.ZSpider, 0.5, QuantumType.Quantum)
    assert phased_gen.param == 0.5
    clifford_gen = CliffordGen(ZXType.PX, True, QuantumType.Quantum)
    assert clifford_gen.param
    directed_gen = DirectedGen(ZXType.Triangle, QuantumType.Quantum)
    assert directed_gen.signature == [QuantumType.Quantum] * 2
    diag = ZXDiagram(4, 4, 0, 0)
    zx_box = ZXBox(diag)
    assert zx_box.diagram.scalar == diag.scalar


def joint_normalise_tensor(
    a: np.ndarray, b: np.ndarray
) -> tuple[np.ndarray, np.ndarray]:
    a_linear = a.reshape(a.size)
    b_linear = b.reshape(b.size)
    max_i = 0
    max_val = 0
    for i in range(a.size):
        if abs(a_linear[i]) > max_val:
            max_i = i
            max_val = abs(a_linear[i])
    return (a * (1 / a_linear[max_i]), b * (1 / b_linear[max_i]))


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_XY_extraction() -> None:
    # Identical to the diagram in test_ZXExtraction.cpp
    diag = ZXDiagram(3, 3, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    v00 = diag.add_vertex(ZXType.XY, 0.7)
    v01 = diag.add_vertex(ZXType.XY, 0.2)
    v02 = diag.add_vertex(ZXType.XY, 1.9)
    v10 = diag.add_vertex(ZXType.XY, 0.56)
    v11 = diag.add_vertex(ZXType.XY, 1.2)
    v12 = diag.add_vertex(ZXType.XY, 0.9)
    o0 = diag.add_vertex(ZXType.PX)
    o1 = diag.add_vertex(ZXType.PX)
    o2 = diag.add_vertex(ZXType.PX)
    diag.add_wire(ins[0], v00)
    diag.add_wire(ins[1], v01)
    diag.add_wire(ins[2], v02)
    diag.add_wire(v00, v10, ZXWireType.H)
    diag.add_wire(v00, v12, ZXWireType.H)
    diag.add_wire(v01, v10, ZXWireType.H)
    diag.add_wire(v01, v11, ZXWireType.H)
    diag.add_wire(v01, v12, ZXWireType.H)
    diag.add_wire(v02, v11, ZXWireType.H)
    diag.add_wire(v02, v12, ZXWireType.H)
    diag.add_wire(v10, o0, ZXWireType.H)
    diag.add_wire(v10, o2, ZXWireType.H)
    diag.add_wire(v11, o0, ZXWireType.H)
    diag.add_wire(v11, o1, ZXWireType.H)
    diag.add_wire(v11, o2, ZXWireType.H)
    diag.add_wire(v12, o1, ZXWireType.H)
    diag.add_wire(v12, o2, ZXWireType.H)
    diag.add_wire(o0, outs[0])
    diag.add_wire(o1, outs[1])
    diag.add_wire(o2, outs[2])
    diag.check_validity()
    circ, _ = diag.to_circuit()
    assert circ.n_qubits == 3
    diag_u = unitary_from_quantum_diagram(diag)
    circ_u = circ.get_unitary()
    assert compare_unitaries(diag_u, circ_u)


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_XY_YZ_extraction() -> None:  # noqa: PLR0915
    # Almost identical to the diagram in test_ZXExtraction.cpp
    # Gadgets g3 and g8 removed as they made tensor evaluation real slow
    diag = ZXDiagram(5, 5, 0, 0)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    i0 = diag.add_vertex(ZXType.XY)
    i1 = diag.add_vertex(ZXType.XY)
    i2 = diag.add_vertex(ZXType.XY, 0.25)
    i3ext = diag.add_vertex(ZXType.XY)
    i3 = diag.add_vertex(ZXType.XY, 0.25)
    i4 = diag.add_vertex(ZXType.XY)
    inter0 = diag.add_vertex(ZXType.XY, -0.25)
    inter1 = diag.add_vertex(ZXType.XY, -0.25)
    o0 = diag.add_vertex(ZXType.XY)
    o0ext = diag.add_vertex(ZXType.PX)
    o1 = diag.add_vertex(ZXType.XY)
    o1ext = diag.add_vertex(ZXType.PX)
    o2 = diag.add_vertex(ZXType.XY)
    o2ext = diag.add_vertex(ZXType.PX)
    o3 = diag.add_vertex(ZXType.PX)
    o4 = diag.add_vertex(ZXType.XY)
    o4ext = diag.add_vertex(ZXType.PX)
    g0 = diag.add_vertex(ZXType.YZ, -0.25)
    g1 = diag.add_vertex(ZXType.YZ, 0.25)
    g2 = diag.add_vertex(ZXType.YZ, 0.25)
    g4 = diag.add_vertex(ZXType.YZ, 0.25)
    g5 = diag.add_vertex(ZXType.YZ, 0.25)
    g6 = diag.add_vertex(ZXType.YZ, -0.25)
    g7 = diag.add_vertex(ZXType.YZ, -0.25)
    g9 = diag.add_vertex(ZXType.YZ, -0.25)
    # Input wires
    diag.add_wire(ins[0], i0)
    diag.add_wire(ins[1], i1)
    diag.add_wire(ins[2], i2)
    diag.add_wire(ins[3], i3ext)
    diag.add_wire(ins[4], i4)
    diag.add_wire(i3ext, i3, ZXWireType.H)
    # Interior wires
    diag.add_wire(i0, i1, ZXWireType.H)
    diag.add_wire(i0, i3, ZXWireType.H)
    diag.add_wire(i0, i4, ZXWireType.H)
    diag.add_wire(i0, inter1, ZXWireType.H)
    diag.add_wire(i0, o0, ZXWireType.H)
    diag.add_wire(i1, o1, ZXWireType.H)
    diag.add_wire(i2, o2, ZXWireType.H)
    diag.add_wire(i3, inter0, ZXWireType.H)
    diag.add_wire(i3, o3, ZXWireType.H)
    diag.add_wire(i3, o4, ZXWireType.H)
    diag.add_wire(i4, inter0, ZXWireType.H)
    diag.add_wire(inter0, inter1, ZXWireType.H)
    diag.add_wire(inter1, o4, ZXWireType.H)
    # Gadget wires
    diag.add_wire(g0, i0, ZXWireType.H)
    diag.add_wire(g0, i1, ZXWireType.H)
    diag.add_wire(g0, inter0, ZXWireType.H)
    diag.add_wire(g1, i1, ZXWireType.H)
    diag.add_wire(g1, inter0, ZXWireType.H)
    diag.add_wire(g2, i0, ZXWireType.H)
    diag.add_wire(g2, inter0, ZXWireType.H)
    diag.add_wire(g4, i3, ZXWireType.H)
    diag.add_wire(g4, inter1, ZXWireType.H)
    diag.add_wire(g5, i2, ZXWireType.H)
    diag.add_wire(g5, inter1, ZXWireType.H)
    diag.add_wire(g6, i2, ZXWireType.H)
    diag.add_wire(g6, i3, ZXWireType.H)
    diag.add_wire(g6, inter1, ZXWireType.H)
    diag.add_wire(g7, i1, ZXWireType.H)
    diag.add_wire(g7, o4, ZXWireType.H)
    diag.add_wire(g9, i2, ZXWireType.H)
    diag.add_wire(g9, i3, ZXWireType.H)
    # Output wires
    diag.add_wire(o0, o0ext, ZXWireType.H)
    diag.add_wire(o1, o1ext, ZXWireType.H)
    diag.add_wire(o2, o2ext, ZXWireType.H)
    diag.add_wire(o4, o4ext, ZXWireType.H)
    diag.add_wire(o0ext, outs[0])
    diag.add_wire(o1ext, outs[1])
    diag.add_wire(o2ext, outs[2])
    diag.add_wire(o3, outs[3])
    diag.add_wire(o4ext, outs[4])
    diag.check_validity()
    circ, _ = diag.to_circuit()
    assert circ.n_qubits == 5
    diag_u = unitary_from_quantum_diagram(diag)
    circ_u = circ.get_unitary()
    assert compare_unitaries(diag_u, circ_u)


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_ZX_rebase() -> None:  # noqa: PLR0915
    diag = ZXDiagram(2, 1, 0, 1)
    ins = diag.get_boundary(ZXType.Input)
    outs = diag.get_boundary(ZXType.Output)
    h0 = diag.add_vertex(ZXType.Hbox)
    h1 = diag.add_vertex(ZXType.Hbox, -3.7, QuantumType.Classical)
    xy = diag.add_vertex(ZXType.XY, 0.4)
    xz = diag.add_vertex(ZXType.XZ, 0.7, QuantumType.Classical)
    yz = diag.add_vertex(ZXType.YZ, 1.2)
    px = diag.add_vertex(ZXType.PX, False, QuantumType.Classical)
    py = diag.add_vertex(ZXType.PY, True, QuantumType.Classical)
    pz = diag.add_vertex(ZXType.PZ, True)
    zspid = diag.add_vertex(ZXType.ZSpider, 0.9)
    xspid = diag.add_vertex(ZXType.XSpider, 1.8, QuantumType.Classical)
    diag.add_wire(ins[0], h0)
    diag.add_wire(h0, xy)
    diag.add_wire(xy, yz)
    diag.add_wire(pz, outs[0])
    diag.add_wire(yz, zspid)
    diag.add_wire(ins[1], xspid)
    diag.add_wire(xspid, xz, ZXWireType.Basic, QuantumType.Classical)
    diag.add_wire(xz, px, ZXWireType.Basic, QuantumType.Classical)
    diag.add_wire(xz, py, ZXWireType.Basic, QuantumType.Classical)
    diag.add_wire(py, h1, ZXWireType.Basic, QuantumType.Classical)
    diag.add_wire(h1, h1)
    diag.add_wire(h1, outs[1], ZXWireType.Basic, QuantumType.Classical)
    diag.check_validity()
    t0 = tensor_from_mixed_diagram(diag)

    # Rebasing to ZX
    Rewrite.rebase_to_zx().apply(diag)
    diag.check_validity()
    assert diag.count_vertices(ZXType.Hbox) == 0
    assert diag.count_vertices(ZXType.XY) == 0
    assert diag.count_vertices(ZXType.XZ) == 0
    assert diag.count_vertices(ZXType.YZ) == 0
    assert diag.count_vertices(ZXType.PX) == 0
    assert diag.count_vertices(ZXType.PY) == 0
    assert diag.count_vertices(ZXType.PZ) == 0
    assert diag.count_vertices(ZXType.Triangle) == 0
    assert diag.count_vertices(ZXType.ZXBox) == 0
    t1 = tensor_from_mixed_diagram(diag)
    (t0, t1) = joint_normalise_tensor(t0, t1)
    assert np.allclose(t0, t1)

    # Rebasing to MBQC
    Rewrite.rebase_to_mbqc().apply(diag)
    diag.check_validity()
    assert diag.count_vertices(ZXType.Hbox) == 0
    assert diag.count_vertices(ZXType.ZSpider) == 0
    assert diag.count_vertices(ZXType.XSpider) == 0
    assert diag.count_vertices(ZXType.Triangle) == 0
    assert diag.count_vertices(ZXType.ZXBox) == 0
    t2 = tensor_from_mixed_diagram(diag)
    (t0, t2) = joint_normalise_tensor(t0, t2)
    assert np.allclose(t0, t2)


@pytest.mark.skipif(not have_quimb, reason="quimb not installed")
def test_internalise_gadgets() -> None:
    for axis_basis, axis_angle in [
        (ZXType.XY, 0.25),
        (ZXType.PX, False),
        (ZXType.PX, True),
        (ZXType.PY, False),
        (ZXType.PY, True),
    ]:
        for gadget_basis, gadget_angle in [
            (ZXType.XY, 0.25),
            (ZXType.XZ, 0.25),
            (ZXType.YZ, 0.25),
            (ZXType.PX, False),
            (ZXType.PX, True),
            (ZXType.PY, False),
            (ZXType.PY, True),
            (ZXType.PZ, False),
            (ZXType.PZ, True),
        ]:
            diag = ZXDiagram(1, 1, 0, 0)
            ins = diag.get_boundary(ZXType.Input)
            outs = diag.get_boundary(ZXType.Output)
            in_v = diag.add_vertex(ZXType.PX, False)
            out_v = diag.add_vertex(ZXType.PX, False)
            axis = diag.add_vertex(axis_basis, axis_angle)
            gadget = diag.add_vertex(gadget_basis, gadget_angle)
            diag.add_wire(ins[0], in_v)
            diag.add_wire(in_v, axis, ZXWireType.H)
            diag.add_wire(axis, out_v, ZXWireType.H)
            diag.add_wire(out_v, outs[0])
            diag.add_wire(axis, gadget, ZXWireType.H)
            t = tensor_from_quantum_diagram(diag)
            Rewrite.internalise_gadgets().apply(diag)
            if (axis_basis == ZXType.XY) and (gadget_basis in [ZXType.XY, ZXType.XZ]):
                assert diag.n_vertices == 6
            else:
                assert diag.n_vertices == 5
            t2 = tensor_from_quantum_diagram(diag)
            (t, t2) = joint_normalise_tensor(t, t2)
            assert np.allclose(t, t2)


def test_round_trip() -> None:
    circ = Circuit(5)
    circ.CCX(0, 1, 4)
    circ.CCX(2, 4, 3)
    circ.CCX(0, 1, 4)
    AutoRebase(
        {OpType.Rx, OpType.Rz, OpType.X, OpType.Z, OpType.H, OpType.CX, OpType.CZ}
    ).apply(circ)
    diag, _ = circuit_to_zx(circ)

    Rewrite.to_graphlike_form().apply(diag)
    Rewrite.reduce_graphlike_form().apply(diag)
    Rewrite.to_MBQC_diag().apply(diag)

    c, _ = diag.to_circuit()
    assert compare_unitaries(circ.get_unitary(), c.get_unitary())
