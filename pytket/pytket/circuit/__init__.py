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

"""The circuit module provides an API to interact with the
tket :py:class:`~.Circuit` data structure.
This module is provided in binary form during the PyPI installation."""

from collections.abc import Callable, Sequence
from typing import (
    Any,
    Optional,
    Union,
)

from pytket import wasm
from pytket._tket.circuit import (
    BarrierOp,
    BasisOrder,
    CircBox,
    Circuit,
    ClassicalEvalOp,
    ClassicalOp,
    ClBitVar,
    ClExpr,
    ClExprOp,
    ClOp,
    ClRegVar,
    Command,
    Conditional,
    ConjugationBox,
    CopyBitsOp,
    CustomGate,
    CustomGateDef,
    CXConfigType,
    DiagonalBox,
    DummyBox,
    EdgeType,
    ExpBox,
    MetaOp,
    MultiBitOp,
    MultiplexedRotationBox,
    MultiplexedTensoredU2Box,
    MultiplexedU2Box,
    MultiplexorBox,
    Op,
    OpType,
    PauliExpBox,
    PauliExpCommutingSetBox,
    PauliExpPairBox,
    PhasePolyBox,
    ProjectorAssertionBox,
    QControlBox,
    RangePredicateOp,
    ResourceBounds,
    ResourceData,
    SetBitsOp,
    StabiliserAssertionBox,
    StatePreparationBox,
    TermSequenceBox,
    ToffoliBox,
    ToffoliBoxSynthStrat,
    Unitary1qBox,
    Unitary2qBox,
    Unitary3qBox,
    WASMOp,
    WiredClExpr,
    fresh_symbol,
)
from pytket._tket.pauli import Pauli

# prefixes for assertion bits
from pytket._tket.unit_id import (
    _DEBUG_ONE_REG_PREFIX,
    _DEBUG_ZERO_REG_PREFIX,
    Bit,
    BitRegister,
    Node,
    Qubit,
    QubitRegister,
    UnitID,
    UnitType,
)

from .clexpr import wired_clexpr_from_logic_exp
from .logic_exp import (
    BinaryOp,
    LogicExp,
    Ops,
    if_bit,
    if_not_bit,
    reg_eq,
    reg_geq,
    reg_gt,
    reg_leq,
    reg_lt,
    reg_neq,
)


def add_wasm(  # noqa: PLR0913
    self: Circuit,
    funcname: str,
    filehandler: wasm.WasmModuleHandler,
    list_i: Sequence[int],
    list_o: Sequence[int],
    args: Union[Sequence[int], Sequence[Bit]],  # noqa: UP007
    args_wasm: Sequence[int] | None = None,
    **kwargs: Any,
) -> Circuit:
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

    if args_wasm is None:
        args_wasm = [0]

    for x in list_i:
        if x > filehandler._int_size:  # noqa: SLF001
            raise ValueError(
                f"only functions with i{filehandler._int_size} type are allowed"  # noqa: SLF001
            )

    for x in list_o:
        if x > filehandler._int_size:  # noqa: SLF001
            raise ValueError(
                f"only functions with i{filehandler._int_size} type are allowed"  # noqa: SLF001
            )

    if filehandler.check_function(funcname, len(list_i), len(list_o)):
        if (len(args_wasm)) > 0:
            self._add_w_register(max(args_wasm) + 1)
        return self._add_wasm(
            funcname, str(filehandler), list_i, list_o, args, args_wasm, **kwargs
        )

    raise ValueError(f"{funcname} not found, check {filehandler!r}")


setattr(Circuit, "add_wasm", add_wasm)  # noqa: B010


def add_wasm_to_reg(  # noqa: PLR0913
    self: Circuit,
    funcname: str,
    filehandler: wasm.WasmModuleHandler,
    list_i: Sequence[BitRegister],
    list_o: Sequence[BitRegister],
    args_wasm: Sequence[int] | None = None,
    **kwargs: Any,
) -> Circuit:
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

    if args_wasm is None:
        args_wasm = [0]

    if filehandler.checked:
        for reg in list_i:
            if reg.size > 32:  # noqa: PLR2004
                raise ValueError(
                    """wasm is only supporting 32 bit size registers,
please use only registers of at most 32 bits"""
                )

        for reg in list_o:
            if reg.size > 32:  # noqa: PLR2004
                raise ValueError(
                    """wasm is only supporting 32 bit size registers,
please use only registers of at most 32 bits"""
                )

    # If the filehandler has not been checked we allow it to
    # be added without checking the function arity.
    if not filehandler.checked or filehandler.check_function(
        funcname, len(list_i), len(list_o)
    ):
        if (len(args_wasm)) > 0:
            self._add_w_register(max(args_wasm) + 1)
        return self._add_wasm(
            funcname, str(filehandler), list_i, list_o, args_wasm, **kwargs
        )

    raise ValueError(f"{funcname} not found, check {filehandler!r}")


setattr(Circuit, "add_wasm_to_reg", add_wasm_to_reg)  # noqa: B010


def set_rng_seed(self: Circuit, creg: BitRegister, **kwargs: Any) -> Circuit:
    """
    Seed an RNG from the contents of a classical register.

    The classical register must be exactly 64 bits.

    :param creg: register of classical bits
    :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
     are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
    :return: the new :py:class:`Circuit`
    """
    self._add_r_register(1)
    if creg.size != 64:  # noqa: PLR2004
        raise ValueError(
            f"Register passed to `set_rng_seed()` has size {creg.size} (should be 64)"
        )
    return self._set_rng_seed(creg, 0, **kwargs)


