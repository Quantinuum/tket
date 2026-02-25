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

#include "tket/Utils/Json.hpp"

namespace tket {

/**
 * Named operation types.
 *
 * When a unitary matrix is specified in the descriptions below, the order of
 * rows and columns follows the \ref BasisOrder::ilo convention.
 *
 * Operations have defined phase.
 */
enum class OpType {
  /**
   * Quantum input node of the circuit
   */
  Input,

  /**
   * Quantum output node of the circuit
   */
  Output,

  /**
   * Quantum node with no predecessors, implicitly in zero state.
   */
  Create,

  /**
   * Quantum node with no successors, not composable with input nodes of other
   * circuits.
   */
  Discard,

  /**
   * Classical input node of the circuit
   */
  ClInput,

  /**
   * Classical output node of the circuit
   */
  ClOutput,

  /**
   * WASM input node of the circuit
   */
  WASMInput,

  /**
   * WASM output node of the circuit
   */
  WASMOutput,

  // when adding more in and out optypes make sure to add them in the exclusion
  // of the python test in
  // def optypes(draw: Callable[[SearchStrategy[Any]], Any]) -> OpType:

  /**
   * No-op that must be preserved by compilation
   */
  Barrier,

  /**
   * FlowOp introducing a target for Branch or Goto commands
   */
  Label,

  /**
   * Execution jumps to a label if a condition bit is true (1),
   * otherwise continues to next command
   */
  Branch,

  /**
   * Execution jumps to a label unconditionally
   */
  Goto,

  /**
   * Execution halts and the program terminates
   */
  Stop,

  /**
   * A general classical operation where all inputs are also outputs
   */
  ClassicalTransform,

  /**
   * Op containing a classical wasm function call
   */
  WASM,

  /**
   * An operation to set some bits to specified values
   */
  SetBits,

  /**
   * An operation to copy some bit values
   */
  CopyBits,

  /**
   * A classical predicate defined by a range of values in binary encoding
   */
  RangePredicate,

  /**
   * A classical predicate defined by a truth table
   */
  ExplicitPredicate,

  /**
   * An operation defined by a truth table that modifies one bit
   */
  ExplicitModifier,

  /**
   * A classical operation applied to multiple bits simultaneously
   */
  MultiBit,

  /**
   * \f$ \mathrm{Phase}(\alpha) = \left[ \begin{array}{c} e^{i \pi \alpha}
   * \end{array} \right] \f$
   */
  Phase,

  /**
   * \f$ \left[ \begin{array}{cc} 1 & 0 \\ 0 & -1 \end{array} \right] \f$
   */
  Z,

  /**
   * \f$ \left[ \begin{array}{cc} 0 & 1 \\ 1 & 0 \end{array} \right] \f$
   */
  X,

  /**
   * \f$ \left[ \begin{array}{cc} 0 & -i \\ i & 0 \end{array} \right] \f$
   */
  Y,

  /**
   * \f$ \left[ \begin{array}{cc} 1 & 0 \\ 0 & i \end{array} \right] =
   * \mathrm{U1}(\frac12) \f$
   */
  S,

  /**
   * \f$ \left[ \begin{array}{cc} 1 & 0 \\ 0 & -i \end{array} \right] =
   * \mathrm{U1}(-\frac12) \f$
   */
  Sdg,

  /**
   * \f$ \left[ \begin{array}{cc} 1 & 0 \\ 0 & e^{i\pi/4} \end{array} \right]
   * = \mathrm{U1}(\frac14) \f$
   */
  T,

  /**
   * \f$ \left[ \begin{array}{cc} 1 & 0 \\ 0 & e^{-i\pi/4} \end{array} \right]
   * \equiv \mathrm{U1}(-\frac14) \f$
   */
  Tdg,

  /**
   * \f$ \frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & -i \\ -i & 1
   * \end{array} \right] = \mathrm{Rx}(\frac12) \f$
   */
  V,

