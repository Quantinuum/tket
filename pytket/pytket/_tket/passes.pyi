from typing import Any
from collections.abc import Callable, Mapping, Sequence, Set
import enum
from typing import Union, overload

import sympy.core.expr

import pytket._tket.architecture
import pytket._tket.circuit
import pytket._tket.mapping
import pytket._tket.placement
import pytket._tket.predicates
import pytket._tket.transform
import pytket._tket.unit_id


class SafetyMode(enum.Enum):
    Audit = 0
    """
    Checks which predicates a circuit satisfies after the application of each base pass
    """

    Default = 1
    """
    Only check that a circuit satisfies the preconditions of the overall pass at the start and the postconditions at the end
    """

Audit: SafetyMode = SafetyMode.Audit

Default: SafetyMode = SafetyMode.Default

class CNotSynthType(enum.Enum):
    SWAP = 0
    """swap-based algorithm for CNOT synthesis"""

    HamPath = 1
    """
    Hamilton-path-based method for CNOT synthesis; this method will fail if there is no Hamilton path in the given architecture
    """

    Rec = 2
    """recursive Steiner--Gauss method for CNOT synthesis"""

SWAP: CNotSynthType = CNotSynthType.SWAP

HamPath: CNotSynthType = CNotSynthType.HamPath

Rec: CNotSynthType = CNotSynthType.Rec

class BasePass:
    """Base class for passes."""

    @overload
    def apply(self, compilation_unit: pytket._tket.predicates.CompilationUnit, safety_mode: SafetyMode = SafetyMode.Default) -> bool:
        """
        Apply to a :py:class:`~.CompilationUnit`.

        :return: True if the pass modified the circuit. Note that in some cases the method may return True even when the circuit is unmodified (but a return value of False definitely implies no modification).
        """

    @overload
    def apply(self, circuit: pytket._tket.circuit.Circuit) -> bool:
        """
        Apply to a :py:class:`~.Circuit` in-place.

        :return: True if pass modified the circuit, else False
        """

    @overload
    def apply(self, circuit: pytket._tket.circuit.Circuit, before_apply: Callable[[pytket._tket.predicates.CompilationUnit, Any], None], after_apply: Callable[[pytket._tket.predicates.CompilationUnit, Any], None]) -> bool:
        """
        Apply to a :py:class:`~.Circuit` in-place and invoke callbacks for all nested passes.


        :param before_apply: Invoked before a pass is applied. The CompilationUnit and a summary of the pass configuration are passed into the callback.
        :param after_apply: Invoked after a pass is applied. The CompilationUnit and a summary of the pass configuration are passed into the callback.
        :return: True if pass modified the circuit, else False
        """

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

    def to_dict(self) -> dict:
        """:return: A JSON serializable dictionary representation of the Pass."""

    def get_preconditions(self) -> list[pytket._tket.predicates.Predicate]:
        """
        Returns the precondition Predicates for the given pass.
        :return: A list of Predicate
        """

    def get_postconditions(self) -> list[pytket._tket.predicates.Predicate]:
        """
        Returns the postcondition Predicates for the given pass.

        :return: A list of :py:class:`~.Predicate`
        """

    def get_gate_set(self) -> set[pytket._tket.circuit.OpType] | None:
        """:return: A set of allowed OpType"""

    @staticmethod
    def from_dict(base_pass_dict: dict, custom_deserialisation: Mapping[str, Callable[[pytket._tket.circuit.Circuit], pytket._tket.circuit.Circuit]] = {}, custom_map_deserialisation: Mapping[str, Callable[[pytket._tket.circuit.Circuit], tuple[pytket._tket.circuit.Circuit, tuple[Mapping[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID], Mapping[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID]]]]] = {}) -> BasePass:
        """
        Construct a new Pass instance from a JSON serializable dictionary representation. `custom_deserialisation` is a map between `CustomPass` label attributes and a Circuit to Circuit function matching the `CustomPass` `transform` argument. This allows the construction of some `CustomPass` from JSON. `CustomPass` without a matching entry in `custom_deserialisation` will be rejected.
        """

    def __getstate__(self) -> tuple: ...

    def __setstate__(self, arg: tuple, /) -> None: ...

