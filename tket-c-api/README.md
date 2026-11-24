# Native C API

The numeric API in `include/tket-c-api.h` constructs circuits directly and exposes
operations through a pull iterator. The existing JSON entry points are still
available, but callers don't need them to construct a circuit, create a pass
pipeline or read its output.

The gate vocabulary covers all unitary `TketOp` operations in tket2, plus
PhasedX, ZZPhase and PhasedXX for the Helios and Sol platforms. TK1, SWAP,
XXPhase, YYPhase and ZZMax are also supported. Parameters use TKET half-turns,
including the phase-axis parameters of PhasedX and PhasedXX. Global phase is
returned separately as the exponent in `exp(i*pi*phase)`.

Measurement, reset and barriers have explicit operation identifiers. Measurement
arguments are a qubit followed by a bit. Creation and discard are circuit
boundary annotations. Runtime allocation failure, lazy measurement futures and
leakage results still belong to the runtime adapter; treating them as unitary
operations would lose their semantics. Symbolic parameters return an error from
the numeric iterator rather than being approximated or dropped.

## Iteration and ownership

Create an iterator with `tket_circuit_iterator_create`, then call
`tket_iterator_next` until `out_has_operation` is false. The returned argument
and parameter arrays belong to the iterator and remain valid until its next
`next` call or destruction. No caller callback runs while TKET is traversing the
circuit.

The iterator keeps the circuit's storage alive, so freeing the circuit handle
first is safe. Mutating a circuit invalidates existing iterators; their next call
returns `TKET_ERROR_ITERATOR_INVALIDATED`. Handles aren't safe for concurrent
access, including access through another handle sharing the same storage.

Traversal uses a ready-vertex frontier and an edge-to-wire map. It doesn't
materialise a command list or use the slice-based command iterator. Independent
operations may be returned in a different order from JSON export. Argument port
order is preserved, including control/target order.

The numeric API uses default, single-index `q` and `c` registers. Other register
names or shapes, unsupported operations and symbolic gate parameters report
errors. After an iterator error, free the iterator. C++ exceptions are caught at
the C boundary; `tket_last_error_message` explains the error on the calling
thread. Free functions accept NULL. Native output handles are set to NULL on
failure.

## Building and checking

From the repository root, enter devenv and run the source build:

```sh
devenv shell -- bash tket-c-api/scripts/build-from-source.sh
```

The shell supplies the compiler tools and third-party dependencies. The script
builds the libraries under `libs/`, the TKET core and the C API from this
checkout, then runs the C API tests. The install prefix is
`build/native/source/install`. Use that directory as `TKET_C_API_PATH` when
building a caller such as selene-qblaze against this build.

The tests compile the header as C and exercise every operation, argument order,
iterator lifetime, invalidation, exhaustion, phase, native passes and error
handling. Each operation's argument and parameter buffers can be borrowed directly
from the iterator in Rust; no text or circuit snapshot needs to cross the ABI.
