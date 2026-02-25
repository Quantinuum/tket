from typing import Union

import sympy.core.expr

import pytket._tket.circuit


def BRIDGE_using_CX_0() -> pytket._tket.circuit.Circuit:
    """Equivalent to BRIDGE, using four CX, first CX has control on qubit 0"""

def BRIDGE_using_CX_1() -> pytket._tket.circuit.Circuit:
    """Equivalent to BRIDGE, using four CX, first CX has control on qubit 1"""

def CX_using_TK2() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using a TK2 and single-qubit gates"""

def TK2_using_CX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Given expressions α, β and γ, return circuit equivalent to TK2(α, β, γ) using up to 3 CX and single-qubit gates.

    The decomposition minimizes the number of CX gates.
    """

def TK2_using_CX_and_swap(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Given expressions α, β and γ, return circuit equivalent to TK2(α, β, γ), up to a wire swap that is encoded in the implicit qubit permutation of the Circuit, using up to 3 CX and single-qubit gates.

    The decomposition minimizes the number of CX gates.
    """

def approx_TK2_using_1xCX() -> pytket._tket.circuit.Circuit:
    """
    Best approximation of TK2 using 1 CX gate and single-qubit gates, using squared trace fidelity metric. No parameter is required for this approximation. The returned circuit will be equivalent to TK2(0.5, 0, 0).
    """

def approx_TK2_using_2xCX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Best approximation of TK2 using 2 CX gates and single-qubit gates, using squared trace fidelity metric. Given expressions α and β, with 0.5 ≥ α ≥ β ≥ 0, return a circuit equivalent to TK2(α, β, 0).
    """

def TK2_using_3xCX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Given expressions α, β and γ, return circuit equivalent to TK2(α, β, γ) using 3 CX and single-qubit gates.

    Prefer using `_TK2_using_CX` unless you wish to explicitly use 3 CX or if α, β and γ are not normalised to the Weyl chamber.
    """

def CX_using_flipped_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX[0,1], using a CX[1,0] and four H gates"""

def CX_using_ECR() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only ECR, Rx and U3 gates"""

def CX_using_ZZMax() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only ZZMax, Rx and Rz gates"""

def CX_using_ISWAPMax() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only ISWAPMax and single-qubit gates"""

def CX_using_ISWAPMax_and_swap() -> pytket._tket.circuit.Circuit:
    """
    Equivalent to CX, using only ISWAPMax and single-qubit gates, up to a wire swap that is encoded in the implicit qubit permutation of the Circuit
    """

def CX_using_ZZPhase() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only ZZPhase, Rx and Rz gates"""

def CX_using_XXPhase_0() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only XXPhase, Rx, Ry and Rz gates"""

def CX_using_XXPhase_1() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using only XXPhase, Rx, Ry and Rz gates"""

def CX_VS_CX_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/V,S/CX"""

def CX_V_CX_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/V,-/CX"""

def CX_S_CX_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/-,S/CX (= ZZMax)"""

def CX_V_S_XC_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/V,-/S,-/XC"""

def CX_S_V_XC_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/-,S/-,V/XC"""

def CX_XC_reduced() -> pytket._tket.circuit.Circuit:
    """CX-reduced form of CX/XC"""

def SWAP_using_CX_0() -> pytket._tket.circuit.Circuit:
    """Equivalent to SWAP, using three CX, outer CX have control on qubit 0"""

def SWAP_using_CX_1() -> pytket._tket.circuit.Circuit:
    """Equivalent to SWAP, using three CX, outer CX have control on qubit 1"""

def X1_CX() -> pytket._tket.circuit.Circuit:
    """X[1]; CX[0,1]"""

def Z0_CX() -> pytket._tket.circuit.Circuit:
    """Z[0]; CX[0,1]"""

def CCX_modulo_phase_shift() -> pytket._tket.circuit.Circuit:
    """
    Equivalent to CCX up to phase shift, using three CX. Warning: this is not equivalent to CCX up to global phase so cannot be used as a direct substitution except when the phase reversal can be cancelled. Its unitary representation is like CCX but with a -1 at the (5,5) position.
    """

def CCX_normal_decomp() -> pytket._tket.circuit.Circuit:
    """Equivalent to CCX, using 6 CX"""

def C3X_normal_decomp() -> pytket._tket.circuit.Circuit:
    """Equivalent to CCCX, using 14 CX"""

def C4X_normal_decomp() -> pytket._tket.circuit.Circuit:
    """Equivalent to CCCCX, using 36 CX"""

def CnX_vchain_decomp(n: int, zeroed_ancillas: bool = True) -> pytket._tket.circuit.Circuit:
    r"""
    CnX decomposition from https://arxiv.org/abs/1906.01734/1508.03273.

    :param n: Number of control qubits
    :param zeroed_ancillas: If True, the gate will be implemented assuming that all ancilla qubits start in state :math:`\ket{0}`. If False, ancilla qubits may be initialized in any state, at the cost of higher CX-count.

    :return: Circuit with control qubits at indices :math:`0, \ldots, n-1`, target qubit :math:`n`, and ancilla qubits :math:`n+1, \ldots, n + \lfloor(n-1)/2\rfloor`.
    """

def ladder_down() -> pytket._tket.circuit.Circuit:
    """CX[0,1]; CX[2,0]; CCX[0,1,2]"""

def ladder_down_2() -> pytket._tket.circuit.Circuit:
    """CX[0,1]; X[0]; X[2]; CCX[0,1,2]"""

def ladder_up() -> pytket._tket.circuit.Circuit:
    """CCX[0,1,2]; CX[2,0]; CX[2,1]"""

def X() -> pytket._tket.circuit.Circuit:
    """Just an X gate"""

def CX() -> pytket._tket.circuit.Circuit:
    """Just a CX[0,1] gate"""

def CCX() -> pytket._tket.circuit.Circuit:
    """Just a CCX[0,1,2] gate"""

def BRIDGE() -> pytket._tket.circuit.Circuit:
    """Just a BRIDGE[0,1,2] gate"""

def H_CZ_H() -> pytket._tket.circuit.Circuit:
    """H[1]; CZ[0,1]; H[1]"""

def CZ_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CZ, using CX and single-qubit gates"""

def CY_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CY, using CX and single-qubit gates"""

def CH_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CH, using CX and single-qubit gates"""

def CV_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CV, using CX and single-qubit gates"""

def CVdg_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CVdg, using CX and single-qubit gates"""

def CSX_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CSX, using CX and single-qubit gates"""

def CSXdg_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CSXdg, using CX and single-qubit gates"""

def CS_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CS, using CX and single-qubit gates"""

def CSdg_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CSdg, using CX and single-qubit gates"""

def CSWAP_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to CSWAP, using CX and single-qubit gates"""

def ECR_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to ECR, using CX, Rx and U3 gates"""

def ZZMax_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to ZZMax, using CX, Rz and U3 gates"""

def CRz_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRz, using a TK2 and TK1 gates"""

def CRz_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRz, using CX and Rz gates"""

def CRx_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRx, using a TK2 and TK1 gates"""

def CRx_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRx, using CX, H and Rx gates"""

def CRy_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRy, using a TK2 and TK1 gates"""

def CRy_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CRy, using CX and Ry gates"""

def CU1_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CU1, using a TK2 and TK1 gates"""

def CU1_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CU1, using CX and U1 gates"""

def CU3_using_CX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to CU1, using CX, U1 and U3 gates"""

def ISWAP_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ISWAP, using a TK2 gate"""

def ISWAP_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ISWAP, using CX, U3 and Rz gates"""

def ISWAPMax_using_TK2() -> pytket._tket.circuit.Circuit:
    """Equivalent to ISWAPMax, using a TK2 gate"""

def ISWAPMax_using_CX() -> pytket._tket.circuit.Circuit:
    """Equivalent to ISWAPMax, using CX, U3 and Rz gates"""

def XXPhase_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to XXPhase, using a TK2 gate"""

def XXPhase_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to XXPhase, using CX and U3 gates"""

def PhasedXX_using_CX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to PhasedXX, using CX, Rz, Ry, S and Sdg gates"""

def YYPhase_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to YYPhase, using a TK2 gate"""

def YYPhase_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Equivalent to YYPhase, using two CX gates and one Ry, one Sdg and one S gate.
    """

def ZZPhase_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ZZPhase, using a TK2 gate"""

def ZZPhase_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ZZPhase, using CX and Rz gates"""

def TK2_using_ZZPhase(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to TK2, using 3 ZZPhase gates"""

def TK2_using_ZZPhase_and_swap(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Equivalent to TK2, up to a wire swap that is encoded in the implicit qubit permutation of the Circuit, using up to 3 ZZPhase gates.
    """

def TK2_using_TK2_or_swap(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Either the exact TK2, or a wire swap encoded in the implicit qubit permutation of the Circuit and single qubit gates.
    """

def TK2_using_TK2(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A circuit of a single TK2 gate with given parameters"""

def approx_TK2_using_1xZZPhase(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Approximate equivalent to TK2, using 1 ZZPhase gate and single-qubit gates. Only requires the first angle of the TK2 gate.
    """

def approx_TK2_using_2xZZPhase(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Approximate equivalent to TK2, using 2 ZZPhase gates and single-qubit gates. Only requires the first two angles of the TK2 gate.
    """

def TK2_using_ZZMax(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to TK2, using up to 3 ZZMax gates."""

def TK2_using_ZZMax_and_swap(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Equivalent to TK2, up to a wire swap that is encoded in the implicit qubit permutation of the Circuit, using up to 3 ZZMax gates.
    """

def TK2_using_ISWAPMax(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to TK2, using only ISWAPMax and single-qubit gates."""

def TK2_using_ISWAPMax_and_swap(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    Equivalent to TK2, using only ISWAPMax and single-qubit gates, up to a wire swap that is encoded in the implicit qubit permutation of the Circuit.
    """

def XXPhase3_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to XXPhase3, using three TK2 gates"""

def XXPhase3_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to 3-qubit MS interaction, using CX and U3 gates"""

def ESWAP_using_TK2(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ESWAP, using a TK2 and (Clifford) TK1 gates"""

def ESWAP_using_CX(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ESWAP, using CX, X, S, Ry and U1 gates"""

def FSim_using_TK2(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to FSim, using a TK2 and TK1 gates"""

def FSim_using_CX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to Fsim, using CX, X, S, U1 and U3 gates"""

def PhasedISWAP_using_TK2(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to PhasedISWAP, using a TK2 and Rz gates"""

def PhasedISWAP_using_CX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to PhasedISWAP, using CX, U3 and Rz gates"""

def NPhasedX_using_PhasedX(arg0: int, arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Unwrap NPhasedX, into number_of_qubits PhasedX gates"""

def TK2_using_normalised_TK2(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    TK2(a, b, c)-equivalent circuit, using a single normalised TK2 and single-qb gates
    """

def TK1_to_PhasedXRz(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of PhasedX, Rz"""

def TK1_to_PhasedX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of PhasedX"""

def TK1_to_RzRx(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of Rz, Rx"""

def TK1_to_RxRy(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of Rx, Ry"""

def TK1_to_RzH(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of Rz, H"""

def TK1_to_RzSX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of Rz, Sx"""

def TK1_to_RzXSX(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A tk1 equivalent circuit given tk1 parameters in terms of Rz, X, Sx"""

def TK1_to_TK1(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """A circuit of a single tk1 gate with given parameters"""

def TK1_to_U3(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """
    A tk1 equivalent circuit given tk1 parameters in terms of U3 and global phase
    """

def Rx_using_GPI(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to Rx, using GPI and GPI2 gates"""

def Ry_using_GPI(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to Ry, using GPI and GPI2 gates"""

def Rz_using_GPI(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to Rz, using GPI gates"""

def XXPhase_using_AAMS(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to XXPhase, using AAMS gates"""

def YYPhase_using_AAMS(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to YYPhase, using AAMS gates"""

def ZZPhase_using_AAMS(arg: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to ZZPhase, using AAMS, GPI and GPI2 gates"""

def CX_using_AAMS() -> pytket._tket.circuit.Circuit:
    """Equivalent to CX, using AAMS, GPI and GPI2 gates"""

def TK1_using_GPI(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to TK1, using GPI and GPI2 gates"""

def TK2_using_AAMS(arg0: Union[sympy.core.expr.Expr, float], arg1: Union[sympy.core.expr.Expr, float], arg2: Union[sympy.core.expr.Expr, float], /) -> pytket._tket.circuit.Circuit:
    """Equivalent to TK2, using AAMS, GPI and GPI2 gates"""
