#include "tket-c-api.h"

int main(void) {
  TketCircuit *circuit = NULL;
  TketCommandIterator *iterator = NULL;
  const TketWire qubit = {TKET_WIRE_QUBIT, 0};
  TketOperationView operation;
  bool has_operation;
  int result = 1;

  // Build X on q[0]. The wire array is borrowed only during the call, and
  // X takes no angles, so its parameter array can be NULL.
  if (tket_circuit_create(1, 0, &circuit) != TKET_SUCCESS) goto cleanup;
  if (tket_circuit_add_operation(circuit, TKET_GATE_X, &qubit, 1, NULL, 0) !=
      TKET_SUCCESS)
    goto cleanup;

  // A successful iterator step can either return an operation or report
  // exhaustion. Check both the status and the flag before reading the view.
  if (tket_circuit_iterator_create(circuit, &iterator) != TKET_SUCCESS)
    goto cleanup;
  if (tket_iterator_next(iterator, &operation, &has_operation) !=
          TKET_SUCCESS ||
      !has_operation || operation.gate != TKET_GATE_X)
    goto cleanup;
  if (operation.args_count != 1 || operation.args[0].kind != qubit.kind ||
      operation.args[0].index != qubit.index || operation.params_count != 0)
    goto cleanup;
  if (tket_iterator_next(iterator, &operation, &has_operation) !=
          TKET_SUCCESS ||
      has_operation)
    goto cleanup;

  result = 0;
cleanup:
  // Both free functions accept NULL, so partial setup has the same cleanup.
  tket_free_iterator(iterator);
  tket_free_circuit(circuit);
  return result;
}
