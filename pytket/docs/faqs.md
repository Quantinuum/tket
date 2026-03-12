# TKET FAQs

These are frequently asked questions that relate to the use of pytket. For installation FAQs see the [installation troubleshooting](install.md) page.

## Rebases

Q: Can I convert a pytket {py:class}`~.Circuit` to a gateset of my choice?

A: Yes, this can be done in many cases provided the target gateset is a set of one and two qubit pytket {py:class}`~.OpType` s.
There are two types of rebase

1. {py:meth}`~.passes.AutoRebase` - this uses a set of hardcoded decompositions to convert between gatesets. This can be used quickly when the gateset is one widely used on quantum hardware e.g. IBM's {X, SX, Rz, CX} gateset.

2. {py:meth}`~.passes.RebaseCustom` - This can be used instead of {py:meth}`~.passes.AutoRebase` in cases where there is no hardcoded conversion available.
In this case the user will have to specify how to implement TKET's {TK1, CX} or {TK1, TK2} operations in terms of the target {py:class}`~.OpType` s.

See the manual section on [rebases](https://docs.quantinuum.com/tket/user-guide/manual/manual_compiler.html#rebases) for examples.

## Unitary Synthesis

Q: Can TKET generate a circuit to implement a unitary operator of my choice?

A: Yes but only up to three qubits at present. This can be done with {py:class}`~.Unitary3qBox`.

See the manual section on [unitary synthesis](https://docs.quantinuum.com/tket/user-guide/manual/manual_circuit.html#boxes-for-unitary-synthesis) .

## Qiskit to TKET Conversion

Q: How can I convert my qiskit {py:class}`qiskit.circuit.QuantumCircuit` to a pytket {py:class}`~.Circuit`?

A: This can be achieved using the {py:func}`~pytket.extensions.qiskit.qiskit_to_tk` function from the [pytket-qiskit extension](https://docs.quantinuum.com/tket/extensions/pytket-qiskit/)

```
from qiskit import QuantumCircuit
from pytket import Circuit

# Define qiskit QuantumCircuit
qc = QuantumCircuit(3)
qc.h(0)
qc.cx(0, 1)

# Convert to pytket
tk_circ = qiskit_to_tk(qc)
```

Conversion in the opposite direction can be accomplished using {external+pytket-qiskit:py:func}`pytket.extensions.qiskit.tk_to_qiskit`. In the case where there is no replacement for a pytket operation in qiskit the unsupported operation will be implemented in terms of the available gates.

Note here that `pytket` and `qiskit` use different qubit ordering conventions so care should be taken when
converting between circuit formats and interpreting results.

In some cases qiskit circuits contain higher level operations which cannot be handled by the converter.
In such a case we can use the {py:meth}`qiskit.circuit.QuantumCircuit.decompose` method and then try to perform the conversion again.
