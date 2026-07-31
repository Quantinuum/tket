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

#include "tket/Characterisation/Cycles.hpp"
#include "tket/Circuit/Circuit.hpp"

#include "tket_export.h"

namespace tket {

// Friend class for testing some private methods in FrameRandomisation
class FrameRandomisationTester;

// FrameRandomisation provides methods for applying a circuit noise shaping
// method 'Cycles' i.e. sub-circuits of a given circuit are found such that:
// • every gate Op has a type in some allowed set of OpTypes (cycle_types_)
// • the minimum number of cycles is found where each allowed Op is in a cycle
// New gates are wired into the EdgeType::Quantum boundary of each cycle (adding
// a Frame) such that:
// • Each OpType for vertices in the "in" boundary of each cycle is uniformly
//   sampled from some allowed set of 1-qubit OpTypes (frame_types_)
// • OpTypes for vertices in the "out" boundary and are determined from "in"
//   frame & cycle gates such that the overall circuit unitary is unchanged (up
//   to global phase)
class TKET_EXPORT FrameRandomisation {
 public:
  FrameRandomisation() {}
  virtual ~FrameRandomisation() {}
  // _frame_cycle_conjugates keys are OpTypes from _cycle_types
  // _frame_cycle_conjugate values are maps between OpTypeVector, where all
  // OpType are from _frame_types
  FrameRandomisation(
      const OpTypeSet& _cycle_types, const OpTypeSet& _frame_types,
      std::map<OpType, std::map<OpTypeVector, OpTypeVector>>
          _frame_cycle_conjugates)
      : cycle_types_(_cycle_types),
        frame_types_(_frame_types),
        frame_cycle_conjugates_(_frame_cycle_conjugates) {}
  // Returns samples instances of frame randomisation for circ
  virtual std::vector<Circuit> sample_randomisation_circuits(
      const Circuit& circ, unsigned samples);
  // Returns every combination of frame for every cycle in circ
  std::vector<Circuit> get_all_circuits(const Circuit& circ);

  std::string to_string() const;

 protected:
  OpTypeSet cycle_types_;
  OpTypeSet frame_types_;
  std::map<OpType, std::map<OpTypeVector, OpTypeVector>>
      frame_cycle_conjugates_;
  Circuit circuit_;

  // Each Cycle in cycles has corresponding addresses of frame vertices in circ
  // The OpType of each frame vertex is reassigned given all_frame_ops
  std::vector<Circuit> label_frames(
      const std::vector<std::vector<OpTypeVector>>& all_frame_ops,
      const std::vector<Cycle>& cycles);

  // Labels frame_vertices with in_frame & out_frame OpType
  void assign_vertices(
      const OpTypeVector& in_frame, const OpTypeVector& out_frame,
      const std::vector<std::pair<Vertex, Vertex>>& frame_vertices);

  // Finds cycles of cycle_types_ Op in circ using CycleFinder class
  std::vector<Cycle> get_cycles(const Circuit& circ) const;

  // Uniformly samples size OpType from frame_types_
  OpTypeVector sample_frame(const unsigned& size) const;

  // Returns every permutation of OpType in frame_types_ up to largest
  // frame_sizes value
  std::vector<std::vector<OpTypeVector>> get_all_samples(
      const unsigned& samples, const std::vector<unsigned>& frame_sizes) const;

 private:
  // Returns new OpTypeVector "out_frame"
  // Sets "out_frame" equal to "in_frame"
  // given OpType in "out_frame", sequentially finds action of OpType in cycle
  // on "out_frame" OpType using frame_cycle_conjugates_ Unitarity of circuit
  // preserved up to global phase
  virtual std::pair<OpTypeVector, std::vector<Vertex>> get_out_frame(
      const OpTypeVector& in_frame, const Cycle& cycle);

  friend class FrameRandomisationTester;
};

// Instance of FrameRandomisation where cycle_types_ are "hard" Clifford gates
// frame_types_ are Pauli gates
class TKET_EXPORT PauliFrameRandomisation : public FrameRandomisation {
 public:
  PauliFrameRandomisation() {
    cycle_types_ = {OpType::H, OpType::CX, OpType::S};
    frame_types_ = {OpType::X, OpType::Y, OpType::Z, OpType::noop};
  }

 protected:
  //   PauliTensor class used to find "out_frame"
  std::pair<OpTypeVector, std::vector<Vertex>> get_out_frame(
      const OpTypeVector& in_frame, const Cycle& cycle);
};

// Instance of FrameRandomisation where cycle gates can be modified
class TKET_EXPORT UniversalFrameRandomisation : public FrameRandomisation {
 public:
  UniversalFrameRandomisation() {
    cycle_types_ = {OpType::H, OpType::CX, OpType::Rz};
    frame_types_ = {OpType::X, OpType::Y, OpType::Z, OpType::noop};
  }
  virtual ~UniversalFrameRandomisation() {}

 protected:
  //   PauliTensor class used to find "out_frame"
  std::pair<OpTypeVector, std::vector<Vertex>> get_out_frame(
      const OpTypeVector& in_frame, const Cycle& cycle);
};

// Special Instance of PauliFrameRandomisation
// circ must be one cycle
// cycle + "out_frame" appended to circ cycle_repeats number of times
// one "in_frame" is sampled and each individual "out_frame" for each repeat
// determined
class PowerCycle : public PauliFrameRandomisation {
 public:
  PowerCycle() {
    cycle_types_ = {OpType::Z,   OpType::X,  OpType::Y,   OpType::S,
                    OpType::Sdg, OpType::V,  OpType::Vdg, OpType::H,
                    OpType::CX,  OpType::CY, OpType::CZ,  OpType::noop};
    frame_types_ = {OpType::X, OpType::Y, OpType::Z, OpType::noop};
  }
  std::vector<Circuit> sample_cycles(
      const Circuit& circ, unsigned cycle_repeats, unsigned samples);
};

// Friend class of FrameRandomisation for testing some private methods
class FrameRandomisationTester {
 private:
  FrameRandomisation* fr;

 public:
  explicit FrameRandomisationTester(FrameRandomisation* _fr) : fr(_fr) {}

  std::vector<Cycle> get_cycles(const Circuit& circ);

  OpTypeVector get_out_frame(
      const OpTypeVector& in_frame, const Cycle& cycle_ops);

  std::vector<std::vector<OpTypeVector>> get_all_samples(
      const unsigned& samples, const std::vector<unsigned>& frame_sizes);
};

}  // namespace tket
