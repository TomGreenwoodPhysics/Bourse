# Bourse — build (Phase 1: pure C++ core, no external deps)
CXX      ?= g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude -Itests
BUILD    := build

ENGINE_SRC := src/order_book.cpp
TEST_SRC   := tests/test_main.cpp tests/test_order_book.cpp tests/test_replay.cpp
BENCH_SRC  := tests/bench.cpp

.PHONY: all test bench clean
all: test

$(BUILD):
	@mkdir -p $(BUILD)

# Build and run the correctness suite.
test: $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) $(TEST_SRC) -o $(BUILD)/tests
	./$(BUILD)/tests

# Build and run the throughput benchmark (optional arg: order count).
bench: $(BUILD)
	$(CXX) $(CXXFLAGS) $(ENGINE_SRC) $(BENCH_SRC) -o $(BUILD)/bench
	./$(BUILD)/bench

clean:
	rm -rf $(BUILD)