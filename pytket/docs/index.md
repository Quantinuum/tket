# pytket

`pytket` is a python module for interfacing with tket, a quantum computing toolkit and optimising compiler developed by [Quantinuum]. We currently support circuits and device architectures from
[numerous providers](extensions.md), allowing the
tket tools to be used in conjunction with projects on their platforms.

`pytket` is available for Python 3.10+, on Linux, MacOS and Windows. To install,
run

```
pip install pytket
```

If you have issues installing `pytket` please visit the [installation troubleshooting](install.md) page.

To use `pytket`, you can simply import the appropriate modules into your python code or in an interactive Python notebook. We can build circuits directly using the `pytket` interface by creating a blank circuit and adding gates in the order we want to apply them.

See the [Getting Started](getting_started.md) page for a basic tutorial on using
`pytket`. To get more in depth on features, see the [pytket user guide](https://docs.quantinuum.com/tket/user-guide/) for an extensive introduction to `pytket` functionality and how to use it.

## Extensions

To use pytket in conjunction with other software libraries you must install a
separate python package for the relevant pytket extension.

Each extension adds either some new methods to the `pytket` package to convert between the circuit
representations, or some new backends to submit circuits to within `pytket`.

Extensions are separate python packages can be installed using `pip`. The installation command is `pip install pytket-X` where `X` is the name of the extension.

To install the `pytket-quantinuum` package use the following command.

```
pip install pytket-quantinuum
```

The extensions supported by tket are described
[here](extensions.md).

## How to cite

If you wish to cite tket in any academic publications, we generally recommend citing our [software overview paper](https://doi.org/10.1088/2058-9565/ab8e92) for most cases.

If your work is on the topic of specific compilation tasks, it may be more appropriate to cite one of our other papers:

- ["On the qubit routing problem"](https://doi.org/10.4230/LIPIcs.TQC.2019.5) for qubit placement (aka allocation, mapping) and routing (aka swap network insertion, connectivity solving).
- ["Phase Gadget Synthesis for Shallow Circuits"](https://doi.org/10.4204/EPTCS.318.13) for representing exponentiated Pauli operators in the ZX calculus and their circuit decompositions.
- ["A Generic Compilation Strategy for the Unitary Coupled Cluster Ansatz"](https://arxiv.org/abs/2007.10515) for sequencing of terms in Trotterisation and Pauli diagonalisation.

We are also keen for others to benchmark their compilation techniques against us. We recommend checking our [benchmark repository](https://github.com/quantinuum/tket_benchmarking) for examples on how to run basic benchmarks with the latest version of `pytket`. Please list the release version of `pytket` with any benchmarks you give, and feel free to get in touch for any assistance needed in setting up fair and representative tests.

## User Support

If you have problems with the use of tket or you think that you have found a bug there are several ways to contact us:

- You can write an issue on [github](https://github.com/quantinuum/tket/issues) with details of the problem and we will pick that up. Github issues are the preferred way to report bugs with tket or request features. You can also have a look on that page to see if your problem has already been reported by someone else.
- We have a slack channel for community discussion and support. You can join by following [this link](https://join.slack.com/t/tketusers/shared_invite/zt-3a7x4ots2-nZIUpOJ0cSnMM9E~oBm0xQ)
- Write an email to <mailto:tket-support@quantinuum.com> and ask for help with your problem.
- There is also a tag on [quantum computing stack exchange](https://quantumcomputing.stackexchange.com/questions/tagged/pytket) for questions relating to pytket.

We are really thankful for all help to fix bugs in tket. Usually you will get an answer from someone in the development team of tket soon.

## LICENCE

Licensed under the [Apache 2 License](http://www.apache.org/licenses/LICENSE-2.0).

```{toctree}
:caption: 'Overview:'
:maxdepth: 1

getting_started.rst
changelog.rst
install.rst
faqs.rst
```

```{toctree}
:caption: 'pytket extensions:'

extensions.md
```

```{toctree}
:caption: 'API Reference:'
:maxdepth: 2

backends.rst
circuit.rst
unit_id.rst
pauli.rst
passes.rst
predicates.rst
partition.rst
qasm.rst
quipper.rst
architecture.rst
circuit_library.rst
placement.rst
mapping.rst
tableau.rst
transform.rst
tailoring.rst
wasm.rst
zx.rst
utils.rst
logging.rst
config.rst
```

# Indices and tables

- {ref}`genindex`
- {ref}`modindex`
- {ref}`search`

```{eval-rst}
.. py:module:: pytket
```

```{eval-rst}
.. py:module:: pytket._version
```

```{eval-rst}
.. py:module:: pytket._tket
```

[quantinuum]: https://www.quantinuum.com/
