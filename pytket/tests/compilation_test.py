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

import pytest

from pytket.architecture import Architecture
from pytket.circuit import Circuit, OpType, reg_eq
from pytket.passes import (
    CXMappingPass,
    FullPeepholeOptimise,
    PassSelector,
    scratch_reg_resize_pass,
)
from pytket.placement import Placement
from pytket.unit_id import _TEMP_BIT_NAME, _TEMP_BIT_REG_BASE


def test_compilation() -> None:
    # TKET-2335
    c = Circuit.from_dict(
        {
            "bits": [["c", [0]], ["c", [1]], ["c", [2]]],
            "commands": [
                {"args": [["q", [2]]], "op": {"type": "X"}},
                {"args": [["q", [3]]], "op": {"type": "SX"}},
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {"args": [["q", [4]], ["q", [0]]], "op": {"type": "CX"}},
                {
                    "args": [["q", [2]]],
                    "op": {"params": ["1.92159003147011"], "type": "Rz"},
                },
                {
                    "args": [["q", [3]]],
                    "op": {"params": ["2.27434149111356"], "type": "Rz"},
                },
                {
                    "args": [["q", [0]]],
                    "op": {"params": ["3.6441046525849"], "type": "Rz"},
                },
                {"args": [["q", [2]]], "op": {"type": "X"}},
                {
                    "args": [["q", [3]]],
                    "op": {"params": ["3.89662172760735"], "type": "Rz"},
                },
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {
                    "args": [["q", [0]]],
                    "op": {"params": ["1.16126164227384"], "type": "Rz"},
                },
                {"args": [["q", [2]]], "op": {"type": "SX"}},
                {"args": [["q", [3]]], "op": {"type": "X"}},
                {"args": [["q", [4]]], "op": {"type": "SX"}},
                {"args": [["q", [2]], ["q", [0]]], "op": {"type": "CX"}},
                {"args": [["q", [3]]], "op": {"type": "SX"}},
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {"args": [["q", [0]]], "op": {"type": "X"}},
                {"args": [["q", [1]], ["q", [2]]], "op": {"type": "CX"}},
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {"args": [["q", [0]]], "op": {"type": "X"}},
                {
                    "args": [["q", [1]]],
                    "op": {"params": ["0.803110840391647"], "type": "Rz"},
                },
                {"args": [["q", [2]]], "op": {"type": "SX"}},
                {
                    "args": [["q", [4]]],
                    "op": {"params": ["2.1139514637727"], "type": "Rz"},
                },
                {
                    "args": [["q", [0]]],
                    "op": {"params": ["2.91558747399915"], "type": "Rz"},
                },
                {
                    "args": [["q", [1]]],
                    "op": {"params": ["3.43170320594144"], "type": "Rz"},
                },
                {
                    "args": [["q", [2]]],
                    "op": {"params": ["0.00131066898193488"], "type": "Rz"},
                },
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {"args": [["q", [2]], ["q", [0]]], "op": {"type": "CX"}},
                {
                    "args": [["q", [1]]],
                    "op": {"params": ["0.792771074284277"], "type": "Rz"},
                },
                {"args": [["q", [0]]], "op": {"type": "SX"}},
                {
                    "args": [["q", [1]]],
                    "op": {"params": ["3.5432014179236"], "type": "Rz"},
                },
                {"args": [["q", [2]]], "op": {"type": "SX"}},
                {
                    "args": [["q", [0]]],
                    "op": {"params": ["3.15399322190124"], "type": "Rz"},
                },
                {"args": [["q", [1]]], "op": {"type": "X"}},
                {"args": [["q", [0]], ["q", [4]]], "op": {"type": "CX"}},
                {"args": [["q", [3]], ["q", [1]]], "op": {"type": "CX"}},
                {"args": [["q", [0]]], "op": {"type": "SX"}},
                {
                    "args": [["q", [1]]],
                    "op": {"params": ["0.993390217293483"], "type": "Rz"},
                },
                {"args": [["q", [4]], ["q", [2]]], "op": {"type": "CX"}},
                {"args": [["q", [3]]], "op": {"type": "SX"}},
                {"args": [["q", [0]]], "op": {"type": "X"}},
                {"args": [["q", [2]]], "op": {"type": "X"}},
                {"args": [["q", [4]]], "op": {"type": "SX"}},
                {"args": [["q", [2]], ["q", [1]]], "op": {"type": "CX"}},
                {"args": [["q", [1]]], "op": {"type": "SX"}},
                {"args": [["q", [4]], ["q", [2]]], "op": {"type": "CX"}},
                {"args": [["q", [1]]], "op": {"type": "SX"}},
                {"args": [["q", [2]]], "op": {"type": "X"}},
                {"args": [["q", [4]], ["q", [3]]], "op": {"type": "CX"}},
                {"args": [["q", [2]], ["q", [3]]], "op": {"type": "CX"}},
                {"args": [["q", [4]]], "op": {"type": "X"}},
                {"args": [["q", [3]], ["c", [1]]], "op": {"type": "Measure"}},
                {
                    "args": [["q", [4]]],
                    "op": {"params": ["2.12070321579766"], "type": "Rz"},
                },
                {"args": [["q", [0]], ["q", [4]]], "op": {"type": "CX"}},
                {"args": [["q", [0]], ["c", [2]]], "op": {"type": "Measure"}},
                {
                    "args": [["q", [4]]],
                    "op": {"params": ["2.08463952265192"], "type": "Rz"},
                },
                {"args": [["q", [4]], ["q", [2]]], "op": {"type": "CX"}},
                {
                    "args": [["q", [2]]],
                    "op": {"params": ["3.23214826870156"], "type": "Rz"},
                },
                {"args": [["q", [4]], ["q", [2]]], "op": {"type": "CX"}},
                {"args": [["q", [2]]], "op": {"type": "X"}},
                {
                    "args": [["q", [4]]],
                    "op": {"params": ["2.02237956848157"], "type": "Rz"},
                },
                {"args": [["q", [4]], ["c", [0]]], "op": {"type": "Measure"}},
            ],
            "implicit_permutation": [
                [["q", [0]], ["q", [0]]],
                [["q", [1]], ["q", [1]]],
                [["q", [2]], ["q", [2]]],
                [["q", [3]], ["q", [3]]],
                [["q", [4]], ["q", [4]]],
            ],
            "phase": "0.0",
            "qubits": [["q", [0]], ["q", [1]], ["q", [2]], ["q", [3]], ["q", [4]]],
        }
    )
    assert FullPeepholeOptimise().apply(c)