class SequencePass(BasePass):
    """A sequence of compilation passes."""

    def __init__(self, pass_list: Sequence[BasePass], strict: bool = True) -> None:
        """
        Construct from a list of compilation passes arranged in order of application.

        :param pass_list: sequence of passes
        :param strict: if True (the default), check that all postconditions and preconditions of the passes in the sequence are compatible and raise an exception if not.
        :return: a pass that applies the sequence
        """

    def __str__(self) -> str: ...

    def to_dict(self) -> dict:
        """
        :return: A JSON serializable dictionary representation of the SequencePass.
        """

    def get_sequence(self) -> list[BasePass]:
        """:return: The underlying sequence of passes."""

class RepeatPass(BasePass):
    """
    Repeat a pass until its `apply()` method returns False, or if `strict_check` is True until it stops modifying the circuit.
    """

    def __init__(self, compilation_pass: BasePass, strict_check: bool = False) -> None:
        """Construct from a compilation pass."""

    def __str__(self) -> str: ...

    def get_pass(self) -> BasePass:
        """:return: The underlying compilation pass."""

class RepeatWithMetricPass(BasePass):
    """Repeat a compilation pass until the given metric stops decreasing."""

    def __init__(self, compilation_pass: BasePass, metric: Callable[[pytket._tket.circuit.Circuit], int]) -> None:
        """Construct from a compilation pass and a metric function."""

    def __str__(self) -> str: ...

    def get_pass(self) -> BasePass:
        """:return: The underlying compilation pass."""

    def get_metric(self) -> Callable[[pytket._tket.circuit.Circuit], int]:
        """:return: The underlying metric."""

class RepeatUntilSatisfiedPass(BasePass):
    """
    Repeat a compilation pass until a predicate on the circuit is satisfied.
    """

    @overload
    def __init__(self, compilation_pass: BasePass, predicate: pytket._tket.predicates.Predicate) -> None:
        """Construct from a compilation pass and a predicate."""

    @overload
    def __init__(self, compilation_pass: BasePass, check_function: Callable[[pytket._tket.circuit.Circuit], bool]) -> None:
        """
        Construct from a compilation pass and a user-defined function from :py:class:`~.Circuit` to `bool`.
        """

    def __str__(self) -> str: ...

    def get_pass(self) -> BasePass:
        """:return: The underlying compilation pass."""

    def get_predicate(self) -> pytket._tket.predicates.Predicate:
        """:return: The underlying predicate."""

@overload
def KAKDecomposition(target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX, cx_fidelity: float = 1.0, allow_swaps: bool = True) -> BasePass:
    """
    Squash sequences of two-qubit operations into minimal form.

    Pass to squash together sequences of single- and two-qubit gates into minimal form. Can decompose to TK2 or CX gates.

    Two-qubit operations can always be expressed in a minimal form of maximum three CXs, or as a single TK2 gate (a result also known as the KAK or Cartan decomposition).

    It is in general recommended to squash to TK2 gates, and to then use the `DecomposeTK2` pass for noise-aware decompositions to other gatesets. For backward compatibility, decompositions to CX are also supported. In this case, `cx_fidelity` can be provided to perform approximate decompositions to CX gates.

    When decomposing to TK2 gates, any sequence of two or more two-qubit gates on the same set of qubits are replaced by a single TK2 gate. When decomposing to CX, the substitution is only performed if it results in a reduction of the number of CX gates, or if at least one of the two-qubit gates is not a CX.

    Using the `allow_swaps=True` (default) option, qubits will be swapped when convenient to further reduce the two-qubit gate count (only applicable when decomposing to CX gates).

    Note that gates containing symbolic parameters are not squashed.

    :param target_2qb_gate: OpType to decompose to. Either TK2 or CX.
    :param cx_fidelity: Estimated CX gate fidelity, used when target_2qb_gate=CX.
    :param allow_swaps: Whether to allow implicit wire swaps.
    """

@overload
def KAKDecomposition(cx_fidelity: float) -> BasePass: ...

