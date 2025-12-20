# Documentation

This directory contains the Sphinx documentation source files.

## Building Documentation

```bash
# Install dependencies
pip install -r requirements.txt

# Build HTML
make html

# View documentation
open _build/html/index.html
```

## Structure

- `conf.py` - Sphinx configuration
- `index.rst` - Main documentation index
- `getting-started.rst` - Getting started guide
- `architecture.rst` - Architecture documentation
- `api/` - API reference documentation
- `development.rst` - Development guide
- `deployment.rst` - Deployment guide

## GitHub Pages

Documentation is automatically deployed to GitHub Pages on push to main branch via `.github/workflows/docs.yml`.

