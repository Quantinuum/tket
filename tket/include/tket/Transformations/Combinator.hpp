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

#include "Transform.hpp"
#include "tket_export.h"

namespace tket {

namespace Transforms {

// compose transforms in sequence
// sequences return true if any transform made a change, even if it was
// overwritten later
TKET_EXPORT Transform sequence(std::vector<Transform> &tvec);

// repeats a transform until it makes no changes (returns false)
TKET_EXPORT Transform repeat(const Transform &trans);

// repeats a transform and stops when the metric stops decreasing
Transform repeat_with_metric(
    const Transform &trans, const Transform::Metric &eval);

TKET_EXPORT Transform
repeat_while(const Transform &cond, const Transform &body);

}  // namespace Transforms

}  // namespace tket