def DecomposeTK2(allow_swaps: bool = True, **kwargs: Any) -> BasePass:
    """
    Decompose each TK2 gate into two-qubit gates.

    Gate fidelities can be passed as keyword arguments to perform noise-aware decompositions. If the fidelities of several gate types are provided, the best will be chosen.

    We currently support `CX_fidelity`, `ZZMax_fidelity` and `ZZPhase_fidelity`. If provided, the `CX` and `ZZMax` fidelities must be given by a single floating point fidelity. The `ZZPhase` fidelity is given as a lambda float -> float, mapping a ZZPhase angle parameter to its fidelity, or by a single float. These parameters will be used to return the optimal decomposition of each TK2 gate, taking noise into consideration.

    If no fidelities are provided, the TK2 gates will be decomposed exactly using CX gates. For equal fidelities, ZZPhase will be preferred over ZZMax and CX if the decomposition results in fewer two-qubit gates.

    All TK2 gate parameters must be normalised, i.e. they must satisfy `NormalisedTK2Predicate`. (This can be achieved by applying the :py:meth:`NormaliseTK2` pass beforehand.)

    Using the `allow_swaps=True` (default) option, qubits will be swapped when convenient to reduce the two-qubit gate count of the decomposed TK2.

    If the TK2 angles are symbolic values, the decomposition will be exact (i.e. not noise-aware). It is not possible in general to obtain optimal decompositions for arbitrary symbolic parameters, so consider substituting for concrete values if possible.

    :param allow_swaps: Whether to allow implicit wire swaps.
    """

def NormaliseTK2() -> BasePass:
    """
    Normalises all TK2 gates.

    TK2 gates have three angles in the interval [0, 4], but these can always be normalised to be within the so-called Weyl chamber by adding single-qubit gates.

    More precisely, the three angles a, b, c of TK2(a, b, c) are normalised exactly when the two following conditions are met:
     - numerical values must be in the Weyl chamber, ie `1/2 >= a >= b >= |c|`,
     - symbolic values must come before any numerical value in the array.

    After this pass, all TK2 angles will be normalised and the circuit will satisfy `NormalisedTK2Predicate`.
    """

def ThreeQubitSquash(allow_swaps: bool = True) -> BasePass:
    """
    Squash three-qubit subcircuits into subcircuits having fewer CX gates, when possible, and apply Clifford simplification.

    The circuit to which this is applied must consist of single-qubit, pure-classical and CX gates, and Measure, Collapse, Reset, Phase and conditional gates.

    :param allow_swaps: whether to allow implicit wire swaps
    """

def CommuteThroughMultis() -> BasePass:
    """
    Moves single-qubit operations past multi-qubit operations that they commute with, towards the front of the circuit.
    """

def DecomposeArbitrarilyControlledGates() -> BasePass:
    """
    Decomposes CCX, CnX, CnY, CnZ, CnRy, CnRz and CnRx gates into CX and single-qubit gates.
    """

def DecomposeBoxes(excluded_types: Set[pytket._tket.circuit.OpType] = ..., excluded_opgroups: Set[str] = ..., included_types: Set[pytket._tket.circuit.OpType] | None = None, included_opgroups: Set[str] | None = None) -> BasePass:
    """
    Recursively replaces all boxes by their decomposition into circuits. 

    Arguments specify ways to filter which boxes are decomposed. A box must satisfy ALL filters in order to be decomposed (i.e. be in the inclusive sets and not in the exclusive sets).

    :param excluded_types: box :py:class:`~.OpType` s excluded from decomposition
    :param excluded_opgroups: opgroups excluded from decomposition
    :param included_types: optional, only decompose these box :py:class:`~.OpType` s
    :param included_opgroups: optional, only decompose these opgroups
    """

def DecomposeClassicalExp() -> BasePass:
    """Replaces each `ClExprOp` by a sequence of classical gates."""

def DecomposeMultiQubitsCX() -> BasePass:
    """Converts all multi-qubit gates into CX and single-qubit gates."""

def DecomposeSingleQubitsTK1() -> BasePass:
    """Converts all single-qubit gates into TK1 gates."""

def PeepholeOptimise2Q(allow_swaps: bool = True) -> BasePass:
    """
    Performs peephole optimisation including resynthesis of 2-qubit gate sequences, and converts to a circuit containing only CX and TK1 gates.

    :param allow_swaps: whether to allow implicit wire swaps
    """

def FullPeepholeOptimise(allow_swaps: bool = True, target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX) -> BasePass:
    """
    Performs peephole optimisation including resynthesis of 2- and 3-qubit gate sequences, and converts to a circuit containing only the given 2-qubit gate (which may be CX or TK2) and TK1 gates.

    :param allow_swaps: whether to allow implicit wire swaps
    """

def RebaseTket() -> BasePass:
    """
    Converts all gates to CX, TK1 and Phase. (Any Measure and Reset operations are left untouched; Conditional gates are also allowed.)
    """

def RxFromSX() -> BasePass:
    """
    Replaces all SX in the circuit with Rx(1/2) and all SXdg with Rx(-1/2).
    """

