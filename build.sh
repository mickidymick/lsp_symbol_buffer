#!/usr/bin/env bash

if [[ $(uname) == "Darwin" ]]; then
    WARN="-Wno-writable-strings -Wno-extern-c-compat"
else
    WARN="-Wno-write-strings -Wno-extern-c-compat"
fi

LIB=include

g++ -o lsp_symbol_menu.so -I ./${LIB} lsp_symbol_menu.cpp -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)
