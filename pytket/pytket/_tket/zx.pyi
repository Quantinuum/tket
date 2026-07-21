from collections.abc import Mapping, Sequence
import enum
from typing import Union, overload

import sympy.core.expr
import sympy.core.symbol

import pytket._tket.circuit
import pytket._tket.unit_id


class ZXType(enum.Enum):
    """Enum for available types of generators in :py:class:`~.ZXDiagram` s."""

    Input = 0
    """
    An input boundary vertex. Can either be Quantum or Classical. Must have degree 1. No ports.
    """

    Output = 1
    """
    An output boundary vertex. Can either be Quantum or Classical. Must have degree 1. No ports.
    """

    Open = 2
    """
    A boundary vertex that has not yet been specified as input or output. Can either be Quantum or Classical. Must have degree 1. No ports.
    """

    ZSpider = 3
    """
    A Z (green) spider. Parameterised by a single phase in half-turns. Can either be Quantum or Classical - Quantum spiders can only have Quantum wires, Quantum wires on Classical spiders act as two wires. Can have arbitrary degree. No ports.
    """

    XSpider = 4
    """
    An X (red) spider. Parameterised by a single phase in half-turns. Can either be Quantum or Classical - Quantum spiders can only have Quantum wires, Quantum wires on Classical spiders act as two wires. Can have arbitrary degree. No ports.
    """

    Hbox = 5
    """
    A Hadamard box for ZH diagrams. Parameterised by a single complex value. Can either be Quantum or Classical - Quantum spiders can only have Quantum wires, Quantum wires on Classical spiders act as two wires. Can have arbitrary degree. No ports.
    """

    XY = 6
    """
    A (postselected) XY qubit in MBQC. Corresponds to a Z spider with negative phase.
    """

    XZ = 7
    """
    A (postselected) XZ qubit in MBQC. Corresponds to a 0.5-phase (n+1)-ary Z spider connected to a phaseful 1-ary X spider.
    """

    YZ = 8
    """
    A (postselected) YZ qubit in MBQC. Corresponds to a 0-phase (n+1)-ary Z spider connected to a phaseful 1-ary X spider.
    """

    PX = 9
    """
    A (postselected) Pauli X qubit in MBQC. Corresponds to a Z spider with phase either 0 (param=False) or 1 (param=True).
    """

    PY = 10
    """
    A (postselected) Pauli Y qubit in MBQC. Corresponds to a Z spider with phase either -0.5 (param=False) or +0.5 (param=True).
    """

    PZ = 11
    """
    A (postselected) Pauli Z qubit in MBQC. Corresponds to a 0-phase (n+1)-ary Z spider connected to a 1-ary X spider with phase either 0 (param=False) or 1 (param=True).
    """

    Triangle = 12
    """
    A Triangle operator, [[1, 1], [0, 1]]. Can either be Quantum or Classical, only admitting wires of the same type. Port 0 for the base of the triangle (input), port 1 for the tip (output).
    """

    ZXBox = 13
    """
    A box encapsulating another :py:class:`~.ZXDiagram`. Inherits ports from the boundary of the internal diagram, with port numbers matching the boundary order and :py:class:`~.QuantumType` admitted at each port matching that of the boundary vertex.
    """

class ZXWireType(enum.Enum):
    """Enum for available types of wires in :py:class:`~.ZXDiagram` s."""

    Basic = 0
    """A basic identity wire."""

    H = 1
    """A Hadamard edge."""

class QuantumType(enum.Enum):
    """
    Enum for specifying quantumness of vertices, ports, and wires in :py:class:`~.ZXDiagram` s for mixed quantum-classical processes.
    """

    Quantum = 0
    """
    Quantum components of diagrams, represented in the framework of completely-positive maps by two parallel copies of a system related by conjugation.
    """

    Classical = 1
    """
    Classical components of diagrams, represented in the framework of completely-positive maps by a single self-conjugate system.
    """

class ZXVert:
    """
    A handle to a vertex in a :py:class:`~.ZXDiagram`. Each instance is specific to a given :py:class:`~.ZXDiagram` instance and can be invalidated by rewrites. Exceptions or errors may occur if calling functions on a :py:class:`~.ZXVert` that is not present in the given :py:class:`~.ZXDiagram`.
    """

    def __repr__(self) -> str: ...

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int: ...

class ZXWire:
    """
    A handle to a wire in a :py:class:`~.ZXDiagram`. Each instance is specific to a given :py:class:`~.ZXDiagram` instance and can be invalidated by rewrites. Exceptions or errors may occur if calling functions on a :py:class:`~.ZXWire` that is not present in the given :py:class:`~.ZXDiagram`.
    """

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int: ...