def RemoveRedundancies() -> BasePass:
    """
    Removes gate-inverse pairs, merges rotations, removes identity rotations, and removes redundant gates before measurement. Does not add any new gate types.

    When merging rotations with the same op group name, the merged operation keeps the same name.
    """

def SynthesiseTK() -> BasePass:
    """Optimises and converts all gates to TK2, TK1 and Phase gates."""

def SynthesiseTket() -> BasePass:
    """Optimises and converts all gates to CX, TK1 and Phase gates."""

def SquashTK1() -> BasePass:
    """Squash sequences of single-qubit gates to TK1 gates."""

def SquashRzPhasedX() -> BasePass:
    """Squash single qubit gates into PhasedX and Rz gates."""

def FlattenRegisters() -> BasePass:
    """
    Merges all quantum and classical registers into their respective default registers with contiguous indexing.
    """

def SquashCustom(singleqs: Set[pytket._tket.circuit.OpType], tk1_replacement: Callable[[Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float]], pytket._tket.circuit.Circuit], always_squash_symbols: bool = False) -> BasePass:
    """
    Squash sequences of single qubit gates from the target gate set into an optimal form given by `tk1_replacement`.

    :param singleqs: The types of single qubit gates in the target gate set. This pass will only affect sequences of gates that are already in this set.
    :param tk1_replacement: A function which, given the parameters of an Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the desired basis.
    :param always_squash_symbols: If true, always squash symbolic gates regardless of the blow-up in complexity. Default is false, meaning that symbolic gates are only squashed if doing so reduces the overall symbolic complexity.
    """

def AutoSquash(singleqs: Set[pytket._tket.circuit.OpType]) -> BasePass:
    """
    Attempt to generate a squash pass automatically for the given target single qubit gateset.
    Raises an error if no known TK1 decomposition can be found based on the given gateset, in which case try using :py:meth:`~.SquashCustom` with your own decomposition.

    :param singleqs: The types of single qubit gates in the target gate set. This pass will only affect sequences of gates that are already in this set.
    """

def DelayMeasures(allow_partial: bool = True) -> BasePass:
    """
    Commutes Measure operations to the end of the circuit. Throws an exception when this is not possible because of gates following the measure which are dependent on either the resulting quantum state or classical values.

    :param allow_partial: Whether to allow measurements that cannot be commuted to the end, and delay them as much as possible instead. If false, the pass includes a :py:class:`~.CommutableMeasuresPredicate` precondition.
    """

def RemoveDiscarded() -> BasePass:
    """
    A pass to remove all operations that have no ``OpType.Output`` or ``OpType.ClOutput`` in their causal future (in other words, all operations whose causal future is discarded).
    """

def SimplifyMeasured() -> BasePass:
    """
    A pass to replace all 'classical maps' followed by measure operations whose quantum output is discarded with classical operations following the measure. (A 'classical map' is a quantum operation that acts as a permutation of the computational basis states followed by a diagonal operation.)
    """

def RemoveBarriers() -> BasePass:
    """A pass to remove all barrier instructions from the circuit."""

def RemovePhaseOps() -> BasePass:
    """
    A pass to remove all Phase operations from the circuit. This includes conditional Phase operations, but not Phase operations inside CircBoxes, QControlBoxes or other nested structures.
    """

def ZXGraphlikeOptimisation(allow_swaps: bool = True) -> BasePass:
    """
    Attempt to optimise the circuit by simplifying in ZX calculus and extracting a circuit back out. Due to limitations in extraction, may not work if the circuit contains created or discarded qubits. As a resynthesis pass, this will ignore almost all optimisations achieved beforehand and may increase the cost of the circuit.

    :param allow_swaps: Whether to allow implicit wire swaps (default True).
    """

@overload
def RebaseCustom(gateset: Set[pytket._tket.circuit.OpType], cx_replacement: pytket._tket.circuit.Circuit, tk1_replacement: Callable[[Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float]], pytket._tket.circuit.Circuit]) -> BasePass:
    r"""
    Construct a custom rebase pass, given user-defined rebases for TK1 and CX. This pass:

    1. decomposes multi-qubit gates not in the set of gate types `gateset` to CX gates;
    2. if CX is not in `gateset`, replaces CX gates with `cx_replacement`;
    3. converts any single-qubit gates not in the gate type set to the form :math:`\mathrm{Rz}(a)\mathrm{Rx}(b)\mathrm{Rz}(c)` (in matrix-multiplication order, i.e. reverse order in the circuit);
    4. applies the `tk1_replacement` function to each of these triples :math:`(a,b,c)` to generate replacement circuits.

    :param gateset: the allowed operations in the rebased circuit (in addition, Measure and Reset operations are always allowed and are left alone; conditional operations may be present; and Phase gates may also be introduced by the rebase)
    :param cx_replacement: the equivalent circuit to replace a CX gate using two qubit gates from the desired basis (can use any single qubit OpTypes)
    :param tk1_replacement: a function which, given the parameters of an Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the desired basis
    :return: a pass that rebases to the given gate set (possibly including conditional and phase operations, and Measure and Reset
    """

