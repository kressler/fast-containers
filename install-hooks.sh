#!/bin/bash
#
# Installation script for git hooks
#
# This script installs the pre-commit hook that automatically formats
# C++ files with clang-format before committing.
#
# Usage: ./install-hooks.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOKS_DIR="$SCRIPT_DIR/hooks"
GIT_HOOKS_DIR="$SCRIPT_DIR/.git/hooks"

echo "Installing git hooks..."

# Check if .git/hooks directory exists
if [ ! -d "$GIT_HOOKS_DIR" ]; then
    echo "Error: .git/hooks directory not found."
    echo "Are you running this from the repository root?"
    exit 1
fi

# Install pre-commit hook
if [ -f "$HOOKS_DIR/pre-commit" ]; then
    echo "  Installing pre-commit hook..."
    cp "$HOOKS_DIR/pre-commit" "$GIT_HOOKS_DIR/pre-commit"
    chmod +x "$GIT_HOOKS_DIR/pre-commit"
    echo "  ✓ pre-commit hook installed"
else
    echo "  Warning: hooks/pre-commit not found"
fi

echo ""
echo "Git hooks installed successfully!"
echo ""
echo "The pre-commit hook will automatically format C++ files with clang-format"
echo "before each commit. Make sure clang-format is installed on your system."
echo ""
echo "To bypass the hook for a specific commit, use: git commit --no-verify"
