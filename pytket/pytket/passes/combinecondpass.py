from typing import Any, cast

from pytket import unit_id
from pytket.circuit import BarrierOp, CircBox, Circuit, Command, Conditional

from .._tket.passes import BasePass, CustomPass


def extract_cond(cmd: Command) -> tuple[int, list[Any]] | None:
    if isinstance(cmd.op, Conditional):
        return (cmd.op.value, cmd.args[: cmd.op.width])
    return None


def combine_conditionals(circuit: Circuit) -> Circuit:  # noqa: PLR0912
    """Walk the sequence of commands in the circuit and combine contiguous subsequences
    of conditionals with the same predicate into conditional boxes."""

    # the output circuit
    new_circuit = Circuit()
    for qb in circuit.qubits:
        new_circuit.add_qubit(qb)
    for cb in circuit.bits:
        new_circuit.add_bit(cb)
    for qr in circuit.q_registers:
        new_circuit.add_q_register(qr)
    for cr in circuit.c_registers:
        new_circuit.add_c_register(cr)

    # the tuple of value and args describing the current conditional
    curr_cond = None
    # subcircuit for the current subsequence
    sub_circ = Circuit()
    sub_args = set()

    def emit_cond_box(
        top_circ: Circuit, sub_circ: Circuit, cond: tuple[int, list[Any]]
    ) -> None:
        cond_value = cond[0]
        cond_args = cond[1]
        sub_arg_list = sub_circ.qubits + sub_circ.bits
        if sub_circ.n_gates == 1:
            # if there was only one predicated op, don't emit a CircBox
            sub_cmd = sub_circ.get_commands()[0]
            top_circ.add_gate(
                sub_cmd.op,
                sub_arg_list,
                condition_bits=cond_args,
                condition_value=cond_value,
            )
        else:
            new_circuit.add_gate(
                CircBox(sub_circ),
                sub_arg_list,
                condition_bits=cond_args,
                condition_value=cond_value,
            )

    for cmd in circuit.get_commands():
        cond = extract_cond(cmd)
        # if this is not part of the ongoing subsequence,
        # emit the previous subsequence to the new circuit
        if curr_cond is not None and curr_cond != cond:
            emit_cond_box(new_circuit, sub_circ, curr_cond)

            sub_circ = Circuit()
            sub_args.clear()
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
        emit_cond_box(new_circuit, sub_circ, curr_cond)

    return new_circuit


def combine_cond_pass() -> BasePass:
    return CustomPass(combine_conditionals, label="combine_conditionals")
