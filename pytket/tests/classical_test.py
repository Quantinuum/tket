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

import json
import operator
from collections.abc import Callable
from pathlib import Path
from typing import TypeVar

import pytest
from hypothesis import given, settings, strategies
from hypothesis.strategies import SearchStrategy
from jsonschema import validate  # type: ignore
from strategies import binary_digits, reg_name_regex, uint32, uint64  # type: ignore
from sympy import Symbol

from pytket import wasm
from pytket._tket import unit_id
from pytket.circuit import (
    Bit,
    BitRegister,
    CircBox,
    Circuit,
    Conditional,
    MultiBitOp,
    Op,
    OpType,
    Qubit,
    QubitRegister,
    RangePredicateOp,
    SetBitsOp,
)
from pytket.circuit.logic_exp import (
    BinaryOp,
    BitLogicExp,
    BitWiseOp,
    LogicExp,
    NullaryOp,
    PredicateExp,
    RegEq,
    RegGeq,
    RegGt,
    RegLeq,
    RegLogicExp,
    RegLt,
    RegNeq,
    RegPow,
    RegWiseOp,
    UnaryOp,
    create_bit_logic_exp,
    create_reg_logic_exp,
    if_bit,
    if_not_bit,
    reg_eq,
    reg_geq,
    reg_gt,
    reg_leq,
    reg_lt,
    reg_neq,
)
from pytket.passes import DecomposeClassicalExp

curr_file_path = Path(__file__).resolve().parent

with open(curr_file_path.parent.parent / "schemas/circuit_v1.json") as f:
    schema = json.load(f)


def print_commands(c: Circuit) -> None:
    print("\n".join(map(str, c)))  # noqa: T201


def test_c_ops() -> None:
    c = Circuit(0, 4)
    cl_cx_values = [0, 3, 2, 1]  # classical CX transform
    c.add_c_transform(cl_cx_values, [0, 1], "ClCX")
    c.add_c_transform(cl_cx_values, [Bit(1), Bit(2)], "ClCX")
    eq_pred_values = [True, False, False, True]  # test 2 bits for equality
    c.add_c_predicate(eq_pred_values, [0, 1], 2, "EQ")
    c.add_c_predicate(eq_pred_values, [Bit(1), Bit(2)], Bit(3), "EQ")
    and_values = [bool(i) for i in [0, 0, 0, 1]]  # binary AND
    c.add_c_modifier(and_values, [1], 2)
    c.add_c_modifier(and_values, [Bit(2)], Bit(3))
    c.add_c_and(1, 2, 3)
    c.add_c_or(Bit(2), Bit(3), Bit(0))
    c.add_c_not(0, 1)
    c.add_c_and(2, 3, 3)
    c.add_c_or(0, 3, 0)
    c.add_c_range_predicate(2, 6, [0, 1, 2], 3)
    c.add_c_setbits([True], [1])
    c.add_c_setbits([False, True], [Bit(2), Bit(3)])
    c.add_c_copybits([0, 1], [2, 3])
    c.add_c_range_predicate(2, 6, [1, 2, 3], 0)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)
    c.add_c_and_to_registers(c0, c1, c2)
    c.add_c_xor_to_registers(c2, c1, c2)
    c.add_c_not_to_registers(c1, c2)

    c.add_c_setreg(3, c0)
    c.add_c_copyreg(c1, c0)

    cmds = c.get_commands()
    assert len(cmds) == 21
    assert len([cmd for cmd in cmds if cmd.op.get_name() == "AND"]) == 2
    rp_cmds = [cmd for cmd in cmds if cmd.op.type == OpType.RangePredicate]
    assert len(rp_cmds) == 2
    assert rp_cmds[0].op == rp_cmds[1].op
    mb_cmds = [cmd for cmd in cmds if cmd.op.type == OpType.MultiBit]
    assert len(mb_cmds) == 3
    assert (
        mb_cmds[0] != mb_cmds[1]
        and mb_cmds[0] != mb_cmds[2]
        and mb_cmds[1] != mb_cmds[2]
    )
    assert str(cmds[6]) == "CopyBits c1[0], c1[1], c1[2], c0[0], c0[1], c0[2];"
    assert str(cmds[3]) == "SetBits(110) c0[0], c0[1], c0[2];"


def test_add_c_setreg_with_size_gt_32bits() -> None:
    c = Circuit()
    b = c.add_c_register("b", 64)
    c.add_c_setreg(100, b)

    expected_reg = [False] * 64
    expected_reg[2] = expected_reg[5] = expected_reg[6] = True
    com = c.get_commands()[0]
    assert len(com.bits) == 64
    op = com.op
    assert isinstance(op, SetBitsOp)
    assert op.values == expected_reg


def test_add_c_setreg_raises_runtime_error() -> None:
    c = Circuit()
    b = c.add_c_register("b", 2)
    with pytest.raises(RuntimeError):
        c.add_c_setreg(100, b)