@overload
def RebaseCustom(gateset: Set[pytket._tket.circuit.OpType], tk2_replacement: Callable[[Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float]], pytket._tket.circuit.Circuit], tk1_replacement: Callable[[Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float]], pytket._tket.circuit.Circuit]) -> BasePass:
    """
    Construct a custom rebase pass, given user-defined rebases for TK1 and TK2. This pass:

    1. decomposes multi-qubit gates not in the set of gate types `gateset` to TK2 gates;
    2. if TK2 is not in `gateset`, replaces TK2(a,b,c) gates via the `tk2_replacement` function;
    3. converts any single-qubit gates not in the gate type set to TK1;
    4. if TK2 is not in `gateset`. applies the `tk1_replacement` function to each TK1(a,b,c).

    :param gateset: the allowed operations in the rebased circuit (in addition, Measure and Reset always allowed and are left alone; conditional operations may be present; and Phase gates may also be introduced by the rebase)
    :param tk2_replacement: a function which, given the parameters (a,b,c) of an XXPhase(a)YYPhase(b)ZZPhase(c) triple, returns an equivalent circuit in the desired basis
    :param tk1_replacement: a function which, given the parameters (a,b,c) of an Rz(a)Rx(b)Rz(c) triple, returns an equivalent circuit in the desired basis
    :return: a pass that rebases to the given gate set (possibly including conditional and phase operations, and Measure and Reset)
    """

def AutoRebase(gateset: Set[pytket._tket.circuit.OpType], allow_swaps: bool = False) -> BasePass:
    """
    Attempt to generate a rebase pass automatically for the given target gateset. Checks if there are known existing decompositions to target gateset and TK1 to target gateset and uses those to construct a custom rebase.
    Raises an error if no known decompositions can be found, in which case try using :py:meth:`~.RebaseCustom` with your own decompositions.

    :param gateset: Set of supported OpTypes, target gate set. (in addition, Measure and Reset operations are always allowed and are left alone; conditional operations may be present; and Phase gates may also be introduced by the rebase)
    :param allow_swaps: Whether to allow implicit wire swaps. Default to False.
    """

def EulerAngleReduction(q: pytket._tket.circuit.OpType, p: pytket._tket.circuit.OpType, strict: bool = False) -> BasePass:
    """
    Uses Euler angle decompositions to squash all chains of P and Q rotations, where P,Q ∈ {Rx,Ry,Rz}. By default (`strict=False`), this pass will try to decompose the chains into pairs of -P-Q- or -Q-P- rotations, commuting any third rotation past multi-qubit gates. If `strict=True`, all chains will be decomposed to P-Q-P triples and no further optimisation is performed.

    :param q: The type of the Q rotation (Q ∈ {Rx,Ry,Rz}).
    :param p: The type of the P rotation (P ∈ {Rx,Ry,Rz}, P ≠ Q).
    :param strict: Optionally performs strict P-Q-P Euler decomposition
    :return: a pass that squashes chains of P and Q rotations
    """

def CustomRoutingPass(arc: pytket._tket.architecture.Architecture, config: Sequence[pytket._tket.mapping.RoutingMethod]) -> BasePass:
    """
    Construct a pass to route to the connectivity graph of an :py:class:`~.Architecture`. Edge direction is ignored. 

    :return: a pass that routes to the given device architecture
    """

def RoutingPass(arc: pytket._tket.architecture.Architecture) -> BasePass:
    """
    Construct a pass to route to the connectivity graph of an :py:class:`~.Architecture`. Edge direction is ignored. Uses :py:class:`~.LexiLabellingMethod` and :py:class:`~.LexiRouteRoutingMethod`.

    :return: a pass that routes to the given device architecture
    """

def PlacementPass(placer: pytket._tket.placement.Placement) -> BasePass:
    """
    :param placer: The Placement used for relabelling.
    :return: a pass to relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes
    """

def NaivePlacementPass(architecture: pytket._tket.architecture.Architecture) -> BasePass:
    """
    :param architecture: The Architecture used for relabelling.
    :return: a pass to relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes
    """

