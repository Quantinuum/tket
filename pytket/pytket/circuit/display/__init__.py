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

"""Display a circuit as html."""

import json
import os
import tempfile
import time
import uuid
import webbrowser
from dataclasses import dataclass, field
from typing import Any, Literal, cast

from jinja2 import Environment, FileSystemLoader, PrefixLoader, nodes
from jinja2.ext import Extension
from jinja2.parser import Parser
from jinja2.utils import markupsafe

from pytket.circuit import Circuit
from pytket.config import PytketExtConfig


# js scripts to be loaded must not be parsed as template files.
class IncludeRawExtension(Extension):
    tags = {"include_raw"}  # noqa: RUF012

    def parse(self, parser: Parser) -> nodes.Output:
        lineno = parser.stream.expect("name:include_raw").lineno
        template = parser.parse_expression()
        result = self.call_method("_render", [template], lineno=lineno)
        return nodes.Output([result], lineno=lineno)

    def _render(self, filename: str) -> markupsafe.Markup:
        if self.environment.loader is not None:
            return markupsafe.Markup(
                self.environment.loader.get_source(self.environment, filename)[0]
            )
        return markupsafe.Markup("")


# Set up jinja to access our templates
dirname = os.path.dirname(__file__)

# Define the base loaders.
html_loader = FileSystemLoader(searchpath=os.path.join(dirname, "static"))
js_loader = FileSystemLoader(searchpath=os.path.join(dirname, "js"))

loader = PrefixLoader(
    {
        "html": html_loader,
        "js": js_loader,
    }
)

jinja_env = Environment(loader=loader, extensions=[IncludeRawExtension])


def _use_vue_dev_build() -> bool:
    """Whether to load the development build of Vue.

    During development Vue provides a number of features that improve the
    debugging experience but are useless (and add a small performance overhead)
    in production:

    - warnings for common errors and pitfalls;
    - props / events validation;
    - reactivity debugging hooks;
    - devtools integration.

    The production build (dist files ending in ``.prod.js``) is pre-minified
    with all of these development-only code branches removed. Set the
    ``PYTKET_CIRCUIT_RENDERER_DEV`` environment variable to a truthy value to
    load the development build instead; otherwise the production build is used.

    See https://vuejs.org/guide/best-practices/production-deployment for
    details.
    """
    return os.environ.get("PYTKET_CIRCUIT_RENDERER_DEV", "").lower() in (
        "1",
        "true",
        "yes",
        "on",
    )

RenderCircuit = dict[str, str | float | dict] | Circuit
Orientation = Literal["row"] | Literal["column"]


@dataclass(kw_only=True)
class RenderOptions:
    zx_style: bool | None = None  # display zx style gates where possible.
    condense_c_bits: bool | None = None  # collapse classical bits into a single wire.
    recursive: bool | None = None  # display nested circuits inline.
    condensed: bool | None = None  # display circuit on one line only.
    dark_theme: bool | None = None  # use dark mode.
    system_theme: bool | None = None  # use the system theme mode (overrides dark mode).
    transparent_bg: bool | None = None  # transparent circuit background.
    crop_params: bool | None = None  # shorten parameter expressions for display.
    interpret_math: bool | None = (
        None  # try to display parameters and box names as math.
    )

    def __post_init__(self) -> None:
        self.ALLOWED_RENDER_OPTIONS = {
            "zx_style": "zxStyle",
            "condense_c_bits": "condenseCBits",
            "recursive": "recursive",
            "condensed": "condensed",
            "dark_theme": "darkTheme",
            "system_theme": "systemTheme",
            "transparent_bg": "transparentBg",
            "crop_params": "cropParams",
            "interpret_math": "interpretMath",
        }

    def get_render_options(
        self, full: bool = False, _for_js: bool = False
    ) -> dict[str, bool]:
        """
        Get a dict of the current render options.

        :param full: whether to list all available options, even if not set.
        :param _for_js: Whether to convert options to js-compatible format,
            for internal use only.
        """
        return {
            (js_key if _for_js else key): self.__getattribute__(key)
            for key, js_key in self.ALLOWED_RENDER_OPTIONS.items()
            if full or self.__getattribute__(key) is not None
        }


@dataclass(kw_only=True)
class CircuitDisplayConfig(PytketExtConfig):
    ext_dict_key = "circuit_display"

    # Layout options
    min_height: str = "400px"
    min_width: str = "500px"
    orient: Orientation | None = None
    render_options: RenderOptions = field(default_factory=RenderOptions)

    @classmethod
    def from_extension_dict(cls, ext_dict: dict[str, Any]) -> "CircuitDisplayConfig":
        min_h = ext_dict.get("min_height")
        min_w = ext_dict.get("min_width")
        return CircuitDisplayConfig(
            min_height=str(min_h) if min_h is not None else "400px",
            min_width=str(min_w) if min_w is not None else "500px",
            orient=ext_dict.get("orient"),
            render_options=RenderOptions(
                **(ext_dict["render_options"] if "render_options" in ext_dict else {})  # noqa: SIM401
            ),
        )