def test_wasm() -> None:
    c = Circuit(0, 6)
    c._add_wasm("funcname", "wasmfileuid", [1, 1], [], [Bit(0), Bit(1)], [0])  # noqa: SLF001
    c._add_wasm("funcname", "wasmfileuid", [1, 1], [], [Bit(0), Bit(2)], [0])  # noqa: SLF001
    c._add_wasm("funcname", "wasmfileuid", [1, 1], [2], [0, 1, 2, 3], [0])  # noqa: SLF001
    c._add_wasm("funcname", "wasmfileuid", [1, 1], [2], [0, 1, 2, 4], [0])  # noqa: SLF001
    c._add_wasm("funcname", "wasmfileuid", [1], [1, 2], [0, 1, 2, 3], [0])  # noqa: SLF001
    c._add_wasm("funcname", "wasmfileuid", [2, 1], [3], [0, 1, 2, 3, 4, 5], [0])  # noqa: SLF001

    assert c.depth() == 6


def test_wasm_2() -> None:
    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    c._add_wasm("funcname", "wasmfileuid", [c0, c1], [c2], [0])  # noqa: SLF001

    assert c.depth() == 1


def test_wasm_3() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    assert w.bytecode() == bytecode

    c = Circuit(0, 6)

    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)])

    assert c.depth() == 1


def test_wasm_3_bytes() -> None:
    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    c = Circuit(0, 6)

    w = wasm.WasmModuleHandler(bytecode)

    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)])

    assert c.depth() == 1


def test_wasm_4() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0, 3)

    with pytest.raises(ValueError):
        c.add_wasm("add_one", w, [1, 1], [1], [Bit(0), Bit(1), Bit(2)])


def test_wasm_5() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0, 3)

    with pytest.raises(ValueError):
        c.add_wasm("add_one", w, [1], [1, 1], [Bit(0), Bit(1), Bit(2)])


def test_wasm_6() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0, 35)

    with pytest.raises(ValueError):
        c.add_wasm(
            "add_one",
            w,
            [34],
            [1],
            [
                Bit(0),
                Bit(1),
                Bit(2),
                Bit(3),
                Bit(4),
                Bit(5),
                Bit(6),
                Bit(7),
                Bit(8),
                Bit(9),
                Bit(10),
                Bit(11),
                Bit(12),
                Bit(13),
                Bit(14),
                Bit(15),
                Bit(16),
                Bit(17),
                Bit(18),
                Bit(19),
                Bit(20),
                Bit(21),
                Bit(22),
                Bit(23),
                Bit(24),
                Bit(25),
                Bit(26),
                Bit(27),
                Bit(28),
                Bit(29),
                Bit(30),
                Bit(31),
                Bit(32),
                Bit(33),
                Bit(34),
            ],
        )


def test_wasm_7() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0, 35)
    with pytest.raises(ValueError):
        c.add_wasm(
            "add_one",
            w,
            [1],
            [34],
            [
                Bit(0),
                Bit(1),
                Bit(2),
                Bit(3),
                Bit(4),
                Bit(5),
                Bit(6),
                Bit(7),
                Bit(8),
                Bit(9),
                Bit(10),
                Bit(11),
                Bit(12),
                Bit(13),
                Bit(14),
                Bit(15),
                Bit(16),
                Bit(17),
                Bit(18),
                Bit(19),
                Bit(20),
                Bit(21),
                Bit(22),
                Bit(23),
                Bit(24),
                Bit(25),
                Bit(26),
                Bit(27),
                Bit(28),
                Bit(29),
                Bit(30),
                Bit(31),
                Bit(32),
                Bit(33),
                Bit(34),
            ],
        )


def test_wasm_8() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], [0])
    assert c.depth() == 1


def test_wasm_9() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], [0, 1])
    assert c.depth() == 1


def test_wasm_10() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], [1])


def test_wasm_11() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], [1])
    assert c.depth() == 1


def test_wasm_12() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], [1, 4])
    assert c.depth() == 1


def test_wasm_handler() -> None:
    wasm.WasmFileHandler("testfile.wasm")

    with pytest.raises(ValueError):
        wasm.WasmFileHandler("notexistingfile.wasm")


def test_wasm_function_check() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    assert not w.check_function("add_one", 0, 0)
    assert not w.check_function("add_three", 0, 0)


def test_wasm_function_check_2() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    with pytest.raises(ValueError):
        c.add_wasm_to_reg("add_three", w, [c0, c1], [c2])


def test_wasm_function_check_3() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    with pytest.raises(ValueError):
        c.add_wasm_to_reg("add_one", w, [c0, c1], [c2])


def test_wasm_function_check_4() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 10)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    with pytest.raises(ValueError):
        c.add_wasm_to_reg("add_one", w, [c0, c1], [c2])


def test_wasm_function_check_5() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 53)
    c1 = c.add_c_register("c1", 4)

    with pytest.raises(ValueError):
        c.add_wasm_to_reg("add_one", w, [c0], [c1])


def test_wasm_function_check_6() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 32)
    c1 = c.add_c_register("c1", 4)

    c.add_wasm_to_reg("add_one", w, [c0], [c1])
    assert c.depth() == 1


def test_wasm_function_check_6_bytes() -> None:
    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()

    w = wasm.WasmModuleHandler(bytecode)
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 32)
    c1 = c.add_c_register("c1", 4)

    c.add_wasm_to_reg("add_one", w, [c0], [c1])
    assert c.depth() == 1


def test_wasm_function_check_7() -> None:
    w = wasm.WasmFileHandler("testfile.wasm", int_size=32)
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 32)
    c1 = c.add_c_register("c1", 4)

    c.add_wasm_to_reg("add_one", w, [c0], [c1])
    assert c.depth() == 1