class ZXGen:
    """
    Encapsulates the information about the generator depicted by a given vertex in a :py:class:`~.ZXDiagram`.
    """

    @overload
    @staticmethod
    def create(type: ZXType, qtype: QuantumType = QuantumType.Quantum) -> ZXGen:
        """Create a boundary type generator."""

    @overload
    @staticmethod
    def create(type: ZXType, param: Union[sympy.core.expr.Expr, float], qtype: QuantumType = QuantumType.Quantum) -> ZXGen: ...

    @property
    def type(self) -> ZXType:
        """The type of generator."""

    @property
    def qtype(self) -> QuantumType | None:
        """The :py:class:`~.QuantumType` of the generator (if applicable)."""

    def __eq__(self, arg: object, /) -> bool: ...

    def __hash__(self) -> int:
        """
        Hashing is not implemented for this class, attempting to hash an object will raise a type error
        """

    def __repr__(self) -> str: ...

class PhasedGen(ZXGen):
    """
    Specialisation of :py:class:`~.ZXGen` for arbitrary-arity, symmetric generators with a single continuous parameter.
    """

    def __init__(self, zxtype: ZXType, param: Union[sympy.core.expr.Expr, float] = 0.0, qtype: QuantumType = QuantumType.Quantum) -> None:
        """Construct from a ZX type, parameter and quantum type."""

    @property
    def param(self) -> Union[sympy.core.expr.Expr, float]:
        """The parameter of the generator."""

class CliffordGen(ZXGen):
    """
    Specialisation of :py:class:`~.ZXGen` for arbitrary-arity, symmetric Clifford generators with a single boolean parameter.
    """

    def __init__(self, zxtype: ZXType, param: bool = False, qtype: QuantumType = QuantumType.Quantum) -> None:
        """Construct from a ZX type, parameter and quantum type."""

    @property
    def param(self) -> bool:
        """The parameter of the generator."""

class DirectedGen(ZXGen):
    """
    Specialisation of :py:class:`~.ZXGen` for asymmetric ZX generators which can be doubled to form a Quantum variant. Asymmetric effects handled by ports to distinguish operands.
    """

    def __init__(self, zxtype: ZXType, qtype: QuantumType) -> None:
        """Construct from a ZX type and quantum type."""

    @property
    def n_ports(self) -> int:
        """The number of ports on the generator."""

    @property
    def signature(self) -> list[QuantumType]:
        """
        A list of :py:class:`~.QuantumType` s indicating the expected :py:class:`~.QuantumType` at each port.
        """

