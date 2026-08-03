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

import itertools
import os
import re
import uuid
from collections import OrderedDict
from collections.abc import Callable, Generator, Iterable, Iterator, Sequence
from dataclasses import dataclass
from decimal import Decimal
from importlib import import_module
from itertools import chain, groupby
from typing import Any, NewType, TextIO, TypeVar, Union, cast

from lark import Discard, Lark, Token, Transformer, Tree
from sympy import Expr, Symbol, pi

from pytket._tket.circuit import (
    BarrierOp,
    ClExpr,
    ClExprOp,
    Command,
    Conditional,
    CopyBitsOp,
    MultiBitOp,
    RangePredicateOp,
    SetBitsOp,
    WASMOp,
    WiredClExpr,
)
from pytket._tket.unit_id import _TEMP_BIT_NAME, _TEMP_BIT_REG_BASE
from pytket.circuit import (
    Bit,
    BitRegister,
    Circuit,
    Op,
    OpType,
    Qubit,
    QubitRegister,
    UnitID,
)
from pytket.circuit.clexpr import (
    check_register_alignments,
    has_reg_output,
    wired_clexpr_from_logic_exp,
)
from pytket.circuit.decompose_classical import _int_to_bools
from pytket.circuit.logic_exp import (
    ArgType,
    BitLogicExp,
    BitWiseOp,
    LogicExp,
    PredicateExp,
    RegEq,
    RegLogicExp,
    RegNeg,
    RegWiseOp,
    create_logic_exp,
    create_predicate_exp,
)
from pytket.passes import (
    AutoRebase,
    DecomposeBoxes,
    RemoveRedundancies,
    scratch_reg_resize_pass,
)
from pytket.qasm.grammar import grammar
from pytket.wasm import WasmFileHandler, WasmModuleHandler


class QASMParseError(Exception):
    """Error while parsing QASM input."""

    def __init__(self, msg: str, line: int | None = None, fname: str | None = None):
        self.msg = msg
        self.line = line
        self.fname = fname

        ctx = "" if fname is None else f"\nFile:{fname}: "
        ctx += "" if line is None else f"\nLine:{line}. "

        super().__init__(f"{msg}{ctx}")


class QASMUnsupportedError(Exception):
    """
    Error due to QASM input being incompatible with the supported fragment.
    """


Value = Union[int, float, str]  # noqa: UP007
T = TypeVar("T")

_BITOPS = {op.value for op in BitWiseOp}
_BITOPS.update(("+", "-"))  # both are parsed to XOR
_REGOPS = {op.value for op in RegWiseOp}

Arg = Union[list, str]  # noqa: UP007


NOPARAM_COMMANDS = {
    "CX": OpType.CX,  # built-in gate equivalent to "cx"
    "cx": OpType.CX,
    "x": OpType.X,
    "y": OpType.Y,
    "z": OpType.Z,
    "h": OpType.H,
    "s": OpType.S,
    "sdg": OpType.Sdg,
    "t": OpType.T,
    "tdg": OpType.Tdg,
    "sx": OpType.SX,
    "sxdg": OpType.SXdg,
    "cz": OpType.CZ,
    "cy": OpType.CY,
    "ch": OpType.CH,
    "csx": OpType.CSX,
    "ccx": OpType.CCX,
    "c3x": OpType.CnX,
    "c4x": OpType.CnX,
    "ZZ": OpType.ZZMax,
    "measure": OpType.Measure,
    "reset": OpType.Reset,
    "id": OpType.noop,
    "barrier": OpType.Barrier,
    "swap": OpType.SWAP,
    "cswap": OpType.CSWAP,
}
PARAM_COMMANDS = {
    "p": OpType.U1,  # alias. https://github.com/Qiskit/qiskit-terra/pull/4765
    "u": OpType.U3,  # alias. https://github.com/Qiskit/qiskit-terra/pull/4765
    "U": OpType.U3,  # built-in gate equivalent to "u3"
    "u3": OpType.U3,
    "u2": OpType.U2,
    "u1": OpType.U1,
    "rx": OpType.Rx,
    "rxx": OpType.XXPhase,
    "ry": OpType.Ry,
    "rz": OpType.Rz,
    "RZZ": OpType.ZZPhase,
    "rzz": OpType.ZZPhase,
    "Rz": OpType.Rz,
    "U1q": OpType.PhasedX,
    "crz": OpType.CRz,
    "crx": OpType.CRx,
    "cry": OpType.CRy,
    "cu1": OpType.CU1,
    "cu3": OpType.CU3,
    "Rxxyyzz": OpType.TK2,
}

NOPARAM_EXTRA_COMMANDS = {
    "v": OpType.V,
    "vdg": OpType.Vdg,
    "cv": OpType.CV,
    "cvdg": OpType.CVdg,
    "csxdg": OpType.CSXdg,
    "bridge": OpType.BRIDGE,
    "iswapmax": OpType.ISWAPMax,
    "zzmax": OpType.ZZMax,
    "ecr": OpType.ECR,
    "cs": OpType.CS,
    "csdg": OpType.CSdg,
}

PARAM_EXTRA_COMMANDS = {
    "tk2": OpType.TK2,
    "iswap": OpType.ISWAP,
    "phasediswap": OpType.PhasedISWAP,
    "yyphase": OpType.YYPhase,
    "xxphase3": OpType.XXPhase3,
    "eswap": OpType.ESWAP,
    "fsim": OpType.FSim,
}

N_PARAMS_EXTRA_COMMANDS = {
    OpType.TK2: 3,
    OpType.ISWAP: 1,
    OpType.PhasedISWAP: 2,
    OpType.YYPhase: 1,
    OpType.XXPhase3: 1,
    OpType.ESWAP: 1,
    OpType.FSim: 2,
}

_tk_to_qasm_noparams = {item[1]: item[0] for item in NOPARAM_COMMANDS.items()}
_tk_to_qasm_noparams[OpType.CX] = "cx"  # prefer "cx" to "CX"
_tk_to_qasm_params = {item[1]: item[0] for item in PARAM_COMMANDS.items()}
_tk_to_qasm_params[OpType.U3] = "u3"  # prefer "u3" to "U"
_tk_to_qasm_params[OpType.Rz] = "rz"  # prefer "rz" to "Rz"
_tk_to_qasm_extra_noparams = {
    item[1]: item[0] for item in NOPARAM_EXTRA_COMMANDS.items()
}
_tk_to_qasm_extra_params = {item[1]: item[0] for item in PARAM_EXTRA_COMMANDS.items()}

_classical_gatestr_map = {"AND": "&", "OR": "|", "XOR": "^"}


_all_known_gates = (
    set(NOPARAM_COMMANDS.keys())
    .union(PARAM_COMMANDS.keys())
    .union(PARAM_EXTRA_COMMANDS.keys())
    .union(NOPARAM_EXTRA_COMMANDS.keys())
)
_all_string_maps = {
    key: val.name
    for key, val in chain(
        PARAM_COMMANDS.items(),
        NOPARAM_COMMANDS.items(),
        PARAM_EXTRA_COMMANDS.items(),
        NOPARAM_EXTRA_COMMANDS.items(),
    )
}

unit_regex = re.compile(r"([a-z][a-zA-Z0-9_]*)\[([\d]+)\]")
regname_regex = re.compile(r"^[a-z][a-zA-Z0-9_]*$")


def _extract_reg(var: Token) -> tuple[str, int]:
    match = unit_regex.match(var.value)
    if match is None:
        raise QASMParseError(
            f"Invalid register definition '{var.value}'. Register definitions "
            "must follow the pattern '<name> [<size in integer>]'. "
            "For example, 'q [5]'. QASM register names must begin with a "
            "lowercase letter and may only contain lowercase and uppercase "
            "letters, numbers, and underscores."
        )
    return match.group(1), int(match.group(2))


def _load_include_module(
    header_name: str, do_filter: bool, decls_only: bool
) -> dict[str, dict]:
    try:
        if decls_only:
            include_def: dict[str, dict] = import_module(  # noqa: SLF001
                f"pytket.qasm.includes._{header_name}_decls"
            )._INCLUDE_DECLS
        else:
            include_def = import_module(  # noqa: SLF001
                f"pytket.qasm.includes._{header_name}_defs"
            )._INCLUDE_DEFS
    except ModuleNotFoundError as e:
        raise QASMParseError(
            f"Header {header_name} is not known and cannot be loaded."
        ) from e
    return {
        gate: include_def[gate]
        for gate in include_def
        if not do_filter or gate not in _all_known_gates
    }


def _bin_par_exp(op: "str") -> Callable[["_CircuitTransformer", list[str]], str]:
    def f(self: "_CircuitTransformer", vals: list[str]) -> str:
        return f"({vals[0]} {op} {vals[1]})"

    return f


def _un_par_exp(op: "str") -> Callable[["_CircuitTransformer", list[str]], str]:
    def f(self: "_CircuitTransformer", vals: list[str]) -> str:
        return f"({op}{vals[0]})"

    return f


def _un_call_exp(op: "str") -> Callable[["_CircuitTransformer", list[str]], str]:
    def f(self: "_CircuitTransformer", vals: list[str]) -> str:
        return f"{op}({vals[0]})"

    return f