def test_wasm_function_check_8() -> None:
    w = wasm.WasmFileHandler("testfile.wasm", int_size=64)
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 32)
    c1 = c.add_c_register("c1", 4)
    c.add_c_register("c2", 5)

    c.add_wasm_to_reg("add_something", w, [c0], [c1])
    assert c.depth() == 1


def test_wasm_function_check_9() -> None:
    w = wasm.WasmFileHandler("testfile.wasm", int_size=64)
    c = Circuit(20, 20)
    c0 = c.add_c_register("c0", 53)
    c1 = c.add_c_register("c1", 4)
    c.add_c_register("c2", 5)

    with pytest.raises(ValueError):
        c.add_wasm_to_reg("add_something", w, [c0], [c1])


def test_add_wasm_to_reg() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    c.add_wasm_to_reg("multi", w, [c0, c1], [c2])
    c.add_wasm_to_reg("add_one", w, [c2], [c2])
    c.add_wasm_to_reg("no_return", w, [c2], [])
    c.add_wasm_to_reg("no_parameters", w, [], [c2])

    assert c.depth() == 4


def test_wasm_argtypes() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    assert w.bytecode() == bytecode

    c = Circuit(0, 6)

    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)])

    assert c.depth() == 1

    cmd = c.get_commands()[0]

    assert isinstance(cmd.args[0], unit_id.Bit)
    assert isinstance(cmd.args[1], unit_id.Bit)
    assert isinstance(cmd.args[2], unit_id.WasmState)


def test_wasm_uid_from_circuit() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    assert w.bytecode() == bytecode

    c = Circuit(0, 6)

    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)])

    assert c.depth() == 1

    assert (
        c.wasm_uid == "6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de"
    )


def test_wasm_uid_from_circuit_2() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    assert w.bytecode() == bytecode

    c = Circuit(0, 6)

    c.add_wasm("add_one", w, [1], [1], [Bit(0), Bit(1)], condition=Bit(3))

    assert c.depth() == 1

    assert (
        c.wasm_uid == "6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de"
    )


def test_wasm_uid_from_circuit_3() -> None:
    wfh = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0)
    a = c.add_c_register("c", 8)
    c.add_wasm_to_reg("add_one", wfh, [a], [a])
    assert c.depth() == 1

    cbox = CircBox(c)
    d = Circuit(0, 8)

    d.add_circbox(cbox, [0, 1, 2, 3, 4, 5, 6, 7])

    assert (
        d.wasm_uid == "6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de"
    )


def test_wasm_uid_from_circuit_4() -> None:
    wfh = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(0)
    a = c.add_c_register("c", 8)
    c.add_wasm_to_reg("add_one", wfh, [a], [a])
    assert c.depth() == 1

    cbox = CircBox(c)
    d = Circuit(0, 8)

    d.add_circbox(cbox, [0, 1, 2, 3, 4, 5, 6, 7], condition=Bit(0))

    assert (
        d.wasm_uid == "6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de"
    )


def test_wasm_uid_from_circuit_5() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    with open("testfile.wasm", "rb") as f:
        bytecode = f.read()
    assert w.bytecode() == bytecode

    c = Circuit(0, 6)

    assert c.depth() == 0
    assert c.wasm_uid is None


def test_wasm_append() -> None:
    wasmfile = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(1)
    a = c.add_c_register("a", 8)
    c.add_wasm_to_reg("add_one", wasmfile, [a], [a])
    assert c.depth() == 1

    d = Circuit()
    for bit in c.bits:
        d.add_bit(bit)
        d.add_c_setbits([False], [bit])

    d.append(c)
    assert d.depth() == 2


def test_wasm_append_2() -> None:
    wasmfile = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(1)
    a = c.add_c_register("a", 8)
    c.add_wasm_to_reg("add_one", wasmfile, [a], [a])
    assert c.depth() == 1

    d = Circuit()
    for bit in c.bits:
        d.add_bit(bit)
        d.add_c_setbits([False], [bit])
    d.add_wasm_to_reg("no_return", wasmfile, [a], [])

    d.append(c)
    assert d.depth() == 3


def test_wasm_append_3() -> None:
    wfh = wasm.WasmFileHandler("testfile.wasm")
    wfh2 = wasm.WasmFileHandler("testfile-without-init.wasm", check_file=False)

    c = Circuit(1)
    a = c.add_c_register("a", 8)
    c.add_wasm_to_reg("add_one", wfh, [a], [a])
    assert c.depth() == 1

    d = Circuit()
    for bit in c.bits:
        d.add_bit(bit)
        d.add_c_setbits([False], [bit])
    d.add_wasm_to_reg("no_return", wfh2, [a], [])

    with pytest.raises(RuntimeError):
        d.append(c)


def test_wasmfilehandler_without_init() -> None:
    with pytest.raises(ValueError):
        _ = wasm.WasmFileHandler("testfile-without-init.wasm")


def test_wasmfilehandler_without_init_no_check() -> None:
    w = wasm.WasmFileHandler("testfile-without-init.wasm", check_file=False)
    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    c.add_wasm_to_reg("multi", w, [c0, c1], [c2])
    c.add_wasm_to_reg("add_one", w, [c2], [c2])
    c.add_wasm_to_reg("no_return", w, [c2], [])
    c.add_wasm_to_reg("no_parameters", w, [], [c2])

    assert c.depth() == 4


