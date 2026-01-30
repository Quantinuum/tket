"""The tailoring module provides access to noise tailoring tools."""

from collections.abc import Mapping, Set

import pytket._tket.circuit
import pytket._tket.pauli


class FrameRandomisation:
    """
    The base FrameRandomisation class. FrameRandomisation finds subcircuits (cycles) of a given circuit comprised of gates with OpType only from a specified set of OpType, and wires gates into the boundary (frame) of these cycles. Input frame gates are sampled from another set of OpType, and output frame gates deduced such that the circuit unitary doesn't change, achieved by computing the action of cycle gates on frame gates.
    """

    def __init__(self, arg0: Set[pytket._tket.circuit.OpType], arg1: Set[pytket._tket.circuit.OpType], arg2: Mapping[pytket._tket.circuit.OpType, Mapping[tuple, tuple]], /) -> None:
        """
        Constructor for FrameRandomisation.

        :param cycletypes: A set of OpType corresponding to the gates cycles found are comprised of
        :param frametypes: A set of OpType corresponding to the gates Frames are sampled from
        :param conjugates: A map from cycle OpType, to a map between Frame OptypeVector giving the required change to output frame OpType to preserve Unitary from given input frame OpType.
        """

    def get_all_circuits(self, circuit: pytket._tket.circuit.Circuit) -> list[pytket._tket.circuit.Circuit]:
        """
        For given circuit, finds all Cycles, finds all frames for each Cycle, and returns every combination of frame and cycle in a vector of Circuit.

        :param circuit: The circuit to find frames for.
        :return: list of :py:class:`~.Circuit` s
        """

    def sample_circuits(self, circuit: pytket._tket.circuit.Circuit, samples: int) -> list[pytket._tket.circuit.Circuit]:
        """
        Returns a number of instances equal to sample of frame randomisation for the given circuit. Samples individual frame gates uniformly.

        :param circuit: The circuit to perform frame randomisation with Pauli gates on
        :param samples: the number of frame randomised circuits to return.
        :return: list of :py:class:`~.Circuit` s
        """

    def __repr__(self) -> str: ...

class PauliFrameRandomisation:
    """
    The PauliFrameRandomisation class. PauliFrameRandomisation finds subcircuits (cycles) of a given circuit comprised of gates with OpType::H, OpType::CX and OpType::S, and wires gates into the boundary (frame) of these cycles. Input frame gates are sampled from another set of OpType comprised of the Pauli gates, and output frame gates deduced such that the circuit unitary doesn't change, achieved by computing the action of cycle gates on frame gates.
    """

    def __init__(self) -> None:
        """Constructor for PauliFrameRandomisation."""

    def get_all_circuits(self, circuit: pytket._tket.circuit.Circuit) -> list[pytket._tket.circuit.Circuit]:
        """
        For given circuit, finds all Cycles, finds all frames for each Cycle, and returns every combination of frame and cycle in a vector of Circuit.

        :param circuit: The circuit to find frames for.
        :return: list of :py:class:`~.Circuit` s
        """

    def sample_circuits(self, circuit: pytket._tket.circuit.Circuit, samples: int) -> list[pytket._tket.circuit.Circuit]:
        """
        Returns a number of instances equal to sample of frame randomisation for the given circuit. Samples individual frame gates uniformly from the Pauli gates.

        :param circuit: The circuit to perform frame randomisation with Pauli gates on
        :param samples: the number of frame randomised circuits to return.
        :return: list of :py:class:`~.Circuit` s
        """

    def __repr__(self) -> str: ...

class UniversalFrameRandomisation:
    """
    The UniversalFrameRandomisation class. UniversalFrameRandomisation finds subcircuits (cycles) of a given circuit comprised of gates with OpType::H, OpType::CX, and OpType::Rz, and wires gates into the boundary (frame) of these cycles. Input frame gates are sampled from another set of OpType comprised of the Pauli gates, and output frame gates deduced such that the circuit unitary doesn't change, achieved by computing the action of cycle gates on frame gates. Some gates with OpType::Rz may be substituted for their dagger to achieve this.
    """

    def __init__(self) -> None:
        """Constructor for UniversalFrameRandomisation."""

    def get_all_circuits(self, circuit: pytket._tket.circuit.Circuit) -> list[pytket._tket.circuit.Circuit]:
        """
        For given circuit, finds all Cycles, finds all frames for each Cycle, and returns every combination of frame and cycle in a vector of Circuit.

        :param circuit: The circuit to find frames for.
        :return: list of :py:class:`~.Circuit` s
        """

    def sample_circuits(self, circuit: pytket._tket.circuit.Circuit, samples: int) -> list[pytket._tket.circuit.Circuit]:
        """
        Returns a number of instances equal to sample of frame randomisation for the given circuit. Samples individual frame gates uniformly from the Pauli gates.

        :param circuit: The circuit to perform frame randomisation with Pauli gates on
        :param samples: the number of frame randomised circuits to return.
        :return: list of :py:class:`~.Circuit` s
        """

    def __repr__(self) -> str: ...

def apply_clifford_basis_change(pauli: pytket._tket.pauli.QubitPauliString, circuit: pytket._tket.circuit.Circuit) -> pytket._tket.pauli.QubitPauliString:
    """
    Given Pauli operator P and Clifford circuit C, returns C_dagger.P.C in multiplication order. This ignores any -1 phase that could be introduced. 

    :param pauli: Pauli operator being transformed. 
    :param circuit: Clifford circuit acting on Pauli operator. 
    :return: :py:class:`~.QubitPauliString` for new operator
    """

def apply_clifford_basis_change_tensor(pauli: pytket._tket.pauli.QubitPauliTensor, circuit: pytket._tket.circuit.Circuit) -> pytket._tket.pauli.QubitPauliTensor:
    """
    Given Pauli operator P and Clifford circuit C, returns C_dagger.P.C in multiplication order

    :param pauli: Pauli operator being transformed.
    :param circuit: Clifford circuit acting on Pauli operator. 
    :return: :py:class:`~.QubitPauliTensor` for new operator
    """
