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
import pickle
from typing import Any

import numpy as np
import pytest
from sympy import Symbol

from pytket.architecture import Architecture
from pytket.circuit import (
    Bit,
    CircBox,
    Circuit,
    Conditional,
    MultiBitOp,
    Node,
    Op,
    OpType,
    PauliExpBox,
    Qubit,
    RangePredicateOp,
    SetBitsOp,
    Unitary1qBox,
    Unitary2qBox,
)
from pytket.circuit.named_types import ParamType, RenameUnitsMap
from pytket.circuit.named_types import ParamType as Param
from pytket.mapping import (
    LexiLabellingMethod,
    LexiRouteRoutingMethod,
)
from pytket.passes import (
    AASRouting,
    AutoRebase,
    CliffordPushThroughMeasures,
    CliffordResynthesis,
    CliffordSimp,
    CnXPairwiseDecomposition,
    CommuteThroughMultis,
    CXMappingPass,
    DecomposeBoxes,
    DecomposeMultiQubitsCX,
    DecomposeSwapsToCircuit,
    DecomposeSwapsToCXs,
    DefaultMappingPass,
    EulerAngleReduction,
    FlattenRelabelRegistersPass,
    FullMappingPass,
    GreedyPauliSimp,
    NaivePlacementPass,
    PauliSimp,
    PauliSquash,
    PeepholeOptimise2Q,
    PlacementPass,
    RebaseCustom,
    RebaseTket,
    RemoveBarriers,
    RemoveDiscarded,
    RemoveImplicitQubitPermutation,
    RemovePhaseOps,
    RemoveRedundancies,
    RenameQubitsPass,
    RepeatPass,
    RepeatUntilSatisfiedPass,
    RepeatWithMetricPass,
    RoundAngles,
    RoutingPass,
    RxFromSX,
    SequencePass,
    SimplifyInitial,
    SimplifyMeasured,
    SquashRzPhasedX,
    SquashTK1,
    SynthesiseTK,
    SynthesiseTket,
    ThreeQubitSquash,
    ZXGraphlikeOptimisation,
    ZZPhaseToRz,
)
from pytket.pauli import Pauli
from pytket.placement import GraphPlacement, Placement
from pytket.predicates import (
    CliffordCircuitPredicate,
    CommutableMeasuresPredicate,
    CompilationUnit,
    ConnectivityPredicate,
    DirectednessPredicate,
    GateSetPredicate,
    MaxNClRegPredicate,
    MaxNQubitsPredicate,
    MaxTwoQubitGatesPredicate,
    NoBarriersPredicate,
    NoClassicalBitsPredicate,
    NoClassicalControlPredicate,
    NoFastFeedforwardPredicate,
    NoMidMeasurePredicate,
    NormalisedTK2Predicate,
    NoSymbolsPredicate,
    NoWireSwapsPredicate,
    PlacementPredicate,
)
from pytket.transform import CXConfigType, PauliSynthStrat, Transform

circ2 = Circuit(1)
circ2.Rx(0.25, 0)

ots = {OpType.X, OpType.Z}
gsp = GateSetPredicate(ots)
nccp = NoClassicalControlPredicate()


def tk1_to_phasedxrz(a: Param, b: Param, c: Param) -> Circuit:
    circ = Circuit(1)
    circ.Rz(a + c, 0)
    phasedx_op = Op.create(OpType.PhasedX, [b, a])
    circ.add_gate(phasedx_op, [0])
    Transform.RemoveRedundancies().apply(circ)
    return circ


def test_predicate_generation() -> None:
    string_gsp = repr(gsp)
    assert string_gsp.find("X") != -1
    assert string_gsp.find("Z") != -1
    assert repr(nccp) == "NoClassicalControlPredicate"


def test_compilation_unit_generation() -> None:
    pp_list = [gsp, nccp]
    circ = Circuit(2)
    circ.X(0).Z(1)
    cu = CompilationUnit(circ, pp_list)
    assert cu.check_all_predicates()
    cu2 = CompilationUnit(circ2, pp_list)
    assert not cu2.check_all_predicates()


def test_compilerpass_seq() -> None:
    passlist = [SynthesiseTket(), SynthesiseTK()]
    seq = SequencePass(passlist)
    circ = Circuit(2)
    circ.X(0).Z(1)
    cu = CompilationUnit(circ)
    cu2 = CompilationUnit(circ2)
    assert seq.apply(cu)
    assert seq.apply(cu2)


def test_compilerpass_seq_nonstrict() -> None:
    passlist = [RebaseTket(), ZXGraphlikeOptimisation()]
    with pytest.raises(RuntimeError):
        _ = SequencePass(passlist)
    seq = SequencePass(passlist, strict=False)
    circ = Circuit(2)
    seq.apply(circ)
    assert np.allclose(circ.get_unitary(), np.eye(4, 4, dtype=complex))


def test_rebase_pass_generation() -> None:
    cx = Circuit(2)
    cx.CX(0, 1)
    pz_rebase = RebaseCustom(
        {OpType.CX, OpType.PhasedX, OpType.Rz}, cx, tk1_to_phasedxrz
    )
    circ = Circuit(2)
    circ.X(0).Y(1)
    cu = CompilationUnit(circ)
    assert pz_rebase.apply(cu)
    coms = cu.circuit.get_commands()
    assert str(coms) == "[PhasedX(1, 0) q[0];, PhasedX(1, 0.5) q[1];]"

    passlist = [pz_rebase, SynthesiseTket()]
    seq = SequencePass(passlist)
    assert seq.apply(cu)
    coms = cu.circuit.get_commands()
    assert str(coms) == "[TK1(0, 1, 0) q[0];, TK1(0, 1, 3) q[1];]"


