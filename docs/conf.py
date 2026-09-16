# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys

# Add project root to path for autodoc
sys.path.insert(0, os.path.abspath('..'))
sys.path.insert(0, os.path.abspath('../main'))

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ESP32 Smart Home'
copyright = '2024, ESP32 Smart Home Team'
author = 'ESP32 Smart Home Team'
release = '1.0.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'sphinx.ext.intersphinx',
    'sphinx.ext.todo',
    'myst_parser',
    'breathe',
]

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']
# html_logo = '_static/logo.png'
# html_favicon = '_static/favicon.ico'

# Theme options
# 'display_version' was removed -- modern sphinx_rtd_theme dropped it and
# warns "unsupported theme option" on every build, which now fails CI since
# docs are built with -W.
html_theme_options = {
    'logo_only': False,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    'style_nav_header_background': '#2980B9',
    # Toc options
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False
}

# Excluded from the built site:
#   README.md / DEPLOYMENT.md  -- README documents this directory rather than
#     the product, and DEPLOYMENT.md duplicates deployment.rst.
#   api/*.rst for removed components -- the EventBus, PowerManager,
#     WatchdogSupervisor, audio pipeline and OLED driver no longer exist. The
#     files remain in the repository as a record but would otherwise trip the
#     "not included in any toctree" warning, which fails CI (docs build with -W).
exclude_patterns = [
    'README.md',
    'DEPLOYMENT.md',
    'api/eventbus.rst',
    'api/powermanager.rst',
    'api/watchdog.rst',
    'api/audio.rst',
    'api/oled.rst',
    '_build',
    'Thumbs.db',
    '.DS_Store',
]

# -- Extension configuration -------------------------------------------------

# Napoleon settings for Google/NumPy style docstrings
napoleon_google_docstring = True
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = False
napoleon_include_private_with_doc = False
napoleon_include_special_with_doc = True
napoleon_use_admonition_for_examples = False
napoleon_use_admonition_for_notes = False
napoleon_use_admonition_for_references = False
napoleon_use_ivar = False
napoleon_use_param = True
napoleon_use_rtype = True

# Breathe settings (for Doxygen integration)
breathe_projects = {
    "smart_home": "../doxygen/xml"
}
breathe_default_project = "smart_home"

# Intersphinx mapping
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "esp-idf": ("https://docs.espressif.com/projects/esp-idf/en/latest/esp32/", None),
}


# Todo extension
todo_include_todos = True

# MyST Parser settings
myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "fieldlist",
    "html_admonition",
    "html_image",
    "linkify",
    "replacements",
    "smartquotes",
    "strikethrough",
    "substitution",
    "tasklist",
]