def test_wasmfilehandler_invalid_file_1_c_32() -> None:
    with pytest.raises(ValueError):
        _ = wasm.WasmFileHandler(
            "wasm-generation/wasmfromcpp/invalid-with-print-1-emcc.wasm", int_size=32
        )


def test_wasmfilehandler_invalid_file_1_c_64() -> None:
    with pytest.raises(ValueError):
        _ = wasm.WasmFileHandler(
            "wasm-generation/wasmfromcpp/invalid-with-print-1-emcc.wasm", int_size=64
        )


def test_wasmfilehandler_invalid_file_1_e_32() -> None:
    with pytest.raises(ValueError):
        _ = wasm.WasmFileHandler(
            "wasm-generation/wasmfromcpp/invalid-with-print-2-emcc.wasm", int_size=32
        )


def test_wasmfilehandler_invalid_file_1_e_64() -> None:
    with pytest.raises(ValueError):
        _ = wasm.WasmFileHandler(
            "wasm-generation/wasmfromcpp/invalid-with-print-2-emcc.wasm", int_size=64
        )


def test_wasmfilehandler_invalid_file_1_c_32_no_check() -> None:
    _ = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/invalid-with-print-1-emcc.wasm",
        int_size=32,
        check_file=False,
    )


def test_wasmfilehandler_invalid_file_1_c_64_no_check() -> None:
    _ = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/invalid-with-print-1-emcc.wasm",
        int_size=64,
        check_file=False,
    )


def test_wasmfilehandler_invalid_file_1_e_32_no_check() -> None:
    _ = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/invalid-with-print-2-emcc.wasm",
        int_size=32,
        check_file=False,
    )


def test_wasmfilehandler_invalid_file_1_e_64_no_check() -> None:
    _ = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/invalid-with-print-2-emcc.wasm",
        int_size=64,
        check_file=False,
    )


def test_wasmfilehandler_invalid_file_1_e_32_unchecked_repr() -> None:
    w = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/invalid-with-print-2-emcc.wasm",
        int_size=32,
        check_file=False,
    )
    assert (
        repr(w)
        == "Unchecked wasm module file with the uid 139e7266b9dcc32dc5237ac2a43d8883847dee3c792e147528fae3984229cc5d"
    )


def test_wasmfilehandler_repr() -> None:
    w = wasm.WasmFileHandler("testfile.wasm", int_size=32)

    assert (
        repr(w)
        == """Functions in wasm file with the uid 6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de:
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'add_one' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'multi' with 2 i32 parameter(s) and 1 i32 return value(s)
function 'add_two' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'add_eleven' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'no_return' with 1 i32 parameter(s) and 0 i32 return value(s)
function 'no_parameters' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'new_function' with 0 i32 parameter(s) and 1 i32 return value(s)
unsupported function with invalid parameter or result type: 'add_something' 
"""  # noqa: W291
    )


def test_wasmfilehandler_repr_64() -> None:
    w = wasm.WasmFileHandler("testfile.wasm", int_size=64)
    assert (
        repr(w)
        == """Functions in wasm file with the uid 6a0a29e235cd5c60353254bc2b459e631d381cdd0bded7ae6cb44afb784bd2de:
function 'init' with 0 i64 parameter(s) and 0 i64 return value(s)
function 'add_something' with 1 i64 parameter(s) and 1 i64 return value(s)
unsupported function with invalid parameter or result type: 'add_one' 
unsupported function with invalid parameter or result type: 'multi' 
unsupported function with invalid parameter or result type: 'add_two' 
unsupported function with invalid parameter or result type: 'add_eleven' 
unsupported function with invalid parameter or result type: 'no_return' 
unsupported function with invalid parameter or result type: 'no_parameters' 
unsupported function with invalid parameter or result type: 'new_function' 
"""  # noqa: W291
    )


def test_wasmfilehandler_repr_2() -> None:
    w = wasm.WasmFileHandler("testfile-2.wasm", int_size=32)
    assert (
        repr(w)
        == """Functions in wasm file with the uid 360e60c3b092ad735982ba49207f9c3250b111e5963fb630c69f85266172080b:
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'add_one' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'multi' with 2 i32 parameter(s) and 1 i32 return value(s)
function 'add_two' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'add_something_32' with 2 i32 parameter(s) and 1 i32 return value(s)
function 'add_eleven' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'no_return' with 1 i32 parameter(s) and 0 i32 return value(s)
function 'no_parameters' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'new_function' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'mixed_up' with 1 i32 parameter(s) and 1 i32 return value(s)
function 'mixed_up_2' with 2 i32 parameter(s) and 1 i32 return value(s)
function 'mixed_up_3' with 3 i32 parameter(s) and 1 i32 return value(s)
function 'unse_internal' with 1 i32 parameter(s) and 1 i32 return value(s)
unsupported function with invalid parameter or result type: 'add_something' 
"""  # noqa: W291
    )