def test_rebase_pass_generation_via_TK2() -> None:
    def tk1(a: Param, b: Param, c: Param) -> "Circuit":
        circ = Circuit(1)
        circ.Rz(c, 0).Rx(b, 0).Rz(a, 0)
        return circ

    def tk2(a: Param, b: Param, c: Param) -> "Circuit":
        circ = Circuit(2)
        circ.add_gate(OpType.ZZPhase, c, [0, 1])
        circ.add_gate(OpType.YYPhase, b, [0, 1])
        circ.add_gate(OpType.XXPhase, a, [0, 1])
        return circ

    rebase_pass = RebaseCustom(
        {
            OpType.Rx,
            OpType.Ry,
            OpType.Rz,
            OpType.XXPhase,
            OpType.YYPhase,
            OpType.ZZPhase,
        },
        tk2,
        tk1,
    )

    circ = Circuit(3).H(0).CX(0, 1).H(1).CX(1, 2)
    rebase_pass.apply(circ)
    assert circ.n_gates_of_type(OpType.XXPhase) == 2
    assert circ.n_gates_of_type(OpType.YYPhase) == 0
    assert circ.n_gates_of_type(OpType.ZZPhase) == 0


def test_custom_combinator_generation() -> None:
    def test_CX_size_threshold(circ: Circuit) -> bool:
        return bool(circ.n_gates_of_type(OpType.CX) == 0)

    seq_pass = SequencePass([RemoveRedundancies(), CommuteThroughMultis()])
    custom_pass = RepeatUntilSatisfiedPass(seq_pass, test_CX_size_threshold)

    circ = Circuit(2)
    circ.CX(0, 1)
    circ.X(1)
    circ.CX(0, 1)
    circ.X(1)
    circ.CX(0, 1)
    circ.X(1)
    circ.CX(0, 1)
    circ.Z(1)
    circ.CX(1, 0)
    circ.Z(1)
    circ.CX(1, 0)

    cu = CompilationUnit(circ)
    assert custom_pass.apply(cu)

    # Test in-place application
    circ1 = cu.circuit
    assert custom_pass.apply(circ)
    assert circ == circ1
    assert not custom_pass.apply(circ)


def test_routing_and_placement_pass() -> None:
    # Qubit interaction graph:
    # 1 -- 0 -- 3
    #  \   |
    #   `  4 -- 2
    circ = Circuit()
    q = circ.add_q_register("q", 5)
    circ.CX(0, 1)
    circ.H(0)
    circ.Z(1)
    circ.CX(0, 3)
    circ.Rx(1.5, 3)
    circ.CX(2, 4)
    circ.X(2)
    circ.CX(1, 4)
    circ.CX(0, 4)
    # Architecture graph:
    # f2
    # |
    # b1 -- b0
    # |
    # b2 -- a0 -- f0
    n0 = Node("b", 0)
    n1 = Node("b", 1)
    n2 = Node("b", 2)
    n3 = Node("a", 0)
    n4 = Node("f", 0)
    n5 = Node("f", 2)
    arc = Architecture([(n0, n1), (n1, n2), (n2, n3), (n3, n4), (n1, n5)])
    pl = Placement(arc)
    routing = RoutingPass(arc)
    placement = PlacementPass(pl)
    nplacement = NaivePlacementPass(arc)
    cu = CompilationUnit(circ.copy())
    assert placement.apply(cu)
    assert routing.apply(cu)
    assert nplacement.apply(cu)
    arcnodes = arc.nodes
    expected_map = {
        q[0]: arcnodes[0],
        q[1]: arcnodes[1],
        q[2]: arcnodes[2],
        q[3]: arcnodes[3],
        q[4]: arcnodes[4],
    }
    assert cu.initial_map == expected_map

    # check composition works ok
    seq_pass = SequencePass([SynthesiseTket(), placement, routing])
    cu2 = CompilationUnit(circ.copy())
    assert seq_pass.apply(cu2)
    assert cu2.initial_map == expected_map

    full_pass = FullMappingPass(
        arc, pl, config=[LexiLabellingMethod(), LexiRouteRoutingMethod()]
    )
    cu3 = CompilationUnit(circ.copy())
    assert full_pass.apply(cu3)
    assert cu3.initial_map == expected_map
    assert cu.circuit == cu3.circuit


def test_default_mapping_pass() -> None:
    circ = Circuit()
    circ.add_q_register("q", 6)
    circ.CX(0, 1)
    circ.H(0)
    circ.Z(1)
    circ.CX(0, 3)
    circ.Rx(1.5, 3)
    circ.CX(2, 4)
    circ.X(2)
    circ.CX(1, 4)
    circ.CX(0, 4)
    circ.H(5)
    n0 = Node("b", 0)
    n1 = Node("b", 1)
    n2 = Node("b", 2)
    n3 = Node("a", 0)
    n4 = Node("f", 0)
    n5 = Node("g", 7)
    arc = Architecture([(n0, n1), (n1, n2), (n2, n3), (n3, n4), (n4, n5)])
    pl = GraphPlacement(arc)

    nplacement = NaivePlacementPass(arc)
    routing = RoutingPass(arc)
    placement = PlacementPass(pl)
    default = DefaultMappingPass(arc)
    cu_rp = CompilationUnit(circ.copy())
    cu_def = CompilationUnit(circ.copy())

    assert placement.apply(cu_rp)
    assert routing.apply(cu_rp)
    assert nplacement.apply(cu_rp)
    assert default.apply(cu_def)
    assert cu_rp.circuit == cu_def.circuit


def test_default_mapping_pass_phase_poly_aas() -> None:
    circ = Circuit()
    circ.add_q_register("q", 5)
    circ.CX(0, 1)
    circ.H(0)
    circ.Z(1)
    circ.CX(0, 3)
    circ.Rx(1.5, 3)
    circ.CX(2, 4)
    circ.X(2)
    circ.CX(1, 4)
    circ.CX(0, 4)
    n0 = Node("a", 0)
    n1 = Node("b", 1)
    n2 = Node("c", 2)
    n3 = Node("d", 3)
    n4 = Node("e", 4)
    arc = Architecture([(n0, n1), (n1, n2), (n2, n3), (n3, n4)])
    default = AASRouting(arc, lookahead=1)
    assert default.apply(circ)