def test_PassSelector() -> None:
    fp = FullPeepholeOptimise()
    fp2 = FullPeepholeOptimise(allow_swaps=False)

    def circ_depth(circ: Circuit) -> int:
        return circ.depth()

    sp = PassSelector([fp, fp2], circ_depth)

    circ = Circuit(2).H(1).H(0).H(1).H(0).X(1).CX(1, 0).CX(0, 1).CX(1, 0)

    result = sp.apply(circ)

    assert sp.get_scores() == [1, 4]

    assert result.depth() == min(x for x in sp.get_scores() if x is not None)


def test_PassSelector_wrong_pass() -> None:
    arc = Architecture([(1, 2)])

    pl = Placement(arc)

    cxmp = CXMappingPass(arc, pl)

    def circ_depth(circ: Circuit) -> int:
        return circ.depth()

    sp = PassSelector([cxmp], circ_depth)

    # this circuit has one more qubits than the given arc
    circ = Circuit(3).H(1).H(0).H(1).H(0).X(1).CX(1, 0).CX(0, 1).CX(1, 2)

    with pytest.raises(Exception):  # noqa: B017
        _ = sp.apply(circ)


def test_PassSelector_empty_pass() -> None:
    def circ_depth(circ: Circuit) -> int:
        return circ.depth()

    with pytest.raises(Exception):  # noqa: B017
        _ = PassSelector([], circ_depth)


def test_PassSelector_ii() -> None:
    fp = FullPeepholeOptimise()
    fp2 = FullPeepholeOptimise(allow_swaps=False)

    def count_gates(circ: Circuit) -> int:
        return circ.n_gates_of_type(OpType.CX)

    sp = PassSelector([fp, fp2], count_gates)

    circ = Circuit(2).H(1).H(0).H(1).H(0).X(1).CX(1, 0).CX(0, 1).CX(1, 0)

    result = sp.apply(circ)

    assert sp.get_scores() == [0, 3]

    assert count_gates(result) == min(x for x in sp.get_scores() if x is not None)


