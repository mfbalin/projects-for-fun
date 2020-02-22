.PHONY = all clean

CXX = g++

INC = #-Iinclude

CXX_FLAGS = -std=c++2a -O3 -ggdb3 -Wall -Wextra -pedantic -march=native# -fopt-info-vec-optimized#-fopt-info-vec-missed
LINKER_FLAGS = -pthread

all: main

main: concurrent_trie.cpp
	$(CXX) $(INC) $(CXX_FLAGS) concurrent_trie.cpp -o main $(LINKER_FLAGS)

clean:
	rm -f main