class ZXDiagram:
    """
    Undirected graphs for mixed process ZX diagrams. The boundary is an ordered list which may mix inputs, outputs, and "open" vertices (not specified to be inputs or outputs). Directed vertices (e.g. Boxes, Triangles, etc.) have numbered ports to distinguish different incident edges. The content of each vertex is given by a :py:class:`~.ZXGen` generator, describing the :py:class:`~.ZXType` (e.g. XSpider, Input, Triangle), the QuantumType for single/doubled versions of typical generators, and any parameters such as phase. Wires are undirected and have a :py:class:`~.ZXWireType` (e.g. Basic, Hadamard) and :py:class:`~.QuantumType` (a single wire or a doubled pair for a quantum system).
    """

    @overload
    def __init__(self) -> None:
        """Constructs an empty ZX diagram."""

    @overload
    def __init__(self, inputs: int, outputs: int, classical_inputs: int, classical_outputs: int) -> None:
        """
        Constructs an empty ZX diagram with a given number of unconnected boundary vertices.

        :param in: Number of quantum inputs.
        :param out: Number of quantum outputs.
        :param classical_in: Number of classical inputs.
        :param classical_out: Number of classical outputs.
        """

    @overload
    def __init__(self, other: ZXDiagram) -> None:
        """
        Constructs a copy of an existing ZX diagram.

        :param other: ZX diagram to copy.
        """

    def get_boundary(self, type: ZXType | None = None, qtype: QuantumType | None = None) -> list[ZXVert]:
        """
        Returns handles to boundary vertices in order. Optionally filter by type of boundary vertex.

        :param type: :py:class:`~.ZXType` to filter by, from {:py:meth:`ZXType.Input`, :py:meth:`ZXType.Output`, :py:meth:`ZXType.Open`, None}. Defaults to None.

        :param qtype: :py:class:`~.QuantumType` to filter by, from {:py:meth:`QuantumType.Quantum`, :py:meth:`QuantumType.Classical`, None}. Defaults to None.
        """

    @property
    def scalar(self) -> Union[sympy.core.expr.Expr, float]:
        """
        Returns the global scalar stored numerically. This may be a symbolic expression.
        """

    def multiply_scalar(self, scalar: Union[sympy.core.expr.Expr, float]) -> None:
        """
        Multiplies the global scalar by a numerical (possibly symbolic) constant.
        """

    @property
    def vertices(self) -> list[ZXVert]:
        """
        Returns a list of handles to all vertices in the diagram. The order of vertices may not be semantically relevant.
        """

    @property
    def wires(self) -> list[ZXWire]:
        """
        Returns a list of handles to all wires in the diagram. The order of wires may not be semantically relevant.
        """

    @property
    def n_vertices(self) -> int:
        """
        Counts the number of vertices in the diagram. Includes boundary vertices and disconnected vertices.
        """

    @property
    def n_wires(self) -> int:
        """Counts the number of edges in the diagram."""

    def count_vertices(self, type: ZXType) -> int:
        """
        Counts the number of vertices of a given :py:class:`~.ZXType` in the diagram.
        """

    def count_wires(self, type: ZXWireType) -> int:
        """
        Counts the number of wired of a given :py:class:`~.ZXWireType` in the diagram.
        """

    def degree(self, v: ZXVert) -> int:
        """Returns the degree of the given vertex."""

    def neighbours(self, v: ZXVert) -> list[ZXVert]:
        """
        Given a vertex, returns a list of all vertices neighbouring it. Each neighbour will only appear in the list once regardless of how many shared edges there are. The order of the neighbour list may not be semantically relevant.
        """

    def adj_wires(self, v: ZXVert) -> list[ZXWire]:
        """
        Given a vertex, returns a list of all incident wires. Self-loops will only appear once in the list. The order of the wire list may not be semantically relevant.
        """

    def wires_between(self, u: ZXVert, v: ZXVert) -> list[ZXWire]:
        """
        Given two vertices, returns a list of all wires between them. The order of the wire list may not be semantically relevant.
        """

    def wire_between(self, u: ZXVert, v: ZXVert) -> ZXWire | None:
        """
        Given two vertices, returns either an arbitrary edge between them if one exists or None if they are not adjacent.
        """

    def wire_at_port(self, v: ZXVert, port: int) -> ZXWire:
        """
        Given a vertex, returns the unique wire at the given port number. Raises an exception if multiple wires are found at the given port.
        """

    def get_vertex_ZXGen(self, v: ZXVert) -> ZXGen:
        """Returns the content of a given vertex as a :py:class:`~.ZXGen`."""

    def get_name(self, v: ZXVert) -> str:
        """Returns the readable string description of a given vertex"""

    def get_zxtype(self, v: ZXVert) -> ZXType:
        """Returns the :py:class:`~.ZXType` of the given vertex."""

    def get_qtype(self, v: ZXVert) -> QuantumType | None:
        """
        Returns the :py:class:`~.QuantumType` of the given vertex if defined, None otherwise.
        """

    def set_vertex_ZXGen(self, v: ZXVert, gen: ZXGen) -> None:
        """
        Updates the content of a given vertex to a particular :py:class:`~.ZXGen`.
        """

    def get_wire_qtype(self, w: ZXWire) -> QuantumType:
        """Returns the :py:class:`~.QuantumType` of the given wire."""

    def get_wire_type(self, w: ZXWire) -> ZXWireType:
        """Returns the :py:class:`~.ZXWireType` of the given wire."""

    def set_wire_qtype(self, w: ZXWire, qtype: QuantumType) -> None:
        """Updates the :py:class:`~.QuantumType` of the given wire."""

    def set_wire_type(self, w: ZXWire, type: ZXWireType) -> None:
        """Updates the :py:class:`~.ZXWireType` of the given wire."""

    def get_wire_ends(self, w: ZXWire) -> tuple[tuple[ZXVert, int | None], tuple[ZXVert, int | None]]:
        """
        Returns a tuple ((vertex0, port0), (vertex1, port1)) describing the two ends of the wire.
        """

    def other_end(self, w: ZXWire, v: ZXVert) -> ZXVert:
        """
        Given a wire and a vertex at one end of the wire, gives the vertex at the other end of the wire. This can be used to traverse the undirected edges of the graph.
        """

    def check_validity(self) -> None:
        """
        Performs a check for the internal validity of the :py:class:`~.ZXDiagram` and raises an exception if it is invalid.
        - Inputs/Outputs must have degree 1 and all exist within the boundary.
        - Undirected vertices (those without ports) have no ports on incident edges.
        - Directed vertices (those with ports) have exactly one incident edge at each port.
        - :py:class:`~.QuantumType` of wires are compatible with the :py:class:`~.QuantumType` s of the ports they attach to.
        """

    def symbol_substitution(self, symbol_map: Mapping[sympy.core.symbol.Symbol, Union[sympy.core.expr.Expr, float]]) -> None:
        """
        In-place substitution for symbolic expressions; iterated through each parameterised vertex and performs the substitution. This will not affect any symbols captured within boxed operations.

        :param symbol_map: A map from SymPy symbols to SymPy expressions or floats.
        """

    def free_symbols(self) -> set[sympy.core.symbol.Symbol]:
        """Returns the set of symbolic parameters in the diagram."""

    def is_symbolic(self) -> bool:
        """
        Returns True if the diagram contains any free symbols, False otherwise.
        """

    @overload
    def add_vertex(self, gen: ZXGen) -> ZXVert:
        """
        Adds a new vertex to the diagram for an arbitrary :py:class:`~.ZXGen`.

        :param gen: The :py:class:`~.ZXGen` for the new vertex.
        :return: The handle to the new vertex.
        """

    @overload
    def add_vertex(self, type: ZXType, qtype: QuantumType = QuantumType.Quantum) -> ZXVert:
        """
        Adds a new vertex to the diagram for an unparameterised, doubleable generator type.

        :param type: The :py:class:`~.ZXType` for the new vertex.
        :param qtype: The :py:class:`~.QuantumType` for the new vertex. Defaults to Quantum.
        :return: The handle to the new vertex.
        """

    @overload
    def add_vertex(self, type: ZXType, param: bool, qtype: QuantumType = QuantumType.Quantum) -> ZXVert:
        """
        Adds a new vertex to the diagram for a Boolean-parameterised, doubleable generator type.

        :param type: The :py:class:`~.ZXType` for the new vertex.
        :param param: The parameter for the new vertex.
        :param qtype: The :py:class:`~.QuantumType` for the new vertex. Defaults to Quantum.
        :return: The handle to the new vertex.
        """

    @overload
    def add_vertex(self, type: ZXType, param: Union[sympy.core.expr.Expr, float], qtype: QuantumType = QuantumType.Quantum) -> ZXVert:
        """
        Adds a new vertex to the diagram for a parameterised, doubleable generator type.

        :param type: The :py:class:`~.ZXType` for the new vertex.
        :param param: The parameter for the new vertex.
        :param qtype: The :py:class:`~.QuantumType` for the new vertex. Defaults to Quantum.
        :return: The handle to the new vertex.
        """

    def add_zxbox(self, inner: ZXDiagram) -> ZXVert:
        """
        Adds a new vertex to the diagram for a box with some inner implementation.

        :param inner: The :py:class:`~.ZXDiagram` to internalise inside the box. The current state is copied by value.
        :return: The handle to the new vertex.
        """

    def add_wire(self, u: ZXVert, v: ZXVert, type: ZXWireType = ZXWireType.Basic, qtype: QuantumType = QuantumType.Quantum, u_port: int | None = None, v_port: int | None = None) -> ZXWire:
        """
        Adds a new wire to the diagram between the given vertices.

        :param u: Handle to the first vertex.
        :param v: Handle to the other vertex.
        :param type: :py:class:`~.ZXWireType` for the wire. Defaults to Basic.
        :param qtype: :py:class:`~.QuantumType` for the wire. Defaults to Quantum.
        :param u_port: Port on vertex u to connect to. Defaults to None.
        :param v_port: Port on vertex v to connect to. Defaults to None.
        :return: The handle to the new wire.
        """

    def remove_vertex(self, v: ZXVert) -> None:
        """
        Removes the given vertex and all incident wires from the diagram. If the vertex is in the boundary, it is removed from the boundary.
        """

    def remove_wire(self, w: ZXWire) -> None:
        """Removes the given wire from the diagram."""

    def to_circuit(self) -> tuple[pytket._tket.circuit.Circuit, dict[ZXVert, pytket._tket.unit_id.UnitID]]:
        """
        Extracts a unitary diagram in MBQC form as a Circuit following the routine by Backens et al. ("There and back again: A circuit extraction tale").

        :return: A pair of the generated :py:class:`~.Circuit`, and a map from each boundary vertex in the :py:class:`~.ZXDiagram` to its corresponding :py:class:`~.UnitID` in the :py:class:`~.Circuit`.
        """

    def to_doubled_diagram(self) -> ZXDiagram:
        """+ classical boundaries only have the unconjugated version"""

    def to_graphviz_str(self) -> str:
        """Returns a graphviz source string"""