  /**
   * \f$ \frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & i \\ i & 1 \end{array}
   * \right] = \mathrm{Rx}(-\frac12) \f$
   */
  Vdg,

  /**
   * \f$ \frac{1}{2} \left[ \begin{array}{cc} 1+i & 1-i \\ 1-i & 1+i
   * \end{array} \right] = e^{\frac{i\pi}{4}}\mathrm{Rx}(\frac12) \f$
   */
  SX,

  /**
   * \f$ \frac{1}{2} \left[ \begin{array}{cc} 1-i & 1+i \\ 1+i & 1-i
   * \end{array} \right] = e^{\frac{-i\pi}{4}}\mathrm{Rx}(-\frac12) \f$
   */
  SXdg,

  /**
   * \f$ \frac{1}{\sqrt 2} \left[ \begin{array}{cc} 1 & 1 \\ 1 & -1
   * \end{array} \right] \f$
   */
  H,

  /**
   * \f$ \mathrm{Rx}(\alpha) = e^{-\frac12 i \pi \alpha X} = \left[
   * \begin{array}{cc} \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} \\
   * -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} \end{array} \right]
   * \f$
   */
  Rx,

  /**
   * \f$ \mathrm{Ry}(\alpha) = e^{-\frac12 i \pi \alpha Y} = \left[
   * \begin{array}{cc} \cos\frac{\pi\alpha}{2} & -\sin\frac{\pi\alpha}{2}
   * \\ \sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} \end{array} \right]
   * \f$
   */
  Ry,

  /**
   * \f$ \mathrm{Rz}(\alpha) = e^{-\frac12 i \pi \alpha Z} = \left[
   * \begin{array}{cc} e^{-\frac12 i \pi\alpha} & 0 \\ 0 & e^{\frac12 i
   * \pi\alpha} \end{array} \right] \f$
   */
  Rz,

  /**
   * \f$ \mathrm{U3}(\theta, \phi, \lambda) = \left[ \begin{array}{cc}
   * \cos\frac{\pi\theta}{2} & -e^{i\pi\lambda} \sin\frac{\pi\theta}{2} \\
   * e^{i\pi\phi} \sin\frac{\pi\theta}{2} & e^{i\pi(\lambda+\phi)}
   * \cos\frac{\pi\theta}{2} \end{array} \right] = e^{\frac12
   * i\pi(\lambda+\phi)} \mathrm{Rz}(\phi) \mathrm{Ry}(\theta)
   * \mathrm{Rz}(\lambda) \f$
   */
  U3,

  /**
   * \f$ \mathrm{U2}(\phi, \lambda) = \mathrm{U3}(\frac12, \phi, \lambda)
   * = e^{\frac12 i\pi(\lambda+\phi)} \mathrm{Rz}(\phi) \mathrm{Ry}(\frac12)
   * \mathrm{Rz}(\lambda) \f$
   */
  U2,

  /**
   * \f$ \mathrm{U1}(\lambda) = \mathrm{U3}(0, 0, \lambda) = e^{\frac12
   * i\pi\lambda} \mathrm{Rz}(\lambda) \f$
   */
  U1,

  /**
   * \f$ \mathrm{GPI}(\phi) = \left[ \begin{array}{cc}
   * 0 & e^{-i\pi\phi} \\ e^{i\pi\phi} & 0
   * \end{array} \right] \f$
   */
  GPI,

  /**
   * \f$ \mathrm{GPI2}(\phi) = \frac{1}{\sqrt 2} \left[ \begin{array}{cc}
   * 1 & -ie^{-i\pi\phi} \\ -ie^{i\pi\phi} & 1
   * \end{array} \right] \f$
   */
  GPI2,