def _hashable_uid(arg: list) -> tuple[str, int]:
    return arg[0], arg[1][0]


def _can_treat_as_bit(arg: ArgType) -> bool:
    if isinstance(arg, Bit | BitLogicExp):
        return True
    if isinstance(arg, int):
        return arg in (0, 1)
    if isinstance(arg, BitRegister):
        return arg.size == 1
    return False


Reg = NewType("Reg", str)
CommandDict = dict[str, Any]


@dataclass
class _ParsMap:
    pars: Iterable[str]

    def __iter__(self) -> Iterable[str]:
        return self.pars


class _CircuitTransformer(Transformer):
    def __init__(
        self,
        return_gate_dict: bool = False,
        maxwidth: int = 32,
    ) -> None:
        super().__init__()
        self.q_registers: dict[str, int] = {}
        self.c_registers: dict[str, int] = {}
        self.gate_dict: dict[str, dict] = {}
        self.wasm: WasmModuleHandler | None = None
        self.include = ""
        self.return_gate_dict = return_gate_dict
        self.maxwidth = maxwidth

    def _fresh_temp_bit(self) -> list:
        if _TEMP_BIT_NAME in self.c_registers:
            idx = self.c_registers[_TEMP_BIT_NAME]
        else:
            idx = 0
        self.c_registers[_TEMP_BIT_NAME] = idx + 1

        return [_TEMP_BIT_NAME, [idx]]

    def _reset_context(self, reset_wasm: bool = True) -> None:
        self.q_registers = {}
        self.c_registers = {}
        self.gate_dict = {}
        self.include = ""
        if reset_wasm:
            self.wasm = None

    def _get_reg(self, name: str) -> Reg:
        return Reg(name)

    def _get_uid(self, iarg: Token) -> list:
        name, idx = _extract_reg(iarg)
        return [name, [idx]]

    def _get_arg(self, arg: Token) -> Arg:
        if arg.type == "IARG":
            return self._get_uid(arg)
        return self._get_reg(arg.value)

    def unroll_all_args(self, args: Iterable[Arg]) -> Iterator[list[Any]]:
        for arg in args:
            if isinstance(arg, str):
                size = (
                    self.q_registers[arg]
                    if arg in self.q_registers
                    else self.c_registers[arg]
                )
                yield [[arg, [idx]] for idx in range(size)]
            else:
                yield [arg]

    def margs(self, tree: Iterable[Token]) -> Iterator[Arg]:
        return map(self._get_arg, tree)

    def iargs(self, tree: Iterable[Token]) -> Iterator[list]:
        return map(self._get_uid, tree)

    def args(self, tree: Iterable[Token]) -> Iterator[list]:
        return ([tok.value, [0]] for tok in tree)

    def creg(self, tree: list[Token]) -> None:
        name, size = _extract_reg(tree[0])
        if size > self.maxwidth:
            raise QASMUnsupportedError(
                f"Circuit contains classical register {name} of size {size} > "
                f"{self.maxwidth}: try setting the `maxwidth` parameter to a larger "
                "value."
            )
        self.c_registers[Reg(name)] = size

    def qreg(self, tree: list[Token]) -> None:
        name, size = _extract_reg(tree[0])
        self.q_registers[Reg(name)] = size

    def meas(self, tree: list[Token]) -> Iterable[CommandDict]:
        for args in zip(*self.unroll_all_args(self.margs(tree)), strict=False):
            yield {"args": list(args), "op": {"type": "Measure"}}

    def barr(self, tree: list[Arg]) -> Iterable[CommandDict]:
        args = [q for qs in self.unroll_all_args(tree[0]) for q in qs]
        signature: list[str] = []
        for arg in args:
            if arg[0] in self.c_registers:
                signature.append("C")
            elif arg[0] in self.q_registers:
                signature.append("Q")
            else:
                raise QASMParseError(
                    "UnitID " + str(arg) + " in Barrier arguments is not declared."
                )
        yield {
            "args": args,
            "op": {"signature": signature, "type": "Barrier"},
        }

    def reset(self, tree: list[Token]) -> Iterable[CommandDict]:
        for qb in next(self.unroll_all_args(self.margs(tree))):
            yield {"args": [qb], "op": {"type": "Reset"}}

    def pars(self, vals: Iterable[str]) -> _ParsMap:
        return _ParsMap(map(str, vals))

    def mixedcall(self, tree: list) -> Iterator[CommandDict]:
        child_iter = iter(tree)

        optoken = next(child_iter)
        opstr = optoken.value
        next_tree = next(child_iter)
        try:
            args = next(child_iter)
            pars = cast("_ParsMap", next_tree).pars
        except StopIteration:
            args = next_tree
            pars = []

        treat_as_barrier = [
            "sleep",
            "order2",
            "order3",
            "order4",
            "order5",
            "order6",
            "order7",
            "order8",
            "order9",
            "order10",
            "order11",
            "order12",
            "order13",
            "order14",
            "order15",
            "order16",
            "order17",
            "order18",
            "order19",
            "order20",
            "group2",
            "group3",
            "group4",
            "group5",
            "group6",
            "group7",
            "group8",
            "group9",
            "group10",
            "group11",
            "group12",
            "group13",
            "group14",
            "group15",
            "group16",
            "group17",
            "group18",
            "group19",
            "group20",
        ]
        # other opaque gates, which are not handled as barrier
        # ["RZZ", "Rxxyyzz", "Rxxyyzz_zphase", "cu", "cp", "rccx", "rc3x", "c3sqrtx"]

        args = list(args)

        if opstr in treat_as_barrier:
            params = [f"{par}" for par in pars]
        else:
            params = [f"({par})/pi" for par in pars]
        if opstr in self.gate_dict:
            op: dict[str, Any] = {}
            if opstr in treat_as_barrier:
                op["type"] = "Barrier"
                param_sorted = ",".join(params)

                op["data"] = f"{opstr}({param_sorted})"

                op["signature"] = [arg[0] for arg in args]
            else:
                gdef = self.gate_dict[opstr]
                op["type"] = "CustomGate"
                box = {
                    "type": "CustomGate",
                    "id": str(uuid.uuid4()),
                    "gate": gdef,
                }
                box["params"] = params
                op["box"] = box
                params = []  # to stop duplication in to op
        else:
            try:
                optype = _all_string_maps[opstr]
            except KeyError as e:
                raise QASMParseError(
                    f"Cannot parse gate of type: {opstr}", optoken.line
                ) from e
            op = {"type": optype}
            if params:
                op["params"] = params
            # Operations needing special handling:
            if optype.startswith("Cn"):
                # n-controlled rotations have variable signature
                op["n_qb"] = len(args)
            elif optype == "Barrier":
                op["signature"] = ["Q"] * len(args)

        for arg in zip(*self.unroll_all_args(args), strict=False):
            yield {"args": list(arg), "op": op}

    def gatecall(self, tree: list) -> Iterable[CommandDict]:
        return self.mixedcall(tree)

    def exp_args(self, tree: Iterable[Token]) -> Iterable[Reg]:
        for arg in tree:
            if arg.type == "ARG":
                yield self._get_reg(arg.value)
            else:
                raise QASMParseError(
                    "Non register arguments not supported for extern call.", arg.line
                )

    def _logic_exp(self, tree: list, opstr: str) -> LogicExp:
        args, _line = self._get_logic_args(tree)
        openum: type[BitWiseOp] | type[RegWiseOp]
        if opstr in _BITOPS and opstr not in _REGOPS:
            openum = BitWiseOp
        elif (opstr in _REGOPS and opstr not in _BITOPS) or all(
            isinstance(arg, int) for arg in args
        ):
            openum = RegWiseOp
        elif all(_can_treat_as_bit(arg) for arg in args):
            openum = BitWiseOp
        else:
            openum = RegWiseOp
        if openum is BitWiseOp and opstr in ("+", "-"):
            op: BitWiseOp | RegWiseOp = BitWiseOp.XOR
        else:
            op = openum(opstr)
        return create_logic_exp(op, args)

    def _get_logic_args(
        self, tree: Sequence[Token | LogicExp]
    ) -> tuple[list[LogicExp | Bit | BitRegister | int], int | None]:
        args: list[LogicExp | Bit | BitRegister | int] = []
        line = None
        for tok in tree:
            if isinstance(tok, LogicExp):
                args.append(tok)
            elif isinstance(tok, Token):
                line = tok.line
                if tok.type == "INT":
                    args.append(int(tok.value))
                elif tok.type == "IARG":
                    args.append(Bit(*_extract_reg(tok)))
                elif tok.type == "ARG":
                    args.append(BitRegister(tok.value, self.c_registers[tok.value]))
                else:
                    raise QASMParseError(f"Could not pass argument {tok}")
            else:
                raise QASMParseError(f"Could not pass argument {tok}")
        return args, line

    par_add = _bin_par_exp("+")
    par_sub = _bin_par_exp("-")
    par_mul = _bin_par_exp("*")
    par_div = _bin_par_exp("/")
    par_pow = _bin_par_exp("**")

    par_neg = _un_par_exp("-")

    sqrt = _un_call_exp("sqrt")
    sin = _un_call_exp("sin")
    cos = _un_call_exp("cos")
    tan = _un_call_exp("tan")
    ln = _un_call_exp("ln")

    b_and = lambda self, tree: self._logic_exp(tree, "&")
    b_not = lambda self, tree: self._logic_exp(tree, "~")
    b_or = lambda self, tree: self._logic_exp(tree, "|")
    xor = lambda self, tree: self._logic_exp(tree, "^")
    lshift = lambda self, tree: self._logic_exp(tree, "<<")
    rshift = lambda self, tree: self._logic_exp(tree, ">>")
    add = lambda self, tree: self._logic_exp(tree, "+")
    sub = lambda self, tree: self._logic_exp(tree, "-")
    mul = lambda self, tree: self._logic_exp(tree, "*")
    div = lambda self, tree: self._logic_exp(tree, "/")
    ipow = lambda self, tree: self._logic_exp(tree, "**")

    def neg(self, tree: list[Token | LogicExp]) -> RegNeg:
        arg = self._get_logic_args(tree)[0][0]
        assert isinstance(arg, RegLogicExp | BitRegister | int)
        return RegNeg(arg)

    def cond(self, tree: list[Token]) -> PredicateExp:
        op: BitWiseOp | RegWiseOp
        arg: Bit | BitRegister
        if tree[1].type == "IARG":
            arg = Bit(*_extract_reg(tree[1]))
            op = BitWiseOp(str(tree[2]))
        else:
            arg = BitRegister(tree[1].value, self.c_registers[tree[1].value])
            op = RegWiseOp(str(tree[2]))

        return create_predicate_exp(op, [arg, int(tree[3].value)])

    def ifc(self, tree: Sequence) -> Iterable[CommandDict]:
        condition = cast("PredicateExp", tree[0])

        var, val = condition.args
        condition_bits = []

        if isinstance(var, Bit):
            assert condition.op in (BitWiseOp.EQ, BitWiseOp.NEQ)
            assert isinstance(val, int)
            assert val in (0, 1)
            if condition.op == BitWiseOp.NEQ:
                condition.op = BitWiseOp.EQ
                val = 1 ^ val
            condition_bits = [var.to_list()]

        else:
            assert isinstance(var, BitRegister)
            reg_bits = next(self.unroll_all_args([var.name]))
            if isinstance(condition, RegEq):
                # special case for base qasm
                condition_bits = reg_bits
            else:
                pred_val = cast("int", val)
                minval = 0
                maxval = (1 << self.maxwidth) - 1
                if condition.op == RegWiseOp.LT:
                    maxval = pred_val - 1
                elif condition.op == RegWiseOp.GT:
                    minval = pred_val + 1
                if condition.op in (RegWiseOp.LEQ, RegWiseOp.EQ, RegWiseOp.NEQ):
                    maxval = pred_val
                if condition.op in (RegWiseOp.GEQ, RegWiseOp.EQ, RegWiseOp.NEQ):
                    minval = pred_val

                condition_bit = self._fresh_temp_bit()
                yield {
                    "args": [*reg_bits, condition_bit],
                    "op": {
                        "classical": {
                            "lower": minval,
                            "n_i": len(reg_bits),
                            "upper": maxval,
                        },
                        "type": "RangePredicate",
                    },
                }
                condition_bits = [condition_bit]
                val = int(condition.op != RegWiseOp.NEQ)

        for com in filter(lambda x: x is not None and x is not Discard, tree[1]):
            com["args"] = condition_bits + com["args"]
            com["op"] = {
                "conditional": {
                    "op": com["op"],
                    "value": val,
                    "width": len(condition_bits),
                },
                "type": "Conditional",
            }

            yield com

    def cop(self, tree: Sequence[Iterable[CommandDict]]) -> Iterable[CommandDict]:
        return tree[0]

    def _calc_exp_io(
        self, exp: LogicExp, out_args: list
    ) -> tuple[list[list], dict[str, Any]]:
        all_inps: list[tuple[str, int]] = []
        for inp in exp.all_inputs_ordered():
            if isinstance(inp, Bit):
                all_inps.append((inp.reg_name, inp.index[0]))
            else:
                assert isinstance(inp, BitRegister)
                for bit in inp:
                    all_inps.append((bit.reg_name, bit.index[0]))  # noqa: PERF401
        outs = (_hashable_uid(arg) for arg in out_args)
        o = []
        io = []
        for out in outs:
            if out in all_inps:
                all_inps.remove(out)
                io.append(out)
            else:
                o.append(out)

        exp_args = [[x[0], [x[1]]] for x in chain.from_iterable((all_inps, io, o))]
        numbers_dict = {
            "n_i": len(all_inps),
            "n_io": len(io),
            "n_o": len(o),
        }
        return exp_args, numbers_dict

    def _clexpr_dict(self, exp: LogicExp, out_args: list[list]) -> CommandDict:
        # Convert the LogicExp to a serialization of a command containing the
        # corresponding ClExprOp.
        wexpr, args = wired_clexpr_from_logic_exp(
            exp, [Bit.from_list(arg) for arg in out_args]
        )
        return {
            "op": {
                "type": "ClExpr",
                "expr": wexpr.to_dict(),
            },
            "args": [arg.to_list() for arg in args],
        }

    def assign(self, tree: list) -> Iterable[CommandDict]:  # noqa: PLR0912
        child_iter = iter(tree)
        out_args = list(next(child_iter))
        args_uids = list(self.unroll_all_args(out_args))

        exp_tree = next(child_iter)

        exp: str | list | LogicExp | int = ""
        line = None
        if isinstance(exp_tree, Token):
            if exp_tree.type == "INT":
                exp = int(exp_tree.value)
            elif exp_tree.type in ("ARG", "IARG"):
                exp = self._get_arg(exp_tree)
            line = exp_tree.line
        elif isinstance(exp_tree, Generator):
            # assume to be extern (wasm) call
            chained_uids = list(chain.from_iterable(args_uids))
            com = next(exp_tree)
            com["args"].pop()  # remove the wasmstate from the args
            com["args"] += chained_uids
            com["args"].append(["_w", [0]])
            com["op"]["wasm"]["n"] += len(chained_uids)
            com["op"]["wasm"]["width_o_parameter"] = [
                self.c_registers[reg] for reg in out_args
            ]

            yield com
            return
        else:
            exp = exp_tree

        assert len(out_args) == 1
        out_arg = out_args[0]
        args = args_uids[0]
        if isinstance(out_arg, list):
            if isinstance(exp, LogicExp):
                yield self._clexpr_dict(exp, args)
            elif isinstance(exp, int | bool):
                assert exp in (0, 1, True, False)
                yield {
                    "args": args,
                    "op": {"classical": {"values": [bool(exp)]}, "type": "SetBits"},
                }
            elif isinstance(exp, list):
                yield {
                    "args": [exp, *args],
                    "op": {"classical": {"n_i": 1}, "type": "CopyBits"},
                }
            else:
                raise QASMParseError(f"Unexpected expression in assignment {exp}", line)
        else:
            reg = out_arg
            if isinstance(exp, RegLogicExp):
                yield self._clexpr_dict(exp, args)
            elif isinstance(exp, BitLogicExp):
                yield self._clexpr_dict(exp, args[:1])
            elif isinstance(exp, int):
                yield {
                    "args": args,
                    "op": {
                        "classical": {
                            "values": _int_to_bools(exp, self.c_registers[reg])
                        },
                        "type": "SetBits",
                    },
                }

            elif isinstance(exp, str):
                width = min(self.c_registers[exp], len(args))
                yield {
                    "args": [[exp, [i]] for i in range(width)] + args[:width],
                    "op": {"classical": {"n_i": width}, "type": "CopyBits"},
                }

            else:
                raise QASMParseError(f"Unexpected expression in assignment {exp}", line)

    def extern(self, tree: list[Any]) -> Any:
        # TODO parse extern defs
        return Discard

    def ccall(self, tree: list) -> Iterable[CommandDict]:
        return self.cce_call(tree)

    def cce_call(self, tree: list) -> Iterable[CommandDict]:
        name = tree[0].value
        params = list(tree[1])
        if self.wasm is None:
            raise QASMParseError(
                "Cannot include extern calls without a wasm module specified.",
                tree[0].line,
            )
        n_i_vec = [self.c_registers[reg] for reg in params]

        wasm_args = list(chain.from_iterable(self.unroll_all_args(params)))

        wasm_args.append(["_w", [0]])

        yield {
            "args": wasm_args,
            "op": {
                "type": "WASM",
                "wasm": {
                    "func_name": name,
                    "ww_n": 1,
                    "n": sum(n_i_vec),
                    "width_i_parameter": n_i_vec,
                    "width_o_parameter": [],  # this will be set in the assign function
                    "wasm_file_uid": str(self.wasm),
                },
            },
        }

    def transform(self, tree: Tree) -> dict[str, Any]:
        self._reset_context()
        return cast("dict[str, Any]", super().transform(tree))

    def gdef(self, tree: list) -> None:
        child_iter = iter(tree)
        gate = next(child_iter).value
        next_tree = next(child_iter)
        symbols, args = [], []
        if isinstance(next_tree, _ParsMap):
            symbols = list(next_tree.pars)
            args = list(next(child_iter))
        else:
            args = list(next_tree)

        symbol_map = {sym: sym * pi for sym in map(Symbol, symbols)}
        rename_map = {Qubit.from_list(qb): Qubit("q", i) for i, qb in enumerate(args)}

        new = _CircuitTransformer(maxwidth=self.maxwidth)
        circ_dict = new.prog(child_iter)

        circ_dict["qubits"] = args
        gate_circ = Circuit.from_dict(circ_dict)

        # check to see whether gate definition was generated by pytket converter
        # if true, add op as pytket Op
        existing_op: bool = False
        # NOPARAM_EXTRA_COMMANDS and PARAM_EXTRA_COMMANDS are
        # gates that aren't in the standard qasm spec but in the standard TKET
        # optypes
        if gate in NOPARAM_EXTRA_COMMANDS:
            qubit_args = [
                Qubit(gate + "q" + str(index), 0) for index in list(range(len(args)))
            ]
            comparison_circ = _get_gate_circuit(
                NOPARAM_EXTRA_COMMANDS[gate], qubit_args
            )
            if circuit_to_qasm_str(
                comparison_circ, maxwidth=self.maxwidth
            ) == circuit_to_qasm_str(gate_circ, maxwidth=self.maxwidth):
                existing_op = True
        elif gate in PARAM_EXTRA_COMMANDS:
            optype = PARAM_EXTRA_COMMANDS[gate]
            # we check this here, as _get_gate_circuit will find issue if it isn't true
            # the later existing_op=all check will make sure it's the same circuit later
            if len(symbols) != N_PARAMS_EXTRA_COMMANDS[optype]:
                existing_op = False
            else:
                qubit_args = [
                    Qubit(gate + "q" + str(index), 0) for index in range(len(args))
                ]
                comparison_circ = _get_gate_circuit(
                    optype,
                    qubit_args,
                    [
                        Symbol("param" + str(index) + "/pi")
                        for index in range(len(symbols))
                    ],
                )
                # checks that each command has same string
                existing_op = all(
                    str(g) == str(c)
                    for g, c in zip(
                        gate_circ.get_commands(),
                        comparison_circ.get_commands(),
                        strict=False,
                    )
                )
        if not existing_op:
            gate_circ.symbol_substitution(symbol_map)
            gate_circ.rename_units(cast("dict[UnitID, UnitID]", rename_map))
            self.gate_dict[gate] = {
                "definition": gate_circ.to_dict(),
                "args": symbols,
                "name": gate,
            }

    opaq = gdef

    def oqasm(self, tree: list) -> Any:
        return Discard

    def incl(self, tree: list[Token]) -> None:
        self.include = str(tree[0].value).split(".")[0]
        self.gate_dict.update(_load_include_module(self.include, True, False))

    def prog(self, tree: Iterable) -> dict[str, Any]:
        outdict: dict[str, Any] = {
            "commands": list(
                chain.from_iterable(
                    filter(lambda x: x is not None and x is not Discard, tree)
                )
            )
        }
        if self.return_gate_dict:
            return self.gate_dict
        outdict["qubits"] = [
            [reg, [i]] for reg, size in self.q_registers.items() for i in range(size)
        ]
        outdict["bits"] = [
            [reg, [i]] for reg, size in self.c_registers.items() for i in range(size)
        ]
        outdict["implicit_permutation"] = [[q, q] for q in outdict["qubits"]]
        outdict["phase"] = "0.0"
        self._reset_context()
        return outdict


