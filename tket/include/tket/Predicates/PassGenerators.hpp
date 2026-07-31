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

#include <optional>

#include "CompilerPass.hpp"
#include "tket/ArchAwareSynth/SteinerTree.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Mapping/RoutingMethod.hpp"
#include "tket/Placement/Placement.hpp"
#include "tket/Transformations/ContextualReduction.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"

#include "tket_export.h"

namespace tket {

/* a wrapper method for the rebase_factory in Transforms */
TKET_EXPORT PassPtr gen_rebase_pass(
    const OpTypeSet& allowed_gates, const Circuit& cx_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement);

/**
 * Generate a rebase pass give standard replacements for TK1 and TK2 gates.
 *
 * @param[in] allowed_gates set of target gates
 * @param[in] tk2_replacement circuit to replace a given TK2 gate
 * @param[in] tk1_replacement circuit to replace a given TK1 gate
 *
 * @return rebase pass
 */
TKET_EXPORT PassPtr gen_rebase_pass_via_tk2(
    const OpTypeSet& allowed_gates,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk2_replacement,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement);

/* a wrapper method for the squash_factory in Transforms */
TKET_EXPORT PassPtr gen_squash_pass(
    const OpTypeSet& singleqs,
    const std::function<Circuit(const Expr&, const Expr&, const Expr&)>&
        tk1_replacement,
    bool always_squash_symbols = false);

/**
 * @brief Attempt to generate a rebase pass automatically for the given target
 * gateset
 *
 * @param allowed_gates target gateset
 * @param allow_swaps whether to allow implicit wire swaps
 * @return PassPtr
 */
TKET_EXPORT PassPtr gen_auto_rebase_pass(
    const OpTypeSet& allowed_gates, bool allow_swaps = false);

/**
 * @brief Attempt to generate a squash pass automatically for the given target
 * single qubit gateset
 *
 * @param singleqs
 * @return PassPtr
 */
TKET_EXPORT PassPtr gen_auto_squash_pass(const OpTypeSet& singleqs);

TKET_EXPORT PassPtr gen_euler_pass(const OpType& q, const OpType& p, bool strict = false);

TKET_EXPORT PassPtr gen_clifford_simp_pass(
    bool allow_swaps = true, OpType target_2qb_gate = OpType::CX);

/**
 * Pass to resynthesise Clifford subcircuits and simplify using Clifford rules.
 *
 * @param transform optional user-provided resynthesis method to apply to all
 *   Clifford subcircuits (a function taking a Clifford circuit as an argument
 *   and returning an equivalent circuit); if not provided, a default
 *   resynthesis method is applied
 * @param allow_swaps whether the rewriting may introduce wire swaps (only
 *   relevant to the default resynthesis method used when the `transform`
 *   argument is not provided)
 * @return pass to perform Clifford resynthesis
 */
TKET_EXPORT PassPtr gen_clifford_resynthesis_pass(
    std::optional<std::function<Circuit(const Circuit&)>> transform =
        std::nullopt,
    bool allow_swaps = true);

/**
 * Pass that simplifies circuits by resynthesising Clifford subcircuits before
 * end of circuit measurements as a mutual diagonalisation circuit and classical
 * postprocessing.
 *
 * @return pass to resynthesise pre end of circuit measure Clifford subcircuits
 */
TKET_EXPORT PassPtr gen_clifford_push_through_pass();

/**
 * Pass to remove empty Quantum edges from a Circuit and then relabel
 * all Qubit to some new register defined by a passed label.
 * Qubits removed from the Circuit are preserved in the bimap, but not updated
 * to a new labelling.
 */
TKET_EXPORT PassPtr gen_flatten_relabel_registers_pass(const std::string& label);
/**
 * Pass to rename some or all qubits according to the given map.
 *
 * Qubits in the map that do not occur in the circuit are ignored.
 */
TKET_EXPORT PassPtr gen_rename_qubits_pass(const std::map<Qubit, Qubit>& qm);

TKET_EXPORT PassPtr gen_placement_pass(const Placement::Ptr& placement_ptr);

TKET_EXPORT PassPtr gen_naive_placement_pass(const Architecture& arc);
/* This higher order function generates a Routing pass using the
std::vector<RoutingMethodPtr> object */
TKET_EXPORT PassPtr gen_full_mapping_pass(
    const Architecture& arc, const Placement::Ptr& placement_ptr,
    const std::vector<RoutingMethodPtr>& config);
TKET_EXPORT PassPtr gen_default_mapping_pass(
    const Architecture& arc, bool delay_measures = true);
TKET_EXPORT PassPtr gen_cx_mapping_pass(
    const Architecture& arc, const Placement::Ptr& placement_ptr,
    const std::vector<RoutingMethodPtr>& config, bool directed_cx,
    bool delay_measures);
TKET_EXPORT PassPtr gen_routing_pass(
    const Architecture& arc, const std::vector<RoutingMethodPtr>& config);
TKET_EXPORT PassPtr gen_directed_cx_routing_pass(
    const Architecture& arc, const std::vector<RoutingMethodPtr>& config);

/**
 * execute architecture aware synthesis on a given architecture for an already
 * place circuit, only for circuit which contains Cx+Rz+H gates
 * this pass is not able to handle implicit wire swaps
 * @param arc architecture to route on
 * @param lookahead parameter for the recursion depth in the algorithm, the
 * value should be > 0
 * @param cnotsynthtype parameter for the type of cnot synth
 * @return passpointer to perform architecture aware synthesis
 */
TKET_EXPORT PassPtr aas_routing_pass(
    const Architecture& arc, const unsigned lookahead = 1,
    const aas::CNotSynthType cnotsynthtype = aas::CNotSynthType::Rec);

/**
 * execute architecture aware synthesis on a given architecture for any circuit.
 * all unplaced qubits will be placed in this pass
 * @param arc architecture to route on
 * @param lookahead parameter for the recursion depth in the algorithm, the
 * value should be > 0
 * @param cnotsynthtype parameter for the type of cnot synth
 * @param graph_placement_maximum_matches parameter effecting the number of
 * matches found during the GraphPlacement substep
 * @param graph_placement_timeout timeout (ms) for finding subgraph
 * monomorphisms during the GraphPlacement substep
 * @param graph_placement_maximum_pattern_gates parameter affecting the size
 * of the target graph, constructed from a phase polynomial, during
 * the GraphPlacement substep, by restricting the number of gates in
 * the phase polynomial used
 * @param graph_placement_maximum_pattern_depth parameter affecting
 * the size of the target graph, constructed from a phase polynomial,
 * during the GraphPlacement substep, by restricting the depth of gates
 * in the phase polynomial that are added to the target graph
 * @return passpointer to perform architecture aware synthesis
 */
TKET_EXPORT PassPtr gen_full_mapping_pass_phase_poly(
    const Architecture& arc, const unsigned lookahead = 1,
    const aas::CNotSynthType cnotsynthtype = aas::CNotSynthType::Rec,
    unsigned graph_placement_maximum_matches = 2000,
    unsigned graph_placement_timeout = 100,
    unsigned graph_placement_maximum_pattern_gates = 2000,
    unsigned graph_placement_maximum_pattern_depth = 2000);

/**
 * pass to place all not yet placed qubits of the circuit to the given
 * architecture for the architecture aware synthesis.
 * @param arc architecture to place the circuit on
 * @param _maximum_matches parameter effecting the number of
 * matches found during the GraphPlacement substep
 * @param _timeout timeout (ms) for finding subgraph monomorphisms
 * during the GraphPlacement substep
 * @param _maximum_pattern_gates parameter effecting the size
 * of the target graph, constructed from a phase polynomial, during
 * the GraphPlacement substep, by restricting the number of gates in
 * the phase polynomial used
 * @param _maximum_pattern_depth parameter effecting
 * the size of the target graph, constructed from a phase polynomial,
 * during the GraphPlacement substep, by restricting the depth of gates
 * in the phase polynomial that are added to the target graph
 * @return passpointer to perform the mapping
 */
TKET_EXPORT PassPtr gen_placement_pass_phase_poly(
    const Architecture& arc, unsigned _maximum_matches = 2000,
    unsigned _timeout = 100, unsigned _maximum_pattern_gates = 100,
    unsigned _maximum_pattern_depth = 100);

TKET_EXPORT PassPtr gen_decompose_routing_gates_to_cxs_pass(
    const Architecture& arc = Architecture(), bool directed = false);

/* generates a decomposition pass that converts all SWAPs into a chosen
 * replacement circuit */
TKET_EXPORT PassPtr gen_user_defined_swap_decomp_pass(const Circuit& replacement_circ);

/**
 * @brief Squash sequences of two-qubit operations into minimal form.
 *
 * A pass that squashes together sequences of single- and two-qubit gates
 * into minimal form. Can decompose to TK2 or CX gates.
 *
 * Two-qubit operations can always be expressed in a minimal form of
 * maximum three CXs, or as a single TK2 gate (a result also known
 * as the KAK or Cartan decomposition).
 *
 * It is in general recommended to squash to TK2 gates, and to then use the
 * `DecomposeTK2` pass for noise-aware decompositions to other gatesets.
 * For backward compatibility, decompositions to CX are also supported. In this
 * case, `cx_fidelity` can be provided to perform approximate decompositions to
 * CX.
 *
 * When decomposing to TK2 gates, any sequence of two or more two-qubit gates
 * on the same set of qubits are replaced by a single TK2 gate. When decomposing
 * to CX, the substitution is only performed if it results in a reduction of the
 * number of CX gates, or if at least one of the two-qubit gates is not a CX.
 *
 * Using the `allow_swaps=true` (default) option, qubits will be swapped when
 * convenient to further reduce the two-qubit gate count (only applicable
 * when decomposing to CX gates).
 *
 * @param target_2qb_gate OpType to decompose to. Either TK2 or CX.
 * @param cx_fidelity Estimated CX gate fidelity, used when target_2qb_gate=CX.
 * @param allow_swaps Whether to allow implicit wire swaps.
 * @return PassPtr
 */
TKET_EXPORT PassPtr KAKDecomposition(
    OpType target_2qb_gate = OpType::CX, double cx_fidelity = 1.,
    bool allow_swaps = true);

/**
 * @brief Decomposes each TK2 gate into two-qubit gates.
 *
 * We currently support CX, ZZMax and ZZPhase.
 *
 * If one or more gate fidelities are provided, the two-qubit gate
 * type achieving the highest fidelity will be chosen for the
 * decomposition, as measured using squared trace fidelity.
 * If no fidelities are provided, the TK2 gates will be decomposed
 * exactly using CX gates.
 *
 * All TK2(α, β, γ) gates must be normalised to the Weyl chamber, i.e.
 * 0.5 ≥ 𝛼 ≥ 𝛽 ≥ |𝛾|.
 *
 * Gate fidelities are passed as keyword arguments to perform noise-aware
 * decompositions. We currently support `CX_fidelity`, `ZZMax_fidelity` and
 * `ZZPhase_fidelity`. If provided, the `CX` and `ZZMax` fidelities must be
 * given by a single floating point fidelity. The `ZZPhase` fidelity is given as
 * a lambda float -> float, mapping a ZZPhase angle parameter to its fidelity.
 * These parameters will be used to return the optimal decomposition of each TK2
 * gate, taking noise into consideration.

 * Using the `allow_swaps=true` (default) option, qubits will be swapped when
 * convenient to reduce the two-qubit gate count of the decomposed TK2.
 *
 * If the TK2 angles are symbolic values, the decomposition will be exact
 * (i.e. not noise-aware). It is not possible in general to obtain optimal
 * decompositions for arbitrary symbolic parameters, so consider substituting
 * for concrete values if possible.
 *
 * @param fid The two-qubit gate fidelities (optional).
 * @param allow_swaps Allow implicit swaps (default = true).
 * @return PassPtr
 */
TKET_EXPORT PassPtr DecomposeTK2(
    const Transforms::TwoQbFidelities& fid, bool allow_swaps = true);
TKET_EXPORT PassPtr DecomposeTK2(bool allow_swaps = true);

/**
 * Resynthesize and squash three-qubit interactions.
 *
 * Steps through the circuit accumulating sequences of 2- and 3-qubit
 * interactions, where possible squashing them into subcircuits having lower CX
 * count, then applies Clifford simplification.
 *
 * @param allow_swaps whether to allow introduction of implicit swaps
 */
TKET_EXPORT PassPtr ThreeQubitSquash(bool allow_swaps = true);

/**
 * Performs peephole optimisation including resynthesis of 2-qubit gate
 * sequences, and converts to a circuit containing CX and TK1 gates.
 * @param allow_swaps whether to allow introduction of implicit wire swaps
 * Expects: Any gates
 * Produces: CX, TK1
 *
 */
TKET_EXPORT PassPtr PeepholeOptimise2Q(bool allow_swaps = true);

/**
 * Performs peephole optimisation including resynthesis of 2- and 3-qubit gate
 * sequences, and converts to a circuit containing a given 2-qubit gate and TK1
 * gates.
 *
 * @param target_2qb_gate target 2-qubit gate (CX or TK2)
 * @param allow_swaps whether to allow introduction of implicit swaps
 */
TKET_EXPORT PassPtr FullPeepholeOptimise(
    bool allow_swaps = true, OpType target_2qb_gate = OpType::CX);

/* generates an optimisation pass that converts a circuit into phase
gadgets and optimises them using techniques from
https://arxiv.org/abs/1906.01734 */
TKET_EXPORT PassPtr gen_optimise_phase_gadgets(
    CXConfigType cx_config = CXConfigType::Snake);

/* generates an optimisation pass that converts a circuit into Pauli
gadgets and optimises them using techniques from
https://arxiv.org/abs/1906.01734 */
TKET_EXPORT PassPtr gen_pairwise_pauli_gadgets(
    CXConfigType cx_config = CXConfigType::Snake);

/* generates an optimisation pass that converts a circuit into a graph
of PauliExpBoxes */
TKET_EXPORT PassPtr gen_pauli_exponentials(
    Transforms::PauliSynthStrat strat = Transforms::PauliSynthStrat::Sets,
    CXConfigType cx_config = CXConfigType::Snake);

/* generates an optimisation pass that converts a circuit into a graph
of Pauli gadgets and optimises them using strategies from <paper to come> */
TKET_EXPORT PassPtr gen_synthesise_pauli_graph(
    Transforms::PauliSynthStrat strat = Transforms::PauliSynthStrat::Sets,
    CXConfigType cx_config = CXConfigType::Snake);

/* generates an optimisation pass that converts a circuit built using
term sequencing techniques from <paper to come> into a graph of Pauli
gadgets and optimises them. */
TKET_EXPORT PassPtr gen_special_UCC_synthesis(
    Transforms::PauliSynthStrat strat = Transforms::PauliSynthStrat::Sets,
    CXConfigType cx_config = CXConfigType::Snake);

/**
 * @brief Greedy synthesis for Pauli graphs.
 *
 * @param discount_rate
 * @param depth_weight
 * @param max_lookahead
 * @param max_tqe_candidates
 * @param seed
 * @param allow_zzphase
 * @param thread_timeout
 * @param only_reduce
 * @param trials
 * @return PassPtr
 */
TKET_EXPORT PassPtr gen_greedy_pauli_simp(
    double discount_rate = 0.7, double depth_weight = 0.3,
    unsigned max_lookahead = 500, unsigned max_tqe_candidates = 500,
    unsigned seed = 0, bool allow_zzphase = false,
    unsigned thread_timeout = 100, bool only_reduce = false,
    unsigned trials = 1);

/**
 * Generate a pass to simplify the circuit where it acts on known basis states.
 *
 * @param allow_classical allow replacement of measures by pure classical set-
 *      bit operations when the measure acts on a known state
 * @param create_all_qubits if enabled, annotate all qubits as initialized
 *      to zero as part of the transform, before applying simplification
 * @param xcirc 1-qubit circuit implementing an X gate (if null, an X gate is
 *      used)
 */
TKET_EXPORT PassPtr gen_simplify_initial(
    Transforms::AllowClassical allow_classical =
        Transforms::AllowClassical::Yes,
    Transforms::CreateAllQubits create_all_qubits =
        Transforms::CreateAllQubits::No,
    std::shared_ptr<const Circuit> xcirc = 0);

/**
 * Generate a pass to perform simplifications dependent on qubit state.
 *
 * @param allow_classical allow insertion of classical operations
 * @param xcirc 1-qubit circuit implementing an X gate (if null, an X gate is
 *      used)
 */
TKET_EXPORT PassPtr gen_contextual_pass(
    Transforms::AllowClassical allow_classical =
        Transforms::AllowClassical::Yes,
    std::shared_ptr<const Circuit> xcirc = 0);

/**
 * Builds a sequence of PauliSimp (gen_synthesise_pauli_graph) and
 * FullPeepholeOptimise
 */
TKET_EXPORT PassPtr PauliSquash(Transforms::PauliSynthStrat strat, CXConfigType cx_config);

/**
 * Generate a pass that rounds all angles to the nearest \f$ \pi / 2^n \f$.
 *
 * In particular, angles smaller than \f$ \pi / 2^{n+1} \f$ are set to zero;
 * if a gate is turned into the identity by this operation it is removed.
 *
 * @param n precision to retain in angles
 * @param only_zeros only set angles smaller than \f$ \pi / 2^{n+1} \f$ to zero
 *
 * @ pre n < 32
 *
 * @return compilation pass that performs rounding
 */
TKET_EXPORT PassPtr RoundAngles(unsigned n, bool only_zeros = false);

/**
 * Generate a custom pass
 *
 * @param transform circuit transformation function
 * @param label optional user-defined label for the pass
 *
 * It is the caller's responsibility to provide a valid transform: there are no
 * checks on this.
 *
 * @return compilation pass that applies the supplied transform
 */
TKET_EXPORT PassPtr CustomPass(
    std::function<Circuit(const Circuit&)> transform,
    const std::string& label = "");

/**
 * Generate a custom pass that tracks map
 *
 * @param transform circuit transformation function and modified initial and
 * final maps
 * @param label optional user-defined label for the pass
 *
 * It is the caller's responsibility to provide a valid transform: there are no
 * checks on this.
 *
 * @return compilation pass that applies the supplied transform
 */
TKET_EXPORT PassPtr CustomPassMap(
    std::function<
        std::pair<Circuit, std::pair<unit_map_t, unit_map_t>>(const Circuit&)>
        transform,
    const std::string& label = "");
}  // namespace tket