  /**
   * \f$ \mathrm{AAMS}(\theta, \phi_0, \phi_1) = \left[ \begin{array}{cccc}
   * \cos\frac{\pi\theta}{2} & 0 & 0 &
   * -ie^{-i\pi(\phi_0+\phi_1)}\sin\frac{\pi\theta}{2} \\
   * 0 & \cos\frac{\pi\theta}{2} &
   * -ie^{i\pi(\phi_1-\phi_0)}\sin\frac{\pi\theta}{2} & 0 \\
   * 0 & -ie^{i\pi(\phi_0-\phi_1)}\sin\frac{\pi\theta}{2} &
   * \cos\frac{\pi\theta}{2} & 0 \\
   * -ie^{i\pi(\phi_0+\phi_1)}\sin\frac{\pi\theta}{2} & 0 & 0 &
   * \cos\frac{\pi\theta}{2} \end{array} \right] \f$
   */
  AAMS,

  /**
   * \f$ \mathrm{TK1}(\alpha, \beta, \gamma) = \mathrm{Rz}(\alpha)
   * \mathrm{Rx}(\beta) \mathrm{Rz}(\gamma) \f$
   */
  TK1,

  /**
   * \f$ \mathrm{TK2}(\alpha, \beta, \gamma) = \mathrm{XXPhase}(\alpha)
   * \mathrm{YYPhase}(\beta) \mathrm{ZZPhase}(\gamma) \f$
   */
  TK2,

  /**
   * Controlled \ref OpType::X
   */
  CX,

  /**
   * Controlled \ref OpType::Y
   */
  CY,

  /**
   * Controlled \ref OpType::Z
   */
  CZ,

  /**
   * Controlled \ref OpType::H
   */
  CH,

  /**
   * Controlled \ref OpType::V
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & \frac{1}{\sqrt 2} & -i \frac{1}{\sqrt 2} \\
   * 0 & 0 & -i \frac{1}{\sqrt 2} & \frac{1}{\sqrt 2}
   * \end{array} \right] \f$
   */
  CV,

  /**
   * Controlled \ref OpType::Vdg
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & \frac{1}{\sqrt 2} & i \frac{1}{\sqrt 2} \\
   * 0 & 0 & i \frac{1}{\sqrt 2} & \frac{1}{\sqrt 2}
   * \end{array} \right] \f$
   */
  CVdg,

  /**
   * Controlled \ref OpType::SX
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & \frac{1+i}{2} & \frac{1-i}{2} \\
   * 0 & 0 & \frac{1-i}{2} & \frac{1+i}{2}
   * \end{array} \right] \f$
   */
  CSX,

  /**
   * Controlled \ref OpType::SXdg
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & \frac{1-i}{2} & \frac{1+i}{2} \\
   * 0 & 0 & \frac{1+i}{2} & \frac{1-i}{2}
   * \end{array} \right] \f$
   */
  CSXdg,

  /**
   * Controlled \ref OpType::S
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & 1 & 0 \\
   * 0 & 0 & 0 & i
   * \end{array} \right] =
   * \mathrm{CU1}(\frac12) \f$
   */
  CS,

  /**
   * Controlled \ref OpType::Sdg
   *
   * \f$ \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & 1 & 0 & 0 \\
   * 0 & 0 & 1 & 0 \\
   * 0 & 0 & 0 & -i
   * \end{array} \right] =
   * \mathrm{CU1}(-\frac12) \f$
   */
  CSdg,

  /**
   * Controlled \ref OpType::Rz
   *
   * \f$ \mathrm{CRz}(\alpha) = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0
   * & 1 & 0 & 0 \\ 0 & 0 & e^{-\frac12 i \pi\alpha} & 0 \\ 0 & 0 & 0 &
   * e^{\frac12 i \pi\alpha} \end{array} \right] \f$
   *
   * The phase parameter \f$ \alpha \f$ is defined modulo \f$ 4 \f$.
   */
  CRz,