def _parser(maxwidth: int) -> Lark:
    return Lark(
        grammar,
        start="prog",
        debug=False,
        parser="lalr",
        cache=True,
        transformer=_CircuitTransformer(maxwidth=maxwidth),
    )


g_parser = None
g_maxwidth = 32


def _set_parser(maxwidth: int) -> None:
    global g_parser, g_maxwidth  # noqa: PLW0603
    if (g_parser is None) or (g_maxwidth != maxwidth):  # type: ignore
        g_parser = _parser(maxwidth=maxwidth)
        g_maxwidth = maxwidth


def circuit_from_qasm(
    input_file: Union[str, "os.PathLike[Any]"],
    encoding: str = "utf-8",
    maxwidth: int = 32,
) -> Circuit:
    """A method to generate a tket Circuit from a qasm file.

    :param input_file: path to qasm file; filename must have ``.qasm`` extension
    :param encoding: file encoding (default utf-8)
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: pytket circuit
    """
    ext = os.path.splitext(input_file)[-1]
    if ext != ".qasm":
        raise TypeError("Can only convert .qasm files")
    with open(input_file, encoding=encoding) as f:
        try:
            circ = circuit_from_qasm_io(f, maxwidth=maxwidth)
        except QASMParseError as e:
            raise QASMParseError(e.msg, e.line, str(input_file))  # noqa: B904
    return circ