def test_wasmfilehandler_repr_64_2() -> None:
    w = wasm.WasmFileHandler("testfile-2.wasm", int_size=64)
    assert (
        repr(w)
        == """Functions in wasm file with the uid 360e60c3b092ad735982ba49207f9c3250b111e5963fb630c69f85266172080b:
function 'init' with 0 i64 parameter(s) and 0 i64 return value(s)
function 'add_something' with 1 i64 parameter(s) and 1 i64 return value(s)
unsupported function with invalid parameter or result type: 'add_one' 
unsupported function with invalid parameter or result type: 'multi' 
unsupported function with invalid parameter or result type: 'add_two' 
unsupported function with invalid parameter or result type: 'add_something_32' 
unsupported function with invalid parameter or result type: 'add_eleven' 
unsupported function with invalid parameter or result type: 'no_return' 
unsupported function with invalid parameter or result type: 'no_parameters' 
unsupported function with invalid parameter or result type: 'new_function' 
unsupported function with invalid parameter or result type: 'mixed_up' 
unsupported function with invalid parameter or result type: 'mixed_up_2' 
unsupported function with invalid parameter or result type: 'mixed_up_3' 
unsupported function with invalid parameter or result type: 'unse_internal' 
"""  # noqa: W291
    )


def test_wasmfilehandler_collatz_clang() -> None:
    w = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/collatz-clang.wasm", int_size=32
    )
    assert (
        repr(w)
        == """Functions in wasm file with the uid 87865ffefb62549d3c6ba8bdea5313edc7bf520255694b1407fbac0a6b233f79:
function '__wasm_call_ctors' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'collatz' with 1 i32 parameter(s) and 1 i32 return value(s)
"""
    )


def test_wasmfilehandler_multivalue_clang() -> None:
    w = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/multivalue-clang.wasm", int_size=32
    )
    assert (
        repr(w)
        == """Functions in wasm file with the uid 6f821422038eec251d2f4e6bf2b9a5717b18b5c96a8a8e01fb49f080d9610f6e:
function '__wasm_call_ctors' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
unsupported function with invalid parameter or result type: 'divmod' 
"""  # noqa: W291
    )


def test_wasmfilehandler_cpp_emcc() -> None:
    w = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/wasm-from-cpp-11-emcc.wasm", int_size=32
    )
    assert (
        repr(w)
        == """Functions in wasm file with the uid b3777860ea6f52263d23669f0abf92c98850670922299eff3ef0c676ea46b112:
function '__wasm_call_ctors' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'myFunction' with 1 i32 parameter(s) and 0 i32 return value(s)
function 'myFunction2' with 7 i32 parameter(s) and 1 i32 return value(s)
function 'myFunction3' with 11 i32 parameter(s) and 1 i32 return value(s)
function 'mightloop_returns1' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'longrun' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'verylongrun' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_v' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_c' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_b' with 0 i32 parameter(s) and 1 i32 return value(s)
function '__errno_location' with 0 i32 parameter(s) and 1 i32 return value(s)
function '__stdio_exit' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'emscripten_stack_init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'emscripten_stack_get_free' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'emscripten_stack_get_base' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'emscripten_stack_get_end' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'stackSave' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'stackRestore' with 1 i32 parameter(s) and 0 i32 return value(s)
function 'stackAlloc' with 1 i32 parameter(s) and 1 i32 return value(s)
"""
    )


def test_wasmfilehandler_cpp_clang() -> None:
    w = wasm.WasmFileHandler(
        "wasm-generation/wasmfromcpp/wasm-from-cpp-00-clang.wasm", int_size=32
    )
    assert (
        repr(w)
        == """Functions in wasm file with the uid 191e8cd233c978de9898bff0974920221b038c5d93b706bb97d4aa099368f163:
function '__wasm_call_ctors' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'init' with 0 i32 parameter(s) and 0 i32 return value(s)
function 'myFunction' with 1 i32 parameter(s) and 0 i32 return value(s)
function 'myFunction2' with 7 i32 parameter(s) and 1 i32 return value(s)
function 'myFunction3' with 11 i32 parameter(s) and 1 i32 return value(s)
function 'mightloop_returns1' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'longrun' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'verylongrun' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_v' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_c' with 0 i32 parameter(s) and 1 i32 return value(s)
function 'get_b' with 0 i32 parameter(s) and 1 i32 return value(s)
"""
    )


def test_wasm_circuit_bits() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    with pytest.raises(ValueError):
        c.add_wasm("add_one", w, [1], [1], c.bits)


T = TypeVar("T")
DrawType = Callable[[SearchStrategy[T]], T]


@strategies.composite
def bit_register(
    draw: DrawType,
    name: SearchStrategy[str] = strategies.from_regex(reg_name_regex, fullmatch=True),  # noqa: B008
    size: SearchStrategy[int] = strategies.integers(min_value=2, max_value=32),  # noqa: B008
) -> BitRegister:
    return BitRegister(draw(name.filter(lambda nm: not nm.startswith("q"))), draw(size))


@strategies.composite
def qubit_register(
    draw: DrawType,
    name: SearchStrategy[str] = strategies.from_regex(reg_name_regex, fullmatch=True),  # noqa: B008
    size: SearchStrategy[int] = strategies.integers(min_value=0, max_value=32),  # noqa: B008
) -> QubitRegister:
    return QubitRegister(draw(name), draw(size))


