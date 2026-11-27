#ifndef TKET_C_API_H
#define TKET_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The C API has a JSON interface and a direct interface.
//
// The JSON interface lets us load and save circuits in TKET's existing format.
// It's useful when another tool already produces that format, or when we want
// to save a circuit for inspection or use in another program.
//
// The direct interface lets us build a circuit from operations and read the
// result one operation at a time. It's useful when the caller has an internal
// representation of gates and will process the result immediately. This avoids
// building and parsing JSON each time we optimise a circuit.
//
// Both interfaces work with the same circuit and pass handles. We can load a
// circuit from JSON and read it with the direct iterator, or build it directly
// and save the result as JSON. Passes work with circuits from either route.
//
// The direct interface has two ways to identify wires. Use register indices
// for the default q and c registers. Use named wires for custom register names
// or coordinates, operation groups and standard WASM and RNG state wires.
// Both forms expose circuit boundaries, global phase and the operations
// listed under TKET_GATE_*.
//
// The named form can also exchange individual operations outside the typed
// list as opaque CBOR when TKET can serialise them. The plain iterator reports
// unsupported operations as errors. Angles can be read as expressions, or as
// finite doubles where the parameters can be evaluated. A circuit handle
// describes a circuit for inspection or optimisation. Execution and measurement
// results belong to the caller's backend.

// ===========================================================================
// Handles and errors
// ===========================================================================
//
// Circuits and passes live inside TKET. We use opaque handles to refer to them
// from C or another language, and free each handle when we're finished with it.
// The functions for freeing them are collected at the end of this file.
//
// Synchronise access to a handle when using it across threads. This also
// applies when an iterator shares storage with a circuit.
//
// Functions returning TketError report success with TKET_SUCCESS. If a call
// fails, tket_last_error_message gives more detail on the calling thread.
// Functions that return a handle directly return NULL on failure. C++
// exceptions are caught inside the API.

// Opaque handle to tket Circuit object
typedef struct TketCircuit TketCircuit;

// Opaque handle to tket BasePass object
typedef struct TketPass TketPass;

typedef enum {
  TKET_SUCCESS = 0,
  TKET_ERROR_NULL_POINTER = 1,
  TKET_ERROR_CIRCUIT_INVALID = 2,
  TKET_ERROR_INVALID_ARGUMENT = 3,
  TKET_ERROR_UNSUPPORTED_OPERATION = 4,
  TKET_ERROR_SYMBOLIC_PARAMETER = 5,
  TKET_ERROR_OUT_OF_MEMORY = 6,
  TKET_ERROR_INTERNAL = 7,
  TKET_ERROR_ITERATOR_INVALIDATED = 8,
} TketError;

/**
 * Get the error message from the last fallible API call on this thread
 *
 * Native functions catch C++ exceptions and return an error code. The message
 * remains valid until the next fallible call on the same thread and must not
 * be freed.
 *
 * @return Borrowed error message
 */
const char *tket_last_error_message(void);

/**
 * Get a short description of an error code
 *
 * @param error Error code to describe
 * @return Static description string. It must not be freed.
 */
const char *tket_error_string(TketError error);

// ===========================================================================
// The JSON interface
// ===========================================================================
//
// Start here if we already have a circuit or pass in TKET's JSON format.
// Loading a circuit gives us a circuit handle that we can optimise or inspect.
// Saving it produces a string that we can keep, send elsewhere or load again.
//
// Pass JSON describes an optimisation to apply to a circuit. Once loaded, a
// pass can be applied with tket_apply_pass in the optimisation section below.

/**
 * Load a circuit from JSON
 *
 * @param json_str Non-NULL, NUL-terminated TKET circuit JSON, borrowed for this
 * call
 * @return Owned circuit handle, or NULL on failure. Free it with
 * tket_free_circuit.
 */
TketCircuit *tket_circuit_from_json(const char *json_str);

/**
 * Save a circuit as JSON
 *
 * @param circuit Non-NULL circuit to read
 * @param json_str Non-NULL output pointer receiving an owned, NUL-terminated
 * JSON string. Set to NULL on failure. Free the string with tket_free_string.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_to_json(const TketCircuit *circuit, char **json_str);

/**
 * Load an optimisation pass from JSON
 *
 * @param json_str Non-NULL, NUL-terminated TKET pass JSON, borrowed for this
 * call
 * @return Owned pass handle, or NULL on failure. Free it with tket_free_pass.
 */
TketPass *tket_pass_from_json(const char *json_str);

// ===========================================================================
// The direct interface: building a circuit
// ===========================================================================