def test_rename_qubits_pass() -> None:
    circ = Circuit()
    circ.add_q_register("a", 2)
    circ.CX(Qubit("a", 0), Qubit("a", 1))
    qm = {Qubit("a", 0): Qubit("b", 1), Qubit("a", 1): Qubit("b", 0)}
    p = RenameQubitsPass(qm)
    cu = CompilationUnit(circ)
    p.apply(cu)
    newcirc = cu.circuit
    assert set(newcirc.qubits) == {Qubit("b", i) for i in range(2)}


def gate_count_metric(circ: Circuit) -> int:
    return int(circ.n_gates)


def test_SynthesiseTket_creation() -> None:
    # my_synthesise_tket should act on a CompilationUnit the same as SynthesiseTket
    seq_pass = SequencePass([CommuteThroughMultis(), RemoveRedundancies()])
    repeat_pass = RepeatPass(seq_pass)
    synth_pass = SequencePass(
        [DecomposeMultiQubitsCX(), RemoveRedundancies(), repeat_pass, SquashTK1()]
    )
    small_part = SequencePass([RemoveRedundancies(), repeat_pass, SquashTK1()])
    repeat_synth_pass = RepeatWithMetricPass(small_part, gate_count_metric)
    my_synthesise_tket = SequencePass([synth_pass, repeat_synth_pass])

    circ1 = Circuit(3)
    circ1.X(0).Y(1).CX(0, 1).Z(0).Rx(1.3, 1).CX(0, 1).Rz(0.4, 0).Ry(0.53, 0).H(1).H(
        2
    ).Rx(1.5, 2).Rx(0.5, 2).H(2)
    cu1 = CompilationUnit(circ1)
    my_synthesise_tket.apply(cu1)
    circ2 = cu1.circuit
    assert circ2.n_gates == 2

    cu2 = CompilationUnit(circ1)
    # Blue Peter voice: here's one I made earlier
    SynthesiseTket().apply(cu2)
    circ3 = cu2.circuit
    assert circ3.n_gates == 2
    assert circ2 == circ3

    # now let's run with routing
    arc = Architecture([(0, 2), (1, 2)])
    pl = Placement(arc)
    pl.place(circ1)
    routing_pass = RoutingPass(arc)
    cu3 = CompilationUnit(circ1)
    cu4 = CompilationUnit(circ1)
    cu5 = CompilationUnit(circ1)
    assert routing_pass.apply(cu3)

    full_pass = SequencePass([SynthesiseTket(), routing_pass])
    full_pass2 = SequencePass([my_synthesise_tket, routing_pass])
    assert full_pass.apply(cu4)
    assert full_pass2.apply(cu5)
    assert cu4.circuit == cu5.circuit


def test_directed_cx_pass() -> None:
    circ = Circuit(5)
    circ.CX(0, 1)
    circ.Rx(1.4, 0)
    circ.H(1)
    circ.CX(0, 2)
    circ.CX(0, 3)
    circ.Sdg(4)
    circ.CX(2, 3)
    circ.CX(3, 4)
    circ.CX(4, 3)
    circ.CX(3, 1)
    arc = Architecture([(0, 1), (1, 2), (2, 3), (3, 4)])
    pl = Placement(arc)
    cu1 = CompilationUnit(circ)
    dir_router = CXMappingPass(arc, pl, directed_cx=True)
    assert dir_router.apply(cu1)

    circ2 = cu1.circuit
    dir_pred = DirectednessPredicate(arc)
    assert dir_pred.verify(circ2)


def test_no_barriers_pred() -> None:
    pred = NoBarriersPredicate()
    c = Circuit(1).H(0)
    assert pred.verify(c)
    c.add_barrier([0]).H(0)
    assert not pred.verify(c)


def test_MaxNClRegPredicate_pred() -> None:
    pred = MaxNClRegPredicate(3)
    c = Circuit(1).H(0)
    c.add_c_register("name", 2)
    c.add_c_register("name2", 1)
    c.add_c_register("name3", 1)
    assert pred.verify(c)
    c.add_barrier([0]).H(0)
    c.add_c_register("name4", 1)
    assert not pred.verify(c)


def test_decompose_routing_gates_to_cxs() -> None:
    circ = Circuit(4)
    circ.CX(1, 0)
    circ.SWAP(0, 1)
    circ.SWAP(1, 2)
    circ.CX(2, 3)

    cu = CompilationUnit(circ)

    arc = Architecture([(0, 1), (1, 2), (2, 3)])
    pss = DecomposeSwapsToCXs(arc)

    assert pss.apply(cu)
    circ1 = cu.circuit

    for cmd in circ1.get_commands():
        assert cmd.op.type == OpType.CX


def test_remove_barriers() -> None:
    circ = Circuit(4)
    circ.CX(0, 1)
    circ.CX(1, 2)
    circ.add_barrier([1, 2])
    circ.CX(2, 3)

    cu = CompilationUnit(circ)
    pss = RemoveBarriers()

    assert pss.apply(cu)
    circ1 = cu.circuit

    for cmd in circ1.get_commands():
        assert cmd.op.type == OpType.CX


def test_remove_phase_ops() -> None:
    circ = Circuit(2, 2).H(0).H(1).measure_all()
    circ0 = circ.copy()
    circ.Phase(0.0)
    circ.Phase(0.1, condition_bits=[Bit(0), Bit(1)], condition_value=3)
    assert RemovePhaseOps().apply(circ)
    assert circ == circ0


def test_user_defined_swap_decomp() -> None:
    circ = Circuit(2)
    circ.SWAP(0, 1)

    cu = CompilationUnit(circ)

    repcirc = Circuit(2)
    repcirc.X(0)
    repcirc.CX(0, 1)
    repcirc.CX(1, 0)
    repcirc.CX(0, 1)
    repcirc.X(1)

    pss = DecomposeSwapsToCircuit(repcirc)

    assert pss.apply(cu)
    circ1 = cu.circuit

    assert circ1 == repcirc


