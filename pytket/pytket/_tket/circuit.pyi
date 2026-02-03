from collections.abc import Iterator, Mapping, Sequence, Set
import enum
import types
from typing import Annotated, Any, Union, overload

import numpy
from numpy.typing import NDArray
import sympy.core.expr
import sympy.core.symbol

import pytket._tket.architecture
import pytket._tket.partition
import pytket._tket.pauli
import pytket._tket.transform
import pytket._tket.unit_id
import pytket.circuit.logic_exp
import pytket.wasm.wasm


class CXConfigType(enum.Enum):
    """Enum for available configurations for CXs upon decompose phase gadgets"""

    Snake = 0
    """linear nearest neighbour CX sequence. Linear depth."""

    Star = 2
    """Every CX has same target, linear depth, good for gate cancellation."""

    Tree = 1
    """Balanced tree: logarithmic depth, harder to route."""

    MultiQGate = 3
    """
    Support for multi-qubit architectures, decomposing to 3-qubit XXPhase3 gates instead of CXs where possible.
    """

class EdgeType(enum.Enum):
    """Type of a wire in a circuit or input to an op"""

    Boolean = 2

    Classical = 1

    Quantum = 0

    WASM = 3

    RNG = 4

class OpType(enum.IntEnum):
    """
    Enum for available operations compatible with tket :py:class:`~.Circuit` s.
    """

    Phase = 21
    r"""
    Global phase: :math:`(\alpha) \mapsto \left[ \begin{array}{c} e^{i\pi\alpha} \end{array} \right]`
    """

    Z = 22
    r"""
    Pauli Z: :math:`\left[ \begin{array}{cc} 1 & 0 \\ 0 & -1 \end{array} \right]`
    """

    X = 23
    r"""
    Pauli X: :math:`\left[ \begin{array}{cc} 0 & 1 \\ 1 & 0 \end{array} \right]`
    """

    Y = 24
    r"""
    Pauli Y: :math:`\left[ \begin{array}{cc} 0 & -i \\ i & 0 \end{array} \right]`
    """

    S = 25
    r"""
    :math:`\left[ \begin{array}{cc} 1 & 0 \\ 0 & i \end{array} \right] = \mathrm{U1}(\frac12)`
    """

    Sdg = 26
    r"""
    :math:`\mathrm{S}^{\dagger} = \left[ \begin{array}{cc} 1 & 0 \\ 0 & -i \end{array} \right] = \mathrm{U1}(-\frac12)`
    """

    T = 27
    r"""
    :math:`\left[ \begin{array}{cc} 1 & 0 \\ 0 & e^{i\pi/4} \end{array} \right] = \mathrm{U1}(\frac14)`
    """

    Tdg = 28
    r"""
    :math:`\mathrm{T}^{\dagger} = \left[ \begin{array}{cc} 1 & 0 \\ 0 & e^{-i\pi/4} \end{array} \right] = \mathrm{U1}(-\frac14)`
    """

    V = 29
    r"""
    :math:`\frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & -i \\ -i & 1 \end{array} \right] = \mathrm{Rx}(\frac12)`
    """

    Vdg = 30
    r"""
    :math:`\mathrm{V}^{\dagger} = \frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & i \\ i & 1 \end{array} \right] = \mathrm{Rx}(-\frac12)`
    """

    SX = 31
    r"""
    :math:`\frac{1}{2} \left[ \begin{array}{cc} 1 + i & 1 - i \\ 1 - i & 1 + i \end{array} \right] = e^{\frac{i\pi}{4}}\mathrm{Rx}(\frac12)`
    """

    SXdg = 32
    r"""
    :math:`\mathrm{SX}^{\dagger} = \frac{1}{2} \left[ \begin{array}{cc} 1 - i & 1 + i \\ 1 + i & 1 - i \end{array} \right] = e^{\frac{-i\pi}{4}}\mathrm{Rx}(-\frac12)`
    """

    H = 33
    r"""
    Hadamard gate: :math:`\frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & 1 \\ 1 & -1 \end{array} \right]`
    """

    Rx = 34
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi \alpha \mathrm{X}} = \left[ \begin{array}{cc} \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} \\ -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} \end{array} \right]`
    """

    Ry = 35
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi \alpha \mathrm{Y}} = \left[ \begin{array}{cc} \cos\frac{\pi\alpha}{2} & -\sin\frac{\pi\alpha}{2} \\ \sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} \end{array} \right]`
    """

    Rz = 36
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi \alpha \mathrm{Z}} = \left[ \begin{array}{cc} e^{-\frac12 i \pi\alpha} & 0 \\ 0 & e^{\frac12 i \pi\alpha} \end{array} \right]`
    """

    U1 = 39
    r"""
    :math:`(\lambda) \mapsto \mathrm{U3}(0, 0, \lambda) = e^{\frac12 i\pi\lambda} \mathrm{Rz}(\lambda)`. U-gates are used by IBM. See https://qiskit.org/documentation/tutorials/circuits/3_summary_of_quantum_operations.html for more information on U-gates.
    """

    U2 = 38
    r"""
    :math:`(\phi, \lambda) \mapsto \mathrm{U3}(\frac12, \phi, \lambda) = e^{\frac12 i\pi(\lambda+\phi)} \mathrm{Rz}(\phi) \mathrm{Ry}(\frac12) \mathrm{Rz}(\lambda)`, defined by matrix multiplication
    """

    U3 = 37
    r"""
    :math:`(\theta, \phi, \lambda) \mapsto  \left[ \begin{array}{cc} \cos\frac{\pi\theta}{2} & -e^{i\pi\lambda} \sin\frac{\pi\theta}{2} \\ e^{i\pi\phi} \sin\frac{\pi\theta}{2} & e^{i\pi(\lambda+\phi)} \cos\frac{\pi\theta}{2} \end{array} \right] = e^{\frac12 i\pi(\lambda+\phi)} \mathrm{Rz}(\phi) \mathrm{Ry}(\theta) \mathrm{Rz}(\lambda)`
    """

    GPI = 40
    r"""
    :math:`(\phi) \mapsto \left[ \begin{array}{cc} 0 & e^{-i\pi\phi} \\ e^{i\pi\phi} & 0 \end{array} \right]`
    """

    GPI2 = 41
    r"""
    :math:`(\phi) \mapsto \frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & -ie^{-i\pi\phi} \\ -ie^{i\pi\phi} & 1 \end{array} \right]`
    """

    AAMS = 42
    r"""
    :math:`(\theta, \phi_0, \phi_1) \mapsto \left[ \begin{array}{cccc} \cos\frac{\pi\theta}{2} & 0 & 0 & -ie^{-i\pi(\phi_0+\phi_1)}\sin\frac{\pi\theta}{2} \\ 0 & \cos\frac{\pi\theta}{2} & -ie^{i\pi(\phi_1-\phi_0)}\sin\frac{\pi\theta}{2} & 0 \\ 0 & -ie^{i\pi(\phi_0-\phi_1)}\sin\frac{\pi\theta}{2} & \cos\frac{\pi\theta}{2} & 0 \\ -ie^{i\pi(\phi_0+\phi_1)}\sin\frac{\pi\theta}{2} & 0 & 0 & \cos\frac{\pi\theta}{2} \end{array} \right]`
    """

    TK1 = 43
    r"""
    :math:`(\alpha, \beta, \gamma) \mapsto \mathrm{Rz}(\alpha) \mathrm{Rx}(\beta) \mathrm{Rz}(\gamma)`
    """

    TK2 = 44
    r"""
    :math:`(\alpha, \beta, \gamma) \mapsto \mathrm{XXPhase}(\alpha) \mathrm{YYPhase}(\beta) \mathrm{ZZPhase}(\gamma)`
    """

    CX = 45
    r"""Controlled :math:`\mathrm{X}` gate"""

    CY = 46
    r"""Controlled :math:`\mathrm{Y}` gate"""

    CZ = 47
    r"""Controlled :math:`\mathrm{Z}` gate"""

    CH = 48
    r"""Controlled :math:`\mathrm{H}` gate"""

    CV = 49
    r"""Controlled :math:`\mathrm{V}` gate"""

    CVdg = 50
    r"""Controlled :math:`\mathrm{V}^{\dagger}` gate"""

    CSX = 51
    r"""Controlled :math:`\mathrm{SX}` gate"""

    CSXdg = 52
    r"""Controlled :math:`\mathrm{SX}^{\dagger}` gate"""

    CS = 53
    r"""Controlled :math:`\mathrm{S}` gate"""

    CSdg = 54
    r"""Controlled :math:`\mathrm{S}^{\dagger}` gate"""

    CRz = 55
    r""":math:`(\alpha) \mapsto` Controlled :math:`\mathrm{Rz}(\alpha)` gate"""

    CRx = 56
    r""":math:`(\alpha) \mapsto` Controlled :math:`\mathrm{Rx}(\alpha)` gate"""

    CRy = 57
    r""":math:`(\alpha) \mapsto` Controlled :math:`\mathrm{Ry}(\alpha)` gate"""

    CU1 = 58
    r"""
    :math:`(\lambda) \mapsto` Controlled :math:`\mathrm{U1}(\lambda)` gate. Note that this is not equivalent to a :math:`\mathrm{CRz}(\lambda)` up to global phase, differing by an extra :math:`\mathrm{Rz}(\frac{\lambda}{2})` on the control qubit.
    """

    CU3 = 59
    r"""
    :math:`(\theta, \phi, \lambda) \mapsto` Controlled :math:`\mathrm{U3}(\theta, \phi, \lambda)` gate. Similar rules apply.
    """

    PhaseGadget = 60
    r""":math:`\alpha \mapsto e^{-\frac12 i \pi\alpha Z^{\otimes n}}`"""

    CCX = 61
    """Toffoli gate"""

    ECR = 69
    r"""
    :math:`\frac{1}{\sqrt 2} \left[ \begin{array}{cccc} 0 & 0 & 1 & i \\0 & 0 & i & 1 \\1 & -i & 0 & 0 \\-i & 1 & 0 & 0 \end{array} \right]`
    """

    SWAP = 62
    """Swap gate"""

    CSWAP = 63
    """Controlled swap gate"""

    noop = 65
    """
    Identity gate. These gates are not permanent and are automatically stripped by the compiler
    """

    Barrier = 8
    """
    Meta-operation preventing compilation through it. Not automatically stripped by the compiler
    """

    Label = 9
    """Label for control flow jumps. Does not appear within a circuit"""

    Branch = 10
    """
    A control flow jump to a label dependent on the value of a given Bit. Does not appear within a circuit
    """

    Goto = 11
    """
    An unconditional control flow jump to a Label. Does not appear within a circuit.
    """

    Stop = 12
    """
    Halts execution immediately. Used to terminate a program. Does not appear within a circuit.
    """

    BRIDGE = 64
    """
    A CX Bridge over 3 qubits. Used to apply a logical CX between the first and third qubits when they are not adjacent on the device, but both neighbour the second qubit. Acts as the identity on the second qubit
    """

    Measure = 66
    """
    Z-basis projective measurement, storing the measurement outcome in a specified bit
    """

    Reset = 68
    r"""Resets the qubit to :math:`\left|0\right>`"""

    CircBox = 89
    """Represents an arbitrary subcircuit"""

    PhasePolyBox = 100
    """
    An operation representing arbitrary circuits made up of CX and Rz gates, represented as a phase polynomial together with a boolean matrix representing an additional linear transformation.
    """

    Unitary1qBox = 90
    """Represents an arbitrary one-qubit unitary operation by its matrix"""

    Unitary2qBox = 91
    """Represents an arbitrary two-qubit unitary operation by its matrix"""

    Unitary3qBox = 92
    """Represents an arbitrary three-qubit unitary operation by its matrix"""

    ExpBox = 93
    """
    A two-qubit operation corresponding to a unitary matrix defined as the exponential :math:`e^{itA}` of an arbitrary 4x4 hermitian matrix :math:`A`.
    """

    PauliExpBox = 94
    r"""
    An operation defined as the exponential :math:`e^{-\frac{i\pi\alpha}{2} P}` of a tensor :math:`P` of Pauli operations.
    """

    PauliExpPairBox = 95
    r"""
    A pair of (not necessarily commuting) Pauli exponentials :math:`e^{-\frac{i\pi\alpha}{2} P}` performed in sequence.
    """

    PauliExpCommutingSetBox = 96
    r"""
    An operation defined as a setof commuting exponentials of the form :math:`e^{-\frac{i\pi\alpha}{2} P}` of a tensor :math:`P` of Pauli operations.
    """

    TermSequenceBox = 97
    """
    An unordered collection of Pauli exponentials that can be synthesised in any order, causing a change in the unitary operation. Synthesis order depends on the synthesis strategy chosen only.
    """

    QControlBox = 101
    """An arbitrary n-controlled operation"""

    ToffoliBox = 112
    """A permutation of classical basis states"""

    ConjugationBox = 108
    """An operation composed of 'action', 'compute' and 'uncompute' circuits"""

    DummyBox = 114
    """A placeholder operation that holds resource data"""

    CustomGate = 99
    r"""
    :math:`(\alpha, \beta, \ldots) \mapsto` A user-defined operation, based on a :py:class:`~.Circuit` :math:`C` with parameters :math:`\alpha, \beta, \ldots` substituted in place of bound symbolic variables in :math:`C`, as defined by the :py:class:`~.CustomGateDef`.
    """

    Conditional = 109
    """
    An operation to be applied conditionally on the value of some classical register
    """

    ISWAP = 70
    r"""
    :math:`(\alpha) \mapsto e^{\frac14 i \pi\alpha (\mathrm{X} \otimes \mathrm{X} + \mathrm{Y} \otimes \mathrm{Y})} = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & \cos\frac{\pi\alpha}{2} & i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} & 0 \\ 0 & 0 & 0 & 1 \end{array} \right]`
    """

    PhasedISWAP = 82
    r"""
    :math:`(p, t) \mapsto \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & \cos\frac{\pi t}{2} & i\sin\frac{\pi t}{2}e^{2i\pi p} & 0 \\ 0 & i\sin\frac{\pi t}{2}e^{-2i\pi p} & \cos\frac{\pi t}{2} & 0 \\ 0 & 0 & 0 & 1 \end{array} \right]` (equivalent to: Rz(p)[0]; Rz(-p)[1]; ISWAP(t); Rz(-p)[0]; Rz(p)[1])
    """

    XXPhase = 74
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi\alpha (\mathrm{X} \otimes \mathrm{X})} = \left[ \begin{array}{cccc} \cos\frac{\pi\alpha}{2} & 0 & 0 & -i\sin\frac{\pi\alpha}{2} \\ 0 & \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} & 0 \\ -i\sin\frac{\pi\alpha}{2} & 0 & 0 & \cos\frac{\pi\alpha}{2} \end{array} \right]`
    """

    YYPhase = 75
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi\alpha (\mathrm{Y} \otimes \mathrm{Y})} = \left[ \begin{array}{cccc} \cos\frac{\pi\alpha}{2} & 0 & 0 & i\sin\frac{\pi\alpha}{2} \\ 0 & \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} & 0 \\ i\sin\frac{\pi\alpha}{2} & 0 & 0 & \cos\frac{\pi\alpha}{2} \end{array} \right]`
    """

    ZZPhase = 76
    r"""
    :math:`(\alpha) \mapsto e^{-\frac12 i \pi\alpha (\mathrm{Z} \otimes \mathrm{Z})} = \left[ \begin{array}{cccc} e^{-\frac12 i \pi\alpha} & 0 & 0 & 0 \\ 0 & e^{\frac12 i \pi\alpha} & 0 & 0 \\ 0 & 0 & e^{\frac12 i \pi\alpha} & 0 \\ 0 & 0 & 0 & e^{-\frac12 i \pi\alpha} \end{array} \right]`
    """

    XXPhase3 = 77
    """
    A 3-qubit gate XXPhase3(α) consists of pairwise 2-qubit XXPhase(α) interactions. Equivalent to XXPhase(α)[0, 1] XXPhase(α)[1, 2] XXPhase(α)[0, 2].
    """

    PhasedX = 71
    r"""
    :math:`(\alpha,\beta) \mapsto \mathrm{Rz}(\beta)\mathrm{Rx}(\alpha)\mathrm{Rz}(-\beta)` (matrix-multiplication order)
    """

    NPhasedX = 72
    r"""
    :math:`(\alpha, \beta) \mapsto \mathrm{PhasedX}(\alpha, \beta)^{\otimes n}` (n-qubit gate composed of identical PhasedX in parallel.
    """

    CnRx = 84
    r""":math:`(\alpha)` := n-controlled :math:`\mathrm{Rx}(\alpha)` gate."""

    CnRy = 83
    r""":math:`(\alpha)` := n-controlled :math:`\mathrm{Ry}(\alpha)` gate."""

    CnRz = 85
    r""":math:`(\alpha)` := n-controlled :math:`\mathrm{Rz}(\alpha)` gate."""

    CnX = 86
    """n-controlled X gate."""

    CnY = 88
    """n-controlled Y gate."""

    CnZ = 87
    """n-controlled Z gate."""

    ZZMax = 73
    r"""
    :math:`e^{-\frac{i\pi}{4}(\mathrm{Z} \otimes \mathrm{Z})}`, a maximally entangling ZZPhase
    """

    ESWAP = 78
    r"""
    :math:`\alpha \mapsto e^{-\frac12 i\pi\alpha \cdot \mathrm{SWAP}} = \left[ \begin{array}{cccc} e^{-\frac12 i \pi\alpha} & 0 & 0 & 0 \\ 0 & \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} & 0 \\ 0 & 0 & 0 & e^{-\frac12 i \pi\alpha} \end{array} \right]`
    """

    FSim = 79
    r"""
    :math:`(\alpha, \beta) \mapsto \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & \cos \pi\alpha & -i\sin \pi\alpha & 0 \\ 0 & -i\sin \pi\alpha & \cos \pi\alpha & 0 \\ 0 & 0 & 0 & e^{-i\pi\beta} \end{array} \right]`
    """

    Sycamore = 80
    r""":math:`\mathrm{FSim}(\frac12, \frac16)`"""

    ISWAPMax = 81
    r"""
    :math:`\mathrm{ISWAP}(1) = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & 0 & i & 0 \\ 0 & i & 0 & 0 \\ 0 & 0 & 0 & 1 \end{array} \right]`
    """

    ClassicalTransform = 13
    """A general classical operation where all inputs are also outputs"""

    WASM = 14
    """Op containing a classical wasm function call"""

    SetBits = 15
    """An operation to set some bits to specified values"""

    CopyBits = 16
    """An operation to copy some bit values"""

    RangePredicate = 17
    """A classical predicate defined by a range of values in binary encoding"""

    ExplicitPredicate = 18
    """A classical predicate defined by a truth table"""

    ExplicitModifier = 19
    """An operation defined by a truth table that modifies one bit"""

    MultiBit = 20
    """A classical operation applied to multiple bits simultaneously"""

    MultiplexorBox = 102
    """A multiplexor (i.e. uniformly controlled operations)"""

    MultiplexedRotationBox = 103
    """
    A multiplexed rotation gate (i.e. uniformly controlled single-axis rotations)
    """

    MultiplexedU2Box = 104
    """A multiplexed U2 gate (i.e. uniformly controlled U2 gate)"""

    MultiplexedTensoredU2Box = 105
    """A multiplexed tensored-U2 gate"""

    StatePreparationBox = 106
    """
    A box for preparing quantum states using multiplexed-Ry and multiplexed-Rz gates
    """

    DiagonalBox = 107
    """
    A box for synthesising a diagonal unitary matrix into a sequence of multiplexed-Rz gates
    """

    ClExpr = 115
    """A classical expression"""

    Input = 0
    """Quantum input node of the circuit"""

    Output = 1
    """Quantum output node of the circuit"""

    Create = 2
    """Quantum node with no predecessors, implicitly in zero state"""

    Discard = 3
    """
    Quantum node with no successors, not composable with input nodes of other circuits
    """

    ClInput = 4
    """Classical input node of the circuit"""

    ClOutput = 5
    """Classical output node of the circuit"""

    WASMInput = 6
    """WASM input node of the circuit"""

    WASMOutput = 7
    """WASM output node of the circuit"""

    Collapse = 67
    """Measure a qubit producing no output"""

    CliffBox = 98
    """NYI"""

    ProjectorAssertionBox = 110
    """See :py:class:`~.ProjectorAssertionBox`"""

    StabiliserAssertionBox = 111
    """See :py:class:`~.StabiliserAssertionBox`"""

    UnitaryTableauBox = 113
    """See :py:class:`~.UnitaryTableauBox`"""

    RNGInput = 116
    """RNG input node"""

    RNGOutput = 117
    """RNG output node"""

    RNGSeed = 118
    """Seed an RNG using 64 bits"""

    RNGBound = 119
    """Set an (inclusive) 32-bit upper bound on RNG output"""

    RNGIndex = 120
    """Set a 32-bit index on an RNG"""

    RNGNum = 121
    """Get 32-bit output from an RNG"""

    JobShotNum = 122
    """
    Get 32-bit (little-endian) shot number when a circuit is being run multiple times
    """

    @staticmethod
    def from_name(arg: str, /) -> OpType:
        """Construct from name"""

class Op:
    """Encapsulates operation information"""

    @overload
    @staticmethod
    def create(arg: OpType, /) -> Op:
        """Create an :py:class:`~.Op` with given type"""

    @overload
    @staticmethod
    def create(arg0: OpType, arg1: Union[sympy.core.expr.Expr, float], /) -> Op:
        """Create an :py:class:`~.Op` with given type and parameter"""

    @overload
    @staticmethod
    def create(arg0: OpType, arg1: Sequence[Union[sympy.core.expr.Expr, float]], /) -> Op:
        """Create an :py:class:`~.Op` with given type and parameters"""

    @property
    def type(self) -> OpType:
        """Type of op being performed"""

    @property
    def params(self) -> list[Union[sympy.core.expr.Expr, float]]:
        r"""
        Angular parameters of the op, in half-turns (e.g. 1.0 half-turns is :math:`\pi` radians). The parameters returned are constrained to the appropriate canonical range, which is usually the half-open interval [0,2) but for some operations (e.g. Rx, Ry and Rz) is [0,4).
        """

    @property
    def n_qubits(self) -> int:
        """Number of qubits of op"""

    @property
    def dagger(self) -> Op:
        """Dagger of op"""

    @property
    def transpose(self) -> Op:
        """Transpose of op"""

    def get_name(self, latex: bool = False) -> str:
        """String representation of op"""

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    def __repr__(self) -> str: ...

    def free_symbols(self) -> set[sympy.core.symbol.Symbol]: ...

    def get_unitary(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')]: ...

    def is_clifford_type(self) -> bool:
        """Check if the operation is one of the Clifford :py:class:`~.OpType` s."""

    def is_clifford(self) -> bool:
        """
        Test whether the operation is in the Clifford group. A return value of true guarantees that the operation is Clifford. However, the converse is not the case as some Clifford operations may not be detected as such.
        """

    def is_gate(self) -> bool: ...

class BasisOrder(enum.Enum):
    r"""
    Enum for readout basis and ordering.
    Readouts are viewed in increasing lexicographic order (ILO) of the bit's UnitID. This is our default convention for column indexing for ALL readout forms (shots, counts, statevector, and unitaries). e.g. :math:`\lvert abc \rangle` corresponds to the readout: ('c', 0) --> :math:`a`, ('c', 1) --> :math:`b`, ('d', 0) --> :math:`c`
    For statevector and unitaries, the string abc is interpreted as an index in a big-endian (BE) fashion. e.g. the statevector :math:`(a_{00}, a_{01}, a_{10}, a_{11})`
    Some backends (Qiskit, ProjectQ, etc.) use a DLO-BE (decreasing lexicographic order, big-endian) convention. This is the same as ILO-LE (little-endian) for statevectors and unitaries, but gives shot tables/readouts in a counter-intuitive manner.
    Every backend and matrix-based box has a BasisOrder option which can toggle between ILO-BE (ilo) and DLO-BE (dlo).
    """

    ilo = 0
    """Increasing Lexicographic Order of UnitID, big-endian"""

    dlo = 1
    """Decreasing Lexicographic Order of UnitID, big-endian"""

class Command:
    """
    A single quantum command in the circuit, defined by the Op, the qubits it acts on, and the op group name if any.
    """

    def __init__(self, op: Op, args: Sequence[pytket._tket.unit_id.UnitID]) -> None:
        """Construct from an operation and a vector of unit IDs"""

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    def __repr__(self) -> str: ...

    @property
    def op(self) -> Op:
        """Operation for this command."""

    @property
    def args(self) -> list[pytket._tket.unit_id.UnitID]:
        """The qubits/bits the command acts on."""

    @property
    def qubits(self) -> list[pytket._tket.unit_id.Qubit]:
        """The qubits the command acts on."""

    @property
    def bits(self) -> list[pytket._tket.unit_id.Bit]:
        """The bits the command could write to (does not include read-only bits)."""

    @property
    def opgroup(self) -> str | None:
        """
        The op group name assigned to the command (or `None` if no name is defined).
        """

    def free_symbols(self) -> set[sympy.core.symbol.Symbol]:
        """:return: set of symbolic parameters for the command"""

class MetaOp(Op):
    """Meta operation, such as input or output vertices."""

    def __init__(self, type: OpType, signature: Sequence[EdgeType], data: str) -> None:
        """
        Construct MetaOp with optype, signature and additional data string

        :param type: type for the meta op
        :param signature: signature for the op
        :param data: additional string stored in the op
        """

    @property
    def data(self) -> str:
        """Get data from MetaOp"""

class BarrierOp(Op):
    """Barrier operations."""

    def __init__(self, signature: Sequence[EdgeType], data: str) -> None:
        """
        Construct BarrierOp with signature and additional data string
        :param signature: signature for the op
        :param data: additional string stored in the op
        """

    @property
    def data(self) -> str:
        """Get data from BarrierOp"""

class Circuit:
    """
    Encapsulates a quantum circuit using a DAG representation.

    >>> from pytket import Circuit
    >>> c = Circuit(4,2) # Create a circuit with 4 qubits and 2 classical bits
    >>> c.H(0) # Apply a gate to qubit 0
    >>> c.Rx(0.5,1) # Angles of rotation are expressed in half-turns (i.e. 0.5 means PI/2)
    >>> c.Measure(1,0) # Measure qubit 1, saving result in bit 0
    """

    @overload
    def __init__(self) -> None:
        """Constructs a circuit with a completely empty DAG."""

    @overload
    def __init__(self, name: str) -> None:
        """
        Constructs a named circuit with a completely empty DAG.

        :param name: name for the circuit
        """

    @overload
    def __init__(self, n_qubits: int, name: str | None = None) -> None:
        """
        Constructs a circuit with a given number of qubits/blank wires.

        >>> c = Circuit()
        >>> c.add_blank_wires(3)

        is equivalent to

        >>> c = Circuit(3)

        :param n_qubits: The number of qubits in the circuit
        :param name: Optional name for the circuit.
        """

    @overload
    def __init__(self, n_qubits: int, n_bits: int, name: str | None = None) -> None:
        """
        Constructs a circuit with a given number of quantum and classical bits

        :param n_qubits: The number of qubits in the circuit
        :param n_bits: The number of classical bits in the circuit
        :param name: Optional name for the circuit.
        """

    @overload
    def add_gate(self, Op: Op, args: Sequence[int] | Sequence[pytket._tket.unit_id.UnitID], **kwargs: Any) -> Circuit:
        """
        Appends a single operation to the end of the circuit on some particular qubits/bits. The number of qubits/bits specified must match the arity of the gate.
        """

    @overload
    def add_gate(self, type: OpType, args: Sequence[int] | Sequence[pytket._tket.unit_id.UnitID], **kwargs: Any) -> Circuit:
        """
        Appends a single (non-parameterised) gate to the end of the circuit on some particular qubits from the default register ('q'). The number of qubits specified must match the arity of the gate. For `OpType.Measure` operations the bit from the default register should follow the qubit.

        >>> c.add_gate(OpType.H, [0]) # equivalent to c.H(0)
        >>> c.add_gate(OpType.CX, [0,1]) # equivalent to c.CX(0,1)

        :param type: The type of operation to add
        :param args: The qubits/bits to apply the gate to
        :param kwargs: Additional properties for classical conditions
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_gate(self, type: OpType, angle: Union[sympy.core.expr.Expr, float], args: Sequence[int] | Sequence[pytket._tket.unit_id.UnitID], **kwargs: Any) -> Circuit:
        """
        Appends a single gate, parameterised by an expression, to the end of circuit on some particular qubits from the default register ('q').

        :param type: The type of gate to add
        :param angle: The parameter for the gate in halfturns
        :param args: The qubits/bits to apply the gate to
        :param kwargs: Additional properties for classical conditions
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_gate(self, type: OpType, angles: Sequence[Union[sympy.core.expr.Expr, float]], args: Sequence[int] | Sequence[pytket._tket.unit_id.UnitID], **kwargs: Any) -> Circuit:
        """
        Appends a single gate to the end of the circuit

        :param type: The type of gate to add
        :param params: The parameters for the gate in halfturns
        :param args: The qubits/bits to apply the gate to
        :param kwargs: Additional properties for classical conditions
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_barrier(self, qubits: Sequence[int], bits: Sequence[int] = [], data: str = '') -> Circuit:
        """
        Append a Barrier on the given units

        :param data: additional data stored in the barrier
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_barrier(self, units: Sequence[pytket._tket.unit_id.UnitID], data: str = '') -> Circuit: ...

    @overload
    def add_conditional_barrier(self, barrier_qubits: Sequence[int], barrier_bits: Sequence[int], condition_bits: Sequence[int], value: int, data: str = '') -> Circuit:
        """
        Append a Conditional Barrier on the given barrier qubits and barrier bits, conditioned on the given condition bits.

        :param barrier_qubits: Qubit in Barrier operation.
        :param barrier_bits: Bit in Barrier operation.
        :param condition_bits: Bit covering classical control condition of barrier operation.
        :param value: Value that classical condition must have to hold (little-endian).
        :param data: Additional data stored in Barrier operation.
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_conditional_barrier(self, barrier_args: Sequence[pytket._tket.unit_id.UnitID], condition_bits: Sequence[pytket._tket.unit_id.Bit], value: int, data: str = '') -> Circuit:
        """
        Append a Conditional Barrier on the given barrier qubits and barrier bits, conditioned on the given condition bits.

        :param barrier_args: Qubit and Bit in Barrier operation.
        :param condition_bits: Bit covering classical control  condition of barrier operation.
        :param value: Value that classical condition must have to hold (little-endian).
        :param data: Additional data stored in Barrier operation.
        :return: the new :py:class:`~.Circuit`
        """

    def add_clexpr(self, expr: WiredClExpr, args: Sequence[pytket._tket.unit_id.Bit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.WiredClExpr` to the circuit.

        :param expr: The expression to append
        :param args: The bits to apply the expression to
        :return: the new :py:class:`~.Circuit`
        """

    def add_circbox(self, circbox: CircBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.UnitID], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.CircBox` to the circuit.

        The qubits and bits of the :py:class:`~.CircBox` are wired into the circuit in lexicographic order. Bits follow qubits in the order of arguments.

        :param circbox: The box to append
        :param args: The qubits/bits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_circbox_regwise(self, circbox: CircBox, qregs: Sequence[pytket._tket.unit_id.QubitRegister], cregs: Sequence[pytket._tket.unit_id.BitRegister], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.CircBox` to the circuit, wiring whole registers together.

        :param circbox: The box to append
        :param qregs: Sequence of :py:class:`~.QubitRegister` from the outer :py:class:`~.Circuit`, the order corresponding to the lexicographic order of corresponding registers in the :py:class:`~.CircBox`
        :param cregs: Sequence of :py:class:`~.BitRegister` from the outer :py:class:`~.Circuit`, the order corresponding to the lexicographic order of corresponding registers in the :py:class:`~.CircBox`
        :return: the new :py:class:`~.Circuit`
        """

    def add_circbox_with_regmap(self, circbox: CircBox, qregmap: Mapping[str, str], cregmap: Mapping[str, str], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.CircBox` to the circuit, wiring whole registers together.

        This method expects two maps (one for qubit registers and one for bit registers), which must have keys corresponding to all register names in the box. The box may not contain any qubits or bits that do not belong to a register, i.e. all must be single-indexed contiguously from zero.

        :param circbox: The box to append
        :param qregmap: Map specifying which qubit register in the :py:class:`~.CircBox` (the map's keys) matches which register in the outer circuit (the map's values)
        :param cregmap: Map specifying which bit register in the :py:class:`~.CircBox` (the map's keys) matches which register in the outer circuit (the map's values)
        :return: the new :py:class:`~.Circuit`
        """

    def add_unitary1qbox(self, unitarybox: Unitary1qBox, qubit_0: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.Unitary1qBox` to the circuit.

        :param unitarybox: The box to append
        :param qubit_0: The qubit to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_unitary2qbox(self, unitarybox: Unitary2qBox, qubit_0: int | pytket._tket.unit_id.Qubit, qubit_1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.Unitary2qBox` to the circuit.

        The matrix representation is ILO-BE.

        :param unitarybox: The box to append
        :param qubit_0: The first target qubit
        :param qubit_1: The second target qubit
        :return: the new :py:class:`~.Circuit`
        """

    def add_unitary3qbox(self, unitarybox: Unitary3qBox, qubit_0: int | pytket._tket.unit_id.Qubit, qubit_1: int | pytket._tket.unit_id.Qubit, qubit_2: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.Unitary3qBox` to the circuit.

        :param unitarybox: box to append
        :param qubit_0: index of target qubit 0
        :param qubit_1: index of target qubit 1
        :param qubit_2: index of target qubit 2
        :return: the new :py:class:`~.Circuit`
        """

    def add_expbox(self, expbox: ExpBox, qubit_0: int | pytket._tket.unit_id.Qubit, qubit_1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Append an :py:class:`~.ExpBox` to the circuit.

        The matrix representation is ILO-BE.

        :param expbox: The box to append
        :param qubit_0: The first target qubit
        :param qubit_1: The second target qubit
        :return: the new :py:class:`~.Circuit`
        """

    def add_pauliexpbox(self, pauliexpbox: PauliExpBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.PauliExpBox` to the circuit.

        :param pauliexpbox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_pauliexppairbox(self, pauliexppairbox: PauliExpPairBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.PauliExpPairBox` to the circuit.

        :param pauliexppairbox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_pauliexpcommutingsetbox(self, pauliexpcommutingsetbox: PauliExpCommutingSetBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.PauliExpCommutingSetBox` to the circuit.

        :param pauliexpcommutingsetbox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_termsequencebox(self, termsequencebox: TermSequenceBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.TermSequenceBox` to the circuit.

        :param termsequencebox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_toffolibox(self, toffolibox: ToffoliBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.ToffoliBox` to the circuit.

        :param toffolibox: The box to append
        :param qubits: Indices of the qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_dummybox(self, dummybox: DummyBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], bits: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.DummyBox` to the circuit.

        :param dummybox: The box to append
        :param qubits: Qubits to append the box to
        :param bits: Bits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_qcontrolbox(self, qcontrolbox: QControlBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.QControlBox` to the circuit.

        :param qcontrolbox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_phasepolybox(self, phasepolybox: PhasePolyBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.PhasePolyBox` to the circuit.

        :param phasepolybox: The box to append
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_custom_gate(self, definition: CustomGateDef, params: Sequence[Union[sympy.core.expr.Expr, float]], qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append an instance of a :py:class:`~.CustomGateDef` to the circuit.

        :param def: The custom gate definition
        :param params: List of parameters to instantiate the gate with, in halfturns
        :param qubits: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_assertion(self, box: ProjectorAssertionBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], ancilla: int | pytket._tket.unit_id.Qubit | None = None, name: str | None = None) -> Circuit:
        """
        Append a :py:class:`~.ProjectorAssertionBox` to the circuit.

        :param box: ProjectorAssertionBox to append
        :param qubits: target qubits
        :param ancilla: ancilla qubit
        :param name: name used to identify this assertion
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_assertion(self, box: StabiliserAssertionBox, qubits: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], ancilla: int | pytket._tket.unit_id.Qubit, name: str | None = None) -> Circuit:
        """
        Append a :py:class:`~.StabiliserAssertionBox` to the circuit.

        :param box: StabiliserAssertionBox to append
        :param qubits: target qubits
        :param ancilla: ancilla qubit
        :param name: name used to identify this assertion
        :return: the new :py:class:`~.Circuit`
        """

    def add_multiplexor(self, box: MultiplexorBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.MultiplexorBox` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_multiplexedrotation(self, box: MultiplexedRotationBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.MultiplexedRotationBox` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_multiplexedu2(self, box: MultiplexedU2Box, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.MultiplexedU2Box` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_multiplexed_tensored_u2(self, box: MultiplexedTensoredU2Box, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.MultiplexedTensoredU2Box` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_state_preparation_box(self, box: StatePreparationBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.StatePreparationBox` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_diagonal_box(self, box: DiagonalBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.DiagonalBox` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def add_conjugation_box(self, box: ConjugationBox, args: Sequence[int] | Sequence[pytket._tket.unit_id.Qubit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.ConjugationBox` to the circuit.

        :param box: The box to append
        :param args: The qubits to append the box to
        :return: the new :py:class:`~.Circuit`
        """

    def H(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a Hadamard gate.

        :return: the new :py:class:`~.Circuit`
        """

    def X(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """:return: the new :py:class:`~.Circuit`"""

    def Y(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """:return: the new :py:class:`~.Circuit`"""

    def Z(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """:return: the new :py:class:`~.Circuit`"""

    def T(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a T gate (equivalent to U1(0.25,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def Tdg(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a T-dagger gate (equivalent to U1(-0.25,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def S(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an S gate (equivalent to U1(0.5,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def Sdg(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an S-dagger gate (equivalent to U1(-0.5,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def V(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a V gate (equivalent to Rx(0.5,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def Vdg(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a V-dagger gate (equivalent to Rx(-0.5,-)).

        :return: the new :py:class:`~.Circuit`
        """

    def SX(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a SX gate (equivalent to Rx(0.5,-) up to a 0.25 global phase).

        :return: the new :py:class:`~.Circuit`
        """

    def SXdg(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a SXdg gate (equivalent to Rx(-0.5,-) up to a -0.25 global phase).

        :return: the new :py:class:`~.Circuit`
        """

    def Measure(self, qubit: int | pytket._tket.unit_id.Qubit, bit: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a single-qubit measurement in the computational (Z) basis.

        :return: the new :py:class:`~.Circuit`
        """

    def Reset(self, qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a Reset operation. Sets a qubit to the Z-basis 0 state. Non-unitary operation.

        :return: the new :py:class:`~.Circuit`
        """

    def Rz(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an Rz gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def Rx(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an Rx gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def Ry(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an Ry gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def U1(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a U1 gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def U2(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a U2 gate with possibly symbolic angles (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def U3(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], angle2: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a U3 gate with possibly symbolic angles (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def GPI(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a GPI gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def GPI2(self, angle: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a GPI2 gate with a possibly symbolic angle (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def AAMS(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], angle2: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an AAMS gate with possibly symbolic angles (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def TK1(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], angle2: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a TK1 gate with possibly symbolic angles (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def TK2(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], angle2: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a TK2 gate with possibly symbolic angles (specified in half-turns).

        :return: the new :py:class:`~.Circuit`
        """

    def CX(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CX gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CY(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CY gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CZ(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CZ gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CH(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CH gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CV(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CV gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CVdg(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CVdg gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CSX(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CSX gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CSXdg(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CSXdg gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CS(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CS gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CSdg(self, control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CSdg gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CRz(self, angle: Union[sympy.core.expr.Expr, float], control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CRz gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CRx(self, angle: Union[sympy.core.expr.Expr, float], control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CRx gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CRy(self, angle: Union[sympy.core.expr.Expr, float], control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CRy gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CU1(self, angle: Union[sympy.core.expr.Expr, float], control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CU1 gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CU3(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], angle2: Union[sympy.core.expr.Expr, float], control_qubit: int | pytket._tket.unit_id.Qubit, target_qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CU3 gate with possibly symbolic angles (specified in half-turns) on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ZZPhase(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a ZZ gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified two qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ZZMax(self, qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a ZZMax gate on the wires for the specified two qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ESWAP(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an ESWAP gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified two qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def FSim(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an FSim gate with possibly symbolic angles (specified in half-turns) on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def Sycamore(self, qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a Sycamore gate on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def XXPhase(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a XX gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified two qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def YYPhase(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a YY gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified two qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def XXPhase3(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, qubit2: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a 3-qubit XX gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified three qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def PhasedX(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], qubit: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a PhasedX gate with possibly symbolic angles (specified in half-turns) on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CCX(self, control_0: int | pytket._tket.unit_id.Qubit, control_1: int | pytket._tket.unit_id.Qubit, target: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CCX gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ECR(self, qubit_0: int | pytket._tket.unit_id.Qubit, qubit_1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an ECR gate on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def SWAP(self, qubit_0: int | pytket._tket.unit_id.Qubit, qubit_1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a SWAP gate on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def CSWAP(self, control: int | pytket._tket.unit_id.Qubit, target_0: int | pytket._tket.unit_id.Qubit, target_1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a CSWAP gate on the wires for the specified control and target qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ISWAP(self, angle: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an ISWAP gate with a possibly symbolic angle (specified in half-turns) on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def ISWAPMax(self, qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends an ISWAPMax gate on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def PhasedISWAP(self, angle0: Union[sympy.core.expr.Expr, float], angle1: Union[sympy.core.expr.Expr, float], qubit0: int | pytket._tket.unit_id.Qubit, qubit1: int | pytket._tket.unit_id.Qubit, **kwargs: Any) -> Circuit:
        """
        Appends a PhasedISWAP gate with possibly symbolic angles (specified in half-turns) on the wires for the specified qubits.

        :return: the new :py:class:`~.Circuit`
        """

    def measure_all(self) -> Circuit:
        """
        Appends a measure gate to all qubits, storing the results in the default classical register. Bits are added to the circuit if they do not already exist.

        :return: the new :py:class:`~.Circuit`
        """

    def measure_register(self, arg0: pytket._tket.unit_id.QubitRegister, arg1: str, /) -> Circuit:
        """
        Appends a measure gate to all qubits in the given register, storing the results in the given classical register with matching indices.The classical register will be created if it doesn't exist.

        :param qreg: the QubitRegister to be measured
        :param creg_name: the name of the BitRegister to store the results
        :return: the new :py:class:`~.Circuit`
        """

    def Phase(self, arg0: Union[sympy.core.expr.Expr, float], /, **kwargs: Any) -> Circuit: ...

    def add_c_transform(self, values: Sequence[int], args: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], name: str = 'ClassicalTransform', **kwargs: Any) -> Circuit:
        """
        Appends a purely classical transformation, defined by a table of values, to the end of the circuit.

        :param values: table of values: bit :math:`j` (in little-endian order) of the term indexed by :math:`sum_i a_i 2^i` is output :math:`j` of the transform applied to inputs :math:`(a_i)`.
        :param args: bits to which the transform is applied
        :param name: operation name
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def _add_wasm(self, funcname: str, wasm_uid: str, width_i_parameter: Sequence[int], width_o_parameter: Sequence[int], args: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], wasm_wire_args: Sequence[int], **kwargs: Any) -> Circuit:
        """
        Add a classical function call from a wasm file to the circuit. 

        :param funcname: name of the function that is called
        :param wasm_uid: unit id to identify the wasm file
        :param width_i_parameter: list of the number of bits in the input variables
        :param width_o_parameter: list of the number of bits in the output variables
        :param args: vector of circuit bits the wasm op should be added to
        :param wasm_wire_args: vector of circuit wasmwires the wasm op should be added to
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def _add_wasm(self, funcname: str, wasm_uid: str, list_reg_in: Sequence[pytket._tket.unit_id.BitRegister], list_reg_out: Sequence[pytket._tket.unit_id.BitRegister], wasm_wire_args: Sequence[int], **kwargs: Any) -> Circuit:
        """
        Add a classical function call from a wasm file to the circuit. 

        :param funcname: name of the function that is called
        :param wasm_uid: unit id to identify the wasm file
        :param list_reg_in: list of the classical registers in the circuit used as inputs
        :param list_reg_out: list of the classical registers in the circuit used as outputs
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def _set_rng_seed(self, creg: pytket._tket.unit_id.BitRegister, rng_wire: int, **kwargs: Any) -> Circuit:
        """
        Seed an RNG from the contents of a classical register.

        The classical register must be exactly 64 bits.

        :param creg: register of classical bits
        :param rng_wire: index of RNG wire
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`~.Circuit`
        """

    def _set_rng_bound(self, creg: pytket._tket.unit_id.BitRegister, rng_wire: int, **kwargs: Any) -> Circuit:
        """
        Set an RNG upper bound from the contents of a classical register.

        The classical register must be exactly 32 bits. It encodes the upper bound in little-endian binary (least significant bit first). The bound is inclusive.

        :param creg: register of classical bits
        :param rng_wire: index of RNG wire
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`~.Circuit`
        """

    def _set_rng_index(self, creg: pytket._tket.unit_id.BitRegister, rng_wire: int, **kwargs: Any) -> Circuit:
        """
        Set an RNG stream index from the contents of a classical register.

        The classical register must be exactly 32 bits. It encodes the index in little-endian binary (least significant bit first).

        :param creg: register of classical bits
        :param rng_wire: index of RNG wire
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`~.Circuit`
        """

    def _get_rng_num(self, creg: pytket._tket.unit_id.BitRegister, rng_wire: int, **kwargs: Any) -> Circuit:
        """
        Get RNG output into a classical register.

        The classical register must be exactly 32 bits. After the operation it encodes the output number in little-endian binary (least significant bit first).

        :param creg: register of classical bits
        :param rng_wire: index of RNG wire
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`~.Circuit`
        """

    def _get_job_shot_num(self, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Get shot number into a classical register.

        The classical register must be exactly 32 bits. After the operation it encodes the shot number in little-endian binary (least significant bit first).

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_setbits(self, values: Sequence[bool], args: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], **kwargs: Any) -> Circuit:
        """
        Appends an operation to set some bit values.

        :param values: values to set
        :param args: bits to set
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_setreg(self, value: int, arg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Set a classical register to an unsigned integer value. The little-endian bitwise representation of the integer is truncated to the register size, up to _TKET_REG_WIDTH bit width. It is zero-padded if the width of the register is greater than _TKET_REG_WIDTH.
        """

    def add_c_copybits(self, args_in: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], args_out: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], **kwargs: Any) -> Circuit:
        """
        Appends a classical copy operation

        :param args_in: source bits
        :param args_out: destination bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_copyreg(self, input_reg: pytket._tket.unit_id.BitRegister, output_reg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Copy a classical register to another. Copying is truncated to the size of the smaller of the two registers.
        """

    def add_c_predicate(self, values: Sequence[bool], args_in: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], arg_out: int | pytket._tket.unit_id.Bit, name: str = 'ExplicitPredicate', **kwargs: Any) -> Circuit:
        """
        :param name: operation name
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_modifier(self, values: Sequence[bool], args_in: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], arg_inout: int | pytket._tket.unit_id.Bit, name: str = 'ExplicitModifier', **kwargs: Any) -> Circuit:
        """
        :param name: operation name
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_and(self, arg0_in: int | pytket._tket.unit_id.Bit, arg1_in: int | pytket._tket.unit_id.Bit, arg_out: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a binary AND operation to the end of the circuit.

        :param arg0_in: first input bit
        :param arg1_in: second input bit
        :param arg_out: output bit
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_or(self, arg0_in: int | pytket._tket.unit_id.Bit, arg1_in: int | pytket._tket.unit_id.Bit, arg_out: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a binary OR operation to the end of the circuit.

        :param arg0_in: first input bit
        :param arg1_in: second input bit
        :param arg_out: output bit
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_xor(self, arg0_in: int | pytket._tket.unit_id.Bit, arg1_in: int | pytket._tket.unit_id.Bit, arg_out: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a binary XOR operation to the end of the circuit.

        :param arg0_in: first input bit
        :param arg1_in: second input bit
        :param arg_out: output bit
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_not(self, arg_in: int | pytket._tket.unit_id.Bit, arg_out: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a NOT operation to the end of the circuit.

        :param arg_in: input bit
        :param arg_out: output bit
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_range_predicate(self, minval: int, maxval: int, args_in: Sequence[int] | Sequence[pytket._tket.unit_id.Bit], arg_out: int | pytket._tket.unit_id.Bit, **kwargs: Any) -> Circuit:
        """
        Appends a range-predicate operation to the end of the circuit.

        :param minval: lower bound of input in little-endian encoding
        :param maxval: upper bound of input in little-endian encoding
        :param args_in: input bits
        :param arg_out: output bit (distinct from input bits)
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_and_to_registers(self, reg0_in: pytket._tket.unit_id.BitRegister, reg1_in: pytket._tket.unit_id.BitRegister, reg_out: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Applies bitwise AND to linear registers.

        The operation is applied to the bits with indices 0, 1, 2, ... in each register, up to the size of the smallest register.

        :param reg0_in: first input register
        :param reg1_in: second input register
        :param reg_out: output register
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_or_to_registers(self, reg0_in: pytket._tket.unit_id.BitRegister, reg1_in: pytket._tket.unit_id.BitRegister, reg_out: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Applies bitwise OR to linear registers.

        The operation is applied to the bits with indices 0, 1, 2, ... in each register, up to the size of the smallest register.

        :param reg0_in: first input register
        :param reg1_in: second input register
        :param reg_out: output register
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_xor_to_registers(self, reg0_in: pytket._tket.unit_id.BitRegister, reg1_in: pytket._tket.unit_id.BitRegister, reg_out: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Applies bitwise XOR to linear registers.

        The operation is applied to the bits with indices 0, 1, 2, ... in each register, up to the size of the smallest register.

        :param reg0_in: first input register
        :param reg1_in: second input register
        :param reg_out: output register
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def add_c_not_to_registers(self, reg_in: pytket._tket.unit_id.BitRegister, reg_out: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Applies bitwise NOT to linear registers.

        The operation is applied to the bits with indices 0, 1, 2, ... in each register, up to the size of the smallest register.

        :param reg_in: input register
        :param reg_out: name of output register
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` . Allowed parameters are `opgroup`,  `condition`, `condition_bits`, `condition_value`
        :return: the new :py:class:`~.Circuit`
        """

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

    def __iter__(self) -> Iterator[Command]:
        """Iterate through the circuit, a Command at a time."""

    def get_commands(self) -> list[Command]:
        """:return: a list of all the Commands in the circuit"""

    def get_unitary(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')]:
        """
        :return: The numerical unitary matrix of the circuit, using ILO-BE convention.
        """

    def get_unitary_times_other(self, matr: Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')]) -> Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')]:
        """
        Calculate UM, where U is the numerical unitary matrix of the circuit, with ILO-BE convention, and M is another matrix. This is more efficient than calculating U separately, if M has fewer columns than U.

        :param matr: The matrix to be multiplied.
        :return: The product of the circuit unitary and the given matrix.
        """

    def get_statevector(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None,), order='C')]:
        """
        Calculate the statevector resulting from applying the circuit to the all-zero state, using ILO-BE convention. The result is a one-dimensional array.

        :return: The calculated vector.
        """

    @overload
    def add_q_register(self, name: str, size: int) -> pytket._tket.unit_id.QubitRegister:
        """
        Constructs a new quantum register with a given name and number of qubits.

        :param name: Unique readable name for the register
        :param size: Number of qubits required
        :return: a map from index to the corresponding UnitIDs
        """

    @overload
    def add_q_register(self, register: pytket._tket.unit_id.QubitRegister) -> pytket._tket.unit_id.QubitRegister:
        """
        Adds QubitRegister to Circuit

        :param register: QubitRegister
        """

    def _add_w_register(self, size: int) -> None:
        """
        Creates given number of wasm bits in the circuit. If there are already wasm bits in circuit only the additional wasm bits will be added. 

        :param size: Number of wasm bits that should be added to the circuit
        """

    def _add_r_register(self, size: int) -> None:
        """
        Creates given number of RNG wires in the circuit. If there are already RNG wires in circuit, only the additional RNG wires will be added.

        :param size: Number of RNG wires that should be added to the circuit
        """

    @overload
    def add_c_register(self, name: str, size: int) -> pytket._tket.unit_id.BitRegister:
        """
        Constructs a new classical register with a given name and number of bits.

        :param name: Unique readable name for the register
        :param size: Number of bits required
        :return: a map from index to the corresponding UnitIDs
        """

    @overload
    def add_c_register(self, register: pytket._tket.unit_id.BitRegister) -> pytket._tket.unit_id.BitRegister:
        """
        Adds BitRegister to Circuit

        :param register: BitRegister
        """

    def get_c_register(self, name: str) -> pytket._tket.unit_id.BitRegister:
        """
        Get the classical register with the given name.

        :param name: name for the register
        :return: the retrieved :py:class:`~.BitRegister`
        """

    @property
    def c_registers(self) -> list[pytket._tket.unit_id.BitRegister]:
        """
        Get all classical registers.

        The list only includes registers that are singly-indexed contiguously from zero.

        :return: List of :py:class:`~.BitRegister`
        """

    def get_q_register(self, name: str) -> pytket._tket.unit_id.QubitRegister:
        """
        Get the quantum register with the given name.

        :param name: name for the register
        :return: the retrieved :py:class:`~.QubitRegister`
        """

    @property
    def q_registers(self) -> list[pytket._tket.unit_id.QubitRegister]:
        """
        Get all quantum registers.

        The list only includes registers that are singly-indexed contiguously from zero.

        :return: List of :py:class:`~.QubitRegister`
        """

    def add_qubit(self, id: pytket._tket.unit_id.Qubit, reject_dups: bool = True) -> None:
        """
        Constructs a single qubit with the given id.

        :param id: Unique id for the qubit
        :param reject_dups: Fail if there is already a qubit in this circuit with the id. Default to True
        """

    def add_bit(self, id: pytket._tket.unit_id.Bit, reject_dups: bool = True) -> None:
        """
        Constructs a single bit with the given id.

        :param id: Unique id for the bit
        :param reject_dups: Fail if there is already a bit in this circuit with the id. Default to True
        """

    @property
    def qubits(self) -> list[pytket._tket.unit_id.Qubit]:
        """A list of all qubit ids in the circuit"""

    @property
    def created_qubits(self) -> list[pytket._tket.unit_id.Qubit]:
        """A list of qubits whose input is a Create operation"""

    @property
    def discarded_qubits(self) -> list[pytket._tket.unit_id.Qubit]:
        """A list of qubits whose output is a Discard operation"""

    @property
    def bits(self) -> list[pytket._tket.unit_id.Bit]:
        """A list of all classical bit ids in the circuit"""

    @property
    def bit_readout(self) -> dict[pytket._tket.unit_id.Bit, int]:
        """
        A map from bit to its (left-to-right) index in readouts from backends (following the increasing lexicographic order convention)
        """

    @property
    def qubit_readout(self) -> dict[pytket._tket.unit_id.Qubit, int]:
        """
        A map from qubit to its (left-to-right) index in readouts from backends. A qubit will feature in this map if it is measured and neither it nor the bit containing the measurement result is subsequently acted on
        """

    @property
    def qubit_to_bit_map(self) -> dict[pytket._tket.unit_id.Qubit, pytket._tket.unit_id.Bit]:
        """
        A map from qubit to the bit it is measured to. A qubit will feature in this map if it is measured and neither it nor the bit containing the measurement result is subsequently acted on
        """

    @property
    def opgroups(self) -> set[str]:
        """A set of all opgroup names in the circuit"""

    def flatten_registers(self) -> dict[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID]:
        """
        Combines all qubits into a single register namespace with the default name, and likewise for bits
        """

    @overload
    def add_circuit(self, circuit: Circuit, qubits: Sequence[pytket._tket.unit_id.Qubit], bits: Sequence[pytket._tket.unit_id.Bit] = []) -> Circuit:
        """
        In-place sequential composition of circuits, appending a copy of the argument onto the end of the circuit. Connects qubits and bits with the same behaviour as :py:meth:`add_gate`.

        :param circuit: The circuit to be appended to the end of `self`
        :param qubits: List mapping the (default register) qubits of `circuit` to the qubits of `self`
        :param bits: List mapping the (default register) bits of `circuit` to the bits of `self`
        :return: the new :py:class:`~.Circuit`
        """

    @overload
    def add_circuit(self, circuit: Circuit, qubits: Sequence[int], bits: Sequence[int] = []) -> Circuit:
        """
        In-place sequential composition of circuits, appending a copy of the argument onto the end of the circuit. Connects qubits and bits with the same behaviour as :py:meth:`add_gate`.

        :param circuit: The circuit to be appended to the end of `self`
        :param qubits: List mapping the (default register) qubits of `circuit` to the (default register) qubits of `self`
        :param bits: List mapping the (default register) bits of `circuit` to the (default register) bits of `self`
        :return: the new :py:class:`~.Circuit`
        """

    def add_circuit_with_map(self, circuit: Circuit, unit_map: Mapping[pytket._tket.unit_id.UnitID, pytket._tket.unit_id.UnitID]) -> Circuit:
        """
        In-place sequential composition of circuits, appending a copy of the argument onto the end of the circuit.

        :param circuit: circuit to be appended
        :param unit_map: map from qubits and bits in the appended circuit to corresponding qubits and bits in `self`
        """

    def append(self, circuit: Circuit) -> None:
        """
        In-place sequential composition of circuits, appending a copy of the argument onto the end of the circuit. Inputs and Outputs are unified if they share the same id, defaulting to parallel composition if there is no match.

        :param circuit: The circuit to be appended to the end of `self`
        """

    def add_phase(self, a: Union[sympy.core.expr.Expr, float]) -> Circuit:
        """
        Add a global phase to the circuit.

        :param a: Phase to add, in halfturns

        :return: circuit with added phase
        """

    def _n_vertices(self) -> int:
        """
        :return: the number of vertices in the DAG, i.e. the sum of the number of operations, inputs, and outputs
        """

    @property
    def is_simple(self) -> bool:
        """
        Checks that the circuit has only 1 quantum and 1 classic register using the default names ('q' and 'c'). This means it is suitable to refer to qubits simply by their integer indices.
        """

    @property
    def n_gates(self) -> int:
        """The number of gates in the Circuit"""

    @property
    def wasm_uid(self) -> str | None:
        """The unique WASM UID of the circuit, or `None` if the circuit has none"""

    @property
    def n_qubits(self) -> int:
        """The number of qubits in the circuit"""

    @property
    def n_bits(self) -> int:
        """The number of classiclal bits in the circuit"""

    @property
    def phase(self) -> Union[sympy.core.expr.Expr, float]:
        """
        The global phase applied to the circuit, in halfturns (not meaningful for circuits with classical interactions)
        """

    @property
    def name(self) -> str | None: ...

    @name.setter
    def name(self, arg: str, /) -> None: ...

    def remove_blank_wires(self, keep_blank_classical_wires: bool = False, remove_classical_only_at_end_of_register: bool = True) -> None:
        """
        Removes any Input-Output pairs in the DAG with no intervening operations, i.e. removes untouched qubits/bits from the circuit. This may occur when optimisations recognise that the operations on a qubit reduce to the identity, or when routing adds wires to "fill out" the architecture. This operation will only remove empty classical wires if there are no used bits with a higher index in the same register. 

        :param keep_blank_classical_wires: if true, empty classical wires are not removed
        :param remove_classical_only_at_end_of_register: if true, empty classical wires are not removed if they don't belong to a register or there are any non-empty wires with a higher index in the same register
        """

    def add_blank_wires(self, number: int) -> None:
        """
        Adds a number of new qubits to the circuit. These will be added to the default register ('q') if possible, filling out the unused indices from 0.

        :param number: Number of qubits to add
        """

    def rename_units(self, map: Mapping[pytket._tket.unit_id.UnitID | pytket._tket.unit_id.Qubit | pytket._tket.unit_id.Bit, pytket._tket.unit_id.UnitID | pytket._tket.unit_id.Qubit | pytket._tket.unit_id.Bit]) -> bool:
        """
        Rename qubits and bits simultaneously according to the map of ids provided

        :param map: Dictionary from current ids to new ids
        """

    def depth(self) -> int:
        """
        Returns the number of interior vertices on the longest path through the DAG, excluding vertices representing barrier operations.

        >>> c = Circuit(3)
        >>> c.depth()
        0
        >>> c.CX(0,1)
        >>> c.CX(1,2)
        >>> c.CX(2,0)
        >>> c.depth()
        3

        :return: the circuit depth
        """

    def n_gates_of_type(self, type: OpType, include_conditional: bool = False) -> int:
        """
        Returns the number of vertices in the dag of a given operation type.

        >>> c.CX(0,1)
        >>> c.H(0)
        >>> c.CX(0,1)
        >>> c.n_gates_of_type(OpType.CX)
        2

        :param type: The operation type to search for

        :param include_conditional: if set to true, conditional gates will be counted, too

        :return: the number of operations matching `type`
        """

    def n_1qb_gates(self) -> int:
        """
        Returns the number of vertices in the dag with one quantum edge.Ignores Input, Create, Output, Discard, Reset, Measure and Barrier vertices.
        """

    def n_2qb_gates(self) -> int:
        """
        Returns the number of vertices in the dag with two quantum edges.Ignores Input, Create, Output, Discard, Reset, Measure and Barrier vertices.
        """

    def n_nqb_gates(self, size: int) -> int:
        """
        Returns the number of vertices in the dag with given number of  quantum edges.Ignores Input, Create, Output, Discard, Reset, Measure and Barrier vertices.
        """

    @overload
    def depth_by_type(self, type: OpType) -> int:
        """
        Returns the number of vertices in the longest path through the sub-DAG consisting of vertices representing operations of the given type.

        >>> c = Circuit(3)
        >>> c.CX(0,1)
        >>> c.Z(1)
        >>> c.CX(1,2)
        >>> c.depth_by_type(OpType.CX)
        2

        :param type: the operation type of interest
        :return: the circuit depth with respect to operations matching `type`
        """

    @overload
    def depth_by_type(self, types: Set[OpType]) -> int:
        """
        Returns the number of vertices in the longest path through the sub-DAG consisting of vertices representing operations of the given types.

        >>> c = Circuit(3)
        >>> c.CZ(0,1)
        >>> c.Z(1)
        >>> c.CX(1,2)
        >>> c.depth_by_type({OpType.CZ, OpType.CX})
        2

        :param types: the set of operation types of interest
        :return: the circuit depth with respect to operations matching an element of `types`
        """

    def depth_2q(self) -> int:
        """
        Returns the number of vertices in the longest path through the sub-DAG consisting of vertices with 2 quantum wires,excluding vertices representing barrier operations.

        >>> c = Circuit(3)
        >>> c.CZ(0,1)
        >>> c.Z(0)
        >>> c.Z(1)
        >>> c.ZZMax(1,2)
        >>> c.CX(1,2)
        >>> c.depth_2q()
        3
        :return: the circuit depth with respect to 2-qubit operations.
        """

    def to_dict(self) -> dict:
        """:return: a JSON serializable dictionary representation of the Circuit"""

    def to_json(self) -> str:
        """:return: a JSON serialized UTF-8 string representation of the Circuit"""

    @staticmethod
    def from_dict(arg: dict, /) -> Circuit:
        """
        Construct Circuit instance from JSON serializable dictionary representation of the Circuit.
        """

    @staticmethod
    def from_json(arg: str, /) -> Circuit:
        """
        Construct Circuit instance from JSON serialized UTF-8 string representation of the Circuit.
        """

    def __getstate__(self) -> tuple: ...

    def __setstate__(self, arg: tuple, /) -> None: ...

    def to_latex_file(self, filename: str) -> None:
        """
        Produces a latex file with a visualisation of the circuit using the Quantikz package.

        :param filename: Name of file to write output to (must end in ".tex")
        """

    def __rshift__(self, arg: Circuit, /) -> Circuit:
        """
        Creates a new Circuit, corresponding to the sequential composition of the given Circuits. Any qubits/bits with the same ids will be unified. Any ids without a match will be added in parallel.
        """

    def __mul__(self, arg: Circuit, /) -> Circuit:
        """
        Creates a new Circuit, corresponding to the parallel composition of the given Circuits. This will fail if the circuits share qubits/bits with the same ids.
        """

    def dagger(self) -> Circuit:
        """
        Given a pure circuit (i.e. without any measurements or conditional gates), produces a new circuit for the inverse/adjoint operation.

        :return: a new :py:class:`~.Circuit` corresponding to the inverse operation
        """

    def transpose(self) -> Circuit:
        """
        Given a pure circuit (i.e. without any measurements or conditional gates), produces a new circuit for the transpose operation.

        :return: a new :py:class:`~.Circuit` corresponding to the transpose operation
        """

    def copy(self) -> Circuit:
        """:return: an identical copy of the circuit"""

    def symbol_substitution(self, symbol_map: Mapping[sympy.core.symbol.Symbol, Union[sympy.core.expr.Expr, float]]) -> None:
        """
        In-place substitution for symbolic expressions; iterates through each parameterised gate/box and performs the substitution. 

        :param symbol_map: A map from SymPy symbols to SymPy expressions or floats
        """

    def free_symbols(self) -> set[sympy.core.symbol.Symbol]:
        """:return: set of symbolic parameters in the circuit"""

    def is_symbolic(self) -> bool:
        """
        :return: True if the circuit contains any free symbols, False otherwise.
        """

    @overload
    def substitute_named(self, op: Op, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given op.The replacement operations retain the same name.

        :param op: the replacement operation
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, repl: Circuit, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given circuit.Named operations in the replacement circuit must not match any named operations in the circuit being modified.

        :param repl: the replacement circuit
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: CircBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement CircBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: Unitary1qBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement Unitary1qBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: Unitary2qBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement Unitary2qBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: Unitary3qBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement Unitary3qBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: ExpBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement ExpBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: PauliExpBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement PauliExpBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: ToffoliBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement ToffoliBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: DummyBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement DummyBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: QControlBox, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement QControlBox
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    @overload
    def substitute_named(self, box: CustomGate, opgroup: str) -> bool:
        """
        Substitute all ops with the given name for the given box.The replacement boxes retain the same name.

        :param box: the replacement CustomGate
        :param opgroup: the name of the operations group to replace
        :return: whether any replacements were made
        """

    def qubit_create(self, arg: pytket._tket.unit_id.Qubit, /) -> None:
        """Make a quantum input a Create operation (initialized to 0)"""

    def qubit_discard(self, arg: pytket._tket.unit_id.Qubit, /) -> None:
        """Make a quantum output a Discard operation"""

    def qubit_create_all(self) -> None:
        """Make all quantum inputs Create operations (initialized to 0)"""

    def qubit_discard_all(self) -> None:
        """Make all quantum outputs Discard operations"""

    def qubit_is_created(self, arg: pytket._tket.unit_id.Qubit, /) -> bool:
        """Query whether a qubit has its initial state set to zero"""

    def qubit_is_discarded(self, arg: pytket._tket.unit_id.Qubit, /) -> bool:
        """Query whether a qubit has its final state discarded"""

    def _classical_eval(self, arg: Mapping[pytket._tket.unit_id.Bit, bool], /) -> dict[pytket._tket.unit_id.Bit, bool]: ...

    def valid_connectivity(self, arch: pytket._tket.architecture.Architecture, directed: bool, allow_bridge: bool = False) -> bool:
        """
        Confirms whether all two qubit gates in given circuit are along some edge of the architecture.

        :param arch: The architecture capturing the desired connectivity
        :param directed: If true, also checks that CX or ECR gates are in the same direction as the edges of the architecture
        :param allow_bridge: Accept BRIDGEs as valid, assuming the middle qubit neighbours the others

        :return: True or False
        """

    def implicit_qubit_permutation(self) -> dict[pytket._tket.unit_id.Qubit, pytket._tket.unit_id.Qubit]:
        """
        :return: dictionary mapping input qubit to output qubit on the same path
        """

    @property
    def has_implicit_wireswaps(self) -> bool:
        """
        Indicates whether the circuit has a non-trivial qubit permutation (i.e., whether there are any implicit wire swaps).
        """

    def replace_SWAPs(self, replace_tk2_equivalents: bool = False) -> bool:
        """
        Replace all SWAP gates with implicit wire swaps.

        :param replace_tk2_equivalents: If True, TK2 gates that are equivalent to SWAPs will also be replaced. Defaults to False.

        :return: whether any SWAP gates were replaced
        """

    def replace_implicit_wire_swaps(self) -> None:
        """Replace all implicit wire swaps with SWAP gates."""

    def ops_of_type(self, optype: OpType) -> list[Op]:
        """
        Get all operations in the circuit of a given type.

        The order is not guaranteed.

        :param optype: operation type

        :return: list of :py:class:`~.Op`
        """

    def commands_of_type(self, optype: OpType) -> list[Command]:
        """
        Get all commands in a circuit of a given type.

        The order is consistent with the causal order of the operations in the circuit.

        :param optype: operation type

        :return: list of :py:class:`~.Command`
        """

    def get_resources(self) -> ResourceData:
        """
        Calculate the overall resources of the circuit.

        This takes account of the data stored in each py:class:`DummyBox` within the circuit, as well as other gates, to compute upper and lower bounds.

        :return: bounds on resources of the circuit

        >>> resource_data0 = ResourceData(
        ...     op_type_count={
        ...         OpType.T: ResourceBounds(1, 2),
        ...         OpType.H: ResourceBounds(0, 1),
        ...         OpType.CX: ResourceBounds(1, 2),
        ...         OpType.CZ: ResourceBounds(3, 3),
        ...     },
        ...     gate_depth=ResourceBounds(5, 8),
        ...     op_type_depth={
        ...         OpType.T: ResourceBounds(0, 10),
        ...         OpType.H: ResourceBounds(0, 10),
        ...         OpType.CX: ResourceBounds(1, 2),
        ...         OpType.CZ: ResourceBounds(3, 3),
        ...     },
        ...     two_qubit_gate_depth=ResourceBounds(4, 5),
        ... )
        >>> dbox0 = DummyBox(n_qubits=2, n_bits=0, resource_data=resource_data0)
        >>> resource_data1 = ResourceData(
        ...     op_type_count={
        ...         OpType.T: ResourceBounds(2, 2),
        ...         OpType.H: ResourceBounds(1, 1),
        ...         OpType.CX: ResourceBounds(2, 3),
        ...         OpType.CZ: ResourceBounds(3, 5),
        ...     },
        ...     gate_depth=ResourceBounds(5, 10),
        ...     op_type_depth={
        ...         OpType.T: ResourceBounds(1, 2),
        ...         OpType.H: ResourceBounds(2, 4),
        ...         OpType.CX: ResourceBounds(1, 1),
        ...         OpType.CZ: ResourceBounds(3, 4),
        ...     },
        ...     two_qubit_gate_depth=ResourceBounds(3, 5),
        ... )
        >>> dbox1 = DummyBox(n_qubits=3, n_bits=0, resource_data=resource_data1)
        >>> c = (
        ...     Circuit(3)
        ...     .H(0)
        ...     .CX(1, 2)
        ...     .CX(0, 1)
        ...     .T(2)
        ...     .H(1)
        ...     .add_dummybox(dbox0, [0, 1], [])
        ...     .CZ(1, 2)
        ...     .add_dummybox(dbox1, [0, 1, 2], [])
        ...     .H(2)
        ... )
        >>> resource_data = c.get_resources()
        >>> print(resource_data)
        ResourceData(op_type_count={OpType.T: ResourceBounds(4, 5), OpType.H: ResourceBounds(4, 5), OpType.CX: ResourceBounds(5, 7), OpType.CZ: ResourceBounds(7, 9), }, gate_depth=ResourceBounds(15, 23), op_type_depth={OpType.T: ResourceBounds(2, 12), OpType.H: ResourceBounds(5, 17), OpType.CX: ResourceBounds(4, 5), OpType.CZ: ResourceBounds(7, 8), }, two_qubit_gate_depth=ResourceBounds(10, 13))
        """

    @property
    def _dag_data(self) -> tuple[set[int], set[int], set[int], set[int], set[int], set[int], set[int], set[int], dict[int, str], dict[int, str], dict[int, str], set[tuple[int, int, int, int, str]]]:
        """DAG data for circuit"""

    def add_wasm(self: Circuit, funcname: str, filehandler: pytket.wasm.wasm.WasmModuleHandler, list_i: Sequence[int], list_o: Sequence[int], args: Union[Sequence[int], Sequence[pytket._tket.unit_id.Bit]], args_wasm: Sequence[int] | None = None, **kwargs: Any) -> Circuit:
        """
        Add a classical function call from a wasm file to the circuit.

        :param funcname: name of the function that is called
        :param filehandler: wasm file or module handler to identify the wasm module
        :param list_i: list of the number of bits in the input variables
        :param list_o: list of the number of bits in the output variables
        :param args: vector of circuit bits the wasm op should be added to
        :param args_wasm: vector of wasmstates the wasm op should be added to
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate`.
         Allowed parameters are `opgroup`,  `condition`, `condition_bits`,
         `condition_value`
        :return: the new :py:class:`Circuit`
        """

    def add_wasm_to_reg(self: Circuit, funcname: str, filehandler: pytket.wasm.wasm.WasmModuleHandler, list_i: Sequence[pytket._tket.unit_id.BitRegister], list_o: Sequence[pytket._tket.unit_id.BitRegister], args_wasm: Sequence[int] | None = None, **kwargs: Any) -> Circuit:
        """
        Add a classical function call from a wasm file to the circuit.

        :param funcname: name of the function that is called
        :param filehandler: wasm file or module handler to identify the wasm module
        :param list_i: list of the classical registers assigned to
         the input variables of the function call
        :param list_o: list of the classical registers assigned to
         the output variables of the function call
        :param args_wasm: vector of wasmstates the wasm op should be added to
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate`.
         Allowed parameters are `opgroup`,  `condition`, `condition_bits`,
         `condition_value`
        :return: the new :py:class:`Circuit`
        """

    def set_rng_seed(self: Circuit, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Seed an RNG from the contents of a classical register.

        The classical register must be exactly 64 bits.

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
         are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`Circuit`
        """

    def set_rng_bound(self: Circuit, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Set an RNG upper bound from the contents of a classical register.

        The classical register must be exactly 32 bits. It encodes the upper bound in
        little-endian binary (least significant bit first). The bound is inclusive.

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
            are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`Circuit`
        """

    def set_rng_index(self: Circuit, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Set an RNG stream index from the contents of a classical register.

        The classical register must be exactly 32 bits. It encodes the index in little-
        endian binary (least significant bit first).

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
            are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`Circuit`
        """

    def get_rng_num(self: Circuit, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Get RNG output into a classical register.

        The classical register must be exactly 32 bits. After the operation it encodes the
        output number in little-endian binary (least significant bit first).

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
            are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`Circuit`
        """

    def get_job_shot_num(self: Circuit, creg: pytket._tket.unit_id.BitRegister, **kwargs: Any) -> Circuit:
        """
        Get shot number into a classical register.

        The classical register must be exactly 32 bits. After the operation it encodes the
        shot number in little-endian binary (least significant bit first).

        :param creg: register of classical bits
        :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
            are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
        :return: the new :py:class:`Circuit`
        """

    def add_clexpr_from_logicexp(circ: Circuit, exp: pytket.circuit.logic_exp.LogicExp, output_bits: list[pytket._tket.unit_id.Bit], **kwargs: Any) -> Circuit:
        """
        Append a :py:class:`~.ClExprOp` defined in terms of a logical expression.

        Example:

        >>> c = Circuit()
        >>> x_reg = c.add_c_register('x', 3)
        >>> y_reg = c.add_c_register('y', 3)
        >>> z_reg = c.add_c_register('z', 3)
        >>> c.add_clexpr_from_logicexp(x_reg | y_reg, z_reg.to_list())
        [ClExpr x[0], x[1], x[2], y[0], y[1], y[2], z[0], z[1], z[2]; ]

        :param exp: logical expression
        :param output_bits: list of bits in output
        :return: the updated circuit
        """

class CircBox(Op):
    """A user-defined operation specified by a :py:class:`~.Circuit`."""

    def __init__(self, circ: Circuit) -> None:
        """Construct from a :py:class:`~.Circuit`."""

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def symbol_substitution(self, symbol_map: Mapping[sympy.core.symbol.Symbol, Union[sympy.core.expr.Expr, float]]) -> None:
        """
        In-place substitution of symbolic expressions within underlying circuit; iterates through each parameterised gate/box within the circuit and performs the substitution. 

         WARNING: This method potentially mutates the CircBox and any changes are propagated to any Circuit that the CircBox has been added to (via Circuit.add_circbox). 

        :param symbol_map: A map from SymPy symbols to SymPy expressions
        """

    @property
    def circuit_name(self) -> str | None:
        """
        The name of the contained circuit. 

         WARNING: Setting this property mutates the CircBox and any changes are propagated to any Circuit that the CircBox has been added to (via Circuit.add_circbox).
        """

    @circuit_name.setter
    def circuit_name(self, arg: str, /) -> None: ...

class Unitary1qBox(Op):
    """A user-defined one-qubit operation specified by a unitary matrix."""

    def __init__(self, m: Annotated[NDArray[numpy.complex128], dict(shape=(2, 2), order='F')]) -> None:
        """Construct from a unitary matrix."""

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_matrix(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(2, 2), order='F')]:
        """:return: the unitary matrix as a numpy array"""

class Unitary2qBox(Op):
    """A user-defined two-qubit operation specified by a unitary matrix."""

    def __init__(self, m: Annotated[NDArray[numpy.complex128], dict(shape=(4, 4), order='F')], basis: BasisOrder = BasisOrder.ilo) -> None:
        """
        Construct from a unitary matrix.

        :param m: The unitary matrix
        :param basis: Whether the provided unitary is in the ILO-BE (increasing lexicographic order of qubit ids, big-endian indexing) format, or DLO-BE (decreasing lexicographic order of ids)
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_matrix(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(4, 4), order='F')]:
        """:return: the unitary matrix (in ILO-BE format) as a numpy array"""

class Unitary3qBox(Op):
    """A user-defined three-qubit operation specified by a unitary matrix."""

    def __init__(self, m: Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')], basis: BasisOrder = BasisOrder.ilo) -> None:
        """
        Construct from a unitary matrix.

        :param m: The unitary matrix
        :param basis: Whether the provided unitary is in the ILO-BE (increasing lexicographic order of qubit ids, big-endian indexing) format, or DLO-BE (decreasing lexicographic order of ids)
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_matrix(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(8, 8), order='F')]:
        """:return: the unitary matrix (in ILO-BE format) as a numpy array"""

class ExpBox(Op):
    """
    A user-defined two-qubit operation whose corresponding unitary matrix is the exponential of a user-defined hermitian matrix.
    """

    def __init__(self, A: Annotated[NDArray[numpy.complex128], dict(shape=(4, 4), order='F')], t: float, basis: BasisOrder = BasisOrder.ilo) -> None:
        """
        Construct :math:`e^{itA}` from a hermitian matrix :math:`A` and a parameter :math:`t`.

        :param A: A hermitian matrix
        :param t: Exponentiation parameter
        :param basis: Whether the provided matrix is in the ILO-BE (increasing lexicographic order of qubit ids, big-endian indexing) format, or DLO-BE (decreasing lexicographic order of ids)
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

class PauliExpBox(Op):
    """
    An operation defined as the exponential of a tensor of Pauli operations and a (possibly symbolic) phase parameter.
    """

    def __init__(self, paulis: Sequence[pytket._tket.pauli.Pauli], t: Union[sympy.core.expr.Expr, float], cx_config_type: CXConfigType = CXConfigType.Tree) -> None:
        r"""
        Construct :math:`e^{-\frac12 i \pi t \sigma_0 \otimes \sigma_1 \otimes \cdots}` from Pauli operators :math:`\sigma_i \in \{I,X,Y,Z\}` and a parameter :math:`t`.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_paulis(self) -> list[pytket._tket.pauli.Pauli]:
        """:return: the corresponding list of :py:class:`~.Pauli` s"""

    def get_phase(self) -> Union[sympy.core.expr.Expr, float]:
        """:return: the corresponding phase parameter"""

    def get_cx_config(self) -> CXConfigType:
        """:return: decomposition method"""

class PauliExpPairBox(Op):
    """
    A pair of (not necessarily commuting) Pauli exponentials performed in sequence.
    Pairing up exponentials for synthesis can reduce gate costs of synthesis compared to synthesising individually, with the best reductions found when the Pauli tensors act on a large number of the same qubits.
    Phase parameters may be symbolic.
    """

    def __init__(self, paulis0: Sequence[pytket._tket.pauli.Pauli], t0: Union[sympy.core.expr.Expr, float], paulis1: Sequence[pytket._tket.pauli.Pauli], t1: Union[sympy.core.expr.Expr, float], cx_config_type: CXConfigType = CXConfigType.Tree) -> None:
        r"""
        Construct a pair of Pauli exponentials of the form :math:`e^{-\frac12 i \pi t_j \sigma_0 \otimes \sigma_1 \otimes \cdots}` from Pauli operator strings :math:`\sigma_i \in \{I,X,Y,Z\}` and parameters :math:`t_j, j \in \{0,1\}`.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_paulis_pair(self) -> tuple[list[pytket._tket.pauli.Pauli], list[pytket._tket.pauli.Pauli]]:
        """
        :return: A tuple containing the two corresponding lists of :py:class:`~.Pauli` s
        """

    def get_phase_pair(self) -> tuple[Union[sympy.core.expr.Expr, float], Union[sympy.core.expr.Expr, float]]:
        """:return: A tuple containing the two phase parameters"""

    def get_cx_config(self) -> CXConfigType:
        """:return: decomposition method"""

class PauliExpCommutingSetBox(Op):
    """
    An operation defined as a set of commuting of exponentials of atensor of Pauli operations and their (possibly symbolic) phase parameters.
    """

    def __init__(self, pauli_gadgets: Sequence[tuple[Sequence[pytket._tket.pauli.Pauli], Union[sympy.core.expr.Expr, float]]], cx_config_type: CXConfigType = CXConfigType.Tree) -> None:
        r"""
        Construct a set of necessarily commuting Pauli exponentials of the form :math:`e^{-\frac12 i \pi t_j \sigma_0 \otimes \sigma_1 \otimes \cdots}` from Pauli operator strings :math:`\sigma_i \in \{I,X,Y,Z\}` and parameters :math:`t_j, j \in \{0, 1, \cdots \}`.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_paulis(self) -> list[tuple[list[pytket._tket.pauli.Pauli], Union[sympy.core.expr.Expr, float]]]:
        """:return: the corresponding list of Pauli gadgets"""

    def get_cx_config(self) -> CXConfigType:
        """:return: decomposition method"""

class TermSequenceBox(Op):
    """
    An unordered collection of Pauli exponentials that can be synthesised in any order, causing a change in the unitary operation. Synthesis order depends on the synthesis strategy chosen only.

     WARNING: Global phase is not preserved when using PauliSynthStrat.Greedy.
    """

    def __init__(self, pauli_gadgets: Sequence[tuple[Sequence[pytket._tket.pauli.Pauli], Union[sympy.core.expr.Expr, float]]], synthesis_strategy: pytket._tket.transform.PauliSynthStrat = pytket._tket.transform.PauliSynthStrat.Sets, partitioning_strategy: pytket._tket.partition.PauliPartitionStrat = pytket._tket.partition.PauliPartitionStrat.CommutingSets, graph_colouring: pytket._tket.partition.GraphColourMethod = pytket._tket.partition.GraphColourMethod.Lazy, cx_config_type: CXConfigType = CXConfigType.Tree, depth_weight: float = 0.3) -> None:
        r"""
        Construct a set of Pauli exponentials of the form :math:`e^{-\frac12 i \pi t_j \sigma_0 \otimes \sigma_1 \otimes \cdots}` from Pauli operator strings :math:`\sigma_i \in \{I,X,Y,Z\}` and parameters :math:`t_j, j \in \{0, 1, \cdots \}`.
        `depth_weight` controls the degree of depth optimisation and only applies to synthesis_strategy `PauliSynthStrat:Greedy`. `partitioning_strategy`, `graph_colouring`, and `cx_config_type` have no effect if `PauliSynthStrat.Greedy` is used.

         WARNING: Global phase is not preserved when using PauliSynthStrat.Greedy.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_paulis(self) -> list[tuple[list[pytket._tket.pauli.Pauli], Union[sympy.core.expr.Expr, float]]]:
        """:return: the corresponding list of Pauli gadgets"""

    def get_synthesis_strategy(self) -> pytket._tket.transform.PauliSynthStrat:
        """:return: synthesis strategy"""

    def get_partition_strategy(self) -> pytket._tket.partition.PauliPartitionStrat:
        """:return: partitioning strategy"""

    def get_graph_colouring_method(self) -> pytket._tket.partition.GraphColourMethod:
        """:return: graph colouring method"""

    def get_cx_config(self) -> CXConfigType:
        """:return: cx decomposition method"""

    def get_depth_weight(self) -> float:
        """:return: depth tuning parameter"""

class ToffoliBoxSynthStrat(enum.Enum):
    """Enum strategies for synthesising ToffoliBoxes"""

    Matching = 0
    """Use multiplexors to perform parallel swaps on hypercubes"""

    Cycle = 1
    """Use CnX gates to perform transpositions"""

class ToffoliBox(Op):
    """
    An operation that constructs a circuit to implement the specified permutation of classical basis states.
    """

    @overload
    def __init__(self, permutation: Sequence[tuple[Sequence[bool], Sequence[bool]]], strat: ToffoliBoxSynthStrat, rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """
        Construct from a permutation of basis states

        :param permutation: a list of bitstring pairs
        :param strat: synthesis strategy
        :param rotation_axis: the rotation axis of the multiplexors used in the decomposition. Can be either Rx or Ry. Only applicable to the Matching strategy. Default to Ry.
        """

    @overload
    def __init__(self, permutation: Sequence[tuple[Sequence[bool], Sequence[bool]]], rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """
        Construct from a permutation of basis states and perform synthesis using the Matching strategy

        :param permutation: a list of bitstring pairs
        :param rotation_axis: the rotation axis of the multiplexors used in the decomposition. Can be either Rx or Ry, default to Ry.
        """

    @overload
    def __init__(self, n_qubits: int, permutation: Sequence[tuple[Sequence[bool], Sequence[bool]]], rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """Constructor for backward compatibility. Subject to deprecation."""

    @overload
    def __init__(self, permutation: dict[tuple[bool, ...], Sequence[bool]], strat: ToffoliBoxSynthStrat, rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """
        Construct from a permutation of basis states

        :param permutation: a map between bitstrings
        :param strat: synthesis strategy
        :param rotation_axis: the rotation axis of the multiplexors used in the decomposition. Can be either Rx or Ry. Only applicable to the Matching strategy. Default to Ry.
        """

    @overload
    def __init__(self, permutation: dict[tuple[bool, ...], Sequence[bool]], rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """
        Construct from a permutation of basis states and perform synthesis using the Matching strategy

        :param permutation: a map between bitstrings
        :param rotation_axis: the rotation axis of the multiplexors used in the decomposition. Can be either Rx or Ry, default to Ry.
        """

    @overload
    def __init__(self, n_qubits: int, permutation: dict[tuple[bool, ...], Sequence[bool]], rotation_axis: OpType = pytket._tket.circuit.OpType.Ry) -> None:
        """Constructor for backward compatibility. Subject to deprecation."""

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_permutation(self) -> dict[tuple, tuple]:
        """:return: the permutation"""

    def get_strat(self) -> ToffoliBoxSynthStrat:
        """:return: the synthesis strategy"""

    def get_rotation_axis(self) -> OpType:
        """:return: the rotation axis"""

class ResourceBounds:
    """
    Structure holding a minimum and maximum value of some resource, where both values are unsigned integers.
    """

    def __init__(self, min: int, max: int) -> None:
        """
        Constructs a ResourceBounds object.

        :param min: minimum value
        :param max: maximum value
        """

    def get_min(self) -> int:
        """:return: the minimum value"""

    def get_max(self) -> int:
        """:return: the maximum value"""

class ResourceData:
    """
    An object holding resource data for use in a :py:class:`~.DummyBox`.

    The object holds several fields representing minimum and maximum values for certain resources. The absence of an :py:class:`~.OpType` in one of these fields is interpreted as the absence of gates of that type in the (imagined) circuit.

    See :py:meth:`~.Circuit.get_resources` for how to use this data.
    """

    def __init__(self, op_type_count: Mapping[OpType, ResourceBounds], gate_depth: ResourceBounds, op_type_depth: Mapping[OpType, ResourceBounds], two_qubit_gate_depth: ResourceBounds) -> None:
        """
        Constructs a ResourceData object.

        :param op_type_count: dictionary of counts of selected :py:class:`~.OpType`
        :param gate_depth: overall gate depth
        :param op_type_depth: dictionary of depths of selected :py:class:`~.OpType`
        :param two_qubit_gate_depth: overall two-qubit-gate depth
        """

    def get_op_type_count(self) -> dict[OpType, ResourceBounds]:
        """:return: bounds on the op type count"""

    def get_gate_depth(self) -> ResourceBounds:
        """:return: bounds on the gate depth"""

    def get_op_type_depth(self) -> dict[OpType, ResourceBounds]:
        """:return: bounds on the op type depth"""

    def get_two_qubit_gate_depth(self) -> ResourceBounds:
        """:return: bounds on the two-qubit-gate depth"""

    def __repr__(self) -> str: ...

class DummyBox(Op):
    """
    A placeholder operation that holds resource data. This box type cannot be decomposed into a circuit. It only serves to record resource data for a region of a circuit: for example, upper and lower bounds on gate counts and depth. A circuit containing such a box cannot be executed.
    """

    def __init__(self, n_qubits: int, n_bits: int, resource_data: ResourceData) -> None:
        """Construct a new instance from some resource data."""

    def get_n_qubits(self) -> int:
        """:return: the number of qubits covered by the box"""

    def get_n_bits(self) -> int:
        """:return: the number of bits covered by the box"""

    def get_resource_data(self) -> ResourceData:
        """:return: the associated resource data"""

class QControlBox(Op):
    """
    A user-defined controlled operation specified by an :py:class:`~.Op`, the number of quantum controls, and the control state expressed as an integer or a bit vector.
    """

    @overload
    def __init__(self, op: Op, n_controls: int = 1, control_state: Sequence[bool] = []) -> None:
        """
        Construct from an :py:class:`~.Op`, a number of quantum controls, and the control state expressed as a bit vector. The controls occupy the low-index ports of the resulting operation.

        :param op: the underlying operator
        :param n_controls: the number of control qubits. Default to 1
        :param control_state: the control state expressed as a bit vector. Default to all 1s
        """

    @overload
    def __init__(self, op: Op, n_controls: int, control_state: int) -> None:
        """
        Construct from an :py:class:`~.Op`, a number of quantum controls, and the control state expressed as an integer. The controls occupy the low-index ports of the resulting operation.

        :param op: the underlying operator
        :param n_controls: the number of control qubits
        :param control_state: the control state expressed as an integer. Big-endian
        """

    @overload
    def __init__(self, op: Op, n: int = 1) -> None:
        """
        Construct from an :py:class:`~.Op` and a number of quantum controls. The controls occupy the low-index ports of the resulting operation.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_op(self) -> Op:
        """:return: the underlying operator"""

    def get_n_controls(self) -> int:
        """:return: the number of control qubits"""

    def get_control_state(self) -> int:
        """
        :return: the control state as an integer (big-endian binary representation)
        """

    def get_control_state_bits(self) -> list[bool]:
        """:return: the control state as a bit vector"""

class CustomGateDef:
    """
    A custom unitary gate definition, given as a composition of other gates
    """

    def __init__(self, arg0: str, arg1: Circuit, arg2: Sequence[sympy.core.symbol.Symbol], /) -> None: ...

    @staticmethod
    def define(name: str, circ: Circuit, args: Sequence[sympy.core.symbol.Symbol]) -> CustomGateDef:
        """
        Define a new custom gate as a composite of other gates

        :param name: Readable name for the new gate
        :param circ: The definition of the gate as a Circuit
        :param args: Symbols to be encapsulated as arguments of the custom gate
        """

    @property
    def name(self) -> str:
        """The readable name of the gate"""

    @property
    def definition(self) -> Circuit:
        """Return definition as a circuit."""

    @property
    def args(self) -> list[sympy.core.symbol.Symbol]:
        """Return symbolic arguments of gate."""

    @property
    def arity(self) -> int:
        """The number of real parameters for the gate"""

    def to_dict(self) -> dict:
        """
        :return: a JSON serializable dictionary representation of the CustomGateDef
        """

    @staticmethod
    def from_dict(arg: dict, /) -> CustomGateDef:
        """
        Construct Circuit instance from JSON serializable dictionary representation of the Circuit.
        """

class CustomGate(Op):
    """A user-defined gate defined by a parametrised :py:class:`~.Circuit`."""

    def __init__(self, gatedef: CustomGateDef, params: Sequence[Union[sympy.core.expr.Expr, float]]) -> None:
        """Instantiate a custom gate."""

    @property
    def name(self) -> str:
        """The readable name of the gate."""

    @property
    def params(self) -> list[Union[sympy.core.expr.Expr, float]]:
        """The parameters of the gate."""

    @property
    def gate(self) -> CustomGateDef:
        """Underlying gate object."""

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the gate."""

class PhasePolyBox(Op):
    """
    Box encapsulating any Circuit made up of CNOT and RZ as a phase polynomial + linear transformation
    """

    @overload
    def __init__(self, n_qubits: int, qubit_indices: Mapping[pytket._tket.unit_id.Qubit, int], phase_polynomial: dict[tuple[bool, ...], Union[sympy.core.expr.Expr, float]], linear_transformation: Annotated[NDArray[numpy.bool_], dict(shape=(None, None), order='F')]) -> None:
        """
        Construct from the number of qubits, the mapping from Qubit to index, the phase polynomial (map from bitstring to phase) and the linear transformation (boolean matrix)
        """

    @overload
    def __init__(self, n_qubits: int, qubit_indices: Mapping[pytket._tket.unit_id.Qubit, int], phase_polynomial: Sequence[tuple[Sequence[bool], Union[sympy.core.expr.Expr, float]]], linear_transformation: Annotated[NDArray[numpy.bool_], dict(shape=(None, None), order='F')]) -> None:
        """
        Construct from the number of qubits, the mapping from Qubit to index, the phase polynomial (list of bitstring phase pairs) and the linear transformation (boolean matrix)

        If any bitstring is repeated in the phase polynomial list, the last given value for that bistring will be used
        """

    @overload
    def __init__(self, circuit: Circuit) -> None:
        """
        Construct a PhasePolyBox from a given circuit containing only Rz and CX gates.
        """

    @property
    def n_qubits(self) -> int:
        """Number of gates the polynomial acts on."""

    @property
    def phase_polynomial_as_list(self) -> list[tuple[list[bool], Union[sympy.core.expr.Expr, float]]]:
        """List of bitstring(basis state)-phase pairs."""

    @property
    def phase_polynomial(self) -> dict[tuple, Union[sympy.core.expr.Expr, float]]:
        """Map from bitstring (basis state) to phase."""

    @property
    def linear_transformation(self) -> Annotated[NDArray[numpy.bool_], dict(shape=(None, None), order='F')]:
        """Boolean matrix corresponding to linear transformation."""

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box."""

    @property
    def qubit_indices(self) -> dict[pytket._tket.unit_id.Qubit, int]:
        """Map from Qubit to index in polynomial."""

class ProjectorAssertionBox(Op):
    """
    A user-defined assertion specified by a 2x2, 4x4, or 8x8 projector matrix.
    """

    def __init__(self, m: Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')], basis: BasisOrder = BasisOrder.ilo) -> None:
        """
        Construct from a projector matrix.

        :param m: The projector matrix
        :param basis: Whether the provided unitary is in the ILO-BE (increasing lexicographic order of qubit ids, big-endian indexing) format, or DLO-BE (decreasing lexicographic order of ids)
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_matrix(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None, None), order='F')]:
        """:return: the unitary matrix (in ILO-BE format) as a numpy array"""

class StabiliserAssertionBox(Op):
    """A user-defined assertion specified by a list of Pauli stabilisers."""

    @overload
    def __init__(self, stabilisers: Sequence[pytket._tket.pauli.PauliStabiliser]) -> None:
        """
        Construct from a list of Pauli stabilisers.

        :param stabilisers: The list of Pauli stabilisers
        """

    @overload
    def __init__(self, stabilisers: Sequence[str]) -> None:
        """
        Construct from a list of Pauli stabilisers.

        :param m: The list of Pauli stabilisers expressed as Python strings
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_stabilisers(self) -> list[pytket._tket.pauli.PauliStabiliser]:
        """:return: the list of Pauli stabilisers"""

class MultiplexorBox(Op):
    """
    A user-defined multiplexor (i.e. uniformly controlled operations) specified by a map from bitstrings to :py:class:`~.Op` sor a list of bitstring-:py:class:`~.Op` s pairs
    """

    @overload
    def __init__(self, bistring_to_op_list: Sequence[tuple[Sequence[bool], Op]]) -> None:
        """
        Construct from a list of bitstring-:py:class:`~.Op` spairs

        :param bitstring_to_op_list: List of bitstring-:py:class:`~.Op` spairs
        """

    @overload
    def __init__(self, op_map: dict[tuple[bool, ...], Op]) -> None:
        """
        Construct from a map from bitstrings to :py:class:`~.Op` s

        :param op_map: Map from bitstrings to :py:class:`~.Op` s
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_op_map(self) -> dict[tuple, Op]:
        """:return: the underlying op map"""

    def get_bitstring_op_pair_list(self) -> list[tuple[list[bool], Op]]:
        """:return: the underlying bistring-op pairs"""

class MultiplexedRotationBox(Op):
    """
    A user-defined multiplexed rotation gate (i.e. uniformly controlled single-axis rotations) specified by a map from bitstrings to :py:class:`~.Op` sor a list of bitstring-:py:class:`~.Op` s pairs. Implementation based on arxiv.org/abs/quant-ph/0410066. The decomposed circuit has at most 2^k single-qubit rotations, 2^k CX gates, and two additional H gates if the rotation axis is X. k is the number of control qubits.
    """

    @overload
    def __init__(self, bistring_to_op_list: Sequence[tuple[Sequence[bool], Op]]) -> None:
        """
        Construct from a list of bitstring-:py:class:`~.Op` spairs

        All :py:class:`~.Op` s  must share the same single-qubit rotation type: Rx, Ry, or Rz.

        :param bitstring_to_op_list: List of bitstring-:py:class:`~.Op` spairs
        """

    @overload
    def __init__(self, op_map: dict[tuple[bool, ...], Op]) -> None:
        """
        Construct from a map from bitstrings to :py:class:`~.Op` s.All :py:class:`~.Op` s  must share the same single-qubit rotation type: Rx, Ry, or Rz.

        :param op_map: Map from bitstrings to :py:class:`~.Op` s
        """

    @overload
    def __init__(self, angles: Sequence[float], axis: OpType) -> None:
        """
        Construct from a list of angles and the rotation axis.

        :param angles: List of rotation angles in half-turns. angles[i] is the angle activated by the binary representation of i
        :param axis: ``OpType.Rx``, ``OpType.Ry`` or ``OpType.Rz``
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_bitstring_op_pair_list(self) -> list[tuple[list[bool], Op]]:
        """:return: the underlying bistring-op pairs"""

    def get_op_map(self) -> dict[tuple, Op]:
        """:return: the underlying op map"""

class MultiplexedU2Box(Op):
    """
    A user-defined multiplexed U2 gate (i.e. uniformly controlled U2 gate) specified by a map from bitstrings to :py:class:`~.Op` sor a list of bitstring-:py:class:`~.Op` s pairsImplementation based on arxiv.org/abs/quant-ph/0410066. The decomposed circuit has at most 2^k single-qubit gates, 2^k -1 CX gates, and a k+1 qubit DiagonalBox at the end. k is the number of control qubits.
    """

    @overload
    def __init__(self, bistring_to_op_list: Sequence[tuple[Sequence[bool], Op]], impl_diag: bool = True) -> None:
        """
        Construct from a list of bitstring-:py:class:`~.Op` spairs

        Only supports single qubit unitary gate types and :py:class:`~.Unitary1qBox`.

        :param op_map: List of bitstring-:py:class:`~.Op` spairs
        :param impl_diag: Whether to implement the final diagonal gate, default to True.
        """

    @overload
    def __init__(self, op_map: dict[tuple[bool, ...], Op], impl_diag: bool = True) -> None:
        """
        Construct from a map from bitstrings to :py:class:`~.Op` s.Only supports single qubit unitary gate types and :py:class:`~.Unitary1qBox`.

        :param op_map: Map from bitstrings to :py:class:`~.Op` s
        :param impl_diag: Whether to implement the final diagonal gate, default to True.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_bitstring_op_pair_list(self) -> list[tuple[list[bool], Op]]:
        """:return: the underlying bistring-op pairs"""

    def get_op_map(self) -> dict[tuple, Op]:
        """:return: the underlying op map"""

    def get_impl_diag(self) -> bool:
        """:return: flag indicating whether to implement the final diagonal gate."""

class MultiplexedTensoredU2Box(Op):
    """
    A user-defined multiplexed tensor product of U2 gates specified by a map from bitstrings to lists of :py:class:`~.Op` sor a list of bitstring-list(:py:class:`~.Op` s) pairs. A box with k control qubits and t target qubits is implemented as t k-controlled multiplexed-U2 gates with their diagonal components merged and commuted to the end. The resulting circuit contains t non-diagonal components of the multiplexed-U2 decomposition, t k-controlled multiplexed-Rz boxes, and a k-qubit DiagonalBox at the end. The total CX count is at most 2^k(2t+1)-t-2.
    """

    @overload
    def __init__(self, bistring_to_op_list: Sequence[tuple[Sequence[bool], Sequence[Op]]]) -> None:
        """
        Construct from a list of bitstring-:py:class:`~.Op` spairs

        Only supports single qubit unitary gate types and :py:class:`~.Unitary1qBox`.

        :param bitstring_to_op_list: List of bitstring-List of :py:class:`~.Op` s pairs
        """

    @overload
    def __init__(self, op_map: dict[tuple[bool, ...], Sequence[Op]]) -> None:
        """
        Construct from a map from bitstrings to equal-sized lists of :py:class:`~.Op` s. Only supports single qubit unitary gate types and :py:class:`~.Unitary1qBox`.

        :param op_map: Map from bitstrings to lists of :py:class:`~.Op` s
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_bitstring_op_pair_list(self) -> list[tuple[list[bool], list[Op]]]:
        """:return: the underlying bistring-op pairs"""

    def get_op_map(self) -> dict[tuple, list[Op]]:
        """:return: the underlying op map"""

class StatePreparationBox(Op):
    """
    A box for preparing quantum states using multiplexed-Ry and multiplexed-Rz gates. Implementation based on Theorem 9 of arxiv.org/abs/quant-ph/0406176. The decomposed circuit has at most 2*(2^n-2) CX gates, and 2^n-2 CX gates if the coefficients are all real.
    """

    def __init__(self, statevector: Annotated[NDArray[numpy.complex128], dict(shape=(None,), order='C')], is_inverse: bool = False, with_initial_reset: bool = False) -> None:
        """
        Construct from a statevector

        :param statevector: normalised statevector
        :param is_inverse: whether to implement the dagger of the state preparation circuit, default to false
        :param with_initial_reset: whether to explicitly set the state to zero initially (by default the initial zero state is assumed and no explicit reset is applied)
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_statevector(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None,), order='C')]:
        """:return: the statevector"""

    def is_inverse(self) -> bool:
        """
        :return: flag indicating whether to implement the dagger of the state preparation circuit
        """

    def with_initial_reset(self) -> bool:
        """
        :return: flag indicating whether the qubits are explicitly set to the zero state initially
        """

class DiagonalBox(Op):
    """
    A box for synthesising a diagonal unitary matrix into a sequence of multiplexed-Rz gates. Implementation based on Theorem 7 of arxiv.org/abs/quant-ph/0406176. The decomposed circuit has at most 2^n-2 CX gates.
    """

    def __init__(self, diagonal: Annotated[NDArray[numpy.complex128], dict(shape=(None,), order='C')], upper_triangle: bool = True) -> None:
        """
        Construct from the diagonal entries of the unitary operator. The size of the vector must be 2^n where n is a positive integer.

        :param diagonal: diagonal entries
        :param upper_triangle: indicates whether the multiplexed-Rz gates take the shape of an upper triangle or a lower triangle. Default to true.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_diagonal(self) -> Annotated[NDArray[numpy.complex128], dict(shape=(None,), order='C')]:
        """:return: the statevector"""

    def is_upper_triangle(self) -> bool:
        """:return: the upper_triangle flag"""

class ConjugationBox(Op):
    """
    A box to express computations that follow the compute-action-uncompute pattern.
    """

    def __init__(self, compute: Op, action: Op, uncompute: Op | None = None) -> None:
        """
        Construct from operations that perform compute, action, and uncompute. All three operations need to be quantum and have the same size.

        :param compute: the compute operation
        :param action: the action operation
        :param uncompute: optional uncompute operation, default to compute.dagger(). If provided, the user needs to make sure that uncompute.dagger() and compute have the same unitary.
        """

    def get_circuit(self) -> Circuit:
        """:return: the :py:class:`~.Circuit` described by the box"""

    def get_compute(self) -> Op:
        """:return: the compute operation"""

    def get_action(self) -> Op:
        """:return: the action operation"""

    def get_uncompute(self) -> Op | None:
        """
        :return: the uncompute operation. Returns None if the default compute.dagger() is used
        """

class Conditional(Op):
    """
    A wrapper for an operation to be applied conditionally on the value of some classical bits (following the nature of conditional operations in the OpenQASM specification).
    """

    def __init__(self, op: Op, width: int, value: int) -> None:
        """Construct from operation, bit width and (little-endian) value"""

    @property
    def op(self) -> Op:
        """The operation to be applied conditionally"""

    @property
    def width(self) -> int:
        """The number of bits in the condition register"""

    @property
    def value(self) -> int:
        """
        The little-endian value the classical register must read in order to apply the operation (e.g. value 2 (10b) means bits[0] must be 0 and bits[1] must be 1)
        """

class ClassicalOp(Op):
    """Classical operation."""

    @property
    def n_inputs(self) -> int:
        """Number of pure inputs."""

    @property
    def n_input_outputs(self) -> int:
        """Number of pure input/output arguments."""

    @property
    def n_outputs(self) -> int:
        """Number of pure outputs."""

class ClassicalEvalOp(ClassicalOp):
    """Evaluatable classical operation."""

class SetBitsOp(ClassicalEvalOp):
    """An operation to set the values of Bits to some constants."""

    def __init__(self, values: Sequence[bool]) -> None:
        """Construct from a table of values."""

    @property
    def values(self) -> list[bool]:
        """The values to set bits to."""

class CopyBitsOp(ClassicalEvalOp):
    """An operation to copy the values of Bits to other Bits."""

class MultiBitOp(ClassicalEvalOp):
    """An operation to apply a classical op multiple times in parallel."""

    def __init__(self, op: ClassicalEvalOp, multiplier: int) -> None:
        """Construct from a basic operation and a multiplier."""

    @property
    def basic_op(self) -> ClassicalEvalOp:
        """Underlying bitwise op."""

    @property
    def multiplier(self) -> int:
        """Multiplier."""

class RangePredicateOp(ClassicalEvalOp):
    """A predicate defined by a range of values in binary encoding."""

    def __init__(self, width: int, lower: int, upper: int) -> None:
        """Construct from a bit width, a lower bound and an upper bound."""

    @property
    def lower(self) -> int:
        """Inclusive lower bound."""

    @property
    def upper(self) -> int:
        """Inclusive upper bound."""

class WASMOp(ClassicalOp):
    """
    An op holding an external classical call, defined by the external module id, the name of the function and the arguments. External calls can only act on entire registers (which will be interpreted as fixed-width integers).
    """

    def __init__(self, num_bits: int, num_w: int, n_inputs: Sequence[int], n_outputs: Sequence[int], func_name: str, wasm_uid: str) -> None:
        """
        Construct from number of bits, bitwidths of inputs and outputs, function name and module id.
        """

    @property
    def wasm_uid(self) -> str:
        """Wasm module id."""

    @property
    def num_w(self) -> int:
        """Number of wasm wire in the op"""

    @property
    def func_name(self) -> str:
        """Name of function."""

    @property
    def num_bits(self) -> int:
        """Number of bits interacted with."""

    @property
    def n_i32(self) -> int:
        """Number of integers acted on."""

    @property
    def input_widths(self) -> list[int]:
        """Widths of input integers."""

    @property
    def output_widths(self) -> list[int]:
        """Widths of output integers."""

class ClOp(enum.IntEnum):
    """A classical operation"""

    INVALID = 0
    """Invalid"""

    BitAnd = 1
    """Bitwise AND"""

    BitOr = 2
    """Bitwise OR"""

    BitXor = 3
    """Bitwise XOR"""

    BitEq = 4
    """Bitwise equality"""

    BitNeq = 5
    """Bitwise inequality"""

    BitNot = 6
    """Bitwise NOT"""

    BitZero = 7
    """Constant zero bit"""

    BitOne = 8
    """Constant one bit"""

    RegAnd = 9
    """Registerwise AND"""

    RegOr = 10
    """Registerwise OR"""

    RegXor = 11
    """Registerwise XOR"""

    RegEq = 12
    """Registerwise equality"""

    RegNeq = 13
    """Registerwise inequality"""

    RegNot = 14
    """Registerwise NOT"""

    RegZero = 15
    """Constant all-zeros register"""

    RegOne = 16
    """Constant all-ones register"""

    RegLt = 17
    """Integer less-than comparison"""

    RegGt = 18
    """Integer greater-than comparison"""

    RegLeq = 19
    """Integer less-than-or-equal comparison"""

    RegGeq = 20
    """Integer greater-than-or-equal comparison"""

    RegAdd = 21
    """Integer addition"""

    RegSub = 22
    """Integer subtraction"""

    RegMul = 23
    """Integer multiplication"""

    RegDiv = 24
    """Integer division"""

    RegPow = 25
    """Integer exponentiation"""

    RegLsh = 26
    """Left shift"""

    RegRsh = 27
    """Right shift"""

    RegNeg = 28
    """Integer negation"""

class ClBitVar:
    """A bit variable within an expression"""

    def __init__(self, i: int) -> None:
        """
        Construct from an integer identifier.

        :param i: integer identifier for the variable
        """

    def __eq__(self, arg: object, /) -> bool: ...

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

    def __hash__(self) -> int: ...

    @property
    def index(self) -> int:
        """integer identifier for the variable"""

class ClRegVar:
    """A register variable within an expression"""

    def __init__(self, i: int) -> None:
        """
        Construct from an integer identifier.

        :param i: integer identifier for the variable
        """

    def __eq__(self, arg: object, /) -> bool: ...

    def __str__(self) -> str: ...

    def __repr__(self) -> str: ...

    def __hash__(self) -> int: ...

    @property
    def index(self) -> int:
        """integer identifier for the variable"""

class ClExpr:
    """A classical expression"""

    def __init__(self, op: ClOp, args: Sequence[int | ClBitVar | ClRegVar | ClExpr]) -> None:
        """
        Construct from an operation type and a list of arguments.

        :param op: the operation type
        :param args: list of arguments to the expression (which may be integers, :py:class:`~.ClBitVar` variables, :py:class:`~.ClRegVar` variables, or other :py:class:`~.ClExpr`)
        """

    def __eq__(self, arg: object, /) -> bool: ...

    def __str__(self) -> str: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    @property
    def op(self) -> ClOp:
        """main operation"""

    @property
    def args(self) -> list[int | ClBitVar | ClRegVar | ClExpr]:
        """arguments"""

    def as_qasm(self, input_bits: Mapping[int, pytket._tket.unit_id.Bit], input_regs: Mapping[int, pytket._tket.unit_id.BitRegister]) -> str:
        """
        QASM-style string representation given corresponding bits and registers
        """

class WiredClExpr:
    """An operation defined by a classical expression over a sequence of bits"""

    def __init__(self, expr: ClExpr, bit_posn: Mapping[int, int] = {}, reg_posn: Mapping[int, Sequence[int]] = {}, output_posn: Sequence[int] = []) -> None:
        """
        Construct from an expression with bit and register positions.

        :param expr: an abstract classical expression
        :param bit_posn: a map whose keys are the indices of the :py:class:`~.ClBitVar` occurring in the expression, and whose values are the positions of the corresponding bits in the arguments of the operation
        :param reg_posn: a map whose keys are the indices of the :py:class:`~.ClRegVar` occurring in the expression, and whose values are the sequences of positions of the corresponding bits in the arguments of the operation
        :param output_posn: a list giving the positions of the output bits in the arguments of the operation
        """

    def __eq__(self, arg: object, /) -> bool: ...

    def __str__(self) -> str: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    @property
    def expr(self) -> ClExpr:
        """expression"""

    @property
    def bit_posn(self) -> dict[int, int]:
        """bit positions"""

    @property
    def reg_posn(self) -> dict[int, list[int]]:
        """register positions"""

    @property
    def output_posn(self) -> list[int]:
        """output positions"""

    def to_dict(self) -> dict:
        """:return: JSON-serializable dict representation"""

    @staticmethod
    def from_dict(arg: dict, /) -> WiredClExpr:
        """Construct from JSON-serializable dict representation"""

    def __getstate__(self) -> tuple: ...

    def __setstate__(self, arg: tuple, /) -> None: ...

class ClExprOp(Op):
    """An operation defined by a classical expression"""

    def __init__(self, arg: WiredClExpr, /) -> None:
        """Construct from a wired classical expression"""

    @property
    def type(self) -> OpType:
        """operation type"""

    @property
    def expr(self) -> WiredClExpr:
        """wired expression"""

def fresh_symbol(preferred: str = 'a') -> sympy.core.symbol.Symbol:
    """
    Given some preferred symbol, this finds an appropriate suffix that will guarantee it has not yet been used in the current python session.

    :param preferred: The preferred readable symbol name as a string (default is 'a')

    :return: A new sympy symbol object
    """