// Create the circuit with the qubits and bits we'll need, then append
// operations in the order we want them to run. We can supply angles as
// doubles or as expressions. Each call borrows its input arrays, so we can
// reuse them as soon as it returns.
//
// The gate identifiers below are the typed vocabulary shared by the builders
// and iterators. The builders in this section identify wires by their index
// in the circuit's default q and c registers.
//
// We can also add a global phase or mark qubits as created and discarded at the
// circuit boundaries. Those annotations describe the circuit. Runtime
// allocation and lazy measurement results remain the responsibility of the
// calling program.

// Native gate identifiers, independent of the C++ OpType values
typedef uint32_t TketGate;
enum {
  TKET_GATE_H = 1,
  TKET_GATE_X = 2,
  TKET_GATE_Y = 3,
  TKET_GATE_Z = 4,
  TKET_GATE_S = 5,
  TKET_GATE_SDG = 6,
  TKET_GATE_T = 7,
  TKET_GATE_TDG = 8,
  TKET_GATE_V = 9,
  TKET_GATE_VDG = 10,

  TKET_GATE_RX = 11,
  TKET_GATE_RY = 12,
  TKET_GATE_RZ = 13,
  TKET_GATE_PHASEDX = 14,

  TKET_GATE_TK1 = 15,
  TKET_GATE_TK2 = 16,

  TKET_GATE_CX = 17,
  TKET_GATE_CY = 18,
  TKET_GATE_CZ = 19,
  TKET_GATE_CRZ = 20,
  TKET_GATE_CCX = 21,
  TKET_GATE_SWAP = 22,

  TKET_GATE_XXPHASE = 23,
  TKET_GATE_YYPHASE = 24,
  TKET_GATE_ZZPHASE = 25,
  TKET_GATE_ZZMAX = 26,
  TKET_GATE_PHASEDXX = 27,

  TKET_GATE_MEASURE = 28,
  TKET_GATE_RESET = 29,
  TKET_GATE_BARRIER = 30,
  TKET_GATE_PHASE = 31,
  TKET_GATE_NOOP = 32,
  TKET_GATE_SET_BITS = 33,
  TKET_GATE_CIRCBOX = 34,
};

typedef uint32_t TketWireKind;
enum { TKET_WIRE_QUBIT = 0, TKET_WIRE_BIT = 1 };
typedef struct {
  /** Quantum or classical wire, identified by TKET_WIRE_QUBIT or TKET_WIRE_BIT.
   */
  TketWireKind kind;
  /** Zero-based index in the q register for qubits or c register for bits. */
  uint32_t index;
} TketWire;