def circuit_from_qasm_str(qasm_str: str, maxwidth: int = 32) -> Circuit:
    """A method to generate a tket Circuit from a qasm string.

    :param qasm_str: qasm string
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: pytket circuit
    """
    global g_parser  # noqa: PLW0602
    _set_parser(maxwidth=maxwidth)
    assert g_parser is not None
    cast("_CircuitTransformer", g_parser.options.transformer)._reset_context(  # noqa: SLF001
        reset_wasm=False
    )
    circ = Circuit.from_dict(g_parser.parse(qasm_str))  # type: ignore[arg-type]
    cpass = scratch_reg_resize_pass(maxwidth)
    cpass.apply(circ)
    return circ


def circuit_from_qasm_io(stream_in: TextIO, maxwidth: int = 32) -> Circuit:
    """A method to generate a tket Circuit from a qasm text stream"""
    return circuit_from_qasm_str(stream_in.read(), maxwidth=maxwidth)


def circuit_from_qasm_wasm(
    input_file: Union[str, "os.PathLike[Any]"],
    wasm_file: Union[str, "os.PathLike[Any]"],
    encoding: str = "utf-8",
    maxwidth: int = 32,
) -> Circuit:
    """A method to generate a tket Circuit from a qasm string and external WASM module.

    :param input_file: path to qasm file; filename must have ``.qasm`` extension
    :param wasm_file: path to WASM file containing functions used in qasm
    :param encoding: encoding of qasm file (default utf-8)
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: pytket circuit
    """
    global g_parser  # noqa: PLW0602
    wasm_module = WasmFileHandler(str(wasm_file))
    _set_parser(maxwidth=maxwidth)
    assert g_parser is not None
    cast("_CircuitTransformer", g_parser.options.transformer).wasm = wasm_module
    return circuit_from_qasm(input_file, encoding=encoding, maxwidth=maxwidth)


def circuit_from_qasm_str_wasm(
    qasm_str: str,
    wasm: bytes,
    maxwidth: int = 32,
) -> Circuit:
    """A method to generate a tket Circuit from a qasm string and external WASM module.

    :param qasm_str: qasm string
    :param wasm: bytes of the corresponding wasm module
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: pytket circuit
    """
    global g_parser  # noqa: PLW0602
    wasm_module = WasmModuleHandler(wasm)
    _set_parser(maxwidth=maxwidth)
    assert g_parser is not None
    cast("_CircuitTransformer", g_parser.options.transformer).wasm = wasm_module
    return circuit_from_qasm_str(qasm_str, maxwidth=maxwidth)


def circuit_from_qasm_str_wasmmh(
    qasm_str: str,
    wasmmh: WasmModuleHandler,
    maxwidth: int = 32,
) -> Circuit:
    """A method to generate a tket Circuit from a qasm string and external WASM module.

    :param qasm_str: qasm string
    :param wasmmh: handler corresponding to the wasm module
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: pytket circuit
    """
    global g_parser  # noqa: PLW0602
    _set_parser(maxwidth=maxwidth)
    assert g_parser is not None
    cast("_CircuitTransformer", g_parser.options.transformer).wasm = wasmmh
    return circuit_from_qasm_str(qasm_str, maxwidth=maxwidth)


def circuit_to_qasm(
    circ: Circuit, output_file: str, header: str = "qelib1", maxwidth: int = 32
) -> None:
    """Convert a Circuit to QASM and write it to a file.

    Classical bits in the pytket circuit must be singly-indexed.

    Note that this will not account for implicit qubit permutations in the Circuit.

    :param circ: pytket circuit
    :param output_file: path to output qasm file
    :param header: qasm header (default "qelib1")
    :param maxwidth: maximum allowed width of classical registers (default 32)
    """
    with open(output_file, "w") as out:
        circuit_to_qasm_io(circ, out, header=header, maxwidth=maxwidth)


def _filtered_qasm_str(qasm: str) -> str:
    # remove any c registers starting with _TEMP_BIT_NAME
    # that are not being used somewhere else
    lines = qasm.split("\n")
    def_matcher = re.compile(rf"creg ({_TEMP_BIT_NAME}\_*\d*)\[\d+\]")
    arg_matcher = re.compile(rf"({_TEMP_BIT_NAME}\_*\d*)\[\d+\]")
    unused_regs = {}
    for i, line in enumerate(lines):
        if reg := def_matcher.match(line):
            # Mark a reg temporarily as unused
            unused_regs[reg.group(1)] = i
        elif args := arg_matcher.findall(line):
            # If the line contains scratch bits that are used as arguments
            # mark these regs as used
            for arg in args:
                if arg in unused_regs:
                    unused_regs.pop(arg)
    # remove unused reg defs
    redundant_lines = sorted(unused_regs.values(), reverse=True)
    for line_index in redundant_lines:
        del lines[line_index]
    return "\n".join(lines)


