#!/usr/bin/env bash
# Builds the test program and generates compile_commands.json for the LSP server.
# Run this once before opening test files in yed.

make clean
bear -- make