def test_pauligraph_synth() -> None:
    circ = Circuit(4, 4, name="test")
    pg = PauliExpBox([Pauli.X, Pauli.Z, Pauli.Y, Pauli.I], 0.3)
    circ.add_pauliexpbox(pg, [0, 1, 2, 3])
    circ.measure_all()
    cu = CompilationUnit(circ)

    pss = PauliSimp(PauliSynthStrat.Sets, CXConfigType.Tree)
    assert pss.apply(cu)
    circ1 = cu.circuit
    assert circ1.depth_by_type(OpType.CX) == 4
    assert circ1.name == "test"


def test_squash_chains() -> None:
    # XY
    c = Circuit(2)
    c.H(0).H(1)
    c.Rx(0.1, 0).Ry(0.2, 0).Rx(0.3, 0).Ry(0.4, 0).Ry(0.5, 0).Rx(0.6, 0)
    c.CX(0, 1).H(1)
    c.Ry(0.7, 1).Rx(0.8, 1).Rx(0.9, 1).Ry(1.1, 1).Rx(1.2, 1)
    u = c.get_unitary()
    EulerAngleReduction(OpType.Rx, OpType.Ry, strict=True).apply(c)
    u1 = c.get_unitary()
    assert np.allclose(u, u1)
    optypes = [cmd.op.type for cmd in c.get_commands()]
    assert optypes == [
        OpType.H,
        OpType.H,
        OpType.Ry,
        OpType.Rx,
        OpType.Ry,
        OpType.CX,
        OpType.H,
        OpType.Ry,
        OpType.Rx,
        OpType.Ry,
    ]
    # XZ
    c = Circuit(2)
    c.H(0).H(1)
    c.Rx(0.1, 0).Rz(0.2, 0).Rx(0.3, 0).Rz(0.4, 0).Rz(0.5, 0).Rx(0.6, 0)
    c.CX(0, 1).H(1)
    c.Rz(0.7, 1).Rx(0.8, 1).Rx(0.9, 1).Rz(1.1, 1).Rx(1.2, 1)
    u = c.get_unitary()
    EulerAngleReduction(OpType.Rx, OpType.Rz, strict=True).apply(c)
    u1 = c.get_unitary()
    assert np.allclose(u, u1)
    optypes = [cmd.op.type for cmd in c.get_commands()]
    assert optypes == [
        OpType.H,
        OpType.H,
        OpType.Rz,
        OpType.Rx,
        OpType.Rz,
        OpType.CX,
        OpType.H,
        OpType.Rz,
        OpType.Rx,
        OpType.Rz,
    ]
    # YZ
    c = Circuit(2)
    c.H(0).H(1)
    c.Ry(0.1, 0).Rz(0.2, 0).Ry(0.3, 0).Rz(0.4, 0).Rz(0.5, 0).Ry(0.6, 0)
    c.CX(0, 1).H(1)
    c.Rz(0.7, 1).Ry(0.8, 1).Ry(0.9, 1).Rz(1.1, 1).Ry(1.2, 1)
    u = c.get_unitary()
    EulerAngleReduction(OpType.Ry, OpType.Rz, strict=True).apply(c)
    u1 = c.get_unitary()
    assert np.allclose(u, u1)
    optypes = [cmd.op.type for cmd in c.get_commands()]
    assert optypes == [
        OpType.H,
        OpType.H,
        OpType.Rz,
        OpType.Ry,
        OpType.Rz,
        OpType.CX,
        OpType.H,
        OpType.Rz,
        OpType.Ry,
        OpType.Rz,
    ]


def test_apply_pass_with_callbacks() -> None:
    class CallbackHandler:
        def __init__(self) -> None:
            self.pass_names: list[str] = []

        def before_apply(self, cu: CompilationUnit, config: dict[str, Any]) -> None:
            if "StandardPass" in config:
                self.pass_names.append(config["StandardPass"]["name"])
            else:
                self.pass_names.append(config["pass_class"])

        def after_apply(self, cu: CompilationUnit, config: dict[str, Any]) -> None:
            return

    def apply_pass(circ: Circuit, handler: CallbackHandler) -> bool:
        p = SequencePass([CommuteThroughMultis(), RemoveRedundancies()])
        return p.apply(circ, handler.before_apply, handler.after_apply)

    circ = Circuit(5)
    circ.CX(0, 1)
    circ.CX(2, 4)
    circ.CX(0, 1)

    handler = CallbackHandler()
    apply_pass(circ, handler)

    assert circ.n_gates_of_type(OpType.CX) == 1
    assert len(handler.pass_names) == 3
    assert handler.pass_names[0] == "SequencePass"
    assert handler.pass_names[1] == "CommuteThroughMultis"
    assert handler.pass_names[2] == "RemoveRedundancies"


def test_remove_discarded() -> None:
    c = Circuit(3, 2)
    c.H(0).H(1).H(2).CX(0, 1).Measure(0, 0).Measure(1, 1).H(0).H(1)
    c.qubit_discard(Qubit(0))
    c.qubit_discard(Qubit(2))
    assert not c.qubit_is_discarded(Qubit(1))
    assert c.qubit_is_discarded(Qubit(2))
    assert RemoveDiscarded().apply(c)
    assert c.n_gates_of_type(OpType.H) == 3
    assert c.n_gates_of_type(OpType.CX) == 1
    assert c.n_gates_of_type(OpType.Measure) == 2


def test_simplify_measured() -> None:
    c = Circuit(3, 3)
    c.H(0).H(1).Z(2)
    u = np.array(
        [
            [0, 0, 0, np.exp(0.1j)],
            [np.exp(0.2j), 0, 0, 0],
            [0, np.exp(0.3j), 0, 0],
            [0, 0, np.exp(0.4j), 0],
        ],
        dtype=complex,
    )
    ubox = Unitary2qBox(u)
    c.add_unitary2qbox(ubox, 0, 1)
    c.measure_all()
    c.qubit_discard(Qubit(0))
    c.qubit_discard(Qubit(1))
    assert SimplifyMeasured().apply(c)
    assert c.n_gates_of_type(OpType.H) == 2
    assert c.n_gates_of_type(OpType.Z) == 1
    assert c.n_gates_of_type(OpType.Unitary2qBox) == 0
    assert c.n_gates_of_type(OpType.Measure) == 3
    assert c.n_gates_of_type(OpType.ClassicalTransform) == 1


