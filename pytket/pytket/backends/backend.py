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

"""Abstract base class for all Backend encapsulations."""

import warnings
from abc import ABC, abstractmethod
from collections.abc import Iterable, Sequence
from importlib import import_module
from types import ModuleType
from typing import Any, Literal, cast, overload

from pytket.circuit import Bit, Circuit, OpType
from pytket.passes import BasePass
from pytket.pauli import QubitPauliString
from pytket.predicates import Predicate
from pytket.utils import QubitPauliOperator
from pytket.utils.outcomearray import OutcomeArray
from pytket.utils.results import KwargTypes

from .backend_exceptions import (
    CircuitNotRunError,
    CircuitNotValidError,
)
from .backendinfo import BackendInfo
from .backendresult import BackendResult
from .resulthandle import ResultHandle, _ResultIdTuple
from .status import CircuitStatus


class ResultHandleTypeError(Exception):
    """Wrong result handle type."""


class Backend(ABC):
    """
    This abstract class defines the structure of a backend as something that
    can run quantum circuits and produce output as at least one of shots,
    counts, state, or unitary
    """

    _supports_shots = False
    _supports_counts = False
    _supports_state = False
    _supports_unitary = False
    _supports_density_matrix = False
    _supports_expectation = False
    _expectation_allows_nonhermitian = False
    _supports_contextual_optimisation = False
    _persistent_handles = False

    def __init__(self) -> None:
        self._cache: dict[ResultHandle, dict[str, Any]] = {}

    @staticmethod
    def empty_result(circuit: Circuit, n_shots: int) -> BackendResult:
        """
        Creates a :py:class:`~pytket.backends.backendresult.BackendResult` mimicking the outcome where every
        bit is 0 for every shot.
        """
        n_bits = len(circuit.bits)
        empty_readouts = [[0] * n_bits for _ in range(n_shots)]
        shots = OutcomeArray.from_readouts(empty_readouts)
        c_bits = [Bit(index) for index in range(n_bits)]
        return BackendResult(shots=shots, c_bits=c_bits)

    @property
    @abstractmethod
    def required_predicates(self) -> list[Predicate]:
        """
        The minimum set of predicates that a circuit must satisfy before it can
        be successfully run on this backend.

        :return: Required predicates.
        """
        ...

    def valid_circuit(self, circuit: Circuit) -> bool:
        """
        Checks that the circuit satisfies all of required_predicates.

        :param circuit: The circuit to check.
        :return: Whether or not all of required_predicates are satisfied.
        """
        return all(pred.verify(circuit) for pred in self.required_predicates)

    @property
    def _uses_lightsabre(self) -> bool:
        # lightsaber is a routing method used by pytket-qiskit
        return False

    def _check_all_circuits(
        self, circuits: Iterable[Circuit], nomeasure_warn: bool | None = None
    ) -> bool:
        if nomeasure_warn is None:
            nomeasure_warn = not (
                self._supports_state
                or self._supports_unitary
                or self._supports_density_matrix
                or self._supports_expectation
            )
        for i, circ in enumerate(circuits):
            errors = (
                CircuitNotValidError(i, repr(pred))
                for pred in self.required_predicates
                if not pred.verify(circ)
            )
            for error in errors:
                raise error
            if nomeasure_warn and circ.n_gates_of_type(OpType.Measure) < 1:
                warnings.warn(  # noqa: B028
                    f"Circuit with index {i} in submitted does not contain a "
                    "measure operation."
                )
        return True

    @abstractmethod
    def rebase_pass(self) -> BasePass:
        """
        A single compilation pass that when run converts all gates in a Circuit to
        an OpType supported by the Backend (ignoring architecture constraints).

        :return: Compilation pass that converts gates to primitives supported by
            Backend.
        """
        ...

    @abstractmethod
    def default_compilation_pass(self, optimisation_level: int = 2) -> BasePass:
        """
        A suggested compilation pass that will will, if possible, produce an equivalent
        circuit suitable for running on this backend.

        At a minimum it will ensure that compatible gates are used and that all two-
        qubit interactions are compatible with the backend's qubit architecture. At
        higher optimisation levels, further optimisations may be applied.

        This is a an abstract method which is implemented in the backend itself, and so
        is tailored to the backend's requirements.

        :param optimisation_level: The level of optimisation to perform during
            compilation.

            - Level 0 does the minimum required to solves the device constraints,
              without any optimisation.
            - Level 1 additionally performs some light optimisations.
            - Level 2 (the default) adds more computationally intensive optimisations
              that should give the best results from execution.

        :return: Compilation pass guaranteeing required predicates.
        """
        ...

    def get_compiled_circuit(
        self, circuit: Circuit, optimisation_level: int = 2
    ) -> Circuit:
        """
        Return a single circuit compiled with :py:meth:`default_compilation_pass`. See
        :py:meth:`~pytket.backends.backend.Backend.get_compiled_circuits`.
        """
        return_circuit = circuit.copy()
        self.default_compilation_pass(optimisation_level).apply(return_circuit)
        return return_circuit

    def get_compiled_circuits(
        self, circuits: Sequence[Circuit], optimisation_level: int = 2
    ) -> list[Circuit]:
        """Compile a sequence of circuits with :py:meth:`default_compilation_pass`
        and return the list of compiled circuits (does not act in place).

        As well as applying a degree of optimisation (controlled by the
        `optimisation_level` parameter), this method tries to ensure that the circuits
        can be run on the backend (i.e. successfully passed to
        :py:meth:`process_circuits`), for example by rebasing to the supported gate set,
        or routing to match the connectivity of the device. However, this is not always
        possible, for example if the circuit contains classical operations that are not
        supported by the backend. You may use :py:meth:`valid_circuit` to check whether
        the circuit meets the backend's requirements after compilation. This validity
        check is included in :py:meth:`process_circuits` by default, before any circuits
        are submitted to the backend.

        If the validity check fails, you can obtain more information about the failure
        by iterating through the predicates in the `required_predicates` property of the
        backend, and running the :py:meth:`~pytket.predicates.Predicate.verify` method on each in turn with your
        circuit.

        :param circuits: The circuits to compile.
        :param optimisation_level: The level of optimisation to perform during
            compilation. See :py:meth:`default_compilation_pass` for a description of
            the different levels (0, 1 or 2). Defaults to 2.
        :return: Compiled circuits.
        """
        return [self.get_compiled_circuit(c, optimisation_level) for c in circuits]

    @property
    @abstractmethod
    def _result_id_type(self) -> _ResultIdTuple:
        """Identifier type signature for ResultHandle for this backend.

        :return: Type signature (tuple of hashable types)
        """
        ...

    def _check_handle_type(self, reshandle: ResultHandle) -> None:
        """Check a result handle is valid for this backend, raises TypeError if not.

        :param reshandle: Handle to check
        :raises TypeError: Types of handle identifiers don't match those of backend.
        """
        if (len(reshandle) != len(self._result_id_type)) or not all(
            isinstance(idval, ty)
            for idval, ty in zip(reshandle, self._result_id_type, strict=False)
        ):
            raise ResultHandleTypeError(
                f"{reshandle!r} does not match expected "
                f"identifier types {self._result_id_type}"
            )

    def process_circuit(
        self,
        circuit: Circuit,
        n_shots: int | None = None,
        valid_check: bool = True,
        **kwargs: KwargTypes,
    ) -> ResultHandle:
        """
        Submit a single circuit to the backend for running. See
        :py:meth:`~pytket.backends.backend.Backend.process_circuits`.
        """

        return self.process_circuits(
            [circuit], n_shots=n_shots, valid_check=valid_check, **kwargs
        )[0]

    @abstractmethod
    def process_circuits(
        self,
        circuits: Sequence[Circuit],
        n_shots: int | Sequence[int] | None = None,
        valid_check: bool = True,
        **kwargs: KwargTypes,
    ) -> list[ResultHandle]:
        """
        Submit circuits to the backend for running. The results will be stored
        in the backend's result cache to be retrieved by the corresponding
        get_<data> method.

        If the `postprocess` keyword argument is set to True, and the backend supports
        the feature (see  :py:attr:`~pytket.backends.backend.Backend.supports_contextual_optimisation`), then contextual
        optimisatioons are applied before running the circuit and retrieved results will
        have any necessary classical postprocessing applied. This is not enabled by
        default.

        Use keyword arguments to specify parameters to be used in submitting circuits
        See specific Backend derived class for available parameters, from the following
        list:

        * `seed`: RNG seed for simulators
        * `postprocess`: if True, apply contextual optimisations

        Note: If a backend is reused many times, the in-memory results cache grows
        indefinitely. Therefore, when processing many circuits on a statevector or
        unitary backend (whose results may occupy significant amounts of memory), it is
        advisable to run :py:meth:`~pytket.backends.backend.Backend.empty_cache` after each result is retrieved.

        :param circuits: Circuits to process on the backend.
        :param n_shots: Number of shots to run per circuit. Optionally, this can be
            a list of shots specifying the number of shots for each circuit separately.
            None is to be used for state/unitary simulators. Defaults to None.
        :param valid_check: Explicitly check that all circuits satisfy all required
            predicates to run on the backend. Defaults to True
        :return: Handles to results for each input circuit, as an iterable in
            the same order as the circuits.
        """
        ...

    @abstractmethod
    def circuit_status(self, handle: ResultHandle) -> CircuitStatus:
        """
        Return a CircuitStatus reporting the status of the circuit execution
        corresponding to the ResultHandle
        """
        ...

    def empty_cache(self) -> None:
        """Manually empty the result cache on the backend."""
        self._cache = {}

    def pop_result(self, handle: ResultHandle) -> dict[str, Any] | None:
        """Remove cache entry corresponding to handle from the cache and return.

        :param handle: ResultHandle object
        :return: Cache entry corresponding to handle, if it was present
        """
        return self._cache.pop(handle, None)

    def get_result(self, handle: ResultHandle, **kwargs: KwargTypes) -> BackendResult:
        """Return a BackendResult corresponding to the handle.

        Use keyword arguments to specify parameters to be used in retrieving results.
        See specific Backend derived class for available parameters, from the following
        list:

        * `timeout`: maximum time to wait for remote job to finish
        * `wait`: polling interval between remote calls to check job status

        :param handle: handle to results
        :return: Results corresponding to handle.
        """
        self._check_handle_type(handle)
        if handle in self._cache and "result" in self._cache[handle]:
            return cast("BackendResult", self._cache[handle]["result"])
        raise CircuitNotRunError(handle)

    def get_results(
        self, handles: Iterable[ResultHandle], **kwargs: KwargTypes
    ) -> list[BackendResult]:
        """Return results corresponding to handles.

        :param handles: Iterable of handles
        :return: List of results

        Keyword arguments are as for :py:meth:`~pytket.backends.backend.Backend.get_result`, and apply to all jobs.
        """
        try:
            return [self.get_result(handle, **kwargs) for handle in handles]
        except ResultHandleTypeError as e:
            try:
                self._check_handle_type(cast("ResultHandle", handles))
            except ResultHandleTypeError:
                raise e  # noqa: B904

            raise ResultHandleTypeError(
                "Possible use of single ResultHandle"
                " where sequence of ResultHandles was expected."
            ) from e

    def run_circuit(
        self,
        circuit: Circuit,
        n_shots: int | None = None,
        valid_check: bool = True,
        **kwargs: KwargTypes,
    ) -> BackendResult:
        """
        Submits a circuit to the backend and returns results

        :param circuit: Circuit to be executed
        :param n_shots: Passed on to :py:meth:`~pytket.backends.backend.Backend.process_circuit`
        :param valid_check: Passed on to :py:meth:`~pytket.backends.backend.Backend.process_circuit`
        :return: Result

        This is a convenience method equivalent to calling
        :py:meth:`~pytket.backends.backend.Backend.process_circuit` followed by :py:meth:`~pytket.backends.backend.Backend.get_result`.
        Any additional keyword arguments are passed on to
        :py:meth:`~pytket.backends.backend.Backend.process_circuit` and :py:meth:`~pytket.backends.backend.Backend.get_result`.
        """
        return self.run_circuits(
            [circuit], n_shots=n_shots, valid_check=valid_check, **kwargs
        )[0]

    def run_circuits(
        self,
        circuits: Sequence[Circuit],
        n_shots: int | Sequence[int] | None = None,
        valid_check: bool = True,
        **kwargs: KwargTypes,
    ) -> list[BackendResult]:
        """
        Submits circuits to the backend and returns results

        :param circuits: Sequence of Circuits to be executed
        :param n_shots: Passed on to :py:meth:`~pytket.backends.backend.Backend.process_circuits`
        :param valid_check: Passed on to :py:meth:`~pytket.backends.backend.Backend.process_circuits`
        :return: List of results

        This is a convenience method equivalent to calling
        :py:meth:`~pytket.backends.backend.Backend.process_circuits` followed by :py:meth:`~pytket.backends.backend.Backend.get_results`.
        Any additional keyword arguments are passed on to
        :py:meth:`~pytket.backends.backend.Backend.process_circuits` and :py:meth:`~pytket.backends.backend.Backend.get_results`.
        """
        handles = self.process_circuits(circuits, n_shots, valid_check, **kwargs)
        results = self.get_results(handles, **kwargs)
        for h in handles:
            self.pop_result(h)
        return results

    def cancel(self, handle: ResultHandle) -> None:
        """
        Cancel a job.

        :param handle: handle to job
        :raises NotImplementedError: If backend does not support job cancellation
        """
        raise NotImplementedError("Backend does not support job cancellation.")

    @property
    def backend_info(self) -> BackendInfo | None:
        """Retrieve all Backend properties in a BackendInfo object, including
        device architecture, supported gate set, gate errors and other hardware-specific
        information.

        :return: The BackendInfo describing this backend if it exists.
        """
        raise NotImplementedError("Backend does not provide any device properties.")

    @classmethod
    def available_devices(cls, **kwargs: Any) -> list[BackendInfo]:
        """Retrieve all available devices as a list of BackendInfo objects, including
        device name, architecture, supported gate set, gate errors,
        and other hardware-specific information.

        :return: A list of BackendInfo objects describing available devices.
        """
        raise NotImplementedError(
            "Backend does not provide information about available devices."
        )

    @property
    def persistent_handles(self) -> bool:
        """
        Whether the backend produces `ResultHandle` objects that can be reused with
        other instances of the backend class.
        """
        return self._persistent_handles

    @property
    def supports_shots(self) -> bool:
        """
        Does this backend support shot result retrieval via
        :py:meth:`~pytket.backends.backendresult.BackendResult.get_shots`.
        """
        return self._supports_shots

    @property
    def supports_counts(self) -> bool:
        """
        Does this backend support counts result retrieval via
        :py:meth:`~pytket.backends.backendresult.BackendResult.get_counts`.
        """
        return self._supports_counts

    @property
    def supports_state(self) -> bool:
        """
        Does this backend support statevector retrieval via
        :py:meth:`~pytket.backends.backendresult.BackendResult.get_state`.
        """
        return self._supports_state

    @property
    def supports_unitary(self) -> bool:
        """
        Does this backend support unitary retrieval via
        :py:meth:`~pytket.backends.backendresult.BackendResult.get_unitary`.
        """
        return self._supports_unitary

    @property
    def supports_density_matrix(self) -> bool:
        """Does this backend support density matrix retrieval via
        :py:meth:`~pytket.backends.backendresult.BackendResult.get_density_matrix`."""
        return self._supports_density_matrix

    @property
    def supports_expectation(self) -> bool:
        """Does this backend support expectation value calculation for operators."""
        return self._supports_expectation

    @property
    def expectation_allows_nonhermitian(self) -> bool:
        """If expectations are supported, is the operator allowed to be non-Hermitan?"""
        return self._expectation_allows_nonhermitian

    @property
    def supports_contextual_optimisation(self) -> bool:
        """Does this backend support contextual optimisation?

        See :py:meth:`process_circuits`."""
        return self._supports_contextual_optimisation

    def _get_extension_module(self) -> ModuleType | None:
        """Return the extension module of the backend if it belongs to a
        pytket-extension package.

        :return: The extension module of the backend if it belongs to a pytket-extension
            package.
        """
        mod_parts = self.__class__.__module__.split(".")[:3]
        if not (mod_parts[0] == "pytket" and mod_parts[1] == "extensions"):
            return None
        return import_module(".".join(mod_parts))

    @property
    def __extension_name__(self) -> str | None:
        """Retrieve the extension name of the backend if it belongs to a
        pytket-extension package.

        :return: The extension name of the backend if it belongs to a pytket-extension
            package.
        """
        try:
            return self._get_extension_module().__extension_name__  # type: ignore
        except AttributeError:
            return None

    @property
    def __extension_version__(self) -> str | None:
        """Retrieve the extension version of the backend if it belongs to a
        pytket-extension package.

        :return: The extension version of the backend if it belongs to a
            pytket-extension package.
        """
        try:
            return self._get_extension_module().__extension_version__  # type: ignore
        except AttributeError:
            return None

    @overload
    @staticmethod
    def _get_n_shots_as_list(
        n_shots: int | Sequence[int | None] | None,
        n_circuits: int,
        optional: Literal[False],
    ) -> list[int]: ...

    @overload
    @staticmethod
    def _get_n_shots_as_list(
        n_shots: int | Sequence[int | None] | None,
        n_circuits: int,
        optional: Literal[True],
        set_zero: Literal[True],
    ) -> list[int]: ...

    @overload
    @staticmethod
    def _get_n_shots_as_list(
        n_shots: int | Sequence[int | None] | None,
        n_circuits: int,
        optional: bool = True,
        set_zero: bool = False,
    ) -> list[int | None] | list[int]: ...

    @staticmethod
    def _get_n_shots_as_list(
        n_shots: int | Sequence[int | None] | None,
        n_circuits: int,
        optional: bool = True,
        set_zero: bool = False,
    ) -> list[int | None] | list[int]:
        """
        Convert any admissible n_shots value into List[Optional[int]] format.

        This validates the n_shots argument for process_circuits. If a single
        value is passed, this value is broadcast to the number of circuits.
        Additional boolean flags control how the argument is validated.
        Raises an exception if n_shots is in an invalid format.

        :param n_shots: The argument to be validated.
        :param n_circuits: Length of the converted argument returned.
        :param optional: Whether n_shots can be None (default: True).
        :param set_zero: Whether None values should be set to 0 (default: False).
        :return: a list of length `n_circuits`, the converted argument
        """

        n_shots_list: list[int | None] = []

        def validate_n_shots(n: int | None) -> bool:
            return optional or (n is not None and n > 0)

        if set_zero and not optional:
            raise ValueError("set_zero cannot be true when optional is false")

        if hasattr(n_shots, "__iter__"):
            assert not isinstance(n_shots, int)
            assert n_shots is not None

            if not all(map(validate_n_shots, n_shots)):
                raise ValueError(
                    "n_shots values are required for all circuits for this backend"
                )
            n_shots_list = list(n_shots)
        else:
            assert n_shots is None or isinstance(n_shots, int)

            if not validate_n_shots(n_shots):
                raise ValueError("Parameter n_shots is required for this backend")
            # convert n_shots to a list
            n_shots_list = [n_shots] * n_circuits

        if len(n_shots_list) != n_circuits:
            raise ValueError("The length of n_shots and circuits must match")

        if set_zero:
            # replace None with 0
            n_shots_list = [n or 0 for n in n_shots_list]

        return n_shots_list

    def get_pauli_expectation_value(
        self, state_circuit: Circuit, pauli: QubitPauliString
    ) -> complex:
        """
        Calculates the expectation value of the given circuit using
        functionality built into the backend.

        Raises an exception if the backend does not provide custom expectation
        value features.
        """
        raise NotImplementedError

    def get_operator_expectation_value(
        self, state_circuit: Circuit, operator: QubitPauliOperator
    ) -> complex:
        """
        Calculates the expectation value of the given circuit with respect to
        the operator using functionality built into the backend.

        Raises an exception if the backend does not provide custom expectation
        value features.
        """
        raise NotImplementedError
