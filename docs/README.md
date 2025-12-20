# ESP32 Smart Home Documentation

This directory contains the documentation source files for the ESP32 Smart Home project.

## Building Documentation Locally

### Prerequisites

```bash
# Install system dependencies (Ubuntu/Debian)
sudo apt-get install doxygen graphviz plantuml default-jre

# Install Python dependencies
pip install -r requirements.txt
```

### Build Steps

```bash
# 1. Generate Doxygen XML (from project root)
cd ..
doxygen Doxyfile

# 2. Build Sphinx HTML
cd docs
make html

# 3. View documentation
open _build/html/index.html  # macOS
xdg-open _build/html/index.html  # Linux
```

## Online Documentation

The documentation is automatically deployed to GitHub Pages on every push to `main`:
- **Sphinx Documentation**: https://sprchuoi.github.io/smart_home_idf/
- **Doxygen API Reference**: https://sprchuoi.github.io/smart_home_idf/doxygen/

## Structure

```
docs/
├── conf.py              # Sphinx configuration
├── index.rst           # Main documentation page
├── getting-started.rst # Setup and installation guide
├── architecture.rst    # System architecture
├── development.rst     # Development guide
├── deployment.rst      # Deployment instructions
├── api/                # API reference documentation
│   ├── index.rst
│   ├── audio.rst
│   ├── eventbus.rst
│   └── ...
├── _static/            # Static files (images, CSS)
└── _templates/         # Custom templates
```

## Writing Documentation

### reStructuredText Basics

```rst
Section Title
=============

Subsection
----------

**bold text**
*italic text*
``code``

.. code-block:: cpp

   void example() {
       // C++ code
   }
```

### Doxygen Integration

The documentation uses Breathe to integrate Doxygen-generated API docs:

```rst
.. doxygenclass:: ClassName
   :members:

.. doxygenfunction:: functionName
```

## Troubleshooting

### Doxygen XML not found

Make sure to run `doxygen Doxyfile` from the project root before building Sphinx docs.

### Breathe errors

Check that the `breathe_projects` path in `conf.py` points to the correct Doxygen XML output directory.

### Missing dependencies

Install all requirements: `pip install -r requirements.txt`
- `index.rst` - Main documentation index
- `getting-started.rst` - Getting started guide
- `architecture.rst` - Architecture documentation
- `api/` - API reference documentation
- `development.rst` - Development guide
- `deployment.rst` - Deployment guide

## GitHub Pages

Documentation is automatically deployed to GitHub Pages on push to main branch via `.github/workflows/docs.yml`.

