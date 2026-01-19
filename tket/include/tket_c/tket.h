#ifndef TKET_C_API_H
#define TKET_C_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Opaque handle to tket Circuit object
typedef struct TketCircuit TketCircuit;

// Opaque handle to tket BasePass object
typedef struct TketPass TketPass;

// Error handling
typedef enum {
  TKET_SUCCESS = 0,
  TKET_ERROR_NULL_POINTER = 1,
  TKET_ERROR_CIRCUIT_INVALID = 2,
} TketError;

// Target gate types for two_qubit_squash
typedef enum { TKET_TARGET_CX = 0, TKET_TARGET_TK2 = 1 } TketTargetGate;

// Conversion between Circuit and c-string JSON
TketCircuit *tket_circuit_from_json(const char *json_str);
TketError tket_circuit_to_json(const TketCircuit *circuit, char **json_str);

// Loading a Pass from its c-string JSON
TketPass *tket_pass_from_json(const char *json_str);

// Applying a pass to a circuit
TketError tket_apply_pass(TketCircuit *circuit, const TketPass *pass);

// Free memory
void tket_free_circuit(TketCircuit *circuit);
void tket_free_pass(TketPass *pass);
void tket_free_string(char *str);

// Transform functions

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

// Utility functions
const char *tket_error_string(TketError error);

#ifdef __cplusplus
}
#endif

#endif  // TKET_C_API_H
