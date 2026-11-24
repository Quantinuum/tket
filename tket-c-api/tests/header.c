// Check that the public declarations compile as C, without C++ headers.
#include "tket-c-api.h"
int main(void) {
  TketCircuit *circuit = NULL;
  TketError error = tket_circuit_create(0, 0, &circuit);
  tket_free_circuit(circuit);
  return error != TKET_SUCCESS;
}
