#!/usr/bin/env bash

# make clean
make

# if [[ $(uname) == "Darwin" ]]; then
#     WARN="-Wno-writable-strings -Wno-extern-c-compat"
# else
#     WARN="-Wno-write-strings -Wno-extern-c-compat"
# fi

# LIB=include
# SRC=src
# OBJ=objs

# rm ${OBJ}/*.o

# g++ -o ${OBJ}/lsp_symbol.o              -I ./${LIB} -c ${SRC}/lsp_symbol.cpp              -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)
# g++ -o ${OBJ}/lsp_goto_declaration.o    -I ./${LIB} -c ${SRC}/lsp_goto_declaration.cpp    -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)
# g++ -o ${OBJ}/lsp_goto_definition.o     -I ./${LIB} -c ${SRC}/lsp_goto_definition.cpp     -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)
# g++ -o ${OBJ}/lsp_goto_implementation.o -I ./${LIB} -c ${SRC}/lsp_goto_implementation.cpp -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)
# g++ -o ${OBJ}/lsp_find_references.o     -I ./${LIB} -c ${SRC}/lsp_find_references.cpp     -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)

# g++ -o lsp_symbol_menu.so -I ./${LIB} \
# ${OBJ}/lsp_symbol.o              \
# ${OBJ}/lsp_goto_declaration.o         \
# ${OBJ}/lsp_goto_definition.o          \
# ${OBJ}/lsp_goto_implementation.o      \
# ${OBJ}/lsp_find_references.o          \
# ${SRC}/lsp_symbol_menu.cpp -std=c++11 ${WARN} ${LINK} $(yed --print-cppflags --print-ldflags)

# g++                                \
# -o lsp_symbol_menu.so              \
# -I ./${LIB}                        \
# lsp_symbol_menu.cpp                \
# ${SRC}/lsp_symbol.cpp              \
# ${SRC}/lsp_goto_declaration.cpp    \
# ${SRC}/lsp_goto_definition.cpp     \
# ${SRC}/lsp_goto_implementation.cpp \
# ${SRC}/lsp_find_references.cpp     \
# -std=c++11                         \
# ${WARN}                            \
# ${LINK}                            \
# $(yed --print-cppflags --print-ldflags)
