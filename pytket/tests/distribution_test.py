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

from collections import Counter

import pytest
from numpy import isclose

from pytket.utils import (
    EmpiricalDistribution,
    ProbabilityDistribution,
    convex_combination,
)


def test_probability_distribution() -> None:
    pd = ProbabilityDistribution({"a": 1 / 3, "b": 2 / 3})
    with pytest.raises(ValueError):
        pd = ProbabilityDistribution({"a": 1 / 3, "b": 1, "c": -1 / 3})
    pd1 = ProbabilityDistribution(
        {"a": 1 / 3, "b": 2 / 3 - 1e-15, "c": 1e-15}, min_p=1e-10
    )
    assert pd1.support == {"a", "b"}
    assert isclose(pd1["a"], 1 / 3)
    assert pd1["c"] == 0.0
    assert pd == pd1
    with pytest.warns(UserWarning):
        _ = ProbabilityDistribution({"a": 2 / 3, "b": 4 / 3})
    pd3 = ProbabilityDistribution({"a": 2 / 9, "b": 4 / 9, "c": 1 / 3})
    pd4 = convex_combination([(pd, 1 / 3), (pd3, 2 / 3)])
    assert pd4 == ProbabilityDistribution({"a": 7 / 27, "b": 14 / 27, "c": 2 / 9})


def test_empirical_distribution() -> None:
    ed1 = EmpiricalDistribution(Counter({"a": 1, "b": 2}))
    ed2 = EmpiricalDistribution(Counter({"b": 1, "c": 3, "d": 0}))
    assert ed2.support == {"b", "c"}
    assert ed2["b"] == 1
    assert ed2["d"] == 0
    ed3 = EmpiricalDistribution(Counter({"a": 2, "c": 0, "e": 4}))
    ed4 = ed1 + ed2 + ed3
    assert ed4 == EmpiricalDistribution(Counter({"a": 3, "b": 3, "c": 3, "e": 4}))

    ed0: EmpiricalDistribution[str] = EmpiricalDistribution(Counter())
    with pytest.raises(ValueError):
        pd6 = ProbabilityDistribution.from_empirical_distribution(ed0)
    pd6 = ProbabilityDistribution.from_empirical_distribution(ed4)
    assert isclose(pd6.as_dict()["a"], ed4.as_counter()["a"] / ed4.total)

    rv, X = pd6.as_rv_discrete()
    for i, x in enumerate(X):
        assert isclose(pd6[x], rv.pmf(i))


def test_marginalization() -> None:
    ed = EmpiricalDistribution(Counter({(0, 0): 3, (0, 1): 2, (1, 0): 4, (1, 1): 0}))
    ed0_ = ed.condition(lambda x: x[0] == 0)
    ed1_ = ed.condition(lambda x: x[0] == 1)
    ed_0 = ed.condition(lambda x: x[1] == 0)
    ed_1 = ed.condition(lambda x: x[1] == 1)
    assert ed0_.total == 5
    assert ed1_.total == 4
    assert ed_0.total == 7
    assert ed_1.total == 2

    pd = ProbabilityDistribution.from_empirical_distribution(ed)
    pd0 = pd.condition(lambda x: x[0] == x[1])
    pd1 = pd.condition(lambda x: x[0] != x[1])
    assert pd0.support == {(0, 0)}
    assert isclose(pd1[(0, 1)], 1 / 3)


def test_representation() -> None:
    ed = EmpiricalDistribution(Counter({"a": 1, "b": 3, 7: 0, (1, 1): 3}))
    assert ed == eval(repr(ed))

    pd = ProbabilityDistribution({"a": 1 / 7, "b": 2 / 7, 7: 0, (1, 1): 4 / 7})
    assert pd == eval(repr(pd))


def test_mapping() -> None:
    ed = EmpiricalDistribution(Counter({(0, 0): 3, (0, 1): 2, (1, 0): 4, (1, 1): 0}))
    ed0 = ed.map(lambda x: x[0])
    ed1 = ed.map(lambda x: x[1])
    assert ed0 == EmpiricalDistribution(Counter({0: 5, 1: 4}))
    assert ed1 == EmpiricalDistribution(Counter({0: 7, 1: 2}))

    pd = ProbabilityDistribution({(0, 0): 0.3, (0, 1): 0.3, (1, 0): 0.4, (1, 1): 0.0})
    pd0 = pd.map(sum)
    assert pd0 == ProbabilityDistribution({0: 0.3, 1: 0.7})


def test_expectation_and_variance() -> None:
    ed = EmpiricalDistribution(Counter({(0, 0): 3, (0, 1): 2, (1, 0): 4, (1, 1): 0}))
    assert isclose(ed.sample_mean(sum), 2 / 3)
    assert isclose(ed.sample_variance(sum), 1 / 4)
    pd = ProbabilityDistribution.from_empirical_distribution(ed)
    assert isclose(pd.expectation(sum), 2 / 3)
    assert isclose(pd.variance(sum), 2 / 9)