class CircuitRenderer:
    """Class to manage circuit rendering within a given jinja2 environment."""

    config: CircuitDisplayConfig

    def __init__(self, env: Environment, config: CircuitDisplayConfig):
        self.env = env
        self.config = config

    def set_render_options(self, **kwargs: bool | str) -> None:
        """
        Set rendering defaults.

        :param min_height: str, initial height of circuit display.
        :param min_width: str, initial width of circuit display.
        :param orient: 'row' | 'column', stacking direction for multi-circuit display.
        :param zx_style: bool, display zx style gates where possible.
        :param condense_c_bits: bool, collapse classical bits into a single wire.
        :param recursive: bool, display nested circuits inline.
        :param condensed: bool, display circuit on one line only.
        :param dark_theme: bool, use dark mode.
        :param system_theme: bool, use the system theme mode.
        :param transparent_bg: bool, remove the circuit background.
        :param crop_params: bool, shorten parameter expressions for display.
        :param interpret_math: bool, try to render params and box names as math.
        """
        for key, val in kwargs.items():
            if key in self.config.render_options.ALLOWED_RENDER_OPTIONS and (
                isinstance(val, bool) or val is None
            ):
                self.config.render_options.__setattr__(key, val)
            if key in ["min_height", "min_width", "orient"] and isinstance(val, str):
                self.config.__setattr__(key, val)

    def get_render_options(
        self, full: bool = False, _for_js: bool = False
    ) -> dict[str, bool]:
        """
        Get a dict of the current render options.

        :param full: whether to list all available options, even if not set.
        :param _for_js: Whether to convert options to js-compatible format,
            for internal use only.
        """
        return self.config.render_options.get_render_options(full, _for_js)

    def save_render_options(self) -> None:
        """Save the current render options to pytket config."""
        self.config.update_default_config_file()

    def render_circuit_as_html(
        self,
        circuit: RenderCircuit | list[RenderCircuit],
        jupyter: bool = False,
        orient: Orientation | None = None,
    ) -> str | None:
        """
        Render a circuit as HTML for inline display.

        :param circuit: the circuit(s) to render.
        :param jupyter: set to true to render generated HTML in cell output.
        :param orient: the direction in which to stack circuits if multiple are present.
            One of 'row' or 'column'.
        """
        circuit_dict: dict | list[dict]
        if isinstance(circuit, list):
            circuit_dict = [
                (
                    circ.to_dict()
                    if isinstance(circ, Circuit)
                    else Circuit.from_dict(circ).to_dict()
                )
                for circ in circuit
            ]
        else:
            circuit_dict = (
                circuit.to_dict()
                if isinstance(circuit, Circuit)
                else Circuit.from_dict(circuit).to_dict()
            )

        uid = uuid.uuid4()
        html_template = self.env.get_template("html/circuit.html")
        html = html_template.render(
            {
                "circuit_json": json.dumps(circuit_dict),
                "uid": uid,
                "jupyter": jupyter,
                "display_options": json.dumps(self.get_render_options(_for_js=True)),
                "min_height": self.config.min_height,
                "min_width": self.config.min_width,
                "view_format": orient or self.config.orient,
                "vue_dev": _use_vue_dev_build(),
            }
        )
        if jupyter:
            # If we are in a notebook, we can tell jupyter to display the html.
            # We don't import at the top in case we are not in a notebook environment.
            from IPython.display import (  # noqa: PLC0415
                HTML,
                display,
            )

            display(HTML(html))
            return None
        return html

    def render_circuit_jupyter(
        self,
        circuit: RenderCircuit | list[RenderCircuit],
        orient: Orientation | None = None,
    ) -> None:
        """Render a circuit as jupyter cell output.

        :param circuit: the circuit(s) to render.
        :param orient: the direction in which to stack circuits if multiple are present.
        """
        self.render_circuit_as_html(circuit, True, orient=orient)

    def view_browser(
        self,
        circuit: RenderCircuit | list[RenderCircuit],
        browser_new: int = 2,
        sleep: int = 5,
    ) -> None:
        """Write circuit render html to a tempfile and open in browser.

        Waits for some time for browser to load then deletes tempfile.

        :param circuit: the Circuit(s) or serialized Circuit(s) to render.
            Either a single circuit or a list of circuits to compare.
        :param browser_new: ``new`` parameter to ``webbrowser.open``, default 2.
        :param sleep: Number of seconds to sleep before deleting file, default 5.

        """

        fp = tempfile.NamedTemporaryFile(  # noqa: SIM115
            mode="w", suffix=".html", delete=False, dir=os.getcwd()
        )
        try:
            fp.write(cast("str", self.render_circuit_as_html(circuit)))
            fp.close()

            webbrowser.open("file://" + os.path.realpath(fp.name), new=browser_new)

            # give browser enough time to open before deleting file
            time.sleep(sleep)
        finally:
            os.remove(fp.name)


def get_circuit_renderer(config: CircuitDisplayConfig | None = None) -> CircuitRenderer:
    """
    Get a configurable instance of the circuit renderer.
    :param config: CircuitDisplayConfig to control the default render options.
    """
    if config is None:
        config = CircuitDisplayConfig.from_default_config_file()

    return CircuitRenderer(jinja_env, config)


# Export the render functions scoped to the default jinja environment.
_default_circuit_renderer = get_circuit_renderer()
render_circuit_as_html = _default_circuit_renderer.render_circuit_as_html
render_circuit_jupyter = _default_circuit_renderer.render_circuit_jupyter
view_browser = _default_circuit_renderer.view_browser
