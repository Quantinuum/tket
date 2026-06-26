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

// This must be the first include since it includes Python.h.
// https://docs.python.org/3/c-api/intro.html#include-files
// clang-format off
#include <nanobind/nanobind.h>
// clang-format on
#include "tket/ZX/Rewrite.hpp"

#include <memory>

#include "typecast.hpp"

namespace nb = nanobind;

namespace tket {
namespace zx {

void init_rewrite(nb::module_ &m) {
  nb::class_<Rewrite> rewrite_cls(
      m, "Rewrite", "An in-place transformation of a ZXDiagram.");
  rewrite_cls
      .def(
          "apply",
          [](const Rewrite &rw, ZXDiagram &diag) { return rw.apply(diag); },
          "Performs the transformation on the diagram in place.\n\n"
          ":param diag: The diagram to be transformed.\n"
          ":return: True if any changes were made, else False.",
          nb::arg("diag"))
      .def_static(
          "sequence",
          [](const nb::tket_custom::SequenceVec<Rewrite> &rvec) {
            return Rewrite::sequence(rvec);
          },
          "Composes a list of :py:class:`~.Rewrite` s together in sequence. "
          "The "
          "apply method will return True if ANY of the individual Rewrites "
          "returned True.\n\n:param sequence: The list of "
          ":py:class:`~.Rewrite` s to be composed.\n:return: The combined "
          ":py:class:`~.Rewrite`.",
          nb::arg("sequence"))
      .def_static(
          "repeat", &Rewrite::repeat,
          "Applies a given :py:class:`~.Rewrite` repeatedly to a diagram until "
          "no further changes are made (i.e. it no longer returns True). apply "
          "will return True if at least one run returned True.\n\n:param "
          "rewrite: The :py:class:`~.Rewrite` to be applied "
          "repeatedly.\n:return: A new :py:class:`~.Rewrite` representing the "
          "iteration.",
          nb::arg("rewrite"))
      .def_static(
          "decompose_boxes", &Rewrite::decompose_boxes,
          "Replaces every :py:class:`~.ZXBox` by its internal diagram "
          "recursively until no :py:class:`~.ZXBox` es remain.")
      .def_static(
          "basic_wires", &Rewrite::basic_wires,
          "Replaces every Hadamard wire by an explicit Hbox node.")
      .def_static(
          "rebase_to_zx", &Rewrite::rebase_to_zx,
          "Expands every generator into ZSpiders, XSpiders, and a combination "
          "of Basic and Hadamard edges.")
      .def_static(
          "rebase_to_mbqc", &Rewrite::rebase_to_mbqc,
          "Expands every generator into MBQC vertices.")
      .def_static(
          "red_to_green", &Rewrite::red_to_green,
          "Converts all red spiders (XSpider) to green (ZSpider) with "
          "Hadamards around them. The Hadamards are applied by flipping the "
          "wire type of incident edges between Basic and H.")
      .def_static(
          "spider_fusion", &Rewrite::spider_fusion,
          "Merges two adjacent ZX spiders (XSpider, ZSpider) of the same "
          "colour connected by a Basic wire into a single spider. Also merges "
          "two adjacent spiders of different colour connected by a H edge.")
      .def_static(
          "self_loop_removal", &Rewrite::self_loop_removal,
          "Removes both H and Basic self loop edges around ZX spiders. Basic "
          "edges can simply be removed. Removing H loops introduces an extra "
          "pi phase on the spider.")
      .def_static(
          "parallel_h_removal", &Rewrite::parallel_h_removal,
          "Remove parallel edges between ZX spiders (a.k.a. the Hopf rule). "
          "Matches either pairs of H edges between spiders of the same colour "
          "or Basic edges between spiders of different colour. This applies to "
          "Quantum edges between a pair of Classical spiders.")
      .def_static(
          "separate_boundaries", &Rewrite::separate_boundaries,
          "Guarantees that each boundary vertex is adjacent to a unique "
          "ZSpider. This adds identity chains when two boundaries are either "
          "directly connected or are adjacent to the same spider.")
      .def_static(
          "io_extension", &Rewrite::io_extension,
          "Guarantees that the edge on each boundary vertex is Basic. If a "
          "boundary has a Hadamard, then we add a ZSpider identity as in I/O "
          "extensions in MBQC.")
      .def_static(
          "remove_interior_cliffords", &Rewrite::remove_interior_cliffords,
          "Removes interior proper Cliffords (spiders where the phase is an "
          "odd multiple of pi/2 radians or 0.5 half-turns). Performs local "
          "complementation about the vertex and removes it.")
      .def_static(
          "remove_interior_paulis", &Rewrite::remove_interior_paulis,
          "Removes adjacent interior Paulis (spiders where the phase is an "
          "integer multiple of pi radians or integer half-turns). Pivots about "
          "the edge connecting the vertices and removes them.")
      .def_static(
          "gadgetise_interior_paulis", &Rewrite::gadgetise_interior_paulis,
          "Identifies interior Paulis (spiders where the phase is an integer "
          "multiple of pi) with all neighbours having non-Pauli phase and "
          "degree > 1. Pivots about an incident edge to yield a gadget node.")
      .def_static(
          "merge_gadgets", &Rewrite::merge_gadgets,
          "Identifies pairs of phase gadgets over the same sets of qubits and "
          "merges them.")
      .def_static(
          "extend_at_boundary_paulis", &Rewrite::extend_at_boundary_paulis,
          "Identifies adjacent Pauli spiders where one is adjacent to a "
          "boundary. This rule applies I/O extensions to push the match into "
          "the interior from which it can be handled by "
          ":py:meth:`remove_interior_paulis`.")
      .def_static(
          "extend_for_PX_outputs", &Rewrite::extend_for_PX_outputs,
          "Identifies output vertices in MBQC form that are given a "
          "measurement basis (i.e. are not PX(0)). This rule applies I/O "
          "extensions to make the phased qubits non-outputs. This is required "
          "before flow identification can be run.")
      .def_static(
          "internalise_gadgets", &Rewrite::internalise_gadgets,
          "Identifies Degree-1 XY vertices next to a PX vertex, e.g. as the "
          "result of rebasing a phase gadget. Replaces matches by a single YZ "
          "vertex.")
      .def_static(
          "to_graphlike_form", &Rewrite::to_graphlike_form,
          "Given a diagram with ZX generators, yields a diagram with only "
          "ZSpiders, connected by at most one Hadamard edge, with boundaries "
          "connected via Basic edges.")
      .def_static(
          "reduce_graphlike_form", &Rewrite::reduce_graphlike_form,
          "Given a diagram in graphlike form, applies local complementations "
          "and pivoting to remove as many interior Clifford-angled vertices as "
          "possible. The only remaining Clifford-angled vertices will be "
          "either the axis of a phase-gadget or near a boundary.")
      .def_static(
          "to_MBQC_diag", &Rewrite::to_MBQC_diag,
          "Given a diagram in graphlike form, will rebase to MBQC generators, "
          "ensure that output qubits are PX(0) (i.e. they match unmeasured "
          "qubits) and degree-1 vertices are absorbed into a PX neighbour, "
          "i.e. reducing phase-gadgets to single vertices in a different "
          "measurement plane.");
}

}  // namespace zx
}  // namespace tket
