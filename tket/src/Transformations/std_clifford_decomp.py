# Copyright Quantinuum
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from numpy import allclose, angle, array, cos, exp, eye, pi, sin, sqrt


def phase_diff(A, B):
    """a s.t. e^{i pi a} A = B"""
    U = B @ A.conjugate().transpose()
    u00 = U[0, 0]
    if not allclose(U, u00 * eye(len(A), dtype=complex)):
        return None
    return angle(u00) / pi


I = eye(2, dtype=complex)
X = array([[0, 1], [1, 0]], dtype=complex)
Z = array([[1, 0], [0, -1]], dtype=complex)
S = array([[1, 0], [0, 1j]], dtype=complex)
V = array([[1, -1j], [-1j, 1]], dtype=complex) / sqrt(2)

Rx = lambda a: array(
    [
        [cos(pi * a / 2), -1j * sin(pi * a / 2)],
        [-1j * sin(pi * a / 2), cos(pi * a / 2)],
    ],
    dtype=complex,
)
Rz = lambda a: array([[exp(-0.5j * pi * a), 0], [0, exp(0.5j * pi * a)]], dtype=complex)


def std_cliff():
    # Return dict d where d[(i,j,k)] gives the decomposition of Rz(i/2)Rx(j/2)Rz(k/2)
    # as a sequence of operations Z? X? S? V? S? followed by a phase
    etable = {}
    for t0 in range(2):
        for t1 in range(2):
            for t2 in range(2):
                for t3 in range(2):
                    for t4 in range(2):
                        m = I
                        if t0:
                            m = Z @ m
                        if t1:
                            m = X @ m
                        if t2:
                            m = S @ m
                        if t3:
                            m = V @ m
                        if t4:
                            m = S @ m
                        etable[(t0, t1, t2, t3, t4)] = m
    d = {}
    for i in range(4):
        for j in range(4):
            for k in range(4):
                u = Rz(i / 2) @ Rx(j / 2) @ Rz(k / 2)
                matches = []
                for ts, m in etable.items():
                    p = phase_diff(m, u)
                    if p is not None:
                        matches.append((ts, p.round(3)))
                if not matches:
                    raise RuntimeError(f"match not found for {i}, {j}, {k}")
                # Return the match with fewest gates
                best_i = min(
                    ((i, sum(x[0])) for i, x in enumerate(matches)), key=lambda y: y[1]
                )[0]
                d[(i, j, k)] = matches[best_i]
    for i in range(4):
        print("{")
        for j in range(4):
            print("    {")
            for k in range(4):
                ts, p = d[(i, j, k)]
                print("        {" + ", ".join(f"{t}" for t in ts) + f", {p}" + "},")
            print("    },")
        print("},")
    return d