def FlattenRelabelRegistersPass(label: str = 'q') -> BasePass:
    """
    Removes empty Quantum wires from the Circuit and relabels all Qubit to a register from passed name. 

    :param label: Name to relabel remaining Qubit to, default 'q'.
    :return: A pass that removes empty wires and relabels.
    """

def RenameQubitsPass(qubit_map: Mapping[pytket._tket.unit_id.Qubit, pytket._tket.unit_id.Qubit]) -> BasePass:
    """
    Rename some or all qubits. 

    :param qubit_map: map from old to new qubit names
    """

def FullMappingPass(arc: pytket._tket.architecture.Architecture, placer: pytket._tket.placement.Placement, config: Sequence[pytket._tket.mapping.RoutingMethod]) -> BasePass:
    """
    Construct a pass to relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes, and then route to the connectivity graph of an :py:class:`~.Architecture`. Edge direction is ignored.

    :param arc: The architecture to use for connectivity information. 
    :param placer: The Placement used for relabelling.
    :param config: Parameters for routing, a list of RoutingMethod, each method is checked and run if applicable in turn.
    :return: a pass to perform the remapping
    """

def DefaultMappingPass(arc: pytket._tket.architecture.Architecture, delay_measures: bool = True) -> BasePass:
    """
    Construct a pass to relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes, and then route to the connectivity graph of the given :py:class:`~.Architecture`. Edge direction is ignored. Placement used is GraphPlacement.

    :param arc: The Architecture used for connectivity information.
    :param delay_measures: Whether to commute measurements to the end of the circuit, defaulting to true.
    :return: a pass to perform the remapping
    """

def AASRouting(arc: pytket._tket.architecture.Architecture, **kwargs: Any) -> BasePass:
    r"""
    Construct a pass to relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes, and then use architecture-aware synthesis to route the circuit. In the steps of the pass the circuit will be converted to CX, Rz, H gateset. The limited connectivity of the :py:class:`~.Architecture` is used for the routing. The direction of the edges is ignored. The placement used is GraphPlacement. This pass can take a few parameters for the routing, described below:

    - (unsigned) lookahead=1: parameter for the recursive iteration
    - (CNotSynthType) cnotsynthtype=CNotSynthType.Rec: CNOT synthesis type

    NB: The circuit needs to have at most as many qubits as the architecture has nodes. The resulting circuit will always have the same number of qubits as the architecture has nodes, even if the input circuit had fewer.

    :param arc: target architecture
    :param \**kwargs: parameters for routing (described above)
    :return: a pass to perform the remapping
    """

def ComposePhasePolyBoxes(min_size: int = 0) -> BasePass:
    """
    Pass to convert a given :py:class:`~.Circuit` to the CX, Rz, H gateset and compose phase polynomial boxes from the groups of the CX+Rz gates.

    - (unsigned) min_size=0: minimal number of CX gates in each phase polynomial box: groups with a smaller number of CX gates are not affected by this transformation

    :return: a pass to perform the composition
    """

def CXMappingPass(arc: pytket._tket.architecture.Architecture, placer: pytket._tket.placement.Placement, **kwargs: Any) -> BasePass:
    r"""
    Construct a pass to convert all gates to CX, relabel :py:class:`~.Circuit` Qubits to :py:class:`~.Architecture` Nodes, route to the connectivity graph of a :py:class:`~.Architecture` and decompose additional routing gates (SWAP and BRIDGE) to CX gates.

    :param arc: The Architecture used for connectivity information.
    :param placer: The placement used for relabelling.
    :param \**kwargs: Parameters for routing: (bool)directed_cx=false, (bool)delay_measures=true
    :return: a pass to perform the remapping
    """

def CliffordSimp(allow_swaps: bool = True, target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX) -> BasePass:
    """
    An optimisation pass that applies a number of rewrite rules for simplifying Clifford gate sequences, similar to Duncan & Fagan (https://arxiv.org/abs/1901.10114). Produces a circuit comprising TK1 gates and the two-qubit gate specified as the target.

    :param allow_swaps: whether the rewriting may introduce implicit wire swaps
    :param target_2qb_gate: target two-qubit gate (either CX or TK2)
    :return: a pass to perform the rewriting
    """