  /**
   * Controlled \ref OpType::Rx
   *
   * \f$ \mathrm{CRx}(\alpha) = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0
   * & 1 & 0 & 0 \\ 0 & 0 & \cos \frac{\pi \alpha}{2} & -i \sin \frac{\pi
   * \alpha}{2}
   * \\ 0 & 0 & -i \sin \frac{\pi \alpha}{2}  & \cos \frac{\pi \alpha}{2}
   * \end{array} \right] \f$
   *
   * The phase parameter \f$ \alpha \f$ is defined modulo \f$ 4 \f$.
   */
  CRx,

  /**
   * Controlled \ref OpType::Ry
   *
   * \f$ \mathrm{CRy}(\alpha) = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0
   * & 1 & 0 & 0 \\ 0 & 0 & \cos \frac{\pi \alpha}{2} & -\sin \frac{\pi
   * \alpha}{2}
   * \\ 0 & 0 & \sin \frac{\pi \alpha}{2}  & \cos \frac{\pi \alpha}{2}
   * \end{array} \right] \f$
   *
   * The phase parameter \f$ \alpha \f$ is defined modulo \f$ 4 \f$.
   */
  CRy,

  /**
   * Controlled \ref OpType::U1
   *
   * \f$ \mathrm{CU1}(\alpha) = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0
   * & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & e^{i\pi\alpha} \end{array}
   * \right] \f$
   */
  CU1,

  /**
   * Controlled \ref OpType::U3
   */
  CU3,

  /**
   * \f$ \alpha \mapsto e^{-\frac12 i \pi\alpha Z^{\otimes n}} \f$
   */
  PhaseGadget,

  /**
   * Controlled \ref OpType::CX
   */
  CCX,

  /**
   * Swap two qubits
   */
  SWAP,

  /**
   * Controlled \ref OpType::SWAP
   */
  CSWAP,

  /**
   * Three-qubit gate that swaps the first and third qubits
   */
  BRIDGE,

  /**
   * Identity
   */
  noop,

  /**
   * Measure a qubit, producing a classical output
   */
  Measure,

  /**
   * Measure a qubit producing no output
   */
  Collapse,

  /**
   * Reset a qubit to the zero state
   */
  Reset,

  /**
   * \f$ \frac{1}{\sqrt 2} \left[ \begin{array}{cccc} 0 & 0 & 1 & i \\ 0 & 0 &
   * i & 1 \\ 1 & -i & 0 & 0 \\ -i & 1 & 0 & 0 \end{array} \right] \f$
   */
  ECR,

  /**
   * \f$ \alpha \mapsto e^{\frac14 i \pi\alpha (X \otimes X + Y \otimes Y)}
   * = \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & \cos\frac{\pi\alpha}{2}
   * & i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & i\sin\frac{\pi\alpha}{2} &
   * \cos\frac{\pi\alpha}{2} & 0 \\ 0 & 0 & 0 & 1 \end{array} \right] \f$
   *
   * Also known as an XY gate.
   */
  ISWAP,

  /**
   * \f$ (\alpha, \beta) \mapsto \mathrm{Rz}(\beta) \mathrm{Rx}(\alpha)
   * \mathrm{Rz}(-\beta) \f$
   */
  PhasedX,

  /**
   * PhasedX gates on multiple qubits
   */
  NPhasedX,

  /**
   * \f$ \mathrm{ZZPhase}(\frac12) \f$
   */
  ZZMax,

  /**
   * \f$ \alpha \mapsto e^{-\frac12 i \pi\alpha (X \otimes X)} = \left[
   * \begin{array}{cccc} \cos\frac{\pi\alpha}{2} & 0 & 0 &
   * -i\sin\frac{\pi\alpha}{2} \\ 0 & \cos\frac{\pi\alpha}{2} &
   * -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & -i\sin\frac{\pi\alpha}{2} &
   * \cos\frac{\pi\alpha}{2} & 0 \\ -i\sin\frac{\pi\alpha}{2} & 0 & 0 &
   * \cos\frac{\pi\alpha}{2} \end{array} \right] \f$
   */
  XXPhase,

