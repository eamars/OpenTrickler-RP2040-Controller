# Host unit tests for the powder analysis tuner.
#   make -C tests/powder_analysis test

CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g

SRC_DIR := ../../src
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/test_powder_analysis_tuner

SOURCES := test_powder_analysis_tuner.cpp simulated_powder.cpp $(SRC_DIR)/powder_analysis_tuner.cpp
HEADERS := simulated_powder.h $(SRC_DIR)/powder_analysis_tuner.h $(SRC_DIR)/powder_analysis_config.h

.PHONY: all test clean

all: $(TEST_BIN)

$(TEST_BIN): $(SOURCES) $(HEADERS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -o $@ $(SOURCES)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
