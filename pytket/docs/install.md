# Installation Troubleshooting

## General Installation Instructions

To install the core `pytket` package, run

```
pip install pytket
```

If the `pip` command is linked to a Python 2 distribution, you will need to replace it with `pip3` and use `python3` for running your code, or use `python -m pip`.

The additional modules `pytket-X` (where `X` is `qiskit`, `pyquil`, etc.) can be installed in the same way, as

```
pip install pytket-X
```

You can update your installation to the most recent version using

```
pip install --upgrade pytket
```

## Building TKET from source

TKET can be built from source by compiling the C++. This is now possible on MacOS, Windows and Linux (including ARM Linux).

For instructions on how to do this see the [tket repository README](https://github.com/quantinuum/tket#how-to-build-tket-and-pytket).

## Installation FAQs

### Is there a build of `pytket` for my system?

The core pytket package, as well as the separate extension modules are available on PyPI. Wheels are built to work on Linux, MacOS and Windows with Python versions 3.10+, and `pip` version 20.0.0+.

:::{note}
On M1-based Macs running in native (arm64) mode, this command may fail
because of an issue installing `scipy`. To fix this:

1. Install [brew](https://brew.sh/) (if you haven't already);
2. `brew install openblas`;
3. `pip install -U pip wheel`;
4. `OPENBLAS="$(brew --prefix openblas)" pip install scipy`;
5. `pip install pytket`
:::

### Can I build TKET from source without using conan?

Yes, this is possible. To do so this follow [this guide](https://github.com/quantinuum/tket/blob/develop/build-without-conan.md) .

### Do all versions of `pytket` work with Windows?

`pytket` versions 0.6 and above are compatible with most recent versions of Windows. LTSC versions of Windows are not supported and an [issue](https://github.com/quantinuum/pytket-docs/issues/36) has been reported with these.

### `pytket` installed but modules mentioned in the docs could not be found. Why?

Some of the modules concerning interaction with another software package (e.g. `qiskit`, `pyquil`, etc.) need to be installed separately.

Otherwise, this may be that the version of `pytket` you obtained is not the most recent version released. We only publish our docs for the most recent version of `pytket`.

### When I ran `pip install pytket`, I could only get an old version. What gives?

At a couple of points in the development of the software, we had to increase the system requirements. Obtaining an old version from PyPI is likely the result of that being the most recent version compliant with your system.

One possibility is that you are using an old version of `pip` that cannot accept the more recent Linux builds. Try running `pip install --upgrade pip` to upgrade it to the most recent version and upgrade `pytket`.

As of pytket release 1.24.0 installing the latest version of pytket requires python version 3.10 or above. If you have an older version of python then you will need to upgrade it to use the latest version of pytket and the extensions.

As of pytket release 1.34.0 MacOS 12 is no longer supported: please upgrade to MacOS 13 or above to use the latest version of pytket.

There is a [known issue](https://github.com/quantinuum/tket/issues/926) with
installing pytket in a conda environment on MacOS: you may not be able to
install versions more recent then 1.11.0. The only known remedy is to use an
official Python distribution instead.

### I've tried the recommended actions here and it still won't work! What can I do?

There is an [issue tracker](http://github.com/quantinuum/tket/issues) on github for current issues. You might find others who have had similar problems there. If not, feel free to add an issue describing your problem and our development team will try to diagnose it and get back to you as soon as possible.
