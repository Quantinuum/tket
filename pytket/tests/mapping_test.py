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


import numpy as np

from pytket import Circuit, OpType
from pytket.architecture import Architecture
from pytket.circuit import Bit, CircBox, Node, PhasePolyBox, Qubit, UnitID
from pytket.circuit.named_types import UnitIdMap
from pytket.mapping import (
    AASLabellingMethod,
    AASRouteRoutingMethod,
    BoxDecompositionRoutingMethod,
    LexiLabellingMethod,
    LexiRouteRoutingMethod,
    MappingManager,
    MultiGateReorderRoutingMethod,
    RoutingMethodCircuit,
)
from pytket.placement import Placement


# simple deterministic heuristic used for testing purposes
def route_subcircuit_func(  # noqa: PLR0912
    circuit: Circuit, architecture: Architecture
) -> tuple[bool, Circuit, UnitIdMap, UnitIdMap]:
    #     make a replacement circuit with identical unitds
    replacement_circuit = Circuit()
    for qb in circuit.qubits:
        replacement_circuit.add_qubit(qb)
    for bit in circuit.bits:
        replacement_circuit.add_bit(bit)

    # "place" unassigned logical qubits to physical qubits
    unused_nodes: list[Node] = []
    relabelling_map: UnitIdMap = {}

    for node in architecture.nodes:
        if node not in circuit.qubits:
            unused_nodes.append(node)  # noqa: PERF401

    for qb in circuit.qubits:
        if qb not in architecture.nodes:
            relabelling_map[qb] = unused_nodes.pop()
        else:
            #           this is so later architecture.get_distance works
            #           yes this is obviously bad, buts its a simple test heuristic so who cares?!
            relabelling_map[qb] = qb

    replacement_circuit.rename_units(relabelling_map)
    permutation_map: UnitIdMap = {}
    for qb in replacement_circuit.qubits:
        permutation_map[qb] = qb

    #   very simple heuristic -> the first time a physically invalid CX is encountered, add a SWAP
    #   then add all remaining gates as is (using updated physical mapping)
    #   note this is possible as routing accepts partially solved problems
    max_swaps = 1
    swaps_added = 0
    for com in circuit.get_commands():
        rp_qubits = tuple(permutation_map[relabelling_map[q]] for q in com.qubits)
        if len(com.qubits) > 2:
            return (False, Circuit(), {}, {})
        if len(com.qubits) == 1:
            replacement_circuit.add_gate(com.op.type, rp_qubits)
        if len(com.qubits) == 2:
            if swaps_added < max_swaps:
                for n in architecture.nodes:
                    if n == rp_qubits[0]:
                        n0 = n
                    if n == rp_qubits[1]:
                        n1 = n
                distance = architecture.get_distance(n0, n1)
                if distance > 1:
                    for node in architecture.get_adjacent_nodes(n0):
                        if architecture.get_distance(
                            node, n1
                        ) < architecture.get_distance(n0, n1):
                            replacement_circuit.add_gate(
                                OpType.SWAP, [rp_qubits[0], node]
                            )

                            permutation_map[rp_qubits[0]] = node
                            permutation_map[node] = rp_qubits[0]
                            rp_qubits = tuple(
                                permutation_map[relabelling_map[q]] for q in com.qubits
                            )
                            swaps_added += 1
                            break

            replacement_circuit.add_gate(com.op.type, rp_qubits)

    return (
        True,
        replacement_circuit,
        relabelling_map,
        permutation_map,
    )


def route_subcircuit_func_false(
    circuit: Circuit, architecture: Architecture
) -> tuple[bool, Circuit, dict[UnitID, UnitID], dict[UnitID, UnitID]]:
    return (False, Circuit(), {}, {})