  /**
   * \f$ \alpha \mapsto e^{-\frac12 i \pi\alpha (Y \otimes Y)} = \left[
   * \begin{array}{cccc} \cos\frac{\pi\alpha}{2} & 0 & 0 &
   * i\sin\frac{\pi\alpha}{2} \\ 0 & \cos\frac{\pi\alpha}{2} &
   * -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 & -i\sin\frac{\pi\alpha}{2} &
   * \cos\frac{\pi\alpha}{2} & 0 \\ i\sin\frac{\pi\alpha}{2} & 0 & 0 &
   * \cos\frac{\pi\alpha}{2} \end{array} \right] \f$
   */
  YYPhase,

  /**
   * \f$ \alpha \mapsto e^{-\frac12 i \pi\alpha (Z \otimes Z)} = \left[
   * \begin{array}{cccc} e^{-\frac12 i \pi\alpha} & 0 & 0 & 0 \\ 0 &
   * e^{\frac12 i \pi\alpha} & 0 & 0 \\ 0 & 0 & e^{\frac12 i \pi\alpha} & 0 \\ 0
   * & 0 & 0 & e^{-\frac12 i \pi\alpha} \end{array} \right] \f$
   */
  ZZPhase,

  /**
   * Three-qubit phase MSGate
   */
  XXPhase3,

  /**
   * \f$ \alpha \mapsto e^{-\frac12 i\pi\alpha \cdot \mathrm{SWAP}} = \left[
   * \begin{array}{cccc} e^{-\frac12 i \pi\alpha} & 0 & 0 & 0 \\ 0 &
   * \cos\frac{\pi\alpha}{2} & -i\sin\frac{\pi\alpha}{2} & 0 \\ 0 &
   * -i\sin\frac{\pi\alpha}{2} & \cos\frac{\pi\alpha}{2} & 0 \\ 0 & 0 & 0 &
   * e^{-\frac12 i \pi\alpha} \end{array} \right] \f$
   */
  ESWAP,

  /**
   * \f$ (\alpha, \beta) \mapsto \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0
   * & \cos \pi\alpha & -i\sin  \pi\alpha & 0 \\ 0 &
   * -i\sin \pi\alpha & \cos \pi\alpha & 0 \\ 0 & 0 & 0 &
   * e^{-i\pi\beta} \end{array} \right] \f$
   */
  FSim,

  /**
   * Fixed instance of a \ref OpType::FSim gate with parameters
   * \f$ (\frac12, \frac16) \f$:
   * \f$ \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & 0 & -i & 0 \\ 0 & -i
   * & 0 & 0 \\ 0 & 0 & 0 & e^{-i\pi/6} \end{array} \right] \f$
   */
  Sycamore,

  /**
   * Fixed instance of a \ref OpType::ISWAP gate with parameter \f$ 1.0 \f$:
   * \f$ \left[ \begin{array}{cccc} 1 & 0 & 0 & 0 \\ 0 & 0 & i & 0 \\ 0 & i
   * & 0 & 0 \\ 0 & 0 & 0 & 1 \end{array} \right] \f$
   */
  ISWAPMax,

  /**
   * \f$ (p, t) \mapsto \left[ \begin{array}{cccc}
   * 1 & 0 & 0 & 0 \\
   * 0 & \cos\frac{\pi t}{2} & i\sin\frac{\pi t}{2}e^{2i\pi p} & 0 \\
   * 0 & i\sin\frac{\pi t}{2}e^{-2i\pi p} & \cos\frac{\pi t}{2} & 0 \\
   * 0 & 0 & 0 & 1
   * \end{array} \right] \f$
   *
   * This is equivalent to:
   *
   *     --Rz(+p)---\    /---Rz(-p)--
   *               ISWAP(t)
   *     --Rz(-p)---/    \---Rz(+p)--
   */
  PhasedISWAP,