def test_PassSelector_iii() -> None:
    fp = FullPeepholeOptimise()
    fp2 = FullPeepholeOptimise(allow_swaps=False)

    def count_gates(circ: Circuit) -> int:
        return circ.n_gates_of_type(OpType.X)

    sp = PassSelector([fp, fp2], count_gates)

    circ = Circuit(2).H(1).H(0).H(1).H(0).X(1).CX(1, 0).CX(0, 1).CX(1, 0)

    result = sp.apply(circ)

    assert sp.get_scores() == [0, 0]

    assert count_gates(result) == min(x for x in sp.get_scores() if x is not None)


def test_resize_scratch_registers() -> None:
    max_c_reg_width_list = [30, 40]
    for max_c_reg_width in max_c_reg_width_list:
        circ = Circuit(1)
        reg_a = circ.add_c_register("a", 1)
        reg_b = circ.add_c_register("b", 1)
        n_scratch_bits = max_c_reg_width * 2 + 2
        for _ in range(n_scratch_bits):
            circ.add_gate(OpType.PhasedX, [1, 0], [0], condition=reg_a[0] ^ reg_b[0])
        original_scratch_reg = circ.get_c_register(_TEMP_BIT_NAME)
        # check the scratch reg size is max_c_reg_width
        assert original_scratch_reg.size == n_scratch_bits

        # apply the resize pass
        c_compiled = circ.copy()
        scratch_reg_resize_pass(max_c_reg_width).apply(c_compiled)

        # check the old register is replaced
        with pytest.raises(RuntimeError) as e:
            c_compiled.get_c_register(_TEMP_BIT_NAME)
        err_msg = "Cannot find classical register"
        assert err_msg in str(e.value)

        # check the new registers have the correct sizes
        scratch_reg1 = c_compiled.get_c_register(f"{_TEMP_BIT_NAME}_0")
        scratch_reg2 = c_compiled.get_c_register(f"{_TEMP_BIT_NAME}_1")
        scratch_reg3 = c_compiled.get_c_register(f"{_TEMP_BIT_NAME}_2")
        assert scratch_reg1.size == max_c_reg_width
        assert scratch_reg2.size == max_c_reg_width
        assert scratch_reg3.size == 2
        args_map = {}
        original_cmds = circ.get_commands()
        for cmd in original_cmds:
            for arg in cmd.args:
                args_map[arg] = arg  # noqa: PERF403

        for i in range(n_scratch_bits):
            args_map[original_scratch_reg[i]] = c_compiled.get_c_register(
                f"{_TEMP_BIT_NAME}_{i // max_c_reg_width}"
            )[i % max_c_reg_width]

        # Check the compiled circuit is equivalent to the original one up to renaming
        compiled_cmds = c_compiled.get_commands()
        for i, cmd in enumerate(original_cmds):
            for j, arg in enumerate(cmd.args):
                assert compiled_cmds[i].args[j] == args_map[arg]

    # If the max width is not exceeded, do nothing
    circ = Circuit(1)
    reg_a = circ.add_c_register("a", 1)
    reg_b = circ.add_c_register("b", 1)
    for _ in range(30):
        circ.add_gate(OpType.PhasedX, [1, 0], [0], condition=reg_a[0] ^ reg_b[0])
    c_compiled = circ.copy()
    scratch_reg_resize_pass(40).apply(c_compiled)
    assert circ == c_compiled

    # Test _TEMP_BIT_REG_BASE is ignored
    circ = Circuit(1, name="test_classical")
    reg_a = circ.add_c_register("a", 1)
    reg_b = circ.add_c_register("b", 1)
    circ.X(0, condition=reg_eq(reg_a ^ reg_b, 1))
    assert circ.get_c_register(f"{_TEMP_BIT_REG_BASE}_0").size == 1
    c_compiled = circ.copy()
    scratch_reg_resize_pass(10).apply(c_compiled)
    assert circ == c_compiled
