# pytket.passes

In pytket, compilation passes perform in-place transformations of circuits. From a user's point of view, passes are similar to {py:class}`~.Transform` s; however passes allow for additional predicate checking and compositionality.

There are passes such as {py:meth}`~.passes.FullPeepholeOptimise` and {py:meth}`~.passes.KAKDecomposition` which are designed for general purpose circuit optimisation.

Also there are special purpose passes such as {py:meth}`~.passes.OptimisePhaseGadgets` and {py:meth}`~.passes.PauliSimp` which perform optimisation by targeting phase gadget and Pauli gadget structures within circuits. For more on these optimisation techniques see the [corresponding publication](https://arxiv.org/abs/1906.01734).

Rebase passes can be used to convert a circuit to a desired gateset. See {py:meth}`~.passes.RebaseCustom` and {py:meth}`~.passes.AutoRebase`.

For more on pytket passes see the [compilation](https://docs.quantinuum.com/tket/user-guide/manual/manual_compiler.html) section of the user manual or the [notebook tutorials](https://docs.quantinuum.com/tket/user-guide)

```{eval-rst}
.. currentmodule:: pytket.passes
```

```{eval-rst}
.. automodule:: pytket.passes
```

```{eval-rst}
.. automodule:: pytket._tket.passes
```

```{eval-rst}
.. autoclass:: pytket.passes.BasePass

   .. automethod:: __init__
   .. automethod:: apply
   .. automethod:: from_dict
   .. automethod:: get_gate_set
   .. automethod:: get_postconditions
   .. automethod:: get_preconditions
   .. automethod:: to_dict
```

```{eval-rst}
.. autoenum:: pytket.passes.CNotSynthType
```

```{eval-rst}
.. autoclass:: pytket.passes.RepeatPass

   .. automethod:: __init__
   .. automethod:: get_pass
```

```{eval-rst}
.. autoclass:: pytket.passes.RepeatUntilSatisfiedPass

   .. automethod:: __init__
   .. automethod:: get_pass
   .. automethod:: get_predicate
```

```{eval-rst}
.. autoclass:: pytket.passes.RepeatWithMetricPass

   .. automethod:: __init__
   .. automethod:: get_metric
   .. automethod:: get_pass
```

```{eval-rst}
.. autoenum:: pytket.passes.SafetyMode
```

```{eval-rst}
.. autoclass:: pytket.passes.SequencePass

   .. automethod:: __init__
   .. automethod:: get_sequence
   .. automethod:: to_dict
```

```{eval-rst}
.. automethod:: pytket.passes.AASRouting
```

```{eval-rst}
.. automethod:: pytket.passes.AutoRebase
```

```{eval-rst}
.. automethod:: pytket.passes.AutoSquash
```

```{eval-rst}
.. automethod:: pytket.passes.CXMappingPass
```

```{eval-rst}
.. automethod:: pytket.passes.CliffordPushThroughMeasures
```

```{eval-rst}
.. automethod:: pytket.passes.CliffordResynthesis
```

```{eval-rst}
.. automethod:: pytket.passes.CliffordSimp
```

```{eval-rst}
.. automethod:: pytket.passes.CnXPairwiseDecomposition
```

```{eval-rst}
.. automethod:: pytket.passes.CommuteThroughMultis
```

```{eval-rst}
.. automethod:: pytket.passes.ComposePhasePolyBoxes
```

```{eval-rst}
.. automethod:: pytket.passes.ContextSimp
```

```{eval-rst}
.. automethod:: pytket.passes.CustomPass
```

```{eval-rst}
.. automethod:: pytket.passes.CustomPassMap
```

```{eval-rst}
.. automethod:: pytket.passes.CustomRoutingPass
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeArbitrarilyControlledGates
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeBoxes
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeClassicalExp
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeMultiQubitsCX
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeSingleQubitsTK1
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeSwapsToCXs
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeSwapsToCircuit
```

```{eval-rst}
.. automethod:: pytket.passes.DecomposeTK2
```

```{eval-rst}
.. automethod:: pytket.passes.DefaultMappingPass
```

```{eval-rst}
.. automethod:: pytket.passes.DelayMeasures
```

```{eval-rst}
.. automethod:: pytket.passes.EulerAngleReduction
```

```{eval-rst}
.. automethod:: pytket.passes.FlattenRegisters
```

```{eval-rst}
.. automethod:: pytket.passes.FlattenRelabelRegistersPass
```

```{eval-rst}
.. automethod:: pytket.passes.FullMappingPass
```

```{eval-rst}
.. automethod:: pytket.passes.FullPeepholeOptimise
```

```{eval-rst}
.. automethod:: pytket.passes.GreedyPauliSimp
```

```{eval-rst}
.. automethod:: pytket.passes.GuidedPauliSimp
```

```{eval-rst}
.. automethod:: pytket.passes.KAKDecomposition
```

```{eval-rst}
.. automethod:: pytket.passes.NaivePlacementPass
```

```{eval-rst}
.. automethod:: pytket.passes.NormaliseTK2
```

```{eval-rst}
.. automethod:: pytket.passes.OptimisePhaseGadgets
```

```{eval-rst}
.. automethod:: pytket.passes.PauliExponentials
```

```{eval-rst}
.. automethod:: pytket.passes.PauliSimp
```

```{eval-rst}
.. automethod:: pytket.passes.PauliSquash
```

```{eval-rst}
.. automethod:: pytket.passes.PeepholeOptimise2Q
```

```{eval-rst}
.. automethod:: pytket.passes.PlacementPass
```

```{eval-rst}
.. automethod:: pytket.passes.RebaseCustom
```

```{eval-rst}
.. automethod:: pytket.passes.RebaseTket
```

```{eval-rst}
.. automethod:: pytket.passes.RemoveBarriers
```

```{eval-rst}
.. automethod:: pytket.passes.RemoveDiscarded
```

```{eval-rst}
.. automethod:: pytket.passes.RemoveImplicitQubitPermutation
```

```{eval-rst}
.. automethod:: pytket.passes.RemovePhaseOps
```

```{eval-rst}
.. automethod:: pytket.passes.RemoveRedundancies
```

```{eval-rst}
.. automethod:: pytket.passes.RenameQubitsPass
```

```{eval-rst}
.. automethod:: pytket.passes.RoundAngles
```

```{eval-rst}
.. automethod:: pytket.passes.RoutingPass
```

```{eval-rst}
.. automethod:: pytket.passes.SimplifyInitial
```

```{eval-rst}
.. automethod:: pytket.passes.SimplifyMeasured
```

```{eval-rst}
.. automethod:: pytket.passes.SquashCustom
```

```{eval-rst}
.. automethod:: pytket.passes.SquashRzPhasedX
```

```{eval-rst}
.. automethod:: pytket.passes.SquashTK1
```

```{eval-rst}
.. automethod:: pytket.passes.SynthesiseTK
```

```{eval-rst}
.. automethod:: pytket.passes.SynthesiseTket
```

```{eval-rst}
.. automethod:: pytket.passes.ThreeQubitSquash
```

```{eval-rst}
.. automethod:: pytket.passes.ZXGraphlikeOptimisation
```

```{eval-rst}
.. automethod:: pytket.passes.ZZPhaseToRz
```

```{eval-rst}
.. automodule:: pytket.passes.combinecondpass

.. autofunction:: pytket.passes.combinecondpass.CombineCondPass
```

## pytket.passes.resizeregpass

```{eval-rst}
.. automodule:: pytket.passes.resizeregpass
```

```{eval-rst}
.. autofunction:: pytket.passes.resizeregpass.scratch_reg_resize_pass
```

## pytket.passes.passselector

```{eval-rst}
.. automodule:: pytket.passes.passselector
```

```{eval-rst}
.. autoclass:: pytket.passes.passselector.PassSelector

   .. automethod:: __init__
   .. automethod:: apply
   .. automethod:: get_scores
```

## pytket.passes.script

```{eval-rst}
.. automodule:: pytket.passes.script
    :members: compilation_pass_from_script, compilation_pass_grammar
```
