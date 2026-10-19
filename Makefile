CXXFLAGS = -std=c++17 -O3 -march=native -Wall -Wextra -pthread

build/kl: src/main.cpp src/kl.hpp | build
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp

build/test: tests/test.cpp src/kl.hpp | build
	$(CXX) $(CXXFLAGS) -o $@ tests/test.cpp

test: build/test
	./build/test

build:
	mkdir -p build

.PHONY: test