def test_LexiRouteRoutingMethod() -> None:
    test_c = Circuit(3).CX(0, 1).CX(0, 2).CX(1, 2)
    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    test_mm = MappingManager(test_a)
    test_mm.route_circuit(test_c, [LexiLabellingMethod(), LexiRouteRoutingMethod()])
    routed_commands = test_c.get_commands()

    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[1], nodes[0]]
    assert routed_commands[1].op.type == OpType.CX
    assert routed_commands[1].qubits == [nodes[1], nodes[2]]
    assert routed_commands[2].op.type == OpType.SWAP
    assert routed_commands[2].qubits == [nodes[2], nodes[1]]
    assert routed_commands[3].op.type == OpType.CX
    assert routed_commands[3].qubits == [nodes[0], nodes[1]]


def test_AASRouteRoutingMethod() -> None:
    test_c = Circuit(3, 3)
    n_qb = 3
    qubit_indices = {Qubit(0): 0, Qubit(1): 1, Qubit(2): 2}
    phase_polynomial = [([True, False, True], 0.333), ([False, False, True], 0.05)]
    linear_transformation = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]])
    p_box = PhasePolyBox(
        n_qb, qubit_indices, list(phase_polynomial), linear_transformation
    )

    test_c.add_phasepolybox(p_box, [0, 1, 2])

    test_c.CX(0, 1).CX(0, 2).CX(1, 2)
    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [
            AASRouteRoutingMethod(1),
            LexiLabellingMethod(),
            LexiRouteRoutingMethod(),
            AASLabellingMethod(),
        ],
    )


def test_AASRouteRoutingMethod_2() -> None:
    test_c = Circuit(3, 3)
    n_qb = 3
    qubit_indices = {Qubit(0): 0, Qubit(1): 1, Qubit(2): 2}
    phase_polynomial = [([True, False, False], 0.333)]
    linear_transformation = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]])
    p_box = PhasePolyBox(
        n_qb, qubit_indices, list(phase_polynomial), linear_transformation
    )

    test_c.add_phasepolybox(p_box, [0, 1, 2])

    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [
            AASRouteRoutingMethod(1),
            LexiLabellingMethod(),
            LexiRouteRoutingMethod(),
            AASLabellingMethod(),
        ],
    )
    routed_commands = test_c.get_commands()

    assert routed_commands[0].op.type == OpType.Rz
    assert routed_commands[0].qubits == [nodes[0]]
    assert len(routed_commands) == 1


def test_AASRouteRoutingMethod_3() -> None:
    test_c = Circuit(3, 3)
    n_qb = 3
    qubit_indices = {Qubit(0): 0, Qubit(1): 1, Qubit(2): 2}
    phase_polynomial = [([True, True, False], 0.333)]
    linear_transformation = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]])
    p_box = PhasePolyBox(
        n_qb, qubit_indices, list(phase_polynomial), linear_transformation
    )

    test_c.add_phasepolybox(p_box, [0, 1, 2])

    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [
            AASRouteRoutingMethod(1),
            AASLabellingMethod(),
        ],
    )
    routed_commands = test_c.get_commands()

    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[0], nodes[1]]
    assert routed_commands[1].op.type == OpType.Rz
    assert routed_commands[1].qubits == [nodes[1]]
    assert routed_commands[2].op.type == OpType.CX
    assert routed_commands[2].qubits == [nodes[0], nodes[1]]
    assert len(routed_commands) == 3


def test_AASRouteRoutingMethod_4() -> None:
    test_c = Circuit(3, 3)
    n_qb = 3
    qubit_indices = {Qubit(0): 0, Qubit(1): 1, Qubit(2): 2}
    phase_polynomial = [([True, True, False], 0.333)]
    linear_transformation = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]])
    p_box = PhasePolyBox(
        n_qb, qubit_indices, list(phase_polynomial), linear_transformation
    )

    test_c.add_phasepolybox(p_box, [0, 1, 2])
    test_c.CX(0, 1)

    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])
    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [
            AASRouteRoutingMethod(1),
            LexiLabellingMethod(),
            LexiRouteRoutingMethod(),
            AASLabellingMethod(),
        ],
    )
    routed_commands = test_c.get_commands()

    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[0], nodes[1]]
    assert routed_commands[1].op.type == OpType.Rz
    assert routed_commands[1].qubits == [nodes[1]]
    assert routed_commands[2].op.type == OpType.CX
    assert routed_commands[2].qubits == [nodes[0], nodes[1]]
    assert routed_commands[3].op.type == OpType.CX
    assert routed_commands[3].qubits == [nodes[0], nodes[1]]
    assert len(routed_commands) == 4


