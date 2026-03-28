CXX       := g++
CXX_FLAGS := -std=c++11 -g -O0
INCLUDE   := ./include
SRC       := ./src
TARGET    := test_program

all: $(TARGET)

$(TARGET): $(SRC)/impl.cpp $(SRC)/main.cpp
	$(CXX) -I$(INCLUDE) $^ -o $@ $(CXX_FLAGS)

clean:
	-rm -f $(TARGET)