def test_simplify_initial_1() -> None:
    c = Circuit(4)
    c.H(0).X(1).CY(1, 2).CX(0, 1).CX(2, 3).H(1).H(2)
    assert not c.qubit_is_created(Qubit(0))
    c.qubit_create(Qubit(0))
    assert c.qubit_is_created(Qubit(0))
    c.qubit_create(Qubit(1))
    c.qubit_create(Qubit(2))
    assert SimplifyInitial().apply(c)
    assert c.n_gates_of_type(OpType.CY) == 0
    assert c.n_gates_of_type(OpType.CX) == 2


def test_simplify_initial_2() -> None:
    c = Circuit(1, 1).Y(0).measure_all()
    c.qubit_create_all()
    c1 = c.copy()
    assert SimplifyInitial(False).apply(c1)
    assert c1.n_gates_of_type(OpType.Y) == 0
    assert c1.n_gates_of_type(OpType.X) == 1
    assert c1.n_gates_of_type(OpType.Measure) == 1
    assert c1.n_gates_of_type(OpType.SetBits) == 0
    c2 = c.copy()
    xcirc = Circuit(1).Rx(1, 0)
    assert SimplifyInitial(xcirc=xcirc).apply(c2)
    assert c2.n_gates_of_type(OpType.Y) == 0
    assert c2.n_gates_of_type(OpType.Rx) == 1
    assert c2.n_gates_of_type(OpType.Measure) == 0
    assert c2.n_gates_of_type(OpType.SetBits) == 1


def test_simplify_initial_3() -> None:
    c = Circuit(2).X(0).CX(0, 1).CX(1, 0).X(1).CX(1, 0)
    c0 = c.copy()
    assert SimplifyInitial(create_all_qubits=True, remove_redundancies=False).apply(c0)
    c0_cmds = c0.get_commands()
    assert len(c0_cmds) > 0
    assert all(cmd.op.type == OpType.X for cmd in c0_cmds)
    c1 = c.copy()
    assert SimplifyInitial(create_all_qubits=True, remove_redundancies=True).apply(c1)
    c1_cmds = c1.get_commands()
    assert len(c1_cmds) == 0


def test_simplify_initial_symbolic() -> None:
    # Symbolic circuits should be left alone, no error
    c = Circuit(1)
    c.qubit_create(Qubit(0))
    c.Rx(Symbol("a"), 0)
    c.measure_all()
    c1 = c.copy()
    SimplifyInitial(allow_classical=False).apply(c)
    assert c1 == c


def test_ZZPhaseToRz() -> None:
    c = (
        Circuit(2)
        .ZZPhase(0.6, 0, 1)
        .ZZPhase(1, 0, 1)
        .ZZPhase(-1, 0, 1)
        .ZZPhase(-0.4, 0, 1)
    )
    comp = (
        Circuit(2)
        .ZZPhase(0.6, 0, 1)
        .Rz(1, 0)
        .Rz(1, 1)
        .Rz(1, 0)
        .Rz(1, 1)
        .ZZPhase(-0.4, 0, 1)
    )
    ZZPhaseToRz().apply(c)
    assert comp == c


def test_pauli_squash() -> None:
    c = Circuit(3)
    c.add_pauliexpbox(PauliExpBox([Pauli.Z, Pauli.X, Pauli.Z], 0.8), [0, 1, 2])
    c.add_pauliexpbox(PauliExpBox([Pauli.Y, Pauli.X, Pauli.X], 0.2), [0, 1, 2])
    for strat in [  # noqa: B007
        PauliSynthStrat.Individual,
        PauliSynthStrat.Pairwise,
        PauliSynthStrat.Sets,
    ]:
        for cx_config in [CXConfigType.Snake, CXConfigType.Star, CXConfigType.Tree]:  # noqa: B007
            c1 = c.copy()
            assert PauliSquash().apply(c1)
            assert c1.n_gates_of_type(OpType.CX) <= 4


def test_three_qubit_squash() -> None:
    c = Circuit(3)
    for i in range(21):
        c.H(i % 3)
        c.CX(i % 3, (i + 1) % 3)
    c.measure_all()
    assert ThreeQubitSquash().apply(c)
    assert c.n_gates_of_type(OpType.CX) <= 18


def test_cnx_pairwise_decomp() -> None:
    c = Circuit(6).add_gate(OpType.CnX, [], [0, 1, 2, 3, 4, 5])
    c.add_gate(OpType.CnX, [], [1, 2, 3, 4, 5, 0])
    c.add_gate(OpType.CnX, [], [3, 1, 4, 5, 0, 2])
    CnXPairwiseDecomposition().apply(c)
    DecomposeMultiQubitsCX().apply(c)
    assert c.n_gates_of_type(OpType.CX) < 217


def test_remove_implicit_qubit_permutation() -> None:
    c = Circuit(3).X(0).SWAP(0, 1).SWAP(1, 2)
    c.replace_SWAPs()
    assert c.n_gates_of_type(OpType.SWAP) == 0
    assert c.implicit_qubit_permutation() == {
        Qubit(0): Qubit(2),
        Qubit(1): Qubit(0),
        Qubit(2): Qubit(1),
    }
    assert c.has_implicit_wireswaps
    assert RemoveImplicitQubitPermutation().apply(c)
    assert c.n_gates_of_type(OpType.SWAP) == 2
    assert c.implicit_qubit_permutation() == {
        Qubit(0): Qubit(0),
        Qubit(1): Qubit(1),
        Qubit(2): Qubit(2),
    }
    assert not c.has_implicit_wireswaps


def test_rz_phasedX_squash() -> None:
    c = Circuit(2)
    c.Rz(0.3, 0)
    c.Rz(0.7, 1)
    c.ZZMax(0, 1)
    c.ZZMax(1, 0)
    c.add_gate(OpType.PhasedX, [0.2, 1.3], [0])
    c.add_gate(OpType.PhasedX, [0.5, 1.7], [1])
    c.ZZMax(0, 1)
    c.ZZMax(1, 0)

    assert SquashRzPhasedX().apply(c)
    assert c.n_gates_of_type(OpType.Rz) == 2
    cmds = c.get_commands()
    assert cmds[-1].op.type == OpType.Rz
    assert cmds[-2].op.type == OpType.Rz