def CliffordResynthesis(transform: Callable[[pytket._tket.circuit.Circuit], pytket._tket.circuit.Circuit] | None = None, allow_swaps: bool = True) -> BasePass:
    """
    An optimisation pass that resynthesises Clifford subcircuits, trying to reduce the 2-qubit gate count as much as possible.

    :param transform: optional user-provided resynthesis method to apply to all Clifford subcircuits (a function taking a Clifford circuit as an argument and returning an equivalent circuit); if not provided, a default resynthesis method is applied
    :param allow_swaps: whether the rewriting may introduce wire swaps (only relevant to the default resynthesis method used when the `transform` argument is not provided)
    :return: a pass to perform the rewriting
    """

def CliffordPushThroughMeasures() -> BasePass:
    """
    An optimisation pass that resynthesise a Clifford subcircuit before end of circuit Measurement operations by implementing the action of the Clifford as a mutual diagonalisation circuit and a permutation on output measurements realised as a series of classical operations.
    : return: a pass to simplify end of circuit Clifford gates.
    """

def DecomposeSwapsToCXs(arc: pytket._tket.architecture.Architecture, respect_direction: bool = False) -> BasePass:
    """
    Construct a pass to decompose SWAP and BRIDGE gates to CX gates, constraining connectivity to an :py:class:`~.Architecture`, optionally taking the directedness of the connectivity graph into account.

    :param arc: The architecture to use for connectivity information.
    :param respect_direction: Optionally takes the directedness of the connectivity graph into account.
    :return: a pass to perform the decomposition
    """

def DecomposeSwapsToCircuit(replacement_circuit: pytket._tket.circuit.Circuit) -> BasePass:
    """
    :param replacement_circuit: An equivalent circuit to replace a SWAP gate with in the desired basis.
    :return: a pass to replace all SWAP gates with the given circuit
    """

