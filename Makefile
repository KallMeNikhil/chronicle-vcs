CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -Isrc -Itests
BUILD    ?= build
JOBS     ?= $(shell nproc 2>/dev/null || echo 4)

LIB_SRCS   := $(filter-out src/main.cpp,$(wildcard src/*/*.cpp src/*/*/*.cpp))
TEST_SRCS  := $(wildcard tests/unit/*.cpp)
BENCH_SRCS := $(wildcard bench/*.cpp)

LIB_OBJS   := $(LIB_SRCS:%.cpp=$(BUILD)/%.o)
TEST_OBJS  := $(TEST_SRCS:%.cpp=$(BUILD)/%.o)
BENCH_OBJS := $(BENCH_SRCS:%.cpp=$(BUILD)/%.o)

BIN        := $(BUILD)/chronicle
TEST_BIN   := $(BUILD)/unit_tests
BENCH_BIN  := $(BUILD)/chronicle-bench

.PHONY: all test unit integration bench asan clean

all: $(BIN) $(TEST_BIN)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libchronicle.a: $(LIB_OBJS)
	@rm -f $@
	ar rcs $@ $^

$(BIN): $(BUILD)/src/main.o $(BUILD)/libchronicle.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(TEST_BIN): $(TEST_OBJS) $(BUILD)/libchronicle.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BENCH_BIN): $(BENCH_OBJS) $(BUILD)/libchronicle.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

unit: $(TEST_BIN)
	./$(TEST_BIN)

integration: $(BIN)
	CHRONICLE=$(abspath $(BIN)) bash tests/integration/run_all.sh

test:
	$(MAKE) -j$(JOBS) all
	$(MAKE) unit
	$(MAKE) integration

bench:
	$(MAKE) -j$(JOBS) $(BENCH_BIN) $(BIN)
	mkdir -p bench/results
	./$(BENCH_BIN) bench/results

asan:
	$(MAKE) test BUILD=build-asan \
	  CXXFLAGS="-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" \
	  LDFLAGS="-fsanitize=address,undefined"

clean:
	rm -rf build build-asan

-include $(LIB_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(BENCH_OBJS:.o=.d) $(BUILD)/src/main.d
