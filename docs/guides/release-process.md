# Release Process Documentation

## Overview

This document outlines the release process for the Mamba repository. As a monorepo with multiple components, releases need to be coordinated across different parts of the system.

## Versioning Strategy

The repository follows semantic versioning (MAJOR.MINOR.PATCH) with optional pre-release identifiers.

- **MAJOR**: Incompatible API changes
- **MINOR**: Backward-compatible functionality additions
- **PATCH**: Backward-compatible bug fixes
- **PRE-RELEASE**: Optional identifiers like `alpha`, `beta`, `rc` for testing

## Release Components

Different components may have different release cycles:

1. **Core Mamba Package** (`mamba_ssm`): Released to PyPI
2. **Plugin Components**: May have separate distribution channels
3. **Applications**: Typically versioned with the core package or released independently

## Release Process

### 1. Preparation

```bash
# Ensure you're on the main branch and up to date
git checkout main
git pull origin main

# Run tests to ensure stability
./scripts/run-tests.sh all

# Check for any uncommitted changes
git status
```

### 2. Version Bump

Update version numbers in relevant files:
- `setup.py` (for Python package)
- Component-specific version files (if any)
- Documentation references

### 3. Changelog Update

Update `CHANGELOG.md` with:
- Summary of changes since last release
- New features, improvements, bug fixes
- Breaking changes (if any)
- Contributors

### 4. Create Release Tag

```bash
# Create annotated tag for the release
git tag -a vX.Y.Z -m "Release vX.Y.Z"

# Push tags to remote
git push origin vX.Y.Z
```

### 5. Build and Distribute

#### Python Package (mamba_ssm)

```bash
# Build distribution packages
python setup.py sdist bdist_wheel

# Upload to PyPI (requires credentials)
twine upload dist/*
```

#### C++ Components

C++ components are typically distributed through:
- Source code releases (tagged commits)
- Pre-built binaries for specific platforms
- Package managers (conda, vcpkg, etc.) - if applicable

### 6. Post-Release Activities

- Update documentation if needed
- Announce release through appropriate channels
- Monitor for issues and feedback
- Create maintenance branches if needed for long-term support

## Automated Releases

The repository uses GitHub Actions for automated builds:
- Workflows in `.github/workflows/` (in the mamba subdirectory)
- Releases triggered by pushing tags matching `v*`
- Automatic building and testing of tagged commits

## Release Signing

For security-sensitive distributions:
- Sign release tags with GPG
- Sign distribution packages
- Publish signatures alongside artifacts

## Maintenance

- Backport critical fixes to maintenance branches
- Clearly label pre-release versions
- Deprecate old versions with appropriate notice periods
