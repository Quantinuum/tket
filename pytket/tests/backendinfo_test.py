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

"""
Tests BackendInfo - there is not much to test, it is not much more
than a dataclass
"""

from json import dumps, loads

import pytest
import strategies as st  # type: ignore
from hypothesis import given, settings

from pytket.architecture import FullyConnected, RingArch, SquareGrid
from pytket.backends.backendinfo import BackendInfo, fully_connected_backendinfo
from pytket.circuit import Node, OpType


def test_nodes() -> None:
    arch = SquareGrid(3, 4)
    bi = BackendInfo("name", "device_name", "version", arch, {OpType.CX, OpType.Rx})
    assert bi.n_nodes == 12

    bi_nodes = set(bi.nodes)
    arch_nodes = set(arch.nodes)
    assert bi_nodes == arch_nodes


def test_default_no_fast_feedforward() -> None:
    bi = BackendInfo(
        "name", "device_name", "version", SquareGrid(3, 4), {OpType.CX, OpType.Rx}
    )
    assert not bi.supports_fast_feedforward


def test_default_no_reset() -> None:
    bi = BackendInfo(
        "name", "device_name", "version", SquareGrid(3, 4), {OpType.CX, OpType.Rx}
    )
    assert not bi.supports_reset


def test_default_no_midcircuit_meas() -> None:
    bi = BackendInfo(
        "name", "device_name", "version", SquareGrid(3, 4), {OpType.CX, OpType.Rx}
    )
    assert not bi.supports_midcircuit_measurement


def test_gate_errors_options() -> None:
    bi = BackendInfo(
        "name", "device_name", "version", SquareGrid(3, 4), {OpType.CX, OpType.Rx}
    )
    assert bi.all_node_gate_errors is None
    assert bi.all_edge_gate_errors is None
    assert bi.all_readout_errors is None
    assert bi.averaged_node_gate_errors is None
    assert bi.averaged_edge_gate_errors is None
    assert bi.averaged_readout_errors is None

    example_node_error = {Node(0): {OpType.H: 0.3, OpType.X: 0.4}}
    example_averaged_readout_errors = {Node(1): 0.4, Node(0): 0.3}
    bi = BackendInfo(
        "name",
        "device_name",
        "version",
        SquareGrid(3, 4),
        {OpType.CX, OpType.Rx},
        all_node_gate_errors=example_node_error,
        averaged_readout_errors=example_averaged_readout_errors,
    )
    assert bi.all_node_gate_errors == example_node_error
    assert bi.averaged_readout_errors == example_averaged_readout_errors


def test_misc() -> None:
    key, val = "noise", [0.1, 0.56]
    otherkey = "error"
    otherval = None

    bi = BackendInfo(
        "name", "device_name", "version", SquareGrid(3, 4), {OpType.CX, OpType.Rx}
    )
    bi.add_misc(key, val)

    assert bi.get_misc(key) == val

    with pytest.raises(KeyError):
        bi.get_misc(otherkey)
    # cannot add with same key
    with pytest.raises(KeyError):
        bi.add_misc(key, otherval)


def test_serialization_squaregrid() -> None:
    bi = BackendInfo(
        "name",
        "device_name",
        "version",
        SquareGrid(3, 4),
        {OpType.CX, OpType.Rx},
        True,
        True,
        True,
    )
    bi_dict = bi.to_dict()
    bi2 = BackendInfo.from_dict(bi_dict)

    assert bi == bi2


def test_serialization_ringarch() -> None:
    bi = BackendInfo(
        "name",
        "device_name",
        "version",
        RingArch(3),
        {OpType.CX, OpType.Rx},
        True,
        True,
        True,
    )
    bi_dict = bi.to_dict()
    bi2 = BackendInfo.from_dict(bi_dict)

    assert bi == bi2


def test_to_json() -> None:
    bi = BackendInfo(
        name="name",
        device_name="device_name",
        version="version",
        architecture=SquareGrid(3, 4),
        gate_set={OpType.CX, OpType.Rx},
        supports_fast_feedforward=True,
        supports_reset=True,
        supports_midcircuit_measurement=True,
        all_node_gate_errors={Node(0): {OpType.Rx: 0.1}},
        all_edge_gate_errors={(Node(0), Node(1)): {OpType.Rx: 0.1}},
        all_readout_errors={Node(0): [[0.1]]},
        averaged_node_gate_errors={Node(0): 0.1},
        averaged_edge_gate_errors={(Node(0), Node(1)): 0.1},
        averaged_readout_errors={Node(0): 0.1},
        misc={"region": "UK"},
    )
    bi_dict = bi.to_dict()
    json_bi = dumps(bi_dict)
    bi2 = BackendInfo.from_dict(loads(json_bi))

    assert bi == bi2


@given(st.backendinfo())
@settings(deadline=None)
def test_backendinfo_serialization(backinfo: BackendInfo) -> None:
    serializable = backinfo.to_dict()
    assert BackendInfo.from_dict(serializable) == backinfo
    assert loads(dumps(serializable)) == serializable


def test_fullyconnected() -> None:
    bi = fully_connected_backendinfo(
        "name", "device_name", "version", 10, {OpType.CX, OpType.Rx}
    )
    assert bi.n_nodes == 10
    assert isinstance(bi.architecture, FullyConnected)

    # https://github.com/quantinuum/tket/issues/390
    d = bi.to_dict()
    assert BackendInfo.from_dict(d) == bi
