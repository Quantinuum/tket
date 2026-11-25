# pytket.mapping

Routing methods are tried in the order supplied to
{py:meth}`~.mapping.MappingManager.route_circuit`. To handle circuits containing
boxes, decompose them before routing or include
{py:class}`~.mapping.BoxDecompositionRoutingMethod` in that list. For example,
it can follow {py:class}`~.mapping.LexiLabellingMethod` and
{py:class}`~.mapping.LexiRouteRoutingMethod` as a fallback for undecomposed boxes.
If no method can handle the current subcircuit, routing raises an error.

```{eval-rst}
.. currentmodule:: pytket.mapping
```

```{eval-rst}
.. automodule:: pytket.mapping
```

```{eval-rst}
.. automodule:: pytket._tket.mapping
```

```{eval-rst}
.. autoclass:: pytket.mapping.AASLabellingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.AASRouteRoutingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.BoxDecompositionRoutingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.LexiLabellingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.LexiRouteRoutingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.MappingManager

   .. automethod:: __init__
   .. automethod:: route_circuit
```

```{eval-rst}
.. autoclass:: pytket.mapping.MultiGateReorderRoutingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.RoutingMethod

   .. automethod:: __init__
```

```{eval-rst}
.. autoclass:: pytket.mapping.RoutingMethodCircuit

   .. automethod:: __init__
```