def test_RoutingMethodCircuit_custom() -> None:
    test_c = Circuit(3).CX(0, 1).CX(0, 2).CX(1, 2)
    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [RoutingMethodCircuit(route_subcircuit_func, 5, 5)],
    )
    routed_commands = test_c.get_commands()

    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[0], nodes[1]]
    assert routed_commands[1].op.type == OpType.SWAP
    assert routed_commands[1].qubits == [nodes[0], nodes[1]]
    assert routed_commands[2].op.type == OpType.CX
    assert routed_commands[2].qubits == [nodes[1], nodes[2]]
    assert routed_commands[3].op.type == OpType.SWAP
    assert routed_commands[3].qubits == [nodes[0], nodes[1]]
    assert routed_commands[4].op.type == OpType.CX
    assert routed_commands[4].qubits == [nodes[1], nodes[2]]


def test_RoutingMethodCircuit_custom_list() -> None:
    test_c = Circuit(3).CX(0, 1).CX(0, 2).CX(1, 2)
    nodes = [Node("test", 0), Node("test", 1), Node("test", 2)]
    test_a = Architecture([(nodes[0], nodes[1]), (nodes[1], nodes[2])])

    test_mm = MappingManager(test_a)
    test_mm.route_circuit(
        test_c,
        [
            RoutingMethodCircuit(route_subcircuit_func_false, 5, 5),
            LexiLabellingMethod(),
            LexiRouteRoutingMethod(),
        ],
    )
    routed_commands = test_c.get_commands()
    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[1], nodes[0]]
    assert routed_commands[1].op.type == OpType.CX
    assert routed_commands[1].qubits == [nodes[1], nodes[2]]
    assert routed_commands[2].op.type == OpType.SWAP
    assert routed_commands[2].qubits == [nodes[2], nodes[1]]
    assert routed_commands[3].op.type == OpType.CX
    assert routed_commands[3].qubits == [nodes[0], nodes[1]]

    test_c = Circuit(3).CX(0, 1).CX(0, 2).CX(1, 2)
    test_mm.route_circuit(
        test_c,
        [
            RoutingMethodCircuit(route_subcircuit_func, 5, 5),
            LexiLabellingMethod(),
            LexiRouteRoutingMethod(),
        ],
    )
    routed_commands = test_c.get_commands()
    assert routed_commands[0].op.type == OpType.CX
    assert routed_commands[0].qubits == [nodes[0], nodes[1]]
    assert routed_commands[1].op.type == OpType.SWAP
    assert routed_commands[1].qubits == [nodes[0], nodes[1]]
    assert routed_commands[2].op.type == OpType.CX
    assert routed_commands[2].qubits == [nodes[1], nodes[2]]
    assert routed_commands[3].op.type == OpType.SWAP
    assert routed_commands[3].qubits == [nodes[0], nodes[1]]
    assert routed_commands[4].op.type == OpType.CX
    assert routed_commands[4].qubits == [nodes[1], nodes[2]]


def test_basic_mapping() -> None:
    circ = Circuit(5)
    arc = Architecture([(0, 1), (1, 2), (2, 3), (3, 4)])
    circ.CX(0, 1)
    circ.CX(0, 3)
    circ.CX(2, 4)
    circ.CX(1, 4)
    circ.CX(0, 4)

    init_map = {}
    init_map[Qubit(0)] = Node(0)
    init_map[Qubit(1)] = Node(1)
    init_map[Qubit(2)] = Node(2)
    init_map[Qubit(3)] = Node(3)
    init_map[Qubit(4)] = Node(4)
    pl = Placement(arc)
    pl.place_with_map(circ, init_map)
    MappingManager(arc).route_circuit(circ, [LexiRouteRoutingMethod(50)])
    assert circ.valid_connectivity(arc, directed=False)
    assert len(circ.get_commands()) == 10


