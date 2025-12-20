# Documentation Deployment Guide

## Overview

The ESP32 Smart Home project uses a dual documentation system:
- **Sphinx** - User and developer guides (reStructuredText)
- **Doxygen** - API reference (C++ code comments)

Both are automatically deployed to GitHub Pages on every push to `main`.

## Live Documentation

- **Main Docs**: https://sprchuoi.github.io/smart_home_idf/
- **API Reference**: https://sprchuoi.github.io/smart_home_idf/doxygen/

## Local Development

### Quick Preview

```bash
cd docs
./preview.sh
```

This will:
1. Generate Doxygen XML/HTML
2. Build Sphinx HTML
3. Start a local web server on http://localhost:8000

### Manual Build

```bash
# Generate Doxygen
doxygen Doxyfile

# Build Sphinx
cd docs
make html

# View output
open _build/html/index.html
```

## GitHub Pages Deployment

### Automatic Deployment

The `.github/workflows/docs.yml` workflow automatically:
1. Triggers on pushes to `main` branch
2. Builds both Sphinx and Doxygen docs
3. Combines them into a single site structure
4. Deploys to GitHub Pages

### Site Structure

```
GitHub Pages Root (https://sprchuoi.github.io/smart_home_idf/)
├── index.html              # Sphinx main page
├── getting-started.html    # Sphinx pages
├── architecture.html
├── api/                    # Sphinx API docs
└── doxygen/               # Doxygen API reference
    └── index.html
```

### Manual Deployment

If you need to manually deploy:

```bash
# Build documentation
doxygen Doxyfile
cd docs && make html && cd ..

# Create deployment structure
mkdir -p _site
cp -r docs/_build/html/* _site/
cp -r doxygen/html _site/doxygen
touch _site/.nojekyll

# Push to gh-pages branch (if not using GitHub Actions)
# This is usually handled automatically by the workflow
```

## Configuration

### GitHub Repository Settings

1. Go to **Settings** > **Pages**
2. Set **Source** to: `GitHub Actions`
3. The workflow will handle the rest

### Workflow Triggers

The documentation builds on:
- Push to `main` branch when files in `docs/`, `main/`, or `Doxyfile` change
- Manual trigger via GitHub Actions UI

## Troubleshooting

### Build Fails

Check the GitHub Actions logs:
1. Go to **Actions** tab
2. Click on the failed workflow run
3. Expand the failed step to see errors

### Doxygen Warnings

Ensure all C++ files have proper documentation:
```cpp
/**
 * @brief Brief description
 * @param param_name Parameter description
 * @return Return value description
 */
```

### Sphinx Warnings

- Check RST syntax in `.rst` files
- Ensure all referenced files exist
- Verify Breathe can find Doxygen XML

### Pages Not Updating

1. Check GitHub Actions completed successfully
2. Wait a few minutes for GitHub Pages to update
3. Clear browser cache
4. Check repository Pages settings

## Writing Documentation

### Sphinx (User Guides)

Edit `.rst` files in `docs/`:
- Use reStructuredText format
- Follow existing structure
- Run local build to preview

### Doxygen (API Reference)

Add comments to C++ code:
```cpp
/**
 * @file FileName.h
 * @brief File description
 */

/**
 * @class ClassName
 * @brief Class description
 */
class ClassName {
public:
    /**
     * @brief Method description
     * @param name Parameter description
     * @return Return description
     */
    bool methodName(const char* name);
};
```

## Dependencies

### System Packages
- doxygen
- graphviz
- plantuml
- default-jre (for PlantUML)

### Python Packages
See `docs/requirements.txt`:
- sphinx
- sphinx_rtd_theme
- breathe
- myst-parser

## Maintenance

### Updating Theme

Edit `docs/conf.py`:
```python
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    # Theme options
}
```

### Adding New Pages

1. Create `.rst` file in `docs/`
2. Add to `toctree` in `index.rst`
3. Build and test locally
4. Commit and push

### Updating Doxygen

Edit `Doxyfile` in project root to change:
- Input directories
- Output format
- Exclusion patterns
