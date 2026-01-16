# Contributing to TKET

Pull requests are welcome. To make a PR, first fork the repo, make your proposed
changes on the `main` branch, and open a PR from your fork. If it passes
tests and is accepted after review, it will be merged in.

When adding a new feature, please add tests for it. When fixing a bug, please
add a test that demonstrates the fix.

If you make a change to one of the libraries in the `libs` directory, please
increase the version number and make a PR with that change only: the component
will then be tested on the CI, and on merge to `main` the new version will be
uploaded. Then it will be possible to update conan requirements to use the new
version.

A new version of TKET is uploaded to our conan repo with each push to `main`
that changes the core library. This process is managed by CI workflows. If you
are making changes only to TKET tests or pytket, you do not need to build TKET
locally: the right version should be downloaded automatically from the conan
repo.

## Code style

### C++

C++20 features may be used whenever they are supported by all the compilers
used on the CI (listed in the README).

All C++ code should be formatted with `clang-format` (v21) using the
configuration file `.clang-format` in the root directory. This is checked on
the CI. The script `do-clang-format` will run this over all C++ files in the
repository and fix them up.

You can mark sections of code with `// clang-format off` and
`// clang-format on` to tell the tool to skip them, e.g. when it is helpful to
have data laid out in a certain way.

In other matters of style, please try to follow the
[Google style guide](https://google.github.io/styleguide/cppguide.html).

Declarations in header files should have Doxygen-style documentation,
sufficient to make it clear what each object is and what each method does.

The macro `TKET_ASSERT(...)` is used to check logical correctness of the code
(_not_ to catch situations that can occur through user error). The condition is
always checked, even in Release builds, so please avoid using it with
conditions that incur significant computational overhead. Please place
`TKET_ASSERT(...);` on a line containing no other code; then the coverage
checker will ignore it when calculating line and branch coverage statistics.

### Python

#### Formatting

All code should be formatted using
[ruff](https://docs.astral.sh/ruff/formatter/), with default options. This is
checked on the CI.

#### Type annotation

On the CI, [mypy](https://mypy.readthedocs.io/en/stable/) is used as a static
type checker and all submissions must pass its checks. You should therefore run
`mypy` locally on any changed files before submitting a PR. The following
command will perform all required checks:

```shell
cd pytket
mypy --config-file=mypy.ini -p pytket -p tests
```

#### Linting

We use [ruff](https://github.com/astral-sh/ruff) on the CI to check compliance with a set of style requirements (listed in `ruff.toml`).
You should run `ruff` over any changed files before submitting a PR, to catch any issues.

An easy way to meet all formatting and linting requirements is to issue `pre-commit run --all-files`.

## Version numbers

If you make any changes in `tket/src`, you should bump the version number of
`tket` in the `TKET_VERSION` file at the root of this repo. This will
automatically bump the version of `tket` used in `pytket`.
Follow the "semantic versioning" convention: any backwards-incompatible changes
to the C++ API require a major version bump; new API features that maintain
backwards compatibility require a minor version bump; internal improvements and
bugfixes require a patch version bump.

## Test coverage

### tket

The code coverage of the `tket` tests is reported
[here](https://quantinuum.github.io/tket/tket/test-coverage/index.html). This report
is generated weekly from the `main` branch.

The libraries' coverage (from their own unit tests) is also reported: for
example [tklog](https://quantinuum.github.io/tket/tket/tklog-coverage/index.html).
(For other libraries, just replace "tklog" with the library name in the URL.)

In both cases, PRs to `main` check that the coverage has not decreased, and
merging is blocked until the coverage is at least as good as before.

### pytket

The code coverage of the `pytket` tests is reported
[here](https://quantinuum.github.io/tket/pytket/test-coverage/index.html). This report
reflects the coverage of the `main` branch, and is updated with every push.
The same report can be found in XML format
[here](https://quantinuum.github.io/tket/pytket/test-coverage/cov.xml).

Lines and branch coverage results are also checked with every PR to `main`.