/**
 * Create a circuit with default quantum and classical registers
 *
 * @param n_qubits Number of qubits in the q register, indexed from zero
 * @param n_bits Number of bits in the c register, indexed from zero
 * @param out_circuit Non-NULL output pointer receiving an owned circuit handle.
 *     Set to NULL on failure. Free the handle with tket_free_circuit.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_create(
    uint32_t n_qubits, uint32_t n_bits, TketCircuit **out_circuit);

/**
 * Set the name of a circuit
 *
 * @param circuit Non-NULL circuit to modify
 * @param name Non-NULL, NUL-terminated name borrowed for this call
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_set_name(TketCircuit *circuit, const char *name);

/**
 * Append an operation to the circuit
 *
 * @param circuit Non-NULL circuit to modify
 * @param gate Operation identifier from TKET_GATE_*. Use the dedicated
 *     builders for SetBits and CircBox.
 * @param args Borrowed array of args_count distinct wires already present in
 *     circuit, in operation port order. CX, CY, CZ and CRz take [control,
 *     target], and CCX takes [control, control, target]. Measure takes [qubit,
 *     bit], Reset takes [qubit], and Barrier takes any number of qubits and
 * bits. Other gates take qubits in their usual TKET port order. May be NULL
 * only when args_count is zero. The array is borrowed for this call only.
 * @param args_count Number of wires in args, matching the operation's arity
 * @param params Borrowed array of params_count finite angles in half-turns
 *     (1.0 is pi radians). PhasedX and PhasedXX take [rotation angle,
 *     phase-axis angle]. TK1 takes [a, b, c] for Rz(a) Rx(b) Rz(c), and
 *     TK2 takes [a, b, c] for XXPhase(a) YYPhase(b) ZZPhase(c).
 *     May be NULL only when params_count is zero. The array is borrowed for
 *     this call only.
 * @param params_count Number of elements in params: one for Rx, Ry, Rz, CRz,
 *     XXPhase, YYPhase and ZZPhase, two for PhasedX and PhasedXX, three for
 *     TK1 and TK2, one for the zero-wire Phase operation, and zero for all
 *     other supported operations
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_operation(
    TketCircuit *circuit, TketGate gate, const TketWire *args,
    size_t args_count, const double *params, size_t params_count);

/**
 * Append an operation whose angles are expressions
 *
 * Use this when an angle contains a symbol or an exact expression that TKET
 * should keep while optimising the circuit.
 *
 * @param circuit Non-NULL circuit to modify
 * @param gate Operation identifier from TKET_GATE_* for a fixed-arity gate
 * @param args Borrowed array of args_count wires in operation port order.
 *     May be NULL when args_count is zero.
 * @param args_count Number of wires in args
 * @param expressions Borrowed array of expression_count NUL-terminated
 *     SymEngine expressions in half-turns (1.0 is pi radians). Use the gate's
 *     parameter order as in tket_circuit_add_operation.
 * @param expression_count Number of angles in expressions
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_operation_with_expressions(
    TketCircuit *circuit, TketGate gate, const TketWire *args,
    size_t args_count, const char *const *expressions, size_t expression_count);

/**
 * Add a barrier with a payload
 *
 * The payload travels with the barrier through optimisation. A caller can
 * use it to identify a region that it will restore after the pass.
 *
 * @param circuit Non-NULL circuit to modify
 * @param args Borrowed array of qubit and bit arguments in port order.
 *     Read-only bit arguments may repeat. May be NULL if args_count is zero.
 * @param args_count Number of wires in args
 * @param signature Optional array of args_count port kinds in argument order.
 *     Use 'Q' for a qubit, 'C' for a read/write bit and 'B' for a read-only
 *     bit. NULL gives qubit and read/write bit ports based on args.
 * @param data Borrowed payload bytes. May be NULL if data_size is zero.
 * @param data_size Number of payload bytes
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_barrier(
    TketCircuit *circuit, const TketWire *args, size_t args_count,
    const char *signature, const char *data, size_t data_size);

/**
 * Assign constants to classical bits
 *
 * @param circuit Non-NULL circuit to modify
 * @param bits Borrowed array of count distinct indices in the c register
 * @param values Borrowed array of count values, each 0 or 1
 * @param count Number of bits and values. Arrays may be NULL when zero.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_set_bits(
    TketCircuit *circuit, const uint32_t *bits, const uint8_t *values,
    size_t count);

/**
 * Append a box containing a copy of another circuit
 *
 * @param circuit Non-NULL circuit to append to
 * @param contents Non-NULL circuit to copy into the box. The caller keeps
 *     ownership and can free or edit it after the call.
 * @param args Array of args_count wires for the box's ports. List the qubits
 *     first, then the bits, in the order of the wires inside contents.
 * @param args_count Number of ports on the box
 * @param id Optional NUL-terminated UUID in canonical 36-character form.
 *     Pass NULL to generate a new ID.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_circbox(
    TketCircuit *circuit, const TketCircuit *contents, const TketWire *args,
    size_t args_count, const char *id);

/**
 * Relabel the output wires of a circuit with dense q indices
 *
 * @param circuit Non-NULL circuit to modify
 * @param outputs Borrowed array of count output indices. Entry i is the new
 *     label for the output currently labelled i. Each index appears once.
 *     May be NULL if count is zero.
 * @param count Number of qubits in the circuit
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_permute_outputs(
    TketCircuit *circuit, const uint32_t *outputs, size_t count);

/**
 * Add a global phase to the circuit
 *
 * @param circuit Non-NULL circuit to modify
 * @param phase Finite phase in half-turns, multiplying the circuit's scalar
 *     phase by exp(i*pi*phase)
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_phase(TketCircuit *circuit, double phase);

/**
 * Add to the circuit's global phase using an expression
 *
 * @param circuit Non-NULL circuit to modify
 * @param expression NUL-terminated SymEngine expression in half-turns
 *     (1.0 is pi radians), borrowed for this call
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_phase_expression(
    TketCircuit *circuit, const char *expression);

/**
 * Mark a qubit as created at the circuit's input boundary
 *
 * @param circuit Non-NULL circuit to modify
 * @param qubit Zero-based index of an existing qubit in the q register
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_mark_created(TketCircuit *circuit, uint32_t qubit);

/**
 * Mark a qubit as discarded at the circuit's output boundary
 *
 * @param circuit Non-NULL circuit to modify
 * @param qubit Zero-based index of an existing qubit in the q register
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_mark_discarded(TketCircuit *circuit, uint32_t qubit);

// ===========================================================================
// The direct interface: reading a circuit
// ===========================================================================
//
// After building or optimising a circuit, create an iterator and ask for the
// next operation until out_has_operation is false. Each operation gives us its
// gate and wires. Choose the numeric iterator call for angles as doubles or
// the expression call to read symbolic angles as well.
//
// The arrays in an operation view belong to the iterator. Finish reading them
// before asking for the next operation, or copy the values if we need them
// later. The iterator keeps the circuit's storage alive if its handle is freed.
// Changing the circuit invalidates existing iterators.
//
// Operations arrive in an order that respects their dependencies. Gates on
// unrelated wires may appear in a different order from the JSON output.
// This iterator expects qubits in q and bits in c, each with a single index.
// Use a named iterator for other register layouts or operation groups. A
// numeric step reports an error if an angle cannot be evaluated as a double.
//
// Global phase, the output permutation and qubit boundary annotations are
// available separately. Use the creation and discard queries to tell which
// qubits start in |0> and which outputs are discarded.

// Opaque handle to a circuit command iterator
typedef struct TketCommandIterator TketCommandIterator;

/**
 * Borrowed view of a circuit operation
 */
