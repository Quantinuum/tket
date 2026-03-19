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

from typing import Any, cast

from pytket import unit_id
from pytket.circuit import BarrierOp, CircBox, Circuit, Command, Conditional

from .._tket.passes import BasePass, CustomPass


def extract_cond(cmd: Command) -> tuple[int, list[Any]] | None:
    if isinstance(cmd.op, Conditional) and not isinstance(cmd.op.op, CircBox):
        return (cmd.op.value, cmd.args[: cmd.op.width])
    return None

def emit_cond_box(
    top_circ: Circuit, sub_circ: Circuit, cond: tuple[int, list[Any]],
    max_wreg: int, max_rreg: int
) -> None:
    # add WASM and RNG args
    if max_wreg > -1:
        sub_circ._add_w_register(max_wreg+1)
    if max_rreg > -1:
        sub_circ._add_r_register(max_rreg+1)

    cond_value = cond[0]
    cond_args = cond[1]
    if len(sub_circ.get_commands()) == 1:
        # if there was only one predicated op, don't emit a CircBox
        sub_cmd = sub_circ.get_commands()[0]
        top_circ.add_gate(
            sub_cmd.op,
            sub_cmd.args,
            condition_bits=cond_args,
            condition_value=cond_value,
        )
    else:
        sub_arg_list = sub_circ.qubits + sub_circ.bits
        top_circ.add_gate(
            CircBox(sub_circ),
            sub_arg_list,
            condition_bits=cond_args,
            condition_value=cond_value,
        )

def combine_conditionals(circuit: Circuit) -> Circuit:  # noqa: PLR0912
    """Walk the sequence of commands in the circuit and combine contiguous subsequences
    of conditionals with the same predicate into conditional boxes."""

    # the output circuit
    new_circuit = Circuit()
    for qb in circuit.qubits:
        new_circuit.add_qubit(qb)
    for cb in circuit.bits:
        new_circuit.add_bit(cb)

    # the tuple of value and args describing the current conditional
    curr_cond = None
    # subcircuit for the current subsequence
    sub_circ = Circuit()
    # arg set for the current subsequence
    sub_args = set()
    # largest WASM/RNG ID seen in the total circuit/current subsequence
    max_wreg = -1
    max_rreg = -1
    max_sub_wreg = -1
    max_sub_rreg = -1

    for cmd in circuit.get_commands():
        cond = extract_cond(cmd)
        # if this is not part of the ongoing subsequence,
        # emit the previous subsequence to the new circuit
        if curr_cond is not None and curr_cond != cond:
            emit_cond_box(new_circuit, sub_circ, curr_cond, max_sub_wreg, max_sub_rreg)

            sub_circ = Circuit()
            sub_args.clear()
            max_sub_wreg = -1
            max_sub_rreg = -1
            curr_cond = None

        # if this is a conditional, add it to the ongoing subcircuit
        # otherwise, emit it directly.
        if cond is not None:
            cond_op = cast("Conditional", cmd.op)
            width = cond_op.width
            for arg in cmd.args[width:]:
                if arg not in sub_args:
                    if isinstance(arg, unit_id.Bit):
                        sub_circ.add_bit(arg)
                    elif isinstance(arg, unit_id.Qubit):
                        sub_circ.add_qubit(arg)
                    elif isinstance(arg, unit_id.WasmState):
                        reg_id_s = str(arg).split('[')[1].split(']')[0]
                        reg_id = int(reg_id_s)
                        max_wreg = max(max_wreg, reg_id)
                        max_sub_wreg = max(max_sub_wreg, reg_id)
                    elif isinstance(arg, unit_id.RngState):
                        reg_id_s = str(arg).split('[')[1].split(']')[0]
                        reg_id = int(reg_id_s)
                        max_rreg = max(max_rreg, reg_id)
                        max_sub_rreg = max(max_sub_rreg, reg_id)
                    else:
                        raise ValueError("Unknown arg type")
                    sub_args.add(arg)

            if isinstance(cond_op.op, BarrierOp):
                sub_circ.add_barrier(cmd.args[width:])
            else:
                sub_circ.add_gate(cond_op.op, cmd.args[width:])
            curr_cond = cond
        elif isinstance(cmd.op, BarrierOp):
            new_circuit.add_barrier(cmd.args)
        else:
            new_circuit.add_gate(cmd.op, cmd.args)

    # emit final if necessary
    if curr_cond is not None:
        emit_cond_box(new_circuit, sub_circ, curr_cond, max_sub_wreg, max_sub_rreg)

    # add WASM states if necessary
    if max_wreg > -1:
        new_circuit._add_w_register(max_wreg+1)
    if max_rreg > -1:
        new_circuit._add_r_register(max_rreg+1)
        
    return new_circuit


def combine_cond_pass() -> BasePass:
    return CustomPass(combine_conditionals, label="combine_conditionals")