def test_conditional_phase() -> None:
    # A conditional H cannot be expressed in terms of TK1 because of the global phase.
    c = Circuit(2, 2)
    c.H(0)
    c.Measure(0, 0)
    c.H(1, condition_bits=[0], condition_value=1)
    c.Measure(1, 1)
    target_gateset = {OpType.TK1, OpType.CX}
    rebase = AutoRebase(target_gateset)
    rebase.apply(c)
    cond_cmds = [cmd for cmd in c.get_commands() if cmd.op.type == OpType.Conditional]
    assert len(cond_cmds) > 0
    any_check_list = []
    for cond_cmd in cond_cmds:
        op = cond_cmd.op
        assert isinstance(op, Conditional)
        any_check_list.append(op.op.type not in target_gateset)
    assert any(any_check_list)


def test_rz_sx_decomp() -> None:
    c = Circuit(1).TK1(0, 1.5, 0, 0)
    AutoRebase({OpType.CX, OpType.SX, OpType.Rz}).apply(c)
    comp = Circuit(1).Rz(1, 0).SX(0).Rz(1, 0)
    comp.add_phase(1.75)
    assert c == comp


def test_iswapmax_autorebase() -> None:
    c = Circuit(2).H(0).CX(0, 1)
    assert AutoRebase({OpType.ISWAPMax, OpType.TK1}).apply(c)
    assert c.n_gates_of_type(OpType.ISWAPMax) <= 2


def test_CZ_PhasedX_autorebase() -> None:
    c = Circuit(2).H(0).CX(0, 1)
    assert AutoRebase({OpType.CZ, OpType.PhasedX}).apply(c)
    assert all(cmd.op.type in (OpType.CZ, OpType.PhasedX) for cmd in c)


def test_flatten_relabel_pass() -> None:
    c = Circuit(3)
    c.H(1).H(2)
    rename_map: RenameUnitsMap = {}
    rename_map[Qubit(0)] = Qubit("a", 4)
    rename_map[Qubit(1)] = Qubit("b", 7)
    rename_map[Qubit(2)] = Qubit("a", 2)
    c.rename_units(rename_map)

    cu = CompilationUnit(c)
    FlattenRelabelRegistersPass("a").apply(cu)

    assert cu.initial_map == cu.final_map
    assert cu.initial_map[Qubit("a", 2)] == Qubit("a", 0)
    assert cu.initial_map[Qubit("a", 4)] == Qubit("a", 4)
    assert cu.initial_map[Qubit("b", 7)] == Qubit("a", 1)
    assert cu.circuit.qubits == [Qubit("a", 0), Qubit("a", 1)]

    # test default argument
    c = Circuit()
    c.add_q_register("p", 4)
    FlattenRelabelRegistersPass().apply(c)
    assert all(q.reg_name == "q" for q in c.qubits)


def test_remove_blank_wires_pass() -> None:
    c = Circuit(2, 4)
    c.Measure(0, 1)

    cu = CompilationUnit(c)
    FlattenRelabelRegistersPass("a").apply(cu)
    assert cu.circuit.qubits == [Qubit("a", 0)]
    assert cu.circuit.bits == c.bits

    c = Circuit()
    c.add_bit(Bit(1))
    c1 = c.copy()
    c1.remove_blank_wires()
    assert len(c1.bits) == 1  # the empty wire is not in a register
    c1 = c.copy()
    c1.remove_blank_wires(remove_classical_only_at_end_of_register=False)
    assert len(c1.bits) == 0

    c = Circuit(1, 2).H(0).Measure(0, 1)
    c1 = c.copy()
    c1.remove_blank_wires()
    assert len(c1.bits) == 2  # the empty wire is not at the end of a register
    c1 = c.copy()
    c1.remove_blank_wires(remove_classical_only_at_end_of_register=False)
    assert len(c1.bits) == 1


def test_round_angles_pass() -> None:
    c0 = Circuit(2).H(0).TK2(0.001, -0.001, 0.001, 0, 1).Rz(0.50001, 0)
    c1 = Circuit(2).H(0).Rz(0.50001, 0)
    assert RoundAngles(8, only_zeros=True).apply(c0)
    assert c0 == c1


def test_PeepholeOptimise2Q() -> None:
    c = Circuit(2).CX(0, 1).CX(1, 0)
    assert PeepholeOptimise2Q().apply(c)
    assert c.has_implicit_wireswaps
    c = Circuit(2).CX(0, 1).CX(1, 0)
    assert not PeepholeOptimise2Q(allow_swaps=False).apply(c)
    assert not c.has_implicit_wireswaps


def test_rebase_custom_tk2() -> None:
    def _tk1_to_phase(a: ParamType, b: ParamType, c: ParamType) -> Circuit:
        return Circuit(1).Rz(c, 0).Rx(b, 0).Rz(a, 0)

    def _tk2_to_phase(a: ParamType, b: ParamType, c: ParamType) -> Circuit:
        return Circuit(2).ZZPhase(c, 0, 1).YYPhase(b, 0, 1).XXPhase(a, 0, 1)

    to_phase_gates = RebaseCustom(
        {OpType.Rx, OpType.Rz, OpType.XXPhase, OpType.YYPhase, OpType.ZZPhase},
        tk2_replacement=_tk2_to_phase,
        tk1_replacement=_tk1_to_phase,
    )

    tk2_c = Circuit(2).TK2(0.123, 0.5634, 0.2345, 0, 1)
    assert to_phase_gates.apply(tk2_c)
    coms = tk2_c.get_commands()
    assert len(coms) == 11
    assert coms[0].op.type == OpType.Rz
    assert coms[1].op.type == OpType.Rz
    assert coms[2].op.type == OpType.Rx
    assert coms[3].op.type == OpType.Rx
    assert coms[4].op.type == OpType.ZZPhase
    assert coms[5].op.type == OpType.YYPhase
    assert coms[6].op.type == OpType.XXPhase
    assert coms[7].op.type == OpType.Rx
    assert coms[8].op.type == OpType.Rx
    assert coms[9].op.type == OpType.Rz
    assert coms[10].op.type == OpType.Rz


