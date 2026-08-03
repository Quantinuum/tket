// Copyright Quantinuum
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "tket/Circuit/Circuit.hpp"
#include "tket/Clifford/ChoiMixTableau.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket/PauliGraph/PauliGraph.hpp"
#include "tket/ZX/ZXDiagram.hpp"

#include "tket_export.h"

namespace tket {

/**
 * Construct the tableau for a given circuit.
 * Will throw an exception if it contains non-Clifford gates.
 */
TKET_EXPORT UnitaryTableau circuit_to_unitary_tableau(const Circuit &circ);
TKET_EXPORT UnitaryRevTableau circuit_to_unitary_rev_tableau(const Circuit &circ);

/**
 * Constructs a circuit producing the same effect as the tableau.
 * Uses the method from Aaronson-Gottesman: Improved Simulation of
 * Stabilizer Circuits, Theorem 8.
 * CAUTION: GATE COUNT IS ATROCIOUS IN PRACTICE
 */
TKET_EXPORT Circuit unitary_tableau_to_circuit(const UnitaryTableau &tab);
TKET_EXPORT Circuit unitary_rev_tableau_to_circuit(const UnitaryRevTableau &tab);

/**
 * Construct a ChoiMixTableau for a given circuit.
 * Will incorporate qubit initialisations and discarding into the circuit.
 * Will throw an exception if it contains non-Clifford gates.
 */
ChoiMixTableau circuit_to_cm_tableau(const Circuit &circ);

/**
 * Constructs a circuit producing the same effect as a ChoiMixTableau.
 * Since Circuit does not support distinct qubit addresses for inputs and
 * outputs, also returns a map from the output qubit IDs in the tableau to their
 * corresponding outputs in the circuit.
 *
 * The circuit produced will be the (possibly non-unitary) channel whose
 * stabilisers are exactly those of the tableau and no more, using
 * initialisations, post-selections, discards, resets, and collapses to ensure
 * this. It will automatically reuse qubits so no more qubits will be needed
 * than max(tab.get_n_inputs(), tab.get_n_outputs()).
 *
 * Example 1:
 * ZXI -> ()
 * YYZ -> ()
 * This becomes a diagonalisation circuit followed by post-selections.
 *
 * Example 2:
 * Z -> ZZ
 * X -> IY
 * Z -> -XX
 * Combining the first and last rows reveals an initialisation is required for I
 * -> YY. Since there are two output qubits, at least one of them does not
 * already exist in the input fragment so we can freely add an extra qubit on
 * the input side, initialise it and apply a unitary mapping IZ -> YY.
 *
 * Example 3:
 * ZX -> IZ
 * II -> ZI
 * We require an initialised qubit for the final row, but both input and output
 * spaces only have q[0] and q[1], of which both inputs need to be open for the
 * first row. We can obtain an initialised qubit by resetting a qubit after
 * reducing the first row to only a single qubit.
 */
std::pair<Circuit, qubit_map_t> cm_tableau_to_exact_circuit(
    const ChoiMixTableau &tab, CXConfigType cx_config = CXConfigType::Snake);

/**
 * We define a unitary extension of a ChoiMixTableau to be a unitary circuit
 * whose stabilizer group contain all the rows of the ChoiMixTableau and
 * possibly more. This is useful when we are treating the ChoiMixTableau as a
 * means to encode a diagonalisation problem, since we are generally looking for
 * a unitary as we may wish to apply the inverse afterwards (e.g. conjugating
 * some rotations to implement a set of Pauli gadgets).
 *
 * Not every ChoiMixTableau can be extended to a unitary by just adding rows,
 * e.g. if it requires any initialisation or post-selections. In this case, the
 * unitary circuit is extended with additional input qubits which are assumed to
 * be zero-initialised, and additional output qubits which are assumed to be
 * post-selected. The synthesis guarantees that, if we take the unitary,
 * initialise all designated inputs, and post-select on all designated outputs,
 * every row from the original tableau is a stabiliser for the remaining
 * projector. When not enough additional qubit names are provided, an error is
 * thrown.
 *
 *
 * Example 1:
 * ZXI -> ()
 * YYZ -> ()
 * Since, in exact synthesis, at least two post-selections would be required, we
 * pick two names from post_names. This is then a diagonalisation circuit which
 * maps each row to an arbitrary diagonal string over post_names.
 *
 * Example 2:
 * Z -> ZZ
 * X -> IY
 * Z -> -XX
 * Combining the first and last rows reveals an initialisation is required for I
 * -> YY. We extend the inputs with a qubit from init_names. The initialisation
 * can manifest as either altering the first row to ZZ -> ZZ or the last row to
 * ZZ -> -XX.
 *
 * Example 3:
 * ZX -> IZ
 * II -> ZI
 * We require an initialised qubit for the final row, but both input and output
 * spaces only have q[0] and q[1], of which both inputs need to be open for the
 * first row. Unlike exact synthesis, we cannot reuse qubits, so the returned
 * circuit will be over 3 qubits, extending with a name from init_names.
 */
std::pair<Circuit, qubit_map_t> cm_tableau_to_unitary_extension_circuit(
    const ChoiMixTableau &tab, const std::vector<Qubit> &init_names = {},
    const std::vector<Qubit> &post_names = {},
    CXConfigType cx_config = CXConfigType::Snake);

PauliGraph circuit_to_pauli_graph(const Circuit &circ);

/**
 * Synthesises a circuit equivalent to the PauliGraph by adding each
 * pauli gadget to the circuit as a PauliExpBox individually
 * in the order given by TopSortIterator.
 * The tableau is then synthesised at the end.
 */
Circuit pauli_graph_to_pauli_exp_box_circuit_individually(
    const PauliGraph &pg, CXConfigType cx_config = CXConfigType::Snake);

/**
 * Synthesises a circuit equivalent to the PauliGraph by inserting pairs of
 * pauli gadgets as PauliExpPairBoxes into the circuit
 * The tableau is then synthesised at the end.
 */
Circuit pauli_graph_to_pauli_exp_box_circuit_pairwise(
    const PauliGraph &pg, CXConfigType cx_config = CXConfigType::Snake);

/**
 * Synthesises a circuit equivalent to the PauliGraph by building
 * sets of mutually commuting pauli gadgets and
 * inserting them into the circuit as PauliExpCommutingSetBoxes
 * The tableau is then synthesised at the end.
 */

Circuit pauli_graph_to_pauli_exp_box_circuit_sets(
    const PauliGraph &pg, CXConfigType cx_config = CXConfigType::Snake);

/**
 * Construct a zx diagram from a given circuit.
 * Return the zx diagram and a map between the zx boundary vertices and the
 * circuit boundary vertices.
 */
TKET_EXPORT std::pair<zx::ZXDiagram, boost::bimap<zx::ZXVert, Vertex>> circuit_to_zx(
    const Circuit &circuit);

/**
 * Takes a unitary ZX diagram in MBQC form with the promise that a gflow exists.
 * Produces an equivalent circuit using the gate extraction method from
 * Backens et al., "There and Back Again: A Circuit Extraction Tale".
 */
TKET_EXPORT Circuit zx_to_circuit(const zx::ZXDiagram &diag);

}  // namespace tket