def _is_empty_customgate(op: Op) -> bool:
    return op.type == OpType.CustomGate and op.get_circuit().n_gates == 0  # type: ignore


def _check_can_convert_circuit(circ: Circuit, header: str, maxwidth: int) -> None:
    if any(
        circ.n_gates_of_type(typ)
        for typ in (
            OpType.RangePredicate,
            OpType.MultiBit,
            OpType.ExplicitPredicate,
            OpType.ExplicitModifier,
            OpType.SetBits,
            OpType.CopyBits,
        )
    ) and (not _hqs_header(header)):
        raise QASMUnsupportedError(
            "Complex classical gates not supported with qelib1: try converting with "
            "`header=hqslib1`"
        )
    if any(bit.index[0] >= maxwidth for bit in circ.bits):
        raise QASMUnsupportedError(
            f"Circuit contains a classical register larger than {maxwidth}: try "
            "setting the `maxwidth` parameter to a higher value."
        )
    set_circ_register = {creg.name for creg in circ.c_registers}
    for b in circ.bits:
        if b.reg_name not in set_circ_register:
            raise QASMUnsupportedError(
                f"Circuit contains an invalid classical register {b.reg_name}."
            )
    # Empty CustomGates should have been removed by DecomposeBoxes().
    for cmd in circ:
        assert not _is_empty_customgate(cmd.op)
        if isinstance(cmd.op, Conditional):
            assert not _is_empty_customgate(cmd.op.op)
    if not check_register_alignments(circ):
        raise QASMUnsupportedError(
            "Circuit contains classical expressions on registers whose arguments or "
            "outputs are not register-aligned."
        )


def circuit_to_qasm_str(
    circ: Circuit,
    header: str = "qelib1",
    include_gate_defs: set[str] | None = None,
    maxwidth: int = 32,
) -> str:
    """Convert a Circuit to QASM and return the string.

    Classical bits in the pytket circuit must be singly-indexed.

    Note that this will not account for implicit qubit permutations in the Circuit.

    :param circ: pytket circuit
    :param header: qasm header (default "qelib1")
    :param output_file: path to output qasm file
    :param include_gate_defs: optional set of gates to include
    :param maxwidth: maximum allowed width of classical registers (default 32)
    :return: qasm string
    """

    qasm_writer = _QasmWriter(
        circ.qubits, circ.bits, header, include_gate_defs, maxwidth
    )
    circ1 = circ.copy()
    DecomposeBoxes().apply(circ1)
    _check_can_convert_circuit(circ1, header, maxwidth)
    for command in circ1:
        assert isinstance(command, Command)
        qasm_writer.add_op(command.op, command.args)
    return qasm_writer.finalize()


TypeReg = TypeVar("TypeReg", BitRegister, QubitRegister)


def _retrieve_registers(
    units: list[UnitID], reg_type: type[TypeReg]
) -> dict[str, TypeReg]:
    if any(len(unit.index) != 1 for unit in units):
        raise NotImplementedError("OPENQASM registers must use a single index")
    maxunits = map(lambda x: max(x[1]), groupby(units, key=lambda un: un.reg_name))  # noqa: C417
    return {
        maxunit.reg_name: reg_type(maxunit.reg_name, maxunit.index[0] + 1)
        for maxunit in maxunits
    }


def _parse_range(minval: int, maxval: int, maxwidth: int) -> tuple[str, int]:
    if maxwidth > 64:  # noqa: PLR2004
        raise NotImplementedError("Register width exceeds maximum of 64.")

    REGMAX = (1 << maxwidth) - 1

    if minval > REGMAX:
        raise NotImplementedError("Range's lower bound exceeds register capacity.")
    if minval > maxval:
        raise NotImplementedError("Range's lower bound exceeds upper bound.")
    maxval = min(maxval, REGMAX)

    if minval == maxval:
        return ("==", minval)
    if minval == 0:
        return ("<=", maxval)
    if maxval == REGMAX:
        return (">=", minval)
    raise NotImplementedError("Range can only be bounded on one side.")


def _negate_comparator(comparator: str) -> str:
    if comparator == "==":
        return "!="
    if comparator == "!=":
        return "=="
    if comparator == "<=":
        return ">"
    if comparator == ">":
        return "<="
    if comparator == ">=":
        return "<"
    assert comparator == "<"
    return ">="


def _get_optype_and_params(op: Op) -> tuple[OpType, list[float | Expr] | None]:
    optype = op.type
    params = (
        op.params
        if (optype in _tk_to_qasm_params) or (optype in _tk_to_qasm_extra_params)
        else None
    )
    if optype == OpType.TK1:
        # convert to U3
        optype = OpType.U3
        params = [op.params[1], op.params[0] - 0.5, op.params[2] + 0.5]
    elif optype == OpType.CustomGate:
        params = op.params
    return optype, params


def _get_gate_circuit(
    optype: OpType, qubits: list[Qubit], symbols: list[Symbol] | None = None
) -> Circuit:
    # create Circuit for constructing qasm from
    unitids = cast("list[UnitID]", qubits)
    gate_circ = Circuit()
    for q in qubits:
        gate_circ.add_qubit(q)
    if symbols:
        exprs = [symbol.as_expr() for symbol in symbols]
        gate_circ.add_gate(optype, exprs, unitids)
    else:
        gate_circ.add_gate(optype, unitids)
    AutoRebase({OpType.CX, OpType.U3}).apply(gate_circ)
    RemoveRedundancies().apply(gate_circ)

    return gate_circ


def _hqs_header(header: str) -> bool:
    return header in ["hqslib1", "hqslib1_dev"]


@dataclass
class _ConditionString:
    variable: str  # variable, e.g. "c[1]"
    comparator: str  # comparator, e.g. "=="
    value: int  # value, e.g. "1"


class _LabelledStringList:
    """
    Wrapper class for an ordered sequence of strings, where each string has a unique
    label, returned when the string is added, and a string may be removed from the
    sequence given its label. There is a method to retrieve the concatenation of all
    strings in order. The conditions (e.g. "if(c[0]==1)") for some strings are stored
    separately in `conditions`. These conditions will be converted to text when
    retrieving the full string.
    """

    def __init__(self) -> None:
        self.strings: OrderedDict[int, str] = OrderedDict()
        self.conditions: dict[int, _ConditionString] = {}
        self.label = 0

    def add_string(self, string: str) -> int:
        label = self.label
        self.strings[label] = string
        self.label += 1
        return label

    def get_string(self, label: int) -> str | None:
        return self.strings.get(label, None)

    def del_string(self, label: int) -> None:
        self.strings.pop(label, None)

    def get_full_string(self) -> str:
        strings = []
        for l, s in self.strings.items():
            condition = self.conditions.get(l)
            if condition is not None:
                strings.append(
                    f"if({condition.variable}{condition.comparator}{condition.value}) "
                    + s
                )
            else:
                strings.append(s)
        return "".join(strings)


def _make_params_str(params: list[float | Expr] | None) -> str:
    s = ""
    if params is not None:
        n_params = len(params)
        s += "("
        for i in range(n_params):
            reduced = True
            try:
                p: float | Expr = float(params[i])
            except TypeError:
                reduced = False
                p = params[i]
            if i < n_params - 1:
                if reduced:
                    s += f"{p}*pi,"
                else:
                    s += f"({p})*pi,"
            elif reduced:
                s += f"{p}*pi)"
            else:
                s += f"({p})*pi)"
    s += " "
    return s


def _make_args_str(args: Sequence[UnitID]) -> str:
    s = ""
    for i in range(len(args)):
        s += f"{args[i]}"
        if i < len(args) - 1:
            s += ","
        else:
            s += ";\n"
    return s


@dataclass
class _ScratchPredicate:
    variable: str  # variable, e.g. "c[1]"
    comparator: str  # comparator, e.g. "=="
    value: int  # value, e.g. "1"
    dest: str  # destination bit, e.g. "tk_SCRATCH_BIT[0]"


def _vars_overlap(v: str, w: str) -> bool:
    """check if two variables have overlapping bits"""
    v_split = v.split("[")
    w_split = w.split("[")
    if v_split[0] != w_split[0]:
        # different registers
        return False
    # e.g. (a[1], a), (a, a[1]), (a[1], a[1]), (a, a)
    return len(v_split) != len(w_split) or v == w


def _var_appears(v: str, s: str) -> bool:
    """check if variable v appears in string s"""
    v_split = v.split("[")
    if len(v_split) == 1:
        # check if v appears in s and is not surrounded by word characters
        # e.g. a = a & b or a = a[1] & b[1]
        return bool(re.search(r"(?<!\w)" + re.escape(v) + r"(?![\w])", s))
    if re.search(r"(?<!\w)" + re.escape(v), s):
        # check if v appears in s and is not proceeded by word characters
        # e.g. a[1] = a[1]
        return True
    # check the register of v appears in s
    # e.g. a[1] = a & b
    return bool(re.search(r"(?<!\w)" + re.escape(v_split[0]) + r"(?![\[\w])", s))