  /**
   * Multiply-controlled \ref OpType::Ry
   *
   * The phase parameter is defined modulo \f$ 4 \f$.
   */
  CnRy,

  /**
   * Multiply-controlled \ref OpType::Rx
   *
   * The phase parameter is defined modulo \f$ 4 \f$.
   */
  CnRx,

  /**
   * Multiply-controlled \ref OpType::Rz
   *
   * The phase parameter is defined modulo \f$ 4 \f$.
   */
  CnRz,

  /**
   * Multiply-controlled \ref OpType::X
   */
  CnX,

  /**
   * Multiply-controlled \ref OpType::Z
   */
  CnZ,

  /**
   * Multiply-controlled \ref OpType::Y
   */
  CnY,

  /**
   * See \ref CircBox
   */
  CircBox,

  /**
   * See \ref Unitary1qBox
   */
  Unitary1qBox,

  /**
   * See \ref Unitary2qBox
   */
  Unitary2qBox,

  /**
   * See \ref Unitary3qBox
   */
  Unitary3qBox,

  /**
   * See \ref ExpBox
   */
  ExpBox,

  /**
   * See \ref PauliExpBox
   */
  PauliExpBox,

  /**
   * See \ref PauliExpPairBox
   */
  PauliExpPairBox,

  /**
   * See \ref PauliExpCommutingSetBox
   */
  PauliExpCommutingSetBox,

  /**
   * See \ref TermSequenceBox
   */
  TermSequenceBox,

  /**
   * NYI
   */
  CliffBox,

  /**
   * See \ref CustomGate
   */
  CustomGate,

  /**
   * See \ref PhasePolyBox
   */
  PhasePolyBox,

  /**
   * See \ref QControlBox
   */
  QControlBox,

  /**
   * See \ref MultiplexorBox
   */
  MultiplexorBox,

  /**
   * See \ref MultiplexedRotationBox
   */
  MultiplexedRotationBox,

  /**
   * See \ref MultiplexedU2Box
   */
  MultiplexedU2Box,

  /**
   * See \ref MultiplexedTensoredU2Box
   */
  MultiplexedTensoredU2Box,

  /**
   * See \ref StatePreparationBox
   */
  StatePreparationBox,

  /**
   * See \ref DiagonalBox
   */
  DiagonalBox,

  /**
   * See \ref ConjugationBox
   */
  ConjugationBox,

  /**
   * See \ref Conditional
   */
  Conditional,

  /**
   * See \ref ProjectorAssertionBox
   */
  ProjectorAssertionBox,

  /**
   * See \ref StabiliserAssertionBox
   */
  StabiliserAssertionBox,

  /**
   * See \ref ToffoliBox
   */
  ToffoliBox,

  /**
   * See \ref UnitaryTableauBox
   */
  UnitaryTableauBox,

  /**
   * See \ref DummyBox
   */
  DummyBox,

  /**
   * Function defined over bits and sequences of bits treated as integers
   */
  ClExpr,

  /**
   * RNG input node of the circuit
   */
  RNGInput,

  /**
   * RNG output node of the circuit
   */
  RNGOutput,

  /**
   * Seed an RNG using 64 bits
   */
  RNGSeed,

  /**
   * Set an (inclusive) 32-bit upper bound on RNG output
   */
  RNGBound,

  /**
   * Set a 32-bit index on an RNG
   */
  RNGIndex,

  /**
   * Get 32-bit output from an RNG
   */
  RNGNum,

  /**
   * Get 32-bit (little-endian) shot number
   */
  JobShotNum,

  /**
   * Two identical \ref PhasedX applied to different qubits
   */
  TwinPhasedX,

  /**
   * An \ref XXPhase conjugated by \ref Rz operations on both qubits
   */
  PhasedXX,
};

JSON_DECL(OpType)

}  // namespace tket