typedef struct {
  /** Operation identifier from TKET_GATE_*. */
  TketGate gate;
  /**
   * Borrowed array of args_count wires in operation port order. Controls
   * precede targets, and measurement has a qubit followed by a bit. Valid
   * until the next tket_iterator_next call or until the iterator is freed.
   * May be NULL when args_count is zero.
   */
  const TketWire *args;
  /** Number of wires in args. */
  size_t args_count;
  /**
   * Borrowed array of params_count finite angles in half-turns (1.0 is pi
   * radians). PhasedX and PhasedXX return [rotation angle, phase-axis angle],
   * and TK1 and TK2 return [a, b, c]. Valid until the next tket_iterator_next
   * call or until the iterator is freed. May be NULL when params_count is zero.
   */
  const double *params;
  /** Number of angle parameters in params. */
  size_t params_count;
} TketOperationView;

/**
 * Create an iterator over the circuit's operations in causal order
 *
 * The iterator uses a frontier of ready vertices. Independent gates may
 * appear in a different order from JSON export. The iterator keeps the
 * circuit's storage alive if its handle is freed, but changing the circuit
 * invalidates the iterator.
 *
 * This iterator reads typed operations on single-index q and c registers.
 * Use tket_iterator_next_with_expressions for symbolic parameters. Use
 * tket_circuit_named_iterator_create for named wires and opaque operations.
 *
 * @param circuit Non-NULL circuit to iterate over. Its storage is shared with
 *     the iterator, so its handle may be freed before the iterator. Mutating
 *     the circuit invalidates the iterator.
 * @param out_iterator Non-NULL output pointer receiving an owned iterator
 *     handle. Set to NULL on failure. Free the handle with tket_free_iterator.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_iterator_create(
    const TketCircuit *circuit, TketCommandIterator **out_iterator);

/**
 * Read the next operation from the circuit iterator
 *
 * @param iterator Non-NULL iterator to advance. Free it after an error.
 *     Repeated calls after exhaustion are allowed.
 * @param out_operation Non-NULL output pointer receiving a borrowed operation
 *     view. Its arrays remain valid until the next call on iterator or until
 *     iterator is freed. Its params contain finite angles in half-turns
 *     (1.0 is pi radians). Cleared on exhaustion or error.
 * @param out_has_operation Non-NULL output pointer receiving true if an
 *     operation was returned, or false on exhaustion or error
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_iterator_next(
    TketCommandIterator *iterator, TketOperationView *out_operation,
    bool *out_has_operation);

/**
 * Borrowed operation view with angles expressed as text
 */
typedef struct {
  /** Operation identifier from TKET_GATE_*. */
  TketGate gate;
  /** Array of args_count wires in operation port order. */
  const TketWire *args;
  /** Number of wires in args. */
  size_t args_count;
  /**
   * Array of expression_count NUL-terminated SymEngine expressions in
   * half-turns (1.0 is pi radians), in the gate's parameter order. Numeric
   * angles are reduced with the same period as in a TKET JSON export.
   */
  const char *const *expressions;
  /** Number of angles in expressions. */
  size_t expression_count;
} TketExpressionOperationView;

