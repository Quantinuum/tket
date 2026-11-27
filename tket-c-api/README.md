# Native C API

The C API offers two ways to move a circuit into and out of TKET. Use the
[direct interface](include/tket-c-api.h) when you already have the operations
and want to append them to TKET's graph and walk the result one command at a
time. Use the JSON interface to load or save a circuit in TKET's exchange format.
The same pass handle works with circuits built either way.

## What the direct API can represent

| Feature | Default-register API | Named-wire API |
| --- | --- | --- |
| Wire identity | Single-index qubits in `q` and bits in `c`. Register counts and the output-permutation builder require contiguous indices from zero. | Named qubits and bits with multidimensional indices, plus the standard WASM and RNG state wires. A register has one wire kind and index dimension. |
| Operations | The typed `TKET_GATE_*` operations in the header. Payload accessors cover barriers, `SetBits` and `CircBox`. | The same typed operations, with optional operation groups. Other operations can be passed individually as opaque CBOR when TKET supports their operation schema. |
| Angles | Finite doubles or SymEngine expressions. Read angles with the iterator that matches the representation you need. | SymEngine expressions, including symbolic angles. |
| Circuit metadata | Name, global phase, input creation, output discard and implicit qubit permutations. | The same metadata. The wire table includes boundary flags and the output reached by each input wire. |

The typed operations include the listed one- and two-qubit gates, Toffoli,
SWAP, measurement, reset, barriers, `SetBits`, circuit boxes, zero-wire
`Phase`, and `noop`. A barrier can read a bit without consuming its wire.
Iteration preserves that read's position relative to writes to the bit.
Angles are in half-turns, so 1.0 is π radians. The numeric iterator reports
`TKET_ERROR_SYMBOLIC_PARAMETER` if an angle cannot be evaluated as a double.

Operations outside the typed list, such as controlled Hadamard, conditionals
and other box types, have no typed builder or view. The named iterator reports
`TKET_GATE_OPAQUE` for them. Use its payload accessor to read a CBOR encoding of
the operation using TKET's operation schema. The default-register iterator reports
`TKET_ERROR_UNSUPPORTED_OPERATION` for the same operations.

The API builds, inspects and transforms circuits. To execute a circuit or
obtain measurement results, use a simulator or execution backend. Use the
native pass constructors for AutoRebase, RemoveRedundancies,
CommuteThroughMultis and pass sequences. AutoRebase requires a basis for which
TKET has known decompositions. The API also exposes two-qubit squash,
Clifford simplification and PhasedX/Rz squash as immediate transforms. Load
other pass definitions through the JSON interface.

## Iteration and ownership

Create an iterator with `tket_circuit_iterator_create`, then call
`tket_iterator_next` or `tket_iterator_next_with_expressions` until
`out_has_operation` is false. The returned argument and parameter arrays
belong to the iterator and remain valid until it advances or is freed.

The iterator keeps the circuit's storage alive, so freeing the circuit handle
first is safe. Mutating a circuit invalidates its iterators. Their next call
returns `TKET_ERROR_ITERATOR_INVALIDATED`. Synchronise access to any handles
that share circuit storage.

The iterator walks the circuit graph in dependency order. Independent
operations can appear in a different order from JSON export. Each operation's
arguments are returned in port order, including controls and targets.

For named wires, use `tket_circuit_create_named` and pass the declared IDs to
the named builders. After optimisation, create a named iterator and look up
the wire IDs it assigns to the result. Each operation view includes its group
if it has one.

After an iterator error, free the iterator. C++ exceptions are caught at the
C boundary. Use `tket_last_error_message` to read the details on the calling
thread. Free functions accept NULL. Native output handles are set to NULL on
failure.

## Building and checking

Follow the [source build guide](../build-without-conan.md#tket-c-api) to build
and test the C API with CMake. Use the resulting install prefix as
`TKET_C_API_PATH` when building a client that uses that variable to locate TKET.

The Conan recipe also runs the C API tests when the build host can run the
target binaries.

The [C example](tests/header.c) builds and walks a circuit using the public
header. The [native tests](tests/native.cpp) walk through symbolic angles,
pass application, circuit boxes, named wires and opaque operations.