class _QasmWriter:
    """
    Helper class for converting a sequence of TKET Commands to QASM, and retrieving the
    final QASM string afterwards.
    """

    def __init__(
        self,
        qubits: list[Qubit],
        bits: list[Bit],
        header: str = "qelib1",
        include_gate_defs: set[str] | None = None,
        maxwidth: int = 32,
    ):
        self.header = header
        self.maxwidth = maxwidth
        self.added_gate_definitions: set[str] = set()
        self.include_module_gates = {"measure", "reset", "barrier"}
        self.include_module_gates.update(
            _load_include_module(header, False, True).keys()
        )
        self.prefix = ""
        self.gatedefs = ""
        self.strings = _LabelledStringList()

        # Record of `RangePredicate` operations that set a "scratch" bit to 0 or 1
        # depending on the value of the predicate. This map is consulted when we
        # encounter a `Conditional` operation to see if the condition bit is one of
        # these scratch bits, which we can then replace with the original.
        self.range_preds: dict[int, _ScratchPredicate] = {}

        if include_gate_defs is None:
            self.include_gate_defs = self.include_module_gates
            self.include_gate_defs.update(NOPARAM_EXTRA_COMMANDS.keys())
            self.include_gate_defs.update(PARAM_EXTRA_COMMANDS.keys())
            self.prefix = f'OPENQASM 2.0;\ninclude "{header}.inc";\n\n'
            self.qregs = _retrieve_registers(
                cast("list[UnitID]", qubits), QubitRegister
            )
            self.cregs = _retrieve_registers(cast("list[UnitID]", bits), BitRegister)
            for reg in self.qregs.values():
                if regname_regex.match(reg.name) is None:
                    raise QASMUnsupportedError(
                        f"Invalid register name '{reg.name}'. QASM register names must "
                        "begin with a lowercase letter and may only contain lowercase "
                        "and uppercase letters, numbers, and underscores. "
                        "Try renaming the register with `rename_units` first."
                    )
            for bit_reg in self.cregs.values():
                if regname_regex.match(bit_reg.name) is None:
                    raise QASMUnsupportedError(
                        f"Invalid register name '{bit_reg.name}'. QASM register names "
                        "must begin with a lowercase letter and may only contain "
                        "lowercase and uppercase letters, numbers, and underscores. "
                        "Try renaming the register with `rename_units` first."
                    )
        else:
            # gate definition, no header necessary for file
            self.include_gate_defs = include_gate_defs
            self.cregs = {}
            self.qregs = {}

        self.cregs_as_bitseqs = {tuple(creg) for creg in self.cregs.values()}

        # for holding condition values when writing Conditional blocks
        # the size changes when adding and removing scratch bits
        self.scratch_reg = BitRegister(
            next(
                f"{_TEMP_BIT_REG_BASE}_{i}"
                for i in itertools.count()
                if f"{_TEMP_BIT_REG_BASE}_{i}" not in self.qregs
            ),
            0,
        )
        # if a string writes to some classical variables, the string label and
        # the affected variables will be recorded.
        self.variable_writes: dict[int, list[str]] = {}

    def fresh_scratch_bit(self) -> Bit:
        self.scratch_reg = BitRegister(self.scratch_reg.name, self.scratch_reg.size + 1)
        return Bit(self.scratch_reg.name, self.scratch_reg.size - 1)

    def remove_last_scratch_bit(self) -> None:
        assert self.scratch_reg.size > 0
        self.scratch_reg = BitRegister(self.scratch_reg.name, self.scratch_reg.size - 1)

    def write_params(self, params: list[float | Expr] | None) -> None:
        params_str = _make_params_str(params)
        self.strings.add_string(params_str)

    def write_args(self, args: Sequence[UnitID]) -> None:
        args_str = _make_args_str(args)
        self.strings.add_string(args_str)

    def make_gate_definition(
        self,
        n_qubits: int,
        opstr: str,
        optype: OpType,
        n_params: int | None = None,
    ) -> str:
        s = "gate " + opstr + " "
        symbols: list[Symbol] | None = None
        if n_params is not None:
            # need to add parameters to gate definition
            s += "("
            symbols = [
                Symbol("param" + str(index) + "/pi") for index in range(n_params)
            ]
            symbols_header = [Symbol("param" + str(index)) for index in range(n_params)]
            for symbol in symbols_header[:-1]:
                s += symbol.name + ", "
            s += symbols_header[-1].name + ") "

        # add qubits to gate definition
        qubit_args = [
            Qubit(opstr + "q" + str(index)) for index in list(range(n_qubits))
        ]
        for qb in qubit_args[:-1]:
            s += str(qb) + ","
        s += str(qubit_args[-1]) + " {\n"
        # get rebased circuit for constructing qasm
        gate_circ = _get_gate_circuit(optype, qubit_args, symbols)
        # write circuit to qasm
        s += circuit_to_qasm_str(
            gate_circ, self.header, self.include_gate_defs, self.maxwidth
        )
        s += "}\n"
        return s

    def mark_as_written(self, label: int, written_variable: str) -> None:
        if label in self.variable_writes:
            self.variable_writes[label].append(written_variable)
        else:
            self.variable_writes[label] = [written_variable]

    def check_range_predicate(self, op: RangePredicateOp, args: list[Bit]) -> None:
        if (not _hqs_header(self.header)) and op.lower != op.upper:
            raise QASMUnsupportedError(
                "OpenQASM conditions must be on a register's fixed value."
            )
        variable = args[0].reg_name
        assert isinstance(variable, str)
        if op.n_inputs != self.cregs[variable].size:
            raise QASMUnsupportedError(
                "RangePredicate conditions must be an entire classical register"
            )
        if args[:-1] != self.cregs[variable].to_list():
            raise QASMUnsupportedError(
                "RangePredicate conditions must be a single classical register"
            )

    def add_range_predicate(self, op: RangePredicateOp, args: list[Bit]) -> None:
        self.check_range_predicate(op, args)
        comparator, value = _parse_range(op.lower, op.upper, self.maxwidth)
        variable = args[0].reg_name
        dest_bit = str(args[-1])
        label = self.strings.add_string(
            "".join(
                [
                    f"if({variable}{comparator}{value}) " + f"{dest_bit} = 1;\n",
                    (f"if({variable}{_negate_comparator(comparator)}{value}) "
                    f"{dest_bit} = 0;\n"),
                ]
            )
        )
        # Record this operation.
        # Later if we find a conditional based on dest_bit, we can replace dest_bit with
        # (variable, comparator, value), provided that variable hasn't been written to
        # in the mean time. (So we must watch for that, and remove the record from the
        # list if it is.)
        # Note that we only perform such rewrites for internal scratch bits.
        if dest_bit.startswith(_TEMP_BIT_NAME):
            self.range_preds[label] = _ScratchPredicate(
                variable, comparator, value, dest_bit
            )

    def replace_condition(self, pred_label: int) -> bool:
        """Given the label of a predicate p=(var, comp, value, dest, label)
        we scan the lines after p:
        1.if dest is the condition of a conditional line we replace dest with
            the predicate and do 2 for the inner command.
        2.if either the variable or the dest gets written, we stop.
        returns true if a replacement is made.
        """
        assert pred_label in self.range_preds
        success = False
        pred = self.range_preds[pred_label]
        line_labels = []
        for label in range(pred_label + 1, self.strings.label):
            string = self.strings.get_string(label)
            if string is None:
                continue
            line_labels.append(label)
            if "\n" not in string:
                continue
            written_variables: list[str] = []
            # (label, condition)
            conditions: list[tuple[int, _ConditionString]] = []
            for l in line_labels:
                written_variables.extend(self.variable_writes.get(l, []))
                cond = self.strings.conditions.get(l)
                if cond:
                    conditions.append((l, cond))
            if len(conditions) == 1 and pred.dest == conditions[0][1].variable:
                # if the condition is dest, replace the condition with pred
                success = True
                if conditions[0][1].value == 1:
                    self.strings.conditions[conditions[0][0]] = _ConditionString(
                        pred.variable, pred.comparator, pred.value
                    )
                else:
                    assert conditions[0][1].value == 0
                    self.strings.conditions[conditions[0][0]] = _ConditionString(
                        pred.variable,
                        _negate_comparator(pred.comparator),
                        pred.value,
                    )
            if any(_vars_overlap(pred.dest, v) for v in written_variables) or any(
                _vars_overlap(pred.variable, v) for v in written_variables
            ):
                return success
            line_labels.clear()
            conditions.clear()
            written_variables.clear()
        return success

    def remove_unused_predicate(self, pred_label: int) -> bool:
        """Given the label of a predicate p=(var, comp, value, dest, label),
        we remove p if dest never appears after p."""
        assert pred_label in self.range_preds
        pred = self.range_preds[pred_label]
        for label in range(pred_label + 1, self.strings.label):
            string = self.strings.get_string(label)
            if string is None:
                continue
            if _var_appears(pred.dest, string) or (
                label in self.strings.conditions
                and _vars_overlap(pred.dest, self.strings.conditions[label].variable)
            ):
                return False
        self.range_preds.pop(pred_label)
        self.strings.del_string(pred_label)
        return True

    def add_conditional(self, op: Conditional, args: Sequence[UnitID]) -> None:
        control_bits = args[: op.width]
        if op.width == 1 and _hqs_header(self.header):
            variable = str(control_bits[0])
        else:
            variable = control_bits[0].reg_name
            if (
                _hqs_header(self.header)
                and control_bits != self.cregs[variable].to_list()
            ):
                raise QASMUnsupportedError(
                    "hqslib1 QASM conditions must be an entire classical "
                    "register or a single bit"
                )
        if not _hqs_header(self.header):
            if op.width != self.cregs[variable].size:
                raise QASMUnsupportedError(
                    "OpenQASM conditions must be an entire classical register"
                )
            if control_bits != self.cregs[variable].to_list():
                raise QASMUnsupportedError(
                    "OpenQASM conditions must be a single classical register"
                )
        if op.op.type == OpType.Phase:
            # Conditional phase is ignored.
            return
        if op.op.type == OpType.RangePredicate:
            # Special handling for nested ifs
            # if condition
            #   if pred dest = 1
            #   if not pred dest = 0
            # can be written as
            # if condition s0 = 1
            # if pred s1 = 1
            # s2 = s0 & s1
            # s3 = s0 & ~s1
            # if s2 dest = 1
            # if s3 dest = 0
            # where s0, s1, s2, and s3 are scratch bits
            s0 = self.fresh_scratch_bit()
            l = self.strings.add_string(f"{s0} = 1;\n")
            # we store the condition in self.strings.conditions
            # as it can be later replaced by `replace_condition`
            # if possible
            self.strings.conditions[l] = _ConditionString(variable, "==", op.value)
            # output the RangePredicate to s1
            s1 = self.fresh_scratch_bit()
            assert isinstance(op.op, RangePredicateOp)
            self.check_range_predicate(op.op, cast("list[Bit]", args[op.width :]))
            pred_comparator, pred_value = _parse_range(
                op.op.lower, op.op.upper, self.maxwidth
            )
            pred_variable = args[op.width :][0].reg_name
            self.strings.add_string(
                f"if({pred_variable}{pred_comparator}{pred_value}) {s1} = 1;\n"
            )
            s2 = self.fresh_scratch_bit()
            self.strings.add_string(f"{s2} = {s0} & {s1};\n")
            s3 = self.fresh_scratch_bit()
            self.strings.add_string(f"{s3} = {s0} & (~ {s1});\n")
            self.strings.add_string(f"if({s2}==1) {args[-1]} = 1;\n")
            self.strings.add_string(f"if({s3}==1) {args[-1]} = 0;\n")
            return
        # we assign the condition to a scratch bit, which we will later remove
        # if the condition variable is unchanged.
        scratch_bit = self.fresh_scratch_bit()
        pred_label = self.strings.add_string(
            f"if({variable}=={op.value}) " + f"{scratch_bit} = 1;\n"
        )
        self.range_preds[pred_label] = _ScratchPredicate(
            variable, "==", op.value, str(scratch_bit)
        )
        # we will later add condition to all lines starting from next_label
        next_label = self.strings.label
        self.add_op(op.op, args[op.width :])
        # add conditions to the lines after the predicate
        is_new_line = True
        for label in range(next_label, self.strings.label):
            string = self.strings.get_string(label)
            assert string is not None
            if is_new_line and string != "\n":
                self.strings.conditions[label] = _ConditionString(
                    str(scratch_bit), "==", 1
                )
            is_new_line = "\n" in string
        if self.replace_condition(pred_label) and self.remove_unused_predicate(
            pred_label
        ):
            # remove the unused scratch bit
            self.remove_last_scratch_bit()

    def add_set_bits(self, op: SetBitsOp, args: list[Bit]) -> None:
        creg_name = args[0].reg_name
        bits, vals = zip(*sorted(zip(args, op.values, strict=False)), strict=False)
        # check if whole register can be set at once
        if bits == tuple(self.cregs[creg_name].to_list()):
            value = int("".join(map(str, map(int, vals[::-1]))), 2)
            label = self.strings.add_string(f"{creg_name} = {value};\n")
            self.mark_as_written(label, f"{creg_name}")
        else:
            for bit, value in zip(bits, vals, strict=False):
                label = self.strings.add_string(f"{bit} = {int(value)};\n")
                self.mark_as_written(label, f"{bit}")

    def add_copy_bits(self, op: CopyBitsOp, args: list[Bit]) -> None:
        l_args = args[op.n_inputs :]
        r_args = args[: op.n_inputs]
        l_name = l_args[0].reg_name
        r_name = r_args[0].reg_name
        # check if whole register can be set at once
        if (
            l_args == self.cregs[l_name].to_list()
            and r_args == self.cregs[r_name].to_list()
        ):
            label = self.strings.add_string(f"{l_name} = {r_name};\n")
            self.mark_as_written(label, f"{l_name}")
        else:
            for bit_l, bit_r in zip(l_args, r_args, strict=False):
                label = self.strings.add_string(f"{bit_l} = {bit_r};\n")
                self.mark_as_written(label, f"{bit_l}")

    def add_multi_bit(self, op: MultiBitOp, args: list[Bit]) -> None:
        basic_op = op.basic_op
        basic_n = basic_op.n_inputs + basic_op.n_outputs + basic_op.n_input_outputs
        n_args = len(args)
        assert n_args % basic_n == 0
        arity = n_args // basic_n

        # If the operation is register-aligned we can write it more succinctly.
        poss_regs = [
            tuple(args[basic_n * i + j] for i in range(arity)) for j in range(basic_n)
        ]
        if all(poss_reg in self.cregs_as_bitseqs for poss_reg in poss_regs):
            # The operation is register-aligned.
            self.add_op(basic_op, [poss_regs[j][0].reg_name for j in range(basic_n)])  # type: ignore
        else:
            # The operation is not register-aligned.
            for i in range(arity):
                basic_args = args[basic_n * i : basic_n * (i + 1)]
                self.add_op(basic_op, basic_args)

    def add_explicit_op(self, op: Op, args: list[Bit]) -> None:
        # &, ^ and | gates
        opstr = str(op)
        if opstr not in _classical_gatestr_map:
            raise QASMUnsupportedError(f"Classical gate {opstr} not supported.")
        label = self.strings.add_string(
            f"{args[-1]} = {args[0]} {_classical_gatestr_map[opstr]} {args[1]};\n"
        )
        self.mark_as_written(label, f"{args[-1]}")

    def add_wired_clexpr(self, op: ClExprOp, args: list[Bit]) -> None:
        wexpr: WiredClExpr = op.expr
        # 1. Determine the mappings from bit variables to bits and from register
        # variables to registers.
        expr: ClExpr = wexpr.expr
        bit_posn: dict[int, int] = wexpr.bit_posn
        reg_posn: dict[int, list[int]] = wexpr.reg_posn
        output_posn: list[int] = wexpr.output_posn
        input_bits: dict[int, Bit] = {i: args[j] for i, j in bit_posn.items()}
        input_regs: dict[int, BitRegister] = {}
        all_cregs = set(self.cregs.values())
        for i, posns in reg_posn.items():
            reg_args = [args[j] for j in posns]
            for creg in all_cregs:
                if creg.to_list() == reg_args:
                    input_regs[i] = creg
                    break
            else:
                assert (
                    not f"ClExprOp ({wexpr}) contains a register variable (r{i}) that "
                    "is not wired to any BitRegister in the circuit."
                )
        # 2. Write the left-hand side of the assignment.
        output_repr: str | None = None
        output_args: list[Bit] = [args[j] for j in output_posn]
        n_output_args = len(output_args)
        expect_reg_output = has_reg_output(expr.op)
        if n_output_args == 0:
            raise QASMUnsupportedError("Expression has no output.")
        if n_output_args == 1:
            output_arg = output_args[0]
            output_repr = output_arg.reg_name if expect_reg_output else str(output_arg)
        else:
            if not expect_reg_output:
                raise QASMUnsupportedError("Unexpected output for operation.")
            for creg in all_cregs:
                if creg.to_list() == output_args:
                    output_repr = creg.name
                    break
            assert output_repr is not None
        self.strings.add_string(f"{output_repr} = ")
        # 3. Write the right-hand side of the assignment.
        self.strings.add_string(
            expr.as_qasm(input_bits=input_bits, input_regs=input_regs)
        )
        self.strings.add_string(";\n")

    def add_wasm(self, op: WASMOp, args: list[Bit]) -> None:
        inputs: list[str] = []
        outputs: list[str] = []
        for reglist, sizes in [(inputs, op.input_widths), (outputs, op.output_widths)]:
            for in_width in sizes:
                bits = args[:in_width]
                args = args[in_width:]
                regname = bits[0].reg_name
                if bits != list(self.cregs[regname]):
                    QASMUnsupportedError("WASM ops must act on entire registers.")
                reglist.append(regname)
        if outputs:
            label = self.strings.add_string(f"{', '.join(outputs)} = ")
        self.strings.add_string(f"{op.func_name}({', '.join(inputs)});\n")
        for variable in outputs:
            self.mark_as_written(label, variable)

    def add_rng_seed(self, args: list[Bit]) -> None:
        assert len(args) == 65  # noqa: PLR2004
        reg_name = args[0].reg_name
        creg = self.cregs[reg_name]
        assert args[:-1] == creg.to_list()
        self.strings.add_string(f"RNGseed({reg_name});\n")

    def add_rng_bound(self, args: list[Bit]) -> None:
        assert len(args) == 33  # noqa: PLR2004
        reg_name = args[0].reg_name
        creg = self.cregs[reg_name]
        assert args[:-1] == creg.to_list()
        self.strings.add_string(f"RNGbound({reg_name});\n")

    def add_rng_index(self, args: list[Bit]) -> None:
        assert len(args) == 33  # noqa: PLR2004
        reg_name = args[0].reg_name
        creg = self.cregs[reg_name]
        assert args[:-1] == creg.to_list()
        self.strings.add_string(f"RNGindex({reg_name});\n")

    def add_rng_num(self, args: list[Bit]) -> None:
        assert len(args) == 33  # noqa: PLR2004
        reg_name = args[0].reg_name
        creg = self.cregs[reg_name]
        assert args[:-1] == creg.to_list()
        label = self.strings.add_string(f"{reg_name} = RNGnum();\n")
        self.mark_as_written(label, reg_name)

    def add_job_shot_num(self, args: list[Bit]) -> None:
        assert len(args) == 32  # noqa: PLR2004
        reg_name = args[0].reg_name
        creg = self.cregs[reg_name]
        assert args == creg.to_list()
        self.strings.add_string(f"{reg_name} = JOB_shotnum;\n")

    def add_measure(self, args: Sequence[UnitID]) -> None:
        label = self.strings.add_string(f"measure {args[0]} -> {args[1]};\n")
        self.mark_as_written(label, f"{args[1]}")

    def add_zzphase(self, param: float | Expr, args: Sequence[UnitID]) -> None:
        # as op.params returns reduced parameters, we can assume
        # that 0 <= param < 4
        if param > 1:
            # first get in to 0 <= param < 2 range
            param = Decimal(str(param)) % Decimal(2)
            # then flip 1 <= param < 2  range into
            # -1 <= param < 0
            if param > 1:
                param = -2 + param
        self.strings.add_string("RZZ")
        self.write_params([param])
        self.write_args(args)

    def add_cnx(self, args: Sequence[UnitID]) -> None:
        n_ctrls = len(args) - 1
        assert n_ctrls >= 0
        match n_ctrls:
            case 0:
                self.strings.add_string("x")
            case 1:
                self.strings.add_string("cx")
            case 2:
                self.strings.add_string("ccx")
            case 3:
                self.strings.add_string("c3x")
            case 4:
                self.strings.add_string("c4x")
            case _:
                raise QASMUnsupportedError("CnX with n > 4 not supported in QASM")
        self.strings.add_string(" ")
        self.write_args(args)

    def add_data(self, op: BarrierOp, args: Sequence[UnitID]) -> None:
        opstr = _tk_to_qasm_noparams[OpType.Barrier] if op.data == "" else op.data
        self.strings.add_string(opstr)
        self.strings.add_string(" ")
        self.write_args(args)

    def add_gate_noparams(self, op: Op, args: Sequence[UnitID]) -> None:
        self.strings.add_string(_tk_to_qasm_noparams[op.type])
        self.strings.add_string(" ")
        self.write_args(args)

    def add_gate_params(self, op: Op, args: Sequence[UnitID]) -> None:
        optype, params = _get_optype_and_params(op)
        self.strings.add_string(_tk_to_qasm_params[optype])
        self.write_params(params)
        self.write_args(args)

    def add_extra_noparams(self, op: Op, args: Sequence[UnitID]) -> tuple[str, str]:
        optype = op.type
        opstr = _tk_to_qasm_extra_noparams[optype]
        gatedefstr = ""
        if opstr not in self.added_gate_definitions:
            self.added_gate_definitions.add(opstr)
            gatedefstr = self.make_gate_definition(op.n_qubits, opstr, optype)
        mainstr = opstr + " " + _make_args_str(args)
        return gatedefstr, mainstr

    def add_extra_params(self, op: Op, args: Sequence[UnitID]) -> tuple[str, str]:
        optype, params = _get_optype_and_params(op)
        assert params is not None
        opstr = _tk_to_qasm_extra_params[optype]
        gatedefstr = ""
        if opstr not in self.added_gate_definitions:
            self.added_gate_definitions.add(opstr)
            gatedefstr = self.make_gate_definition(
                op.n_qubits, opstr, optype, len(params)
            )
        mainstr = opstr + _make_params_str(params) + _make_args_str(args)
        return gatedefstr, mainstr

    def add_op(self, op: Op, args: Sequence[UnitID]) -> None:  # noqa: PLR0912, PLR0915
        optype, _params = _get_optype_and_params(op)
        if optype == OpType.RangePredicate:
            assert isinstance(op, RangePredicateOp)
            self.add_range_predicate(op, cast("list[Bit]", args))
        elif optype == OpType.Conditional:
            assert isinstance(op, Conditional)
            self.add_conditional(op, args)
        elif optype == OpType.Phase:
            # global phase is ignored in QASM
            pass
        elif optype == OpType.SetBits:
            assert isinstance(op, SetBitsOp)
            self.add_set_bits(op, cast("list[Bit]", args))
        elif optype == OpType.CopyBits:
            assert isinstance(op, CopyBitsOp)
            self.add_copy_bits(op, cast("list[Bit]", args))
        elif optype == OpType.MultiBit:
            assert isinstance(op, MultiBitOp)
            self.add_multi_bit(op, cast("list[Bit]", args))
        elif optype in (OpType.ExplicitPredicate, OpType.ExplicitModifier):
            self.add_explicit_op(op, cast("list[Bit]", args))
        elif optype == OpType.ClExpr:
            assert isinstance(op, ClExprOp)
            self.add_wired_clexpr(op, cast("list[Bit]", args))
        elif optype == OpType.WASM:
            assert isinstance(op, WASMOp)
            self.add_wasm(op, cast("list[Bit]", args))
        elif optype == OpType.RNGSeed:
            self.add_rng_seed(cast("list[Bit]", args))
        elif optype == OpType.RNGBound:
            self.add_rng_bound(cast("list[Bit]", args))
        elif optype == OpType.RNGIndex:
            self.add_rng_index(cast("list[Bit]", args))
        elif optype == OpType.RNGNum:
            self.add_rng_num(cast("list[Bit]", args))
        elif optype == OpType.JobShotNum:
            self.add_job_shot_num(cast("list[Bit]", args))
        elif optype == OpType.Measure:
            self.add_measure(args)
        elif _hqs_header(self.header) and optype == OpType.ZZPhase:
            # special handling for zzphase
            assert len(op.params) == 1
            self.add_zzphase(op.params[0], args)
        elif optype == OpType.CnX:
            self.add_cnx(args)
        elif optype == OpType.Barrier and self.header == "hqslib1_dev":
            assert isinstance(op, BarrierOp)
            self.add_data(op, args)
        elif (
            optype in _tk_to_qasm_noparams
            and _tk_to_qasm_noparams[optype] in self.include_module_gates
        ):
            self.add_gate_noparams(op, args)
        elif (
            optype in _tk_to_qasm_params
            and _tk_to_qasm_params[optype] in self.include_module_gates
        ):
            self.add_gate_params(op, args)
        elif optype in _tk_to_qasm_extra_noparams:
            gatedefstr, mainstr = self.add_extra_noparams(op, args)
            self.gatedefs += gatedefstr
            self.strings.add_string(mainstr)
        elif optype in _tk_to_qasm_extra_params:
            gatedefstr, mainstr = self.add_extra_params(op, args)
            self.gatedefs += gatedefstr
            self.strings.add_string(mainstr)
        else:
            raise QASMUnsupportedError(f"Cannot print command of type: {op.get_name()}")

    def finalize(self) -> str:
        # try removing unused predicates
        pred_labels = list(self.range_preds.keys())
        for label in pred_labels:
            # try replacing conditions with a predicate
            self.replace_condition(label)
            # try removing the predicate
            self.remove_unused_predicate(label)
        reg_strings = _LabelledStringList()
        for reg in self.qregs.values():
            reg_strings.add_string(f"qreg {reg.name}[{reg.size}];\n")
        for bit_reg in self.cregs.values():
            reg_strings.add_string(f"creg {bit_reg.name}[{bit_reg.size}];\n")
        if self.scratch_reg.size > 0:
            reg_strings.add_string(
                f"creg {self.scratch_reg.name}[{self.scratch_reg.size}];\n"
            )
        return (
            self.prefix
            + self.gatedefs
            + _filtered_qasm_str(
                reg_strings.get_full_string() + self.strings.get_full_string()
            )
        )


def circuit_to_qasm_io(
    circ: Circuit,
    stream_out: TextIO,
    header: str = "qelib1",
    include_gate_defs: set[str] | None = None,
    maxwidth: int = 32,
) -> None:
    """Convert a Circuit to QASM and write to a text stream.

    Classical bits in the pytket circuit must be singly-indexed.

    Note that this will not account for implicit qubit permutations in the Circuit.

    :param circ: pytket circuit
    :param stream_out: text stream to be written to
    :param header: qasm header (default "qelib1")
    :param include_gate_defs: optional set of gates to include
    :param maxwidth: maximum allowed width of classical registers (default 32)
    """
    stream_out.write(
        circuit_to_qasm_str(
            circ, header=header, include_gate_defs=include_gate_defs, maxwidth=maxwidth
        )
    )