/**
 * Advance the iterator and read angles as expressions
 *
 * This advances the same iterator as tket_iterator_next. Use it when the
 * circuit may contain symbolic angles.
 *
 * @param iterator Non-NULL iterator to advance. Free it after an error.
 * @param out_operation Non-NULL output pointer receiving a borrowed view.
 *     Its arrays and their contents remain valid until the next step or until
 *     the iterator is freed. Cleared on exhaustion or error.
 * @param out_has_operation Non-NULL output pointer receiving true if an
 *     operation was returned, or false on exhaustion or error
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_iterator_next_with_expressions(
    TketCommandIterator *iterator, TketExpressionOperationView *out_operation,
    bool *out_has_operation);

/**
 * Read the payload of the last operation returned by an iterator
 *
 * @param iterator Non-NULL iterator positioned on a Barrier or SetBits
 * @param out_data Non-NULL output pointer receiving borrowed bytes. For a
 *     barrier these are its payload bytes. For SetBits each byte is 0 or 1.
 *     The bytes remain valid until the next iterator call or destruction.
 * @param out_size Non-NULL output pointer receiving the number of bytes
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_iterator_get_payload(
    const TketCommandIterator *iterator, const uint8_t **out_data,
    size_t *out_size);

/**
 * Read the port kinds of the current barrier
 *
 * @param iterator Non-NULL iterator positioned on a Barrier
 * @param out_signature Non-NULL output pointer receiving a borrowed array with
 *     one character per argument. The characters are 'Q' for a qubit, 'C'
 *     for a read/write bit, 'B' for a read-only bit, 'W' for WASM state and
 *     'R' for RNG state. The array is valid until the next iterator step or
 *     until the iterator is freed.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_iterator_get_barrier_signature(
    const TketCommandIterator *iterator, const char **out_signature);

/**
 * Copy the contents and read the identity of the current circuit box
 *
 * @param iterator Non-NULL iterator positioned on a CircBox
 * @param out_contents Non-NULL output pointer receiving an owned circuit
 *     handle. Free it with tket_free_circuit. Set to NULL on failure.
 * @param out_id Non-NULL buffer of at least 37 bytes receiving the box UUID
 *     as a NUL-terminated string
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_iterator_get_circbox(
    const TketCommandIterator *iterator, TketCircuit **out_contents,
    char out_id[37]);

/**
 * Read the name of a circuit
 *
 * @param circuit Non-NULL circuit to read
 * @param out_name Non-NULL output pointer receiving an owned, NUL-terminated
 *     name. Set to NULL for an unnamed circuit or on failure. Free a returned
 *     name with tket_free_string.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_get_name(const TketCircuit *circuit, char **out_name);

/**
 * Read the sizes of the circuit's default registers
 *
 * Circuits with other registers or gaps in the default registers return
 * TKET_ERROR_UNSUPPORTED_OPERATION.
 *
 * @param circuit Non-NULL circuit to read
 * @param out_qubits Non-NULL output pointer receiving the size of q
 * @param out_bits Non-NULL output pointer receiving the size of c
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_register_sizes(
    const TketCircuit *circuit, uint32_t *out_qubits, uint32_t *out_bits);

/**
 * Check for operation groups which need the JSON representation
 *
 * @param circuit Non-NULL circuit to read
 * @param out_has_groups Non-NULL output pointer receiving the result
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_has_opgroups(
    const TketCircuit *circuit, bool *out_has_groups);

/**
 * Read the circuit's global phase as an expression
 *
 * @param circuit Non-NULL circuit to read
 * @param out_expression Non-NULL output pointer receiving an owned,
 *     NUL-terminated SymEngine expression in half-turns (1.0 is pi radians).
 *     Set to NULL on failure. Free the string with tket_free_string.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_get_phase_expression(
    const TketCircuit *circuit, char **out_expression);

/**
 * Read the circuit's global phase
 *
 * @param circuit Non-NULL circuit to read
 * @param out_phase Non-NULL output pointer receiving a phase in half-turns,
 *     representing the scalar exp(i*pi*(*out_phase))
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_get_phase(const TketCircuit *circuit, double *out_phase);

/**
 * Check whether an input qubit is prepared in |0>
 *
 * @param circuit Non-NULL circuit to read
 * @param qubit Zero-based index of an existing input qubit in the q register
 * @param out_created Non-NULL output pointer receiving the creation flag
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_is_created(
    const TketCircuit *circuit, uint32_t qubit, bool *out_created);

/**
 * Check whether an output qubit is discarded
 *
 * @param circuit Non-NULL circuit to read
 * @param qubit Zero-based index of an existing output qubit in the q register
 * @param out_discarded Non-NULL output pointer receiving the discard flag
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_is_discarded(
    const TketCircuit *circuit, uint32_t qubit, bool *out_discarded);

/**
 * Find the output wire reached by an input qubit
 *
 * @param circuit Non-NULL circuit to read
 * @param input Zero-based index of an existing input qubit in the q register
 * @param out_output Non-NULL output pointer receiving the zero-based q-register
 *     index of the corresponding output wire
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_output_qubit(
    const TketCircuit *circuit, uint32_t input, uint32_t *out_output);

/**
 * Check whether the circuit has an implicit output permutation
 *
 * @param circuit Non-NULL circuit to read
 * @param out_has_swaps Non-NULL output pointer receiving true if any input
 * qubit reaches an output wire with a different index, or false otherwise
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_has_implicit_swaps(
    const TketCircuit *circuit, bool *out_has_swaps);

// ===========================================================================
// Working with named wires and operation groups
// ===========================================================================
//
// Use this interface when a circuit already has register names or when a pass
// may rename its wires. Declare the wires once, then use their compact IDs
// while appending commands. When reading a pass result, the iterator assigns
// IDs to the wires in that result and lets us look up their names and indices.
//
// Use the typed builders for the operations listed above. For other TKET
// operations, the CBOR entry points exchange one operation using TKET's
// operation schema. An opaque operation can be read and replayed through those
// entry points. Its fields have no typed C view.

typedef uint32_t TketWireId;
enum { TKET_WIRE_WASM = 2, TKET_WIRE_RNG = 3 };

/** Borrowed bytes. A NULL data pointer denotes an absent optional value. */
typedef struct {
  const uint8_t *data;
  size_t size;
} TketBytes;