@given(
    reg=strategies.one_of(bit_register(), qubit_register()),
    index=strategies.integers(min_value=0, max_value=32),
)
def test_registers(reg: BitRegister | QubitRegister, index: int) -> None:
    unit_type = Qubit if type(reg) is QubitRegister else Bit
    if index < reg.size:
        assert reg[index] == unit_type(reg.name, index)

    assert [reg[i] for i in range(reg.size)] == [
        unit_type(reg.name, i) for i in range(reg.size)
    ]


@strategies.composite
def bits(
    draw: DrawType,
    name: SearchStrategy[str] = strategies.from_regex(reg_name_regex, fullmatch=True),  # noqa: B008
    index: SearchStrategy[int] = uint32,
) -> Bit:
    return Bit(draw(name.filter(lambda nm: not nm.startswith("q"))), draw(index))


@strategies.composite
def primitive_bit_logic_exps(
    draw: DrawType,
    ops: SearchStrategy[BitWiseOp] = strategies.sampled_from(BitWiseOp),  # noqa: B008
    bits: SearchStrategy[Bit] = bits(),  # noqa: B008
) -> BitLogicExp:
    op = draw(ops)
    args = []
    exp_type = LogicExp.factory(op)
    if not issubclass(exp_type, NullaryOp):
        args.append(draw(bits))
        if issubclass(exp_type, BinaryOp):
            if issubclass(exp_type, PredicateExp):
                const_compare = draw(binary_digits)
                args.append(Bit(const_compare))
            else:
                args.append(draw(bits))
    exp = create_bit_logic_exp(op, args)
    assert isinstance(exp, BitLogicExp)
    return exp


def overflow_wrapper(f: Callable[..., int], maxval: int) -> Callable[..., int]:
    def wrapper(*args: int) -> int:
        return f(*args) % maxval

    return wrapper


@given(
    bit_exp=primitive_bit_logic_exps(),
    constants=strategies.tuples(binary_digits, binary_digits),
)
def test_bit_exp(bit_exp: BitLogicExp, constants: tuple[int, int]) -> None:
    iter_c = iter(constants)
    for inp in bit_exp.all_inputs():
        bit_exp.set_value(inp, next(iter_c))
    op_map: dict[BitWiseOp, Callable] = {
        BitWiseOp.AND: operator.and_,
        BitWiseOp.OR: operator.or_,
        BitWiseOp.XOR: operator.xor,
        BitWiseOp.NOT: operator.not_,
        BitWiseOp.EQ: operator.eq,
        BitWiseOp.NEQ: operator.ne,
        BitWiseOp.ZERO: lambda: 0,
        BitWiseOp.ONE: lambda: 1,
    }
    op_map = {key: overflow_wrapper(val, 2) for key, val in op_map.items()}
    eval_val = bit_exp.eval_vals()

    assert eval_val in (0, 1)
    assert isinstance(bit_exp.op, BitWiseOp)

    correct_val = op_map[bit_exp.op](*bit_exp.args)

    if isinstance(correct_val, bool):
        correct_val = int(correct_val)

    assert eval_val == correct_val


@strategies.composite
def primitive_reg_logic_exps(
    draw: Callable,
    ops: SearchStrategy[RegWiseOp] = strategies.sampled_from(RegWiseOp),  # noqa: B008
    bit_regs: SearchStrategy[BitRegister] = bit_register(),  # noqa: B008
) -> RegLogicExp:
    op = draw(ops)

    exp_type = LogicExp.factory(op)
    args: list[BitRegister] = [draw(bit_regs)]
    if issubclass(exp_type, BinaryOp):
        if issubclass(
            exp_type,
            RegEq | RegNeq | RegLt | RegGt | RegLeq | RegGeq,
        ):
            const_compare = draw(uint64)
            args.append(const_compare)
        else:
            args.append(draw(bit_regs))
    else:
        assert issubclass(exp_type, UnaryOp)
    exp = create_reg_logic_exp(op, args)
    assert isinstance(exp, RegLogicExp)
    return exp


@given(
    reg_exp=primitive_reg_logic_exps(),
    constants=strategies.tuples(
        uint64,
        uint64,
    ),
)
def test_reg_exp(reg_exp: RegLogicExp, constants: tuple[int, int]) -> None:
    if isinstance(reg_exp, RegPow):
        # to stop massive numbers
        constants = (min(1000, constants[0]), min(constants[1], 3))
    iter_c = iter(constants)
    for inp in reg_exp.all_inputs():
        reg_exp.set_value(inp, next(iter_c))

    op_map: dict[RegWiseOp, Callable] = {
        RegWiseOp.AND: operator.and_,
        RegWiseOp.OR: operator.or_,
        RegWiseOp.XOR: operator.xor,
        RegWiseOp.NEQ: operator.ne,
        RegWiseOp.EQ: operator.eq,
        RegWiseOp.LT: operator.lt,
        RegWiseOp.GT: operator.gt,
        RegWiseOp.LEQ: operator.le,
        RegWiseOp.GEQ: operator.ge,
    }
    unsupported_ops = {
        RegWiseOp.ADD,
        RegWiseOp.SUB,
        RegWiseOp.MUL,
        RegWiseOp.DIV,
        RegWiseOp.POW,
        RegWiseOp.LSH,
        RegWiseOp.RSH,
        RegWiseOp.NOT,
        RegWiseOp.NEG,
    }
    eval_val = reg_exp.eval_vals()
    op = reg_exp.op
    assert isinstance(op, RegWiseOp)

    correct_val = reg_exp if op in unsupported_ops else op_map[op](*reg_exp.args)
    if isinstance(correct_val, bool):
        correct_val = int(correct_val)

    assert eval_val == correct_val