def test_repeat_pass_strict_check() -> None:
    # https://github.com/quantinuum/tket/issues/985
    c0 = Circuit(1).PhasedX(angle0=0.3, angle1=0.2, qubit=0)
    squash_pass = SquashRzPhasedX()
    c1 = c0.copy()
    assert squash_pass.apply(c1)
    assert c1 == c0
    c2 = c0.copy()
    assert not RepeatPass(squash_pass, strict_check=True).apply(c2)
    assert c2 == c0


def test_selectively_decompose_boxes() -> None:
    circ = Circuit(1)
    ubox = Unitary1qBox(np.array([[1, 0], [0, -1]]))
    ucirc = Circuit(1).add_unitary1qbox(ubox, 0)
    cbox1 = CircBox(ucirc)
    circ.add_circbox(cbox1, [0])
    circ.add_unitary1qbox(ubox, 0)
    cbox2 = CircBox(Circuit(1).X(0))
    circ.add_circbox(cbox2, [0], opgroup="group1")
    assert DecomposeBoxes({OpType.Unitary1qBox}, {"group1"}).apply(circ)
    cmds = circ.get_commands()
    assert len(cmds) == 3
    assert cmds[0].op.type == OpType.Unitary1qBox
    assert cmds[1].op.type == OpType.Unitary1qBox
    assert cmds[2].op.type == OpType.CircBox


def test_clifford_resynthesis() -> None:
    circ = (
        Circuit(5)
        .H(0)
        .X(1)
        .T(2)
        .Y(3)
        .Z(4)
        .SX(1)
        .CX(2, 3)
        .T(2)
        .S(3)
        .V(2)
        .Vdg(3)
        .CY(1, 3)
        .CZ(3, 4)
        .SWAP(2, 3)
        .Sdg(4)
        .ZZMax(0, 3)
        .SXdg(0)
        .measure_all()
    )
    circ0 = circ.copy()
    CliffordResynthesis().apply(circ0)
    assert circ0.depth() <= 9
    assert circ0.n_2qb_gates() <= 4
    circ1 = circ.copy()
    CliffordResynthesis(allow_swaps=False).apply(circ1)
    assert circ1.depth() <= 16
    assert circ1.n_2qb_gates() <= 9
    circ2 = circ.copy()
    CliffordResynthesis(transform=lambda c: c.copy()).apply(circ2)
    assert circ2 == circ


def test_clifford_resynthesis_implicit_swaps() -> None:
    def T(c: Circuit) -> Circuit:
        c1 = c.copy()
        CliffordSimp().apply(c1)
        return c1

    circ = Circuit(2).CX(0, 1).CX(1, 0).CX(0, 1)
    CliffordResynthesis(transform=T).apply(circ)
    assert circ.n_gates == 0
    assert circ.implicit_qubit_permutation() == {Qubit(0): Qubit(1), Qubit(1): Qubit(0)}


def test_clifford_push_through_measures() -> None:
    c_x: Circuit = Circuit(2, 2).X(0).measure_all()
    CliffordPushThroughMeasures().apply(c_x)
    assert c_x == Circuit(2, 2).X(0).measure_all()
    c_cx_x: Circuit = Circuit(2, 2).X(0).CX(0, 1).X(0).measure_all()
    CliffordPushThroughMeasures().apply(c_cx_x)
    assert c_cx_x.n_1qb_gates() == 0
    assert c_cx_x.n_2qb_gates() == 0
    coms = c_cx_x.get_commands()
    assert len(coms) == 8
    assert coms[2].op.type == OpType.SetBits
    assert coms[3].op.type == OpType.ExplicitModifier
    assert coms[4].op.type == OpType.ExplicitModifier
    assert coms[5].op.type == OpType.ExplicitModifier
    assert coms[6].op.type == OpType.ExplicitModifier
    assert coms[7].op.type == OpType.CopyBits


def test_greedy_pauli_synth() -> None:  # noqa: PLR0915
    circ = Circuit(name="test")
    rega = circ.add_q_register("a", 2)
    regb = circ.add_q_register("b", 2)
    circ.Rz(0, rega[0]).H(regb[1]).CX(rega[0], rega[1]).Ry(0.3, rega[0]).S(regb[1]).CZ(
        rega[0], regb[0]
    ).SWAP(regb[1], rega[0])
    d = circ.copy()
    assert GreedyPauliSimp(0.5, 0.5, thread_timeout=10, trials=5).apply(d)

    assert np.allclose(circ.get_unitary(), d.get_unitary())
    assert d.name == "test"
    # test gateset
    range_predicate = RangePredicateOp(3, 0, 6)
    set_bits = SetBitsOp([True, True])
    multi_bit = MultiBitOp(set_bits, 2)
    eq_pred_values = [True, False, False, True]
    and_values = [bool(i) for i in [0, 0, 0, 1]]
    pg1 = PauliExpBox([Pauli.X, Pauli.Z], 0.3)
    circ = Circuit(4, 4, name="test")
    circ.add_pauliexpbox(pg1, [0, 1])
    circ.add_gate(multi_bit, [0, 1, 2, 3])
    circ.add_gate(range_predicate, [0, 1, 2, 3])
    circ.add_c_predicate(eq_pred_values, [0, 1], 2, "EQ")
    circ.add_c_modifier(and_values, [1], 2)
    circ._add_wasm("funcname", "wasmfileuid", [1, 1], [], [Bit(0), Bit(1)], [0])  # noqa: SLF001
    circ.measure_all()
    circ.Reset(0)
    circ.add_pauliexpbox(pg1, [2, 3])
    assert GreedyPauliSimp(0.5, 0.5, 100, 100, 0, True, 100).apply(circ)
    # PauliExpBoxes implemented using ZZPhase
    d = Circuit(4, 4, name="test")
    d.H(0)
    d.ZZPhase(0.3, 0, 1)
    d.H(0)
    d.add_gate(multi_bit, [0, 1, 2, 3])
    d.add_gate(range_predicate, [0, 1, 2, 3])
    d.add_c_predicate(eq_pred_values, [0, 1], 2, "EQ")
    d.add_c_modifier(and_values, [1], 2)
    d._add_wasm("funcname", "wasmfileuid", [1, 1], [], [Bit(0), Bit(1)], [0])  # noqa: SLF001
    d.measure_all()
    d.Reset(0)
    d.H(2)
    d.ZZPhase(0.3, 2, 3)
    d.H(2)
    assert circ == d
    # test barrier
    circ = Circuit(1).add_barrier([0])
    with pytest.raises(RuntimeError) as e:
        GreedyPauliSimp().apply(circ)
    err_msg = "Predicate requirements are not satisfied"
    assert err_msg in str(e.value)
    # large circuit that doesn't complete within thread_timeout argument
    c = Circuit(13)
    for _ in range(20):
        for i in range(13):
            for j in range(i + 1, 13):
                c.CX(i, j)
                c.Rz(0.23, j)
            c.H(i)
    assert not GreedyPauliSimp(thread_timeout=1).apply(c)
    assert GreedyPauliSimp().apply(c)


