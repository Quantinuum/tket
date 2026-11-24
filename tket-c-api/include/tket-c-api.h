#ifndef TKET_C_API_H
#define TKET_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// There are two ways to work with TKET through this C API: A JSON interface
// and a direct interface.
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
// The sections below follow those two routes, then explain how to apply
// optimisations and release the objects we've created.

// ===========================================================================
// Handles and errors
// ===========================================================================
//
// Circuits and passes live inside TKET. We use opaque handles to refer to them
// from C or another language, and free each handle when we're finished with it.
// The functions for freeing them are collected at the end of this file.
//
// A handle is not thread-safe, so is not designed for concurrent use from
// different threads. This also applies when an iterator shares storage with
// a circuit.
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

// Start here when we already have the operations and their numeric parameters.
// Create the circuit with the qubits and bits we'll need, then append
// operations in the order we want them to run. The input arrays are borrowed
// for each call, so we can reuse them as soon as that call returns.
//
// The gate identifiers below cover tket2's unitary gates, the extra gates used
// by Sol and Helios, and the TKET operations supported by this interface.
// Arguments identify wires in the circuit's default q and c registers.
//
// We can also add a global phase or mark qubits as created and discarded at the
// circuit boundaries. Those annotations describe the circuit. Runtime
// allocation and lazy measurement results remain the responsibility of the
// calling program.

// Native gate identifiers, independent of the C++ OpType values
typedef uint32_t TketGate;
enum {
  TKET_GATE_H = 1,
  TKET_GATE_CX = 2,
  TKET_GATE_CY = 3,
  TKET_GATE_CZ = 4,
  TKET_GATE_CRZ = 5,
  TKET_GATE_T = 6,
  TKET_GATE_TDG = 7,
  TKET_GATE_S = 8,
  TKET_GATE_SDG = 9,
  TKET_GATE_X = 10,
  TKET_GATE_Y = 11,
  TKET_GATE_Z = 12,
  TKET_GATE_RX = 13,
  TKET_GATE_RY = 14,
  TKET_GATE_RZ = 15,
  TKET_GATE_CCX = 16,
  TKET_GATE_V = 17,
  TKET_GATE_VDG = 18,
  TKET_GATE_PHASEDX = 19,
  TKET_GATE_ZZPHASE = 20,
  TKET_GATE_PHASEDXX = 21,
  TKET_GATE_TK1 = 22,
  TKET_GATE_SWAP = 23,
  TKET_GATE_XXPHASE = 24,
  TKET_GATE_YYPHASE = 25,
  TKET_GATE_ZZMAX = 26,
  TKET_GATE_MEASURE = 27,
  TKET_GATE_RESET = 28,
  TKET_GATE_BARRIER = 29,
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
 * Append an operation to the circuit
 *
 * @param circuit Non-NULL circuit to modify
 * @param gate Operation identifier from TKET_GATE_*
 * @param args Borrowed array of args_count distinct wires already present in
 *     circuit, in operation port order. CX, CY, CZ and CRz take [control,
 *     target], and CCX takes [control, control, target]. Measure takes [qubit,
 *     bit], Reset takes [qubit], and Barrier takes any number of qubits. Other
 *     gates take qubits in their usual TKET port order. May be NULL only when
 *     args_count is zero. The array is borrowed for this call only.
 * @param args_count Number of wires in args, matching the operation's arity
 * @param params Borrowed array of params_count finite angles in half-turns
 *     (1.0 is pi radians). PhasedX and PhasedXX take [rotation angle,
 *     phase-axis angle], and TK1 takes [a, b, c] for Rz(a) Rx(b) Rz(c).
 *     May be NULL only when params_count is zero. The array is borrowed for
 *     this call only.
 * @param params_count Number of elements in params: one for Rx, Ry, Rz, CRz,
 *     XXPhase, YYPhase and ZZPhase, two for PhasedX and PhasedXX, three for
 *     TK1, and zero for all other supported operations
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_add_operation(
    TketCircuit *circuit, TketGate gate, const TketWire *args,
    size_t args_count, const double *params, size_t params_count);

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
// gate, wires and numeric parameters, so we can process it straight away.
//
// The arrays in an operation view belong to the iterator. Finish reading them
// before asking for the next operation, or copy the values if we need them
// later. The iterator keeps the circuit's storage alive if its handle is freed.
// Changing the circuit invalidates existing iterators.
//
// Operations arrive in an order that respects their dependencies. Gates on
// unrelated wires may appear in a different order from the JSON output.
// Reading a circuit this way supports the gate identifiers above and numeric
// parameters. Unsupported operations, register names and symbolic parameters
// return an error.
//
// The global phase and any implicit output permutation are available
// separately. Check these too when we need to reproduce the complete circuit
// behaviour.

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
   * and TK1 returns [a, b, c]. Valid until the next tket_iterator_next call
   * or until the iterator is freed. May be NULL when params_count is zero.
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
 * Default, single-index q and c registers are supported. Unsupported
 * operations and symbolic parameters return errors during iteration.
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
 * Read the circuit's global phase
 *
 * @param circuit Non-NULL circuit to read
 * @param out_phase Non-NULL output pointer receiving a phase in half-turns,
 *     representing the scalar exp(i*pi*(*out_phase))
 * @return TKET_SUCCESS if successful, error code otherwise
 */
TketError tket_circuit_get_phase(const TketCircuit *circuit, double *out_phase);

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