@strategies.composite
def composite_bit_logic_exps(
    draw: Callable,
    bits: SearchStrategy[Bit] = bits(),  # noqa: B008
    constants: SearchStrategy[int] = binary_digits,
    operators: SearchStrategy[Callable] = strategies.sampled_from(  # noqa: B008
        [
            operator.and_,
            operator.or_,
            operator.xor,
        ]
    ),
    n_terms: SearchStrategy[int] = strategies.integers(min_value=2, max_value=7),  # noqa: B008
) -> BitLogicExp:
    terms = draw(n_terms)
    exp: BitLogicExp
    exp = draw(bits)
    for _ in range(terms):
        chosen_operator = draw(operators)
        if chosen_operator is operator.not_:
            exp = chosen_operator(exp)
        else:
            exp = chosen_operator(exp, draw(strategies.one_of(bits, constants)))
    return exp


@strategies.composite
def composite_reg_logic_exps(
    draw: DrawType,
    regs: SearchStrategy[BitRegister] = bit_register(),  # noqa: B008
    constants: SearchStrategy[int] = uint64,
    operators: SearchStrategy[Callable] = strategies.sampled_from(  # noqa: B008
        [
            operator.and_,
            operator.or_,
            operator.xor,
        ]
    ),
    n_terms: SearchStrategy[int] = strategies.integers(min_value=1, max_value=7),  # noqa: B008
) -> RegLogicExp:
    terms = draw(n_terms)
    exp = draw(regs)
    used_reg_names = {exp.name}
    for _ in range(terms):
        chosen_operator = draw(operators)
        if chosen_operator is operator.not_:
            exp = chosen_operator(exp)
        else:
            second_operand = draw(
                strategies.one_of(
                    regs.filter(lambda x: x.name not in used_reg_names), constants
                )
            )
            exp = chosen_operator(exp, second_operand)
            if isinstance(second_operand, BitRegister):
                used_reg_names.add(second_operand.name)
    assert isinstance(exp, RegLogicExp)
    return exp


@strategies.composite
def bit_const_predicates(
    draw: DrawType,
    exp: SearchStrategy[BitLogicExp] = composite_bit_logic_exps(),  # noqa: B008
    operators: SearchStrategy[
        Callable[[Bit | BitLogicExp], PredicateExp]
    ] = strategies.sampled_from([if_bit, if_not_bit]),  # noqa: B008
) -> PredicateExp:
    func = draw(operators)
    arg = draw(exp)
    return func(arg)  # type: ignore


@strategies.composite
def reg_const_predicates(
    draw: DrawType,
    exp: SearchStrategy[RegLogicExp] = composite_reg_logic_exps(),  # noqa: B008
    operators: SearchStrategy[
        Callable[[RegLogicExp | BitRegister, int], PredicateExp]
    ] = strategies.sampled_from([reg_eq, reg_neq, reg_lt, reg_gt, reg_leq, reg_geq]),  # noqa: B008
    constants: SearchStrategy[int] = uint64,
) -> PredicateExp:
    return draw(operators)(draw(exp), draw(constants))  # type: ignore


@given(condition=strategies.one_of(bit_const_predicates(), reg_const_predicates()))
@settings(print_blob=True, deadline=None)
def test_regpredicate(condition: PredicateExp) -> None:
    assert isinstance(condition, PredicateExp)
    # test serialization round trip here
    # as condition should be the most nested structure
    assert LogicExp.from_dict(condition.to_dict()) == condition
    circ = Circuit()
    qb = Qubit("q_", 1)
    # no bits should start with "q" due to filters
    circ.add_qubit(qb)
    if isinstance(condition, RegLogicExp):
        reg_added = set()
        for inp in condition.all_inputs():
            if inp not in reg_added:
                assert isinstance(inp, BitRegister)
                circ.add_c_register(inp)
                reg_added.add(inp)
    else:
        for inp in condition.all_inputs():
            assert isinstance(inp, Bit)
            circ.add_bit(inp, reject_dups=False)

    circ.X(qb, condition=condition)
    newcirc = circ.copy()
    DecomposeClassicalExp().apply(newcirc)

    assert newcirc.n_gates >= circ.n_gates
    if isinstance(condition, RegLogicExp):
        commands = newcirc.get_commands()
        check_range = commands[-2].op.type == OpType.RangePredicate
        if not check_range:
            print_commands(newcirc)
            print(condition.args[0])  # noqa: T201
        assert check_range


def compare_commands_box(
    circ1: Circuit, circ2: Circuit, print_com: bool = False
) -> bool:
    assert circ1.bits == circ2.bits
    assert circ1.qubits == circ2.qubits
    assert circ1.qubits == circ2.qubits
    assert circ1.name == circ2.name

    commands_equal = True
    for c1, c2 in zip(circ1, circ2, strict=False):
        if print_com:
            print(c1, c2)  # noqa: T201
        if c1.op.type == OpType.Conditional:
            assert isinstance(c1.op, Conditional)
            assert isinstance(c2.op, Conditional)
            commands_equal &= c1.op.value == c2.op.value
            commands_equal &= c1.op.width == c2.op.width
            commands_equal &= c1.op == c2.op
            commands_equal &= c1.args == c2.args
        else:
            commands_equal &= c1 == c2
        if not commands_equal:
            break
    return commands_equal