def test_get_pre_conditions() -> None:
    pre_cons = GreedyPauliSimp().get_preconditions()
    gate_set = pre_cons[0].gate_set  # type: ignore
    assert OpType.CX in gate_set
    assert OpType.Measure in gate_set


def test_get_post_conditions() -> None:
    gate_set = {
        OpType.CX,
        OpType.Rz,
        OpType.H,
        OpType.Reset,
        OpType.Measure,
        OpType.Phase,
    }
    post_cons = AutoRebase(gate_set).get_postconditions()
    assert post_cons[0].gate_set == gate_set  # type: ignore


def test_get_gate_set() -> None:
    gate_set = GreedyPauliSimp().get_gate_set()
    assert gate_set is not None
    assert OpType.CX in gate_set
    assert OpType.Measure in gate_set
    assert CliffordPushThroughMeasures().get_gate_set() is None
    gate_set1 = ThreeQubitSquash().get_gate_set()
    assert gate_set1 is not None
    assert OpType.Collapse in gate_set1
    gate_set2 = ZXGraphlikeOptimisation().get_gate_set()
    assert gate_set2 is not None
    assert OpType.Output in gate_set2


def test_decompose_inside_conditionals() -> None:
    # https://github.com/quantinuum/tket/issues/1583
    cbox1 = CircBox(Circuit(1).H(0))
    cbox0 = CircBox(Circuit(1, 1).add_circbox(cbox1, [Qubit(0)], condition=Bit(0)))
    c = Circuit(1, 2).add_circbox(cbox0, [Qubit(0), Bit(0)], condition=Bit(1))
    DecomposeBoxes().apply(c)
    cmds = c.get_commands()
    assert len(cmds) == 1
    cmd = cmds[0]
    op0 = cmd.op
    assert isinstance(op0, Conditional)
    op1 = op0.op
    assert isinstance(op1, Conditional)
    op2 = op1.op
    assert op2.type == OpType.H


def test_has_implicit_wireswaps() -> None:
    c = Circuit(2)
    c.SWAP(0, 1)
    assert not c.has_implicit_wireswaps
    c.replace_SWAPs()
    assert c.has_implicit_wireswaps

    # Property should be read-only
    with pytest.raises(AttributeError):  # type: ignore[unreachable]
        c.has_implicit_wireswaps = True


def test_zx_optimisation_wireswaps() -> None:
    c = Circuit(3)
    c.CX(0, 1)
    c.CX(1, 2)
    c.CX(2, 0)

    ZXGraphlikeOptimisation(True).apply(c)
    assert c.has_implicit_wireswaps

    ZXGraphlikeOptimisation(False).apply(c)
    assert not c.has_implicit_wireswaps


def test_initial_and_final_map_types() -> None:
    c = Circuit(1).H(0)
    cu = CompilationUnit(c)
    CliffordSimp().apply(cu)
    im = cu.initial_map
    fm = cu.final_map
    im0, im1 = next(iter(im.items()))
    fm0, fm1 = next(iter(fm.items()))
    assert isinstance(im0, Qubit)
    assert isinstance(im1, Qubit)
    assert isinstance(fm0, Qubit)
    assert isinstance(fm1, Qubit)


def test_pickling() -> None:
    for pred in [
        GateSetPredicate({OpType.CX, OpType.TK1}),
        NoClassicalControlPredicate(),
        NoFastFeedforwardPredicate(),
        NoClassicalBitsPredicate(),
        NoWireSwapsPredicate(),
        MaxTwoQubitGatesPredicate(),
        ConnectivityPredicate(Architecture([(0, 1), (1, 2), (2, 3), (3, 0)])),
        DirectednessPredicate(Architecture([(0, 1)])),
        CliffordCircuitPredicate(),
        MaxNQubitsPredicate(6),
        MaxNClRegPredicate(50),
        PlacementPredicate({Node(1), Node(3)}),
        NoBarriersPredicate(),
        CommutableMeasuresPredicate(),
        NoMidMeasurePredicate(),
        NoSymbolsPredicate(),
        NormalisedTK2Predicate(),
    ]:
        s = pickle.dumps(pred)
        pred1 = pickle.loads(s)
        assert pred.to_dict() == pred1.to_dict()


def test_rx_from_sx() -> None:
    c = Circuit(2).H(0).CX(0, 1)
    assert not RxFromSX().apply(c)
    c.SX(0).H(0).SX(0).SXdg(1).H(1)
    assert RxFromSX().apply(c)
    assert c == Circuit(2).H(0).CX(0, 1).Rx(0.5, 0).H(0).Rx(0.5, 0).Rx(-0.5, 1).H(
        1
    ).add_phase(0.25)