class ZXBox(ZXGen):
    """
    Specialisation of :py:class:`~.ZXGen` for encapsulations of some other ZX diagrams. In general, arbitrary diagrams may be asymmetric tensors with both Quantum and Classical boundaries, so ports are used to distinguish each boundary.
    """

    def __init__(self, zxdiag: ZXDiagram) -> None:
        """Construct from a ZX diagram."""

    @property
    def n_ports(self) -> int:
        """The number of ports on the generator."""

    @property
    def signature(self) -> list[QuantumType]:
        """
        A list of :py:class:`~.QuantumType` s indicating the expected :py:class:`~.QuantumType` at each port.
        """

    @property
    def diagram(self) -> ZXDiagram:
        """The internal diagram represented by the box."""

class Flow:
    """
    Data structure for describing the Flow in a given MBQC-form :py:class:`~.ZXDiagram` object. Constructors are identification methods for different classes of Flow.
    """

    def c(self, v: ZXVert) -> list[ZXVert]:
        """The correction set for the given :py:class:`~.ZXVert`."""

    @property
    def cmap(self) -> dict[ZXVert, list[ZXVert]]:
        """The map from a vertex to its correction set"""

    def odd(self, v: ZXVert, diag: ZXDiagram) -> list[ZXVert]:
        """
        The odd neighbourhood of the correction set for the given :py:class:`~.ZXVert`.
        """

    def d(self, arg: ZXVert, /) -> int:
        """
        The depth of the given :py:class:`~.ZXVert` from the outputs in the ordering of the flow, e.g. an output vertex will have depth 0, the last measured vertex has depth 1.
        """

    @property
    def dmap(self) -> dict[ZXVert, int]:
        """The map from a vertex to its depth"""

    def focus(self, diag: ZXDiagram) -> None:
        """Focuses a flow."""

    @staticmethod
    def identify_causal_flow(diag: ZXDiagram) -> Flow:
        """Attempts to identify a causal flow for a diagram."""

    @staticmethod
    def identify_pauli_flow(diag: ZXDiagram) -> Flow:
        """Attempts to identify a Pauli flow for a diagram."""

    @staticmethod
    def identify_focused_sets(diag: ZXDiagram) -> list[list[ZXVert]]:
        """
        Attempts to identify the sets of vertices which are focused over all vertices, i.e. the remaining stabilisers not generated by correction sets within a flow.
        """