def test_compare_commands_box() -> None:
    c = Circuit(2)
    bits = [Bit(i) for i in range(2)]
    for b in bits:
        c.add_bit(b)
    c.X(0).X(0)
    c.X(0, condition=(bits[1] & bits[0]))

    assert compare_commands_box(c, c.copy())
    c2 = Circuit(2)
    for b in bits:
        c2.add_bit(b)
    c2.X(0).X(0)
    # swap bits in expression
    c2.X(0, condition=(bits[0] & bits[1]))
    assert not compare_commands_box(c, c2)


def check_serialization_roundtrip(circ: Circuit) -> None:
    circ_dict = circ.to_dict()
    circ_from_dict = Circuit.from_dict(circ_dict)
    validate(instance=circ_dict, schema=schema)
    assert compare_commands_box(circ_from_dict, circ)
    assert circ_from_dict.to_dict() == circ_dict


def test_conditional() -> None:
    c = Circuit(1, 2)
    c.H(0, condition_bits=[0, 1], condition_value=3)
    op = c.get_commands()[0].op
    cond_op = Conditional(Op.create(OpType.H), 2, 3)
    assert op == cond_op


def test_range_predicate_properties() -> None:
    range_predicate = RangePredicateOp(width=8, lower=3, upper=5)
    assert range_predicate.lower == 3
    assert range_predicate.upper == 5


def test_conditional_classicals() -> None:
    c = Circuit()
    b = c.add_c_register("b", 2)
    c.add_c_and(b[0], b[1], b[1], condition=b[0])
    assert str(c.get_commands()[0]) == "IF ([b[0]] == 1) THEN AND b[0], b[1];"


def test_conditional_wasm() -> None:
    c = Circuit(0, 6)
    b = c.add_c_register("b", 2)
    c._add_wasm(  # noqa: SLF001
        "funcname", "wasmfileuid", [1, 1], [], [Bit(0), Bit(1)], [0], condition=b[0]
    )

    assert c.depth() == 1
    assert str(c.get_commands()[0]) == "IF ([b[0]] == 1) THEN WASM c[0], c[1], _w[0];"


def test_conditional_wasm_ii() -> None:
    c = Circuit(0, 6)
    b = c.add_c_register("b", 2)
    c._add_wasm("funcname", "wasmfileuid", [b], [], [0], condition=b[0])  # noqa: SLF001

    assert c.depth() == 1
    assert str(c.get_commands()[0]) == "IF ([b[0]] == 1) THEN WASM b[0], b[1], _w[0];"


def test_conditional_wasm_iii() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")

    c = Circuit(6, 6)
    c0 = c.add_c_register("c0", 3)
    c1 = c.add_c_register("c1", 4)
    c2 = c.add_c_register("c2", 5)

    b = c.add_c_register("b", 2)

    c.add_wasm_to_reg("multi", w, [c0, c1], [c2], condition=b[0])
    c.add_wasm_to_reg("add_one", w, [c2], [c2], condition=b[1])

    assert c.depth() == 2
    assert (
        str(c.get_commands()[0])
        == "IF ([b[0]] == 1) THEN WASM c0[0], c0[1], c0[2], c1[0], c1[1], c1[2], c1[3], c2[0], c2[1], c2[2], c2[3], c2[4], _w[0];"
    )
    assert (
        str(c.get_commands()[1])
        == "IF ([b[1]] == 1) THEN WASM c2[0], c2[1], c2[2], c2[3], c2[4], c2[0], c2[1], c2[2], c2[3], c2[4], _w[0];"
    )


def test_conditional_wasm_iv() -> None:
    w = wasm.WasmFileHandler("testfile.wasm")
    c = Circuit(0, 6)
    b = c.add_c_register("controlreg", 2)
    c.add_wasm("add_two", w, [1], [1], [Bit(0), Bit(1)], condition=b[0])

    assert c.depth() == 1
    assert (
        str(c.get_commands()[0])
        == "IF ([controlreg[0]] == 1) THEN WASM c[0], c[1], _w[0];"
    )


def test_sym_sub_range_pred() -> None:
    c = Circuit(1, 2)
    c.H(0, condition=reg_eq(BitRegister("c", 2), 3))
    c1 = c.copy()
    c.symbol_substitution({Symbol("a"): 0.5})

    assert c == c1


def test_depth_classical_only() -> None:
    # https://github.com/quantinuum/tket/issues/1673
    set_bits = SetBitsOp([True, True])
    multi_bit = MultiBitOp(set_bits, 2)
    assert multi_bit.multiplier == 2
    eq_pred_values = [True, False, False, True]
    and_values = [bool(i) for i in [0, 0, 0, 1]]
    circ = Circuit(4, 4, name="test")
    circ.add_gate(multi_bit, [0, 1, 2, 3])
    circ.add_c_predicate(eq_pred_values, [0, 1], 2, "EQ")
    circ.add_c_modifier(and_values, [1], 2)
    circ.measure_all()
    assert circ.depth() == 4