def OptimisePhaseGadgets(cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> BasePass:
    """
    Construct a pass that synthesises phase gadgets and converts to a circuit containing only CX, TK1 and Phase gates.

    :param cx_config: A configuration of CXs to convert phase gadgets into.
    :return: a pass to perform the synthesis
    """

def PauliExponentials(strat: pytket._tket.transform.PauliSynthStrat = pytket._tket.transform.PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> BasePass:
    """
    Construct a pass that converts a circuit into a graph of Pauli exponential boxes, with information

    :param strat: A synthesis strategy for the Pauli graph.
    :param cx_config: A configuration of CXs to convert Pauli gadgets into.
    :return: a pass to perform the simplification
    """

def PauliSimp(strat: pytket._tket.transform.PauliSynthStrat = pytket._tket.transform.PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> BasePass:
    """
    Construct a pass that converts a circuit into a graph of Pauli gadgets to account for commutation and phase folding, and resynthesises them as either individual gadgets, pairwise constructions, or by diagonalising sets of commuting gadgets.

    This pass will not preserve the global phase of the circuit.

    :param strat: A synthesis strategy for the Pauli graph.
    :param cx_config: A configuration of CXs to convert Pauli gadgets into.
    :return: a pass to perform the simplification
    """

def GuidedPauliSimp(strat: pytket._tket.transform.PauliSynthStrat = pytket._tket.transform.PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> BasePass:
    """
    Applies the ``PauliSimp`` optimisation pass to any region of the circuit contained within a :py:class:`~.CircBox`. This can be useful to focus the synthesis to target specific sets of commuting operations, rather than the default greedy approach.

    :param strat: A synthesis strategy for the Pauli graph.
    :param cx_config: A configuration of CXs to convert Pauli gadgets into.
    :return: a pass to perform the simplification
    """

def GreedyPauliSimp(discount_rate: float = 0.7, depth_weight: float = 0.3, max_lookahead: int = 500, max_tqe_candidates: int = 500, seed: int = 0, allow_zzphase: bool = False, thread_timeout: int = 100, only_reduce: bool = False, trials: int = 1) -> BasePass:
    """
    Construct a pass that converts a circuit into a graph of Pauli gadgets to account for commutation and phase folding, and resynthesises them using a greedy algorithm adapted from arxiv.org/abs/2103.08602. The method for synthesising the final Clifford operator is adapted from arxiv.org/abs/2305.10966.

    WARNING: this pass will not preserve the global phase of the circuit.

    :param discount_rate: Rate used to discount the cost impact from gadgets that are further away. Default to 0.7.
    :param depth_weight:  Degree of depth optimisation. Default to 0.3.
    :param max_tqe_candidates:  Maximum number of 2-qubit Clifford gate candidates to evaluate at each step. Default to 500.
    :param max_lookahead:  Maximum lookahead when evaluating each Clifford gate candidate. Default to 500.
    :param seed:  Unsigned integer seed used for sampling candidates and tie breaking. Default to 0.
    :param allow_zzphase: If set to True, allows the algorithm to implement 2-qubit rotations using ZZPhase gates when deemed optimal. Defaults to False.
    :param thread_timeout: Sets maximum out of time spent finding a single solution in one thread.
    :param only_reduce: Only returns modified circuit if it has fewer two-qubit gates.
    :param trials: Sets maximum number of found solutions. The smallest circuit is returned, prioritising the number of 2qb-gates, then the number of gates, then the depth.
    :return: a pass to perform the simplification
    """

def PauliSquash(strat: pytket._tket.transform.PauliSynthStrat = pytket._tket.transform.PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> BasePass:
    """
    Applies :py:meth:`PauliSimp` followed by :py:meth:`FullPeepholeOptimise`.

    :param strat: a synthesis strategy for the Pauli graph
    :param cx_config: a configuration of CXs to convert Pauli gadgets into
    :return: a pass to perform the simplification
    """

def SimplifyInitial(allow_classical: bool = True, create_all_qubits: bool = False, remove_redundancies: bool = True, xcirc: pytket._tket.circuit.Circuit | None = None) -> BasePass:
    """
    Simplify the circuit using knowledge of qubit state.

    :param allow_classical: allow replacement of measurements on known state with classical set-bit operations
    :param create_all_qubits: automatically annotate all qubits as initialized to the zero state
    :param remove_redundancies: apply a :py:meth:`RemoveRedundancies` pass after the initial simplification
    :param xcirc: 1-qubit circuit implementing an X gate in the transformed circuit (if omitted, an X gate is used)
    :return: a pass to perform the simplification
    """

def ContextSimp(allow_classical: bool = True, xcirc: pytket._tket.circuit.Circuit | None = None) -> BasePass:
    """
    Applies simplifications enabled by knowledge of qubit state and discarded qubits.

    :param allow_classical: allow replacement of measurements on known state with classical set-bit operations
    :param xcirc: 1-qubit circuit implementing an X gate in the transformed circuit (if omitted, an X gate is used)
    :return: a pass to perform the simplification
    """

def ZZPhaseToRz() -> BasePass:
    """
    Converts all ZZPhase gates in a circuit with angle 1 or -1 (half-turns) into two Rz gates each with a parameter value of 1 (half-turns). ZZPhase gates with parameter values other than 1 or -1 (half-turns) are left unchanged.

    :return: a pass to convert ZZPhase gates to Rz.
    """

def CnXPairwiseDecomposition() -> BasePass:
    """
    Decompose CnX gates to 2-qubit gates and single qubit gates. For every two CnX gates, reorder their control qubits to improve the chance of gate cancellation
    """

def RoundAngles(n: int, only_zeros: bool = False) -> BasePass:
    r"""
    Round angles to the nearest :math:`\pi / 2^n`. 

    :param n: precision parameter, must be >= 0 and < 32 
    :param only_zeros: if True, only round angles less than :math:`\pi / 2^{n+1}` to zero, leave other angles alone (default False)
    """

def RemoveImplicitQubitPermutation() -> BasePass:
    """
    Remove any implicit qubit permutation by appending SWAP gates.

    Note that if the circuit contains measurements, they may become mid-circuit measurements in the transformed circuit.
    """

def CustomPass(transform: Callable[[pytket._tket.circuit.Circuit], pytket._tket.circuit.Circuit], label: str = '') -> BasePass:
    """
    Generate a custom pass from a user-provided circuit transformation function.

    It is the caller's responsibility to provide a valid transform.

    :param transform: function taking a :py:class:`~.Circuit` as an argument and returning a new transformed circuit
    :param label: optional label for the pass
    :return: a pass to perform the transformation
    """

def CustomPassMap(transform: Callable[[pytket._tket.circuit.Circuit], tuple[pytket._tket.circuit.Circuit, tuple[Mapping[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID], Mapping[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID]]]], label: str = '') -> BasePass:
    """
    Generate a custom pass from a user-provided circuit transformation function.

    It is the caller's responsibility to provide a valid transform.

    :param transform: function taking a :py:class:`~.Circuit` as an argument and returning a pair of a new transformed circuit and a pair of maps corresponding to the initial and final maps that the transformation makes.
    :param label: optional label for the pass
    :return: a pass to perform the transformation
    """