/** One quantum, classical or auxiliary wire in a named circuit. */
typedef struct {
  TketWireKind kind;
  /** UTF-8 register name, including its length. */
  TketBytes name;
  /** Array of index_count coordinates in the register. */
  const uint32_t *indices;
  size_t index_count;
} TketNamedWire;

/** Wires and optional group attached to one operation. */
typedef struct {
  /** Borrowed array of count wire IDs in operation port order. */
  const TketWireId *wires;
  size_t count;
  /** Optional UTF-8 group name, or NULL data for an ungrouped command. */
  TketBytes group;
} TketCommandPorts;

/**
 * Build an empty circuit with named quantum and classical wires.
 *
 * @param wires Borrowed array of wire_count quantum and classical declarations
 * @param wire_count Number of declarations. A declaration's position is its ID.
 * @param n_wasm Number of default WASM wires, with IDs after the declarations
 * @param n_rng Number of default RNG wires, with IDs after the WASM wires
 * @param name Optional UTF-8 circuit name. Pass NULL data for an unnamed
 * circuit.
 * @param out_circuit Non-NULL output receiving an owned circuit handle. Free it
 *     with tket_free_circuit. Set to NULL on failure.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_create_named(
    const TketNamedWire *wires, size_t wire_count, uint32_t n_wasm,
    uint32_t n_rng, TketBytes name, TketCircuit **out_circuit);

/**
 * Set a boundary annotation on a declared quantum wire.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param wire ID of a declared quantum wire
 * @param output False to mark the input as created, true to mark the output as
 * discarded
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_mark_named_boundary(
    TketCircuit *circuit, TketWireId wire, bool output);

/**
 * Relabel the quantum output boundary of a named circuit.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param from_ids Borrowed array of count quantum wire IDs in current output
 * order
 * @param to_ids Borrowed array of count new labels, a permutation of from_ids
 * @param count Number of entries in each array
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_permute_named_outputs(
    TketCircuit *circuit, const TketWireId *from_ids, const TketWireId *to_ids,
    size_t count);

/**
 * Append a gate to a named circuit.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param ports Non-NULL command wires and optional group, borrowed for this
 * call
 * @param gate Fixed-arity gate identifier from TKET_GATE_*
 * @param expressions Borrowed array of count NUL-terminated SymEngine angles,
 *     in half-turns (1.0 is pi radians) and in the gate's parameter order
 * @param count Number of angles in expressions
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_append_named_gate(
    TketCircuit *circuit, const TketCommandPorts *ports, TketGate gate,
    const char *const *expressions, size_t count);

/**
 * Append a barrier to a named circuit.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param ports Non-NULL command wires and optional group, borrowed for this
 * call
 * @param signature Optional array with one port kind per wire. Use 'Q' for
 *     a qubit, 'C' for a read/write bit, 'B' for a read-only bit, 'W' for WASM
 *     state or 'R' for RNG state. NULL uses each wire's linear port kind.
 * @param data Barrier payload bytes, borrowed for this call
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_append_named_barrier(
    TketCircuit *circuit, const TketCommandPorts *ports, const char *signature,
    TketBytes data);

/**
 * Append a SetBits to a named circuit.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param ports Non-NULL bit wires and optional group, borrowed for this call
 * @param values Borrowed array with one value of 0 or 1 per wire
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_append_named_set_bits(
    TketCircuit *circuit, const TketCommandPorts *ports, const uint8_t *values);

/**
 * Append a copy of another circuit as a CircBox.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param ports Non-NULL box wires and optional group, borrowed for this call.
 *     List qubits, bits, WASM wires and RNG wires in the box's port order.
 * @param contents Non-NULL circuit to copy. The caller retains ownership.
 * @param id Optional NUL-terminated UUID in canonical 36-character form.
 *     Pass NULL to generate an ID.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_append_named_circbox(
    TketCircuit *circuit, const TketCommandPorts *ports,
    const TketCircuit *contents, const char *id);

/**
 * Append an operation described by a CBOR payload.
 *
 * @param circuit Non-NULL circuit built with tket_circuit_create_named
 * @param ports Non-NULL command wires and optional group, borrowed for this
 * call
 * @param operation CBOR bytes in TKET's operation schema, borrowed for this
 *     call. The payload describes the operation and its attributes.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_append_opaque_operation(
    TketCircuit *circuit, const TketCommandPorts *ports, TketBytes operation);

enum { TKET_GATE_OPAQUE = 0 };

/** Borrowed view of a command in a named circuit. */
typedef struct {
  /** Wires are identified by the IDs assigned by this iterator. A gate of
   * TKET_GATE_OPAQUE has an operation payload instead of angle expressions. */
  TketExpressionOperationView operation;
  /** UTF-8 group, with NULL data if this operation has no group. */
  TketBytes group;
} TketNamedOperationView;

