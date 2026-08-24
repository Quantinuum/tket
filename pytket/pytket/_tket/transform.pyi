from typing import Any
from collections.abc import Callable, Mapping, Sequence, Set
import enum
from typing import overload

import pytket._tket.architecture
import pytket._tket.circuit
import pytket._tket.unit_id


class PauliSynthStrat(enum.Enum):
    """Enum for available strategies to synthesise Pauli gadgets"""

    Individual = 0
    """Synthesise gadgets individually"""

    Pairwise = 1
    """
    Synthesise gadgets using an efficient pairwise strategy from Cowtan et al (https://arxiv.org/abs/1906.01734)
    """

    Sets = 2
    """Synthesise gadgets in commuting sets"""

    Greedy = 3
    """
    Synthesise gadgets using a greedy algorithm adapted from arxiv.org/abs/2103.08602. This strategy is currently only accepted by `TermSequenceBox`. For synthesising general circuits try using `GreedyPauliSimp`.

    WARNING: This strategy will not preserve the global phase of the circuit.
    """

class Transform:
    """An in-place transformation of a :py:class:`~.Circuit`."""

    def __init__(self, arg: Callable[[pytket._tket.circuit.Circuit], bool], /) -> None: ...

    def apply(self, circuit: pytket._tket.circuit.Circuit) -> bool:
        """
        Performs the transformation on the circuit in place.

        :param circuit: The circuit to be transformed
        :return: True if any changes were made, else False
        """

    def __rshift__(self, arg: Transform, /) -> Transform:
        """
        Composes two Transforms together in sequence.

        >>> a >> b

        is equivalent to

        >>> sequence([a,b])
        """

    @staticmethod
    def sequence(sequence: Sequence[Transform]) -> Transform:
        """
        Composes a list of Transforms together in sequence. The :py:meth:`apply` method will return ``True`` if ANY of the individual Transforms returned ``True``.

        :param sequence: The list of Transforms to be composed
        :return: the combined Transform
        """

    @staticmethod
    def repeat(transform: Transform) -> Transform:
        """
        Applies a given Transform repeatedly to a circuit until no further changes are made (i.e. it no longer returns ``True``). :py:meth:`apply` will return ``True`` if at least one run returned ``True``.

        :param transform: The Transform to be applied repeatedly
        :return: a new Transform representing the iteration
        """

    @staticmethod
    def while_repeat(condition: Transform, body: Transform) -> Transform:
        """
        Repeatedly applies the `condition` Transform until it returns ``False``, running `body` in between each `condition` application. Intuitively, this corresponds to "WHILE `condition` DO `body`".

        :param condition: The Transform to be applied repeatedly as the condition of a loop
        :param body: The Transform to be applied after each successful test of the condition
        :return: a new Transform representing the iteration
        """

    @staticmethod
    def RebaseToTket() -> Transform:
        """Rebase from any gate set into TK1, CX."""

    @staticmethod
    def RebaseToRzRx() -> Transform:
        """Rebase single qubit gates into Rz, Rx."""

    @staticmethod
    def RebaseToCliffordSingles(tk2_to_cx: bool = True) -> Transform:
        """
        Replace all single-qubit unitary gates outside the set {Z, X, S, V} that are recognized as Clifford operations with an equivalent sequence of gates from that set.

        :param tk2_to_cx: whether to rebase TK2 gates to CX and standard Cliffords
        """

    @staticmethod
    def RebaseToCirq() -> Transform:
        """Rebase from any gate set into PhasedX, Rz, CZ."""

    @staticmethod
    def RebaseToQuil() -> Transform:
        """Rebase from any gate set into Rx, Rz, CZ."""

    @staticmethod
    def RebaseToPyZX() -> Transform:
        """
        Rebase from any gate set into the gate set supported by PyZX (Rx, Rz, X, Z, S, T, H, CX, CZ, SWAP).
        """

    @staticmethod
    def RebaseToProjectQ() -> Transform:
        """
        Rebase from any gate set into the gate set supported by ProjectQ (Rx, Ry, Rz, X, Y, Z, S, T, V, H, CX, CZ, CRz, SWAP).
        """

    @staticmethod
    def RebaseToIonQ() -> Transform:
        """
        Rebase from any gate set into the gate set supported by IonQ (GPI, GPI2, AAMS).
        """

    @staticmethod
    def DecomposeCCX() -> Transform:
        """Decomposes all 3-qubit Toffoli (CCX) gates into Clifford+T gates."""

    @staticmethod
    def DecomposeControlledRys() -> Transform:
        """
        Decomposes all arbitrarily-quantum-controlled Rys into CX and Ry gates.
        """

    @staticmethod
    def DecomposeSWAP(circuit: pytket._tket.circuit.Circuit) -> Transform:
        """
        Decomposes all SWAP gates to provided replacement circuit.

        :param circuit: A circuit that is logically equivalent to a SWAP operation
        """

    @staticmethod
    def DecomposeSWAPtoCX(arc: pytket._tket.architecture.Architecture) -> Transform:
        """
        Decomposes all SWAP gates into triples of CX gates. If the SWAP is adjacent to a CX, it will prefer to insert in the direction that allows for gate cancellation. If an :py:class:`~.Architecture` is provided, this will prefer to insert the CXs such that fewer need redirecting.

        :param arc: Device architecture used to specify a preference for CX direction
        """

    @staticmethod
    def DecomposeBRIDGE() -> Transform:
        """Decomposes all BRIDGE gates into CX gates."""

    @staticmethod
    def DecomposeCXDirected(arc: pytket._tket.architecture.Architecture) -> Transform:
        """
        Decompose CX gates to H+CX to match the direction of the CXs to edges of the :py:class:`~.Architecture` `arc`. Assumes the circuit already satisfies the connectivity of `arc`.

        :param arc: The architecture for which CXs should be redirected
        """

    @staticmethod
    def DecomposeBoxes(excluded_types: Set[pytket._tket.circuit.OpType] = ..., excluded_opgroups: Set[str] = ..., included_types: Set[pytket._tket.circuit.OpType] | None = None, included_opgroups: Set[str] | None = None) -> Transform:
        """
        Recursively replaces all boxes by their decomposition into circuits. 

        Arguments specify ways to filter which boxes are decomposed. A box must satisfy ALL filters in order to be decomposed (i.e. be in the inclusive sets and not in the exclusive sets).

        :param excluded_types: box :py:class:`~.OpType` s excluded from decomposition
        :param excluded_opgroups: opgroups excluded from decomposition
        :param included_types: optional, only decompose these box :py:class:`~.OpType` s
        :param included_opgroups: optional, only decompose these opgroups
        """

    @staticmethod
    def DecomposeTK2(allow_swaps: bool = True, **kwargs: Any) -> Transform:
        """
        Decompose each TK2 gate into two-qubit gates.

        We currently support CX, ZZMax and ZZPhase.

        If one or more gate fidelities are provided, the two-qubit gate type achieving the highest fidelity will be chosen for the decomposition, as measured using squared trace fidelity. If no fidelities are provided, the TK2 gates will be decomposed exactly using CX gates. For equal fidelities, ZZPhase will be preferred over ZZMax and CX if the decomposition results in fewer two-qubit gates.

        All TK2 gate parameters must be normalised, i.e. they must satisfy `NormalisedTK2Predicate`.

        Gate fidelities are passed as keyword arguments to perform noise-aware decompositions. We currently support `CX_fidelity`, `ZZMax_fidelity` and `ZZPhase_fidelity`. If provided, the `CX` and `ZZMax` fidelities must be given by a single floating point fidelity. The `ZZPhase` fidelity is given as a lambda float -> float, mapping a ZZPhase angle parameter to its fidelity, or by a single float. These parameters will be used to return the optimal decomposition of each TK2 gate, taking noise into consideration.

        Using the `allow_swaps=True` (default) option, qubits will be swapped when convenient to reduce the two-qubit gate count of the decomposed TK2.

        If the TK2 angles are symbolic values, the decomposition will be exact (i.e. not noise-aware). It is not possible in general to obtain optimal decompositions for arbitrary symbolic parameters, so consider substituting for concrete values if possible.

        :param allow_swaps: Whether to allow implicit wire swaps.
        """

    @staticmethod
    def NormaliseTK2() -> Transform:
        """
        Normalises all TK2 gates.

        TK2 gates have three angles in the interval [0, 4], but these can always be normalised to be within the so-called Weyl chamber by adding single-qubit gates.

        More precisely, the three angles a, b, c of TK2(a, b, c) are normalised exactly when the two following conditions are met:
         - numerical values must be in the Weyl chamber, ie `1/2 >= a >= b >= |c|`,
         - symbolic values must come before any numerical value in the array.

        After this transform, all TK2 angles will be normalised and the circuit will satisfy `NormalisedTK2Predicate`.
        """

    @staticmethod
    def OptimiseStandard() -> Transform:
        """
        Fast optimisation pass, performing basic simplifications. Works on any circuit, giving the result in TK1 and TK2 gates. Preserves connectivity of circuit.
        """

    @staticmethod
    def OptimisePostRouting() -> Transform:
        """
        Fast optimisation pass, performing basic simplifications. Works on any circuit, giving the result in TK1 and CX gates. If all multi-qubit gates are CXs, then this preserves their placement and orientation, so it is safe to perform after routing.
        """

    @staticmethod
    def OptimisePhaseGadgets(cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> Transform:
        """
        An optimisation pass that starts by identifying subcircuits corresponding to phase gadgets (see Cowtan, Duncan, Dilkes, Simmons, & Sivarajah https://arxiv.org/abs/1906.01734) and resynthesises them in a balanced-tree form, followed by applying OptimisePostRouting. Results use TK1 and CX gates. This will not preserve CX placement or orientation.
        """

    @staticmethod
    def OptimiseCliffords(allow_swaps: bool = True, target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX) -> Transform:
        """
        An optimisation pass that applies a number of rewrite rules for simplifying Clifford gate sequences, similar to Duncan & Fagan (https://arxiv.org/abs/1901.10114). Produces a circuit comprising TK1 gates and the two-qubit gate specified as the target.

        :param allow_swaps: whether the rewriting may introduce implicit wire swaps
        :param target_2qb_gate: target two-qubit gate (either CX or TK2)
        """

    @staticmethod
    def OptimisePauliGadgets(cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> Transform:
        """
        An optimisation pass that identifies the Pauli gadgets corresponding to any non-Clifford rotations and synthesises them pairwise (see Cowtan, Duncan, Dilkes, Simmons, & Sivarajah https://arxiv.org/abs/1906.01734). Results use TK1, CX gates.
        """

    @staticmethod
    def RemoveRedundancies() -> Transform:
        """
        Applies a collection of simple optimisations, such as removing gate-inverse pairs, merging similar rotation gates, and removing identity gates. Preserves the gate set and any placement/orientation of multi-qubit gates.
        """

    @staticmethod
    def ReduceSingles() -> Transform:
        """Reduces each sequence of single-qubit rotations into a single TK1."""

    @staticmethod
    def CommuteThroughMultis() -> Transform:
        """
        Applies a collection of commutation rules to move single qubit operations past multiqubit operations they commute with, towards the front of the circuit.
        """

    @overload
    @staticmethod
    def KAKDecomposition(target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX, cx_fidelity: float = 1.0, allow_swaps: bool = True) -> Transform:
        """
        Squash sequences of two-qubit operations into minimal form.

        Squash together sequences of single- and two-qubit gates into minimal form. Can decompose to TK2 or CX gates.

        Two-qubit operations can always be expressed in a minimal form of maximum three CXs, or as a single TK2 gate (a result also known as the KAK or Cartan decomposition).

        It is in general recommended to squash to TK2 gates, and to then use the `DecomposeTK2` pass for noise-aware decompositions to other gatesets. For backward compatibility, decompositions to CX are also supported. In this case, `cx_fidelity` can be provided to perform approximate decompositions to CX gates.

        When decomposing to TK2 gates, any sequence of two or more two-qubit gates on the same set of qubits is replaced by a single TK2 gate. When decomposing to CX, the substitution is only performed if it results in a reduction of the number of CX gates, or if at least one of the two-qubit passes is not a CX.

        Using the `allow_swaps=True` (default) option, qubits will be swapped when convenient to further reduce the two-qubit gate count. (only applicable when decomposing to CX gates).

        :param target_2qb_gate: OpType to decompose to. Either TK2 or CX.
        :param cx_fidelity: Estimated CX gate fidelity, used when target_2qb_gate=CX.
        :param allow_swaps: Whether to allow implicit wire swaps.
        """

    @overload
    @staticmethod
    def KAKDecomposition(cx_fidelity: float) -> Transform: ...

    @staticmethod
    def ThreeQubitSquash(target_2qb_gate: pytket._tket.circuit.OpType = pytket._tket.circuit.OpType.CX) -> Transform:
        """
        Squash three-qubit subcircuits into subcircuits having fewer 2-qubit gates of the target type, when possible. The supported target types are CX (default) and TK2.
        """

    @overload
    @staticmethod
    def CommuteSQThroughSWAP(avg_node_errors: Mapping[pytket._tket.unit_id.Node, float]) -> Transform:
        """
        Commutes single qubit gates through SWAP gates, leaving them on the physical qubit with best fidelity for given gate type. Assumes the circuit is already mapped onto the architecture.

        :param avg_node_errors: a dict mapping Nodes to average single-qubit gate errors
        """

    @overload
    @staticmethod
    def CommuteSQThroughSWAP(op_node_errors: Mapping[pytket._tket.unit_id.Node, Mapping[pytket._tket.circuit.OpType, float]]) -> Transform:
        """
        Commutes single qubit gates through SWAP gates, leaving them on the physical qubit with best fidelity for given gate type. Assumes the circuit is already mapped onto the architecture.

        :param avg_node_errors: a dict of dicts, mapping Nodes to dicts of OpType to single-qubit gate error maps
        """

    @staticmethod
    def DecomposeNPhasedX() -> Transform:
        """Decompose NPhasedX gates into single-qubit PhasedX gates."""

    @staticmethod
    def SynthesisePauliGraph(synth_strat: PauliSynthStrat = PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> Transform:
        """Synthesises Pauli Graphs."""

    @staticmethod
    def UCCSynthesis(synth_strat: PauliSynthStrat = PauliSynthStrat.Sets, cx_config: pytket._tket.circuit.CXConfigType = pytket._tket.circuit.CXConfigType.Snake) -> Transform:
        """
        Synthesises UCC circuits in the form that Term Sequencing provides them.
        """

    @staticmethod
    def GreedyPauliSimp(discount_rate: float = 0.7, depth_weight: float = 0.3, max_tqe_candidates: int = 500, max_lookahead: int = 500, seed: int = 0, allow_zzphase: bool = False, thread_timeout: int = 100, trials: int = 1) -> Transform:
        """
        Convert a circuit into a graph of Pauli gadgets to account for commutation and phase folding, and resynthesises them using a greedy algorithm adapted from arxiv.org/abs/2103.08602. The method for synthesising the final Clifford operator is adapted from arxiv.org/abs/2305.10966.

        WARNING: This transformation will not preserve the global phase of the circuit.

        :param discount_rate: Rate used to discount the cost impact from gadgets that are further away. Default to 0.7.
        :param depth_weight:  Degree of depth optimisation. Default to 0.3.
        :param max_tqe_candidates:  Maximum number of 2-qubit Clifford gate candidates to evaluate at each step. Default to 500.
        :param max_lookahead:  Maximum lookahead when evaluating each Clifford gate candidate. Default to 500.
        :param seed:  Unsigned integer seed used for sampling candidates and tie breaking. Default to 0.
        :param allow_zzphase: If set to True, allows the algorithm to implement 2-qubit rotations using ZZPhase gates when deemed optimal. Defaults to False.
        :param thread_timeout: Sets maximum out of time spent finding a single solution in one thread.
        :param trials: Sets maximum number of found solutions. The smallest circuit is returned, prioritising the number of 2qb-gates, then the number of gates, then the depth.
        :return: a pass to perform the simplification
        """

    @staticmethod
    def ZZPhaseToRz() -> Transform:
        """Fixes all ZZPhase gate angles to [-1, 1) half turns."""

    @staticmethod
    def CnXPairwiseDecomposition() -> Transform:
        """
        Decompose CnX gates to 2-qubit gates and single qubit gates. For every two CnX gates, reorder their control qubits to improve the chance of gate cancellation.
        """

    @staticmethod
    def PushCliffordsThroughMeasures() -> Transform:
        """
        Derives a new set of end-of-Circuit measurement operators by acting on end-of-Circuit measurements with a Clifford subcircuit. The new set of measurement operators is necessarily commuting and is implemented by adding a new mutual diagonalisation Clifford subcirciuit to the end of the Circuit and implementing the remaining diagonal measurement operators by measuring and permuting the output.
        """

    @staticmethod
    def round_angles(n: int, only_zeros: bool = False) -> Transform:
        """
        :param only_zeros: if True, only round angles less than :math:`\\pi / 2^{n+1}` to zero, leave other angles alone (default False)
        """

def separate_classical(circ: pytket._tket.circuit.Circuit) -> tuple[pytket._tket.circuit.Circuit, pytket._tket.circuit.Circuit]:
    """
    Separate the input circuit into a 'main' circuit and a classical 'post-processing' circuit, which are equivalent to the original when composed.

    :param circ: circuit to be separated
    """