setattr(Circuit, "set_rng_seed", set_rng_seed)  # noqa: B010


def set_rng_bound(self: Circuit, creg: BitRegister, **kwargs: Any) -> Circuit:
    """
    Set an RNG upper bound from the contents of a classical register.

    The classical register must be exactly 32 bits. It encodes the upper bound in
    little-endian binary (least significant bit first). The bound is inclusive.

    :param creg: register of classical bits
    :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
        are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
    :return: the new :py:class:`Circuit`
    """
    self._add_r_register(1)
    if creg.size != 32:  # noqa: PLR2004
        raise ValueError(
            f"Register passed to `set_rng_bound()` has size {creg.size} (should be 32)"
        )
    return self._set_rng_bound(creg, 0, **kwargs)


setattr(Circuit, "set_rng_bound", set_rng_bound)  # noqa: B010


def set_rng_index(self: Circuit, creg: BitRegister, **kwargs: Any) -> Circuit:
    """
    Set an RNG stream index from the contents of a classical register.

    The classical register must be exactly 32 bits. It encodes the index in little-
    endian binary (least significant bit first).

    :param creg: register of classical bits
    :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
        are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
    :return: the new :py:class:`Circuit`
    """
    self._add_r_register(1)
    if creg.size != 32:  # noqa: PLR2004
        raise ValueError(
            f"Register passed to `set_rng_bound()` has size {creg.size} (should be 32)"
        )
    return self._set_rng_index(creg, 0, **kwargs)


setattr(Circuit, "set_rng_index", set_rng_index)  # noqa: B010


def get_rng_num(self: Circuit, creg: BitRegister, **kwargs: Any) -> Circuit:
    """
    Get RNG output into a classical register.

    The classical register must be exactly 32 bits. After the operation it encodes the
    output number in little-endian binary (least significant bit first).

    :param creg: register of classical bits
    :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
        are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
    :return: the new :py:class:`Circuit`
    """
    self._add_r_register(1)
    if creg.size != 32:  # noqa: PLR2004
        raise ValueError(
            f"Register passed to `get_rng_num()` has size {creg.size} (should be 32)"
        )
    return self._get_rng_num(creg, 0, **kwargs)


setattr(Circuit, "get_rng_num", get_rng_num)  # noqa: B010


def get_job_shot_num(self: Circuit, creg: BitRegister, **kwargs: Any) -> Circuit:
    """
    Get shot number into a classical register.

    The classical register must be exactly 32 bits. After the operation it encodes the
    shot number in little-endian binary (least significant bit first).

    :param creg: register of classical bits
    :param kwargs: additional arguments passed to :py:meth:`~.Circuit.add_gate` (allowed parameters
        are `opgroup`,  `condition`, `condition_bits` and `condition_value`)
    :return: the new :py:class:`Circuit`
    """
    self._add_r_register(1)
    if creg.size != 32:  # noqa: PLR2004
        raise ValueError(
            f"Register passed to `get_job_shot_num()` has size {creg.size} (should be 32)"
        )
    return self._get_job_shot_num(creg, **kwargs)


setattr(Circuit, "get_job_shot_num", get_job_shot_num)  # noqa: B010


def add_clexpr_from_logicexp(
    circ: Circuit, exp: LogicExp, output_bits: list[Bit], **kwargs: Any
) -> Circuit:
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
    wexpr, args = wired_clexpr_from_logic_exp(exp, output_bits)
    circ.add_clexpr(wexpr, args, **kwargs)
    return circ


setattr(Circuit, "add_clexpr_from_logicexp", add_clexpr_from_logicexp)  # noqa: B010

__all__ = [
    "_DEBUG_ONE_REG_PREFIX",
    "_DEBUG_ZERO_REG_PREFIX",
    "BasisOrder",
    "BinaryOp",
    "Bit",
    "BitRegister",
    "CXConfigType",
    "CircBox",
    "Circuit",
    "ClBitVar",
    "ClExpr",
    "ClExprOp",
    "ClOp",
    "ClRegVar",
    "Command",
    "Conditional",
    "ConjugationBox",
    "CustomGate",
    "CustomGateDef",
    "DiagonalBox",
    "DummyBox",
    "ExpBox",
    "LogicExp",
    "MultiBitOp",
    "MultiplexedRotationBox",
    "MultiplexedTensoredU2Box",
    "MultiplexedU2Box",
    "MultiplexorBox",
    "Node",
    "Op",
    "OpType",
    "Ops",
    "Pauli",
    "PauliExpBox",
    "PauliExpCommutingSetBox",
    "PauliExpPairBox",
    "PhasePolyBox",
    "ProjectorAssertionBox",
    "QControlBox",
    "Qubit",
    "QubitRegister",
    "RangePredicateOp",
    "ResourceBounds",
    "ResourceData",
    "SetBitsOp",
    "StabiliserAssertionBox",
    "StatePreparationBox",
    "TermSequenceBox",
    "ToffoliBox",
    "ToffoliBoxSynthStrat",
    "UnitID",
    "Unitary1qBox",
    "Unitary2qBox",
    "Unitary3qBox",
    "WiredClExpr",
    "fresh_symbol",
    "if_bit",
    "if_not_bit",
    "reg_eq",
    "reg_geq",
    "reg_gt",
    "reg_leq",
    "reg_lt",
    "reg_neq",
]