class Rewrite:
    """An in-place transformation of a ZXDiagram."""

    def apply(self, diag: ZXDiagram) -> bool:
        """
        Performs the transformation on the diagram in place.

        :param diag: The diagram to be transformed.
        :return: True if any changes were made, else False.
        """

    @staticmethod
    def sequence(sequence: Sequence[Rewrite]) -> Rewrite:
        """
        Composes a list of :py:class:`~.Rewrite` s together in sequence. The apply method will return True if ANY of the individual Rewrites returned True.

        :param sequence: The list of :py:class:`~.Rewrite` s to be composed.
        :return: The combined :py:class:`~.Rewrite`.
        """

    @staticmethod
    def repeat(rewrite: Rewrite) -> Rewrite:
        """
        Applies a given :py:class:`~.Rewrite` repeatedly to a diagram until no further changes are made (i.e. it no longer returns True). apply will return True if at least one run returned True.

        :param rewrite: The :py:class:`~.Rewrite` to be applied repeatedly.
        :return: A new :py:class:`~.Rewrite` representing the iteration.
        """

    @staticmethod
    def decompose_boxes() -> Rewrite:
        """
        Replaces every :py:class:`~.ZXBox` by its internal diagram recursively until no :py:class:`~.ZXBox` es remain.
        """

    @staticmethod
    def basic_wires() -> Rewrite:
        """Replaces every Hadamard wire by an explicit Hbox node."""

    @staticmethod
    def rebase_to_zx() -> Rewrite:
        """
        Expands every generator into ZSpiders, XSpiders, and a combination of Basic and Hadamard edges.
        """

    @staticmethod
    def rebase_to_mbqc() -> Rewrite:
        """Expands every generator into MBQC vertices."""

    @staticmethod
    def red_to_green() -> Rewrite:
        """
        Converts all red spiders (XSpider) to green (ZSpider) with Hadamards around them. The Hadamards are applied by flipping the wire type of incident edges between Basic and H.
        """

    @staticmethod
    def spider_fusion() -> Rewrite:
        """
        Merges two adjacent ZX spiders (XSpider, ZSpider) of the same colour connected by a Basic wire into a single spider. Also merges two adjacent spiders of different colour connected by a H edge.
        """

    @staticmethod
    def self_loop_removal() -> Rewrite:
        """
        Removes both H and Basic self loop edges around ZX spiders. Basic edges can simply be removed. Removing H loops introduces an extra pi phase on the spider.
        """

    @staticmethod
    def parallel_h_removal() -> Rewrite:
        """
        Remove parallel edges between ZX spiders (a.k.a. the Hopf rule). Matches either pairs of H edges between spiders of the same colour or Basic edges between spiders of different colour. This applies to Quantum edges between a pair of Classical spiders.
        """

    @staticmethod
    def separate_boundaries() -> Rewrite:
        """
        Guarantees that each boundary vertex is adjacent to a unique ZSpider. This adds identity chains when two boundaries are either directly connected or are adjacent to the same spider.
        """

    @staticmethod
    def io_extension() -> Rewrite:
        """
        Guarantees that the edge on each boundary vertex is Basic. If a boundary has a Hadamard, then we add a ZSpider identity as in I/O extensions in MBQC.
        """

    @staticmethod
    def remove_interior_cliffords() -> Rewrite:
        """
        Removes interior proper Cliffords (spiders where the phase is an odd multiple of pi/2 radians or 0.5 half-turns). Performs local complementation about the vertex and removes it.
        """

    @staticmethod
    def remove_interior_paulis() -> Rewrite:
        """
        Removes adjacent interior Paulis (spiders where the phase is an integer multiple of pi radians or integer half-turns). Pivots about the edge connecting the vertices and removes them.
        """

    @staticmethod
    def gadgetise_interior_paulis() -> Rewrite:
        """
        Identifies interior Paulis (spiders where the phase is an integer multiple of pi) with all neighbours having non-Pauli phase and degree > 1. Pivots about an incident edge to yield a gadget node.
        """

    @staticmethod
    def merge_gadgets() -> Rewrite:
        """
        Identifies pairs of phase gadgets over the same sets of qubits and merges them.
        """

    @staticmethod
    def extend_at_boundary_paulis() -> Rewrite:
        """
        Identifies adjacent Pauli spiders where one is adjacent to a boundary. This rule applies I/O extensions to push the match into the interior from which it can be handled by :py:meth:`remove_interior_paulis`.
        """

    @staticmethod
    def extend_for_PX_outputs() -> Rewrite:
        """
        Identifies output vertices in MBQC form that are given a measurement basis (i.e. are not PX(0)). This rule applies I/O extensions to make the phased qubits non-outputs. This is required before flow identification can be run.
        """

    @staticmethod
    def internalise_gadgets() -> Rewrite:
        """
        Identifies Degree-1 XY vertices next to a PX vertex, e.g. as the result of rebasing a phase gadget. Replaces matches by a single YZ vertex.
        """

    @staticmethod
    def to_graphlike_form() -> Rewrite:
        """
        Given a diagram with ZX generators, yields a diagram with only ZSpiders, connected by at most one Hadamard edge, with boundaries connected via Basic edges.
        """

    @staticmethod
    def reduce_graphlike_form() -> Rewrite:
        """
        Given a diagram in graphlike form, applies local complementations and pivoting to remove as many interior Clifford-angled vertices as possible. The only remaining Clifford-angled vertices will be either the axis of a phase-gadget or near a boundary.
        """

    @staticmethod
    def to_MBQC_diag() -> Rewrite:
        """
        Given a diagram in graphlike form, will rebase to MBQC generators, ensure that output qubits are PX(0) (i.e. they match unmeasured qubits) and degree-1 vertices are absorbed into a PX neighbour, i.e. reducing phase-gadgets to single vertices in a different measurement plane.
        """

def circuit_to_zx(arg: pytket._tket.circuit.Circuit, /) -> tuple[ZXDiagram, dict[pytket._tket.unit_id.UnitID, tuple[ZXVert, ZXVert]]]:
    """
    Construct a ZX diagram from a circuit. Return the ZX diagram and a map Between the ZX boundary vertices and the resource UIDs of the circuit.
    """