/**
 * Create an iterator over a circuit with named wires.
 *
 * @param circuit Non-NULL circuit. The iterator shares its storage.
 * @param out_iterator Non-NULL output receiving an owned iterator. Free it
 *     with tket_free_iterator. Set to NULL on failure.
 * @param out_wire_count Non-NULL output receiving the number of wire IDs
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_named_iterator_create(
    const TketCircuit *circuit, TketCommandIterator **out_iterator,
    size_t *out_wire_count);

/**
 * Read an iterator's circuit name.
 *
 * @param iterator Non-NULL iterator over a named circuit
 * @param out_name Non-NULL output receiving UTF-8 bytes, or NULL data for an
 *     unnamed circuit. Copy the bytes before freeing the iterator.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_named_iterator_get_name(
    const TketCommandIterator *iterator, TketBytes *out_name);

/**
 * Look up a wire and its boundaries.
 *
 * @param iterator Non-NULL iterator over a named circuit
 * @param id Wire ID from zero up to the wire count returned at creation
 * @param out_wire Non-NULL output receiving the wire's name and coordinates.
 *     Copy these before the next wire lookup or before freeing the iterator.
 * @param out_created Non-NULL output receiving whether the wire is created
 * @param out_discarded Non-NULL output receiving whether the wire is discarded
 * @param out_output Non-NULL output receiving the output ID reached by this
 *     wire. For classical or auxiliary wires this is the given wire ID.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_named_iterator_get_wire(
    TketCommandIterator *iterator, TketWireId id, TketNamedWire *out_wire,
    bool *out_created, bool *out_discarded, TketWireId *out_output);

/**
 * Advance a named iterator by one operation.
 *
 * @param iterator Non-NULL iterator over a named circuit. Free it after an
 * error.
 * @param out_operation Non-NULL output receiving a borrowed command view.
 *     The view is valid until the next step or until the iterator is freed.
 *     Cleared on exhaustion or error.
 * @param out_has_operation Non-NULL output receiving true if a command was read
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_named_iterator_next(
    TketCommandIterator *iterator, TketNamedOperationView *out_operation,
    bool *out_has_operation);

/**
 * Read the opaque operation payload at the current iterator position.
 *
 * @param iterator Non-NULL named iterator positioned on TKET_GATE_OPAQUE
 * @param out_payload Non-NULL output receiving borrowed CBOR bytes in TKET's
 *     operation schema. Copy these before the next step or freeing the
 * iterator.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_named_iterator_get_opaque_operation(
    TketCommandIterator *iterator, TketBytes *out_payload);

// ===========================================================================
// Creating and applying optimisation passes
// ===========================================================================
//
// A pass describes an optimisation. We can load one from JSON above or use the
// direct constructors below. To run several passes in order, put them into a
// sequence and apply that sequence to the circuit.
//
// Pass handles can be reused for later circuits. A sequence keeps shared
// ownership of its component passes, so their original handles can be freed
// once the sequence has been created. Applying a pass changes the circuit and
// invalidates any iterators that were reading it.

/**
 * Create an AutoRebase pass
 *
 * @param basis Borrowed array of basis_count gate identifiers from TKET_GATE_*.
 *     Borrowed for this call only. May be NULL only when basis_count is zero.
 * @param basis_count Number of gate identifiers in basis
 * @param allow_swaps Whether rebasing may introduce an implicit wire
 * permutation
 * @param out_pass Non-NULL output pointer receiving an owned pass handle.
 *     Set to NULL on failure. Free the handle with tket_free_pass.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_pass_create_auto_rebase(
    const TketGate *basis, size_t basis_count, bool allow_swaps,
    TketPass **out_pass);

/**
 * Create a RemoveRedundancies pass
 *
 * @param out_pass Non-NULL output pointer receiving an owned pass handle.
 *     Set to NULL on failure. Free the handle with tket_free_pass.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_pass_create_remove_redundancies(TketPass **out_pass);

/**
 * Create a CommuteThroughMultis pass
 *
 * @param out_pass Non-NULL output pointer receiving an owned pass handle.
 *     Set to NULL on failure. Free the handle with tket_free_pass.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_pass_create_commute_through_multis(TketPass **out_pass);

/**
 * Create a sequence that applies passes in the supplied order
 *
 * The sequence shares ownership of its component passes, so their handles
 * can be freed after this call. The output handle is set to NULL on failure.
 *
 * @param passes Borrowed array of passes_count non-NULL pass handles in
 *     execution order. Borrowed for this call only. May be NULL only when
 *     passes_count is zero. The sequence shares ownership of each pass, so
 *     their handles can be freed after this call.
 * @param passes_count Number of pass handles in passes
 * @param strict Whether to check that consecutive pass predicates compose
 * @param out_pass Non-NULL output pointer receiving an owned sequence handle.
 *     Set to NULL on failure. Free the handle with tket_free_pass.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_pass_create_sequence(
    const TketPass *const *passes, size_t passes_count, bool strict,
    TketPass **out_pass);

/**
 * Apply an optimisation pass to a circuit
 *
 * @param circuit Non-NULL circuit to modify. Existing iterators are
 * invalidated.
 * @param pass Non-NULL pass to apply. Borrowed for this call and available for
 * reuse.
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_apply_pass(TketCircuit *circuit, const TketPass *pass);

// ===========================================================================
// Applying transforms directly
// ===========================================================================
//
// These functions apply a particular transform immediately. They are convenient
// when we want one of these optimisations without creating a pass handle first.
// They change the circuit and invalidate any existing iterators.

// Target gate types for two_qubit_squash
typedef enum { TKET_TARGET_CX = 0, TKET_TARGET_TK2 = 1 } TketTargetGate;

/**
 * Apply two_qubit_squash transform to the circuit
 *
 * Squash sequences of two-qubit operations into minimal form using KAK
 * decomposition. Can decompose to TK2 or CX gates.
 *
 * @param circuit Circuit to transform (modified in-place)
 * @param target_gate Target two-qubit gate type (CX or TK2)
 * @param cx_fidelity Estimated CX gate fidelity (used when target_gate=CX)
 * @param allow_swaps Whether to allow implicit wire swaps
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_two_qubit_squash(
    TketCircuit *circuit, TketTargetGate target_gate, double cx_fidelity,
    bool allow_swaps);

/**
 * Apply clifford_simp transform to the circuit
 *
 * Resynthesise all Clifford subcircuits and simplify using Clifford rules.
 * This can significantly reduce the two-qubit gate count for Clifford-heavy
 * circuits.
 *
 * @param circuit Circuit to transform (modified in-place)
 * @param target_gate Target two-qubit gate type (CX or TK2)
 * @param allow_swaps Whether the rewriting may introduce wire swaps
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_clifford_simp(
    TketCircuit *circuit, TketTargetGate target_gate, bool allow_swaps);

/**
 * Squash sequences of single-qubit gates into PhasedX and Rz gates.
 *
 * Also remove identity gates. Commute Rz gates to the back if possible.
 *
 * @param circuit Circuit to transform (modified in-place)
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_squash_phasedx_rz(TketCircuit *circuit);

// ===========================================================================
// Releasing handles and strings
// ===========================================================================
//
// Free each circuit, pass and iterator handle when we're finished with it.
// Strings returned by tket_circuit_to_json are freed with tket_free_string.
// All of these functions accept NULL.
//
// Arrays returned in operation views belong to the iterator and must be left
// for it to free. Error message strings also belong to the API and must not be
// freed by the caller.

/**
 * Free a circuit handle
 *
 * @param circuit Circuit handle to free, or NULL.
 *     An iterator sharing its storage can still be used.
 */
void tket_free_circuit(TketCircuit *circuit);

/**
 * Free a pass handle
 *
 * @param pass Pass handle to free, or NULL.
 *     Sequences sharing the pass can still be used.
 */
void tket_free_pass(TketPass *pass);

/**
 * Free iterator memory
 *
 * @param iterator Iterator handle to free, or NULL.
 *     Its borrowed operation arrays become invalid.
 */
void tket_free_iterator(TketCommandIterator *iterator);

/**
 * Free a JSON output string
 *
 * @param str JSON string to free, or NULL.
 *     The string must have been returned by tket_circuit_to_json.
 */
void tket_free_string(char *str);

#ifdef __cplusplus
}
#endif

#endif  // TKET_C_API_H
