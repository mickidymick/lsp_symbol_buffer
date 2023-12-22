# Makefile for Writing Make Files Example

# *****************************************************
# Variables to control Makefile operation

ifeq ($(uname),"Darwin")
	WARN="-Wno-writable-strings -Wno-extern-c-compat"
else
	WARN="-Wno-write-strings -Wno-extern-c-compat"
endif

CXX        := g++
CXX_FLAGS  := -std=c++11 ${WARN} ${LINK}
YED_FLAGS  := $(shell yed --print-cppflags --print-ldflags)
BIN        := .
SRC        := ./src
INCLUDE    := ./include
EXECUTABLE := lsp_symbol_menu.so

all: $(BIN)/$(EXECUTABLE)

$(BIN)/$(EXECUTABLE): $(SRC)/*.cpp
	@echo "🚧 Building..."
	$(CXX) -I$(INCLUDE) $^ -o $@ $(CXX_FLAGS) $(YED_FLAGS)

clean:
	@echo "🧹 Clearing..."
	-rm $(BIN)/$(EXECUTABLE)
