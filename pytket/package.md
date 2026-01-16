Pytket is a python module for interfacing with TKET, a quantum computing toolkit and optimising compiler developed by [Quantinuum](https://www.quantinuum.com/). In addition to pytket there are several pytket extension modules for accessing a range of quantum hardware and classical simulators. The extension modules also allow circuit conversion between several widely used quantum software tools including qiskit, cirq and pennylane.

The source code for the TKET compiler can be found in [this github repository](https://github.com/quantinuum/tket).

## Installation

Installation is supported for Linux, MacOS and Windows. Installation requires python 3.10+.

To install run the pip command: 

`` pip install pytket``

See [Installation troubleshooting](https://docs.quantinuum.com/tket/api-docs/install.html) for help with installation.

To install the pytket extension modules add a hyphen and the extension name to the command:

`` pip install pytket-quantinuum ``

For a list of pytket extensions see this page: https://docs.quantinuum.com/tket/api-docs/extensions.html.

_Warning._ There is a [known issue](https://github.com/quantinuum/tket/issues/926)
with installing pytket in a conda environment on MacOS: you may not be able to
install versions more recent then 1.11.0. The only known remedy is to use an
official Python distribution instead.

## Documentation and Examples

API reference: https://docs.quantinuum.com/tket/api-docs/

To get started using pytket see the [user guide](https://docs.quantinuum.com/tket/user-guide/).

## Support and Discussion

For bugs and feature requests we recommend creating an issue on the [github repository](https://github.com/quantinuum/tket).

User support: tket-support@quantinuum.com

For discussion, join the public slack channel [here](https://join.slack.com/t/tketusers/shared_invite/zt-3a7x4ots2-nZIUpOJ0cSnMM9E~oBm0xQ).

There is also a [pytket tag](https://quantumcomputing.stackexchange.com/questions/tagged/pytket) on quantum computing stack exchange.

Mailing list: join [here](https://list.cambridgequantum.com/cgi-bin/mailman/listinfo/tket-users).

## Citation

If you wish to cite TKET in any academic publications, we generally recommend citing our [software overview](https://arxiv.org/abs/2003.10611) paper for most cases.

If your work is on the topic of specific compilation tasks, it may be more appropriate to cite one of our other papers:

- "On the qubit routing problem" for qubit placement (a.k.a. allocation) and routing (a.k.a. swap network insertion, connectivity solving). https://arxiv.org/abs/1902.08091 .
- "Phase Gadget Synthesis for Shallow Circuits" for representing exponentiated Pauli operators in the ZX calculus and their circuit decompositions. https://arxiv.org/abs/1906.01734 .
- "A Generic Compilation Strategy for the Unitary Coupled Cluster Ansatz" for sequencing of terms in Trotterisation and Pauli diagonalisation. https://arxiv.org/abs/2007.10515 .