def test_MultiGateReorderRoutingMethod() -> None:
    circ = Circuit(5)
    arc = Architecture([(0, 1), (1, 2), (2, 3), (3, 4)])
    # Invalid operation
    circ.CZ(0, 2)
    # Valid operations that can all be commuted to the front
    circ.CZ(0, 1)
    circ.CZ(1, 2)
    circ.CZ(3, 2)
    circ.CX(3, 4)

    init_map = {}
    init_map[Qubit(0)] = Node(0)
    init_map[Qubit(1)] = Node(1)
    init_map[Qubit(2)] = Node(2)
    init_map[Qubit(3)] = Node(3)
    init_map[Qubit(4)] = Node(4)
    pl = Placement(arc)
    pl.place_with_map(circ, init_map)
    # LexiRouteRoutingMethod should insert exactly one SWAP to route the final CZ gate
    MappingManager(arc).route_circuit(
        circ, [MultiGateReorderRoutingMethod(10, 10), LexiRouteRoutingMethod(50)]
    )
    assert circ.valid_connectivity(arc, directed=False)
    assert len(circ.get_commands()) == 6


def test_MultiGateReorderRoutingMethod_with_LexiLabelling() -> None:
    circ = Circuit(4)
    arc = Architecture([(0, 1), (1, 2), (2, 3), (0, 3)])

    # LexiLabellingMethod should label the circuit such that the following 4 ops are valid
    circ.CX(0, 1)
    circ.CX(1, 2)
    circ.CX(2, 3)
    circ.CX(0, 3)

    # Invalid CV
    circ.CV(0, 2)

    # The next op should be commuted to the front of the previous CV
    circ.CZ(0, 1)

    # LexiRouteRoutingMethod should insert exactly one SWAP to route the CV gate
    MappingManager(arc).route_circuit(
        circ,
        [
            LexiLabellingMethod(),
            MultiGateReorderRoutingMethod(10, 10),
            LexiRouteRoutingMethod(50),
        ],
    )
    assert circ.valid_connectivity(arc, directed=False)
    commands = circ.get_commands()
    assert len(commands) == 7
    assert commands[4].op.type == OpType.CZ
    assert commands[5].op.type == OpType.SWAP


def test_BoxDecompositionRoutingMethod() -> None:
    circ = Circuit(5)
    sub_circ = Circuit(5)
    arc = Architecture([(0, 1), (1, 2), (2, 3), (3, 4)])
    # Invalid operations
    sub_circ.CZ(0, 2)
    sub_circ.CZ(1, 3)
    circ_box = CircBox(sub_circ)
    circ.add_circbox(circ_box, [0, 1, 2, 3, 4])
    circ.CZ(1, 3)

    init_map = {}
    init_map[Qubit(0)] = Node(0)
    init_map[Qubit(1)] = Node(1)
    init_map[Qubit(2)] = Node(2)
    init_map[Qubit(3)] = Node(3)
    init_map[Qubit(4)] = Node(4)
    pl = Placement(arc)
    pl.place_with_map(circ, init_map)
    # LexiRouteRoutingMethod should insert exactly one SWAP
    MappingManager(arc).route_circuit(
        circ, [BoxDecompositionRoutingMethod(), LexiRouteRoutingMethod(50)]
    )
    assert circ.valid_connectivity(arc, directed=False)
    assert len(circ.get_commands()) == 4


def test_LexiRoute_with_clexpr_in_lookahead() -> None:
    # https://github.com/Quantinuum/tket/issues/2180
    circ = Circuit(3, 3)
    circ.CX(0, 1)
    circ.CX(2, 1)
    circ.H(2, condition_bits=[2])
    circ.add_clexpr_from_logicexp(Bit(1) | Bit(2), [Bit(0)])
    circ.add_clexpr_from_logicexp(Bit(1) & Bit(0), [Bit(2)])
    circ.CX(0, 2)
    circ.CX(0, 2)
    arc = Architecture([(0, 1), (1, 2)])
    MappingManager(arc).route_circuit(
        circ, [LexiLabellingMethod(), LexiRouteRoutingMethod()]
    )
    assert circ.valid_connectivity(arc, directed=False)
