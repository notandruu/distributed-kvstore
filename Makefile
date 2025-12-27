CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O2 -I src
LDFLAGS = -lpthread

BUILD_DIR = build
SRC_DIR = src

CORE_SRCS = \
	$(SRC_DIR)/common/protocol.cpp \
	$(SRC_DIR)/common/logger.cpp \
	$(SRC_DIR)/storage/kv_store.cpp \
	$(SRC_DIR)/storage/wal.cpp \
	$(SRC_DIR)/network/connection.cpp \
	$(SRC_DIR)/network/tcp_server.cpp \
	$(SRC_DIR)/network/tcp_client.cpp \
	$(SRC_DIR)/threading/thread_pool.cpp \
	$(SRC_DIR)/replication/replication_manager.cpp \
	$(SRC_DIR)/server/config.cpp \
	$(SRC_DIR)/server/node.cpp \
	$(SRC_DIR)/client/kv_client.cpp

BENCH_SRCS = \
	$(SRC_DIR)/bench/bench_main.cpp \
	$(SRC_DIR)/bench/load_generator.cpp \
	$(SRC_DIR)/bench/stats.cpp

CORE_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))
SERVER_OBJ = $(BUILD_DIR)/main.o
BENCH_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(BENCH_SRCS))

.PHONY: all clean

all: $(BUILD_DIR)/kvstore-server $(BUILD_DIR)/kvstore-bench

$(BUILD_DIR)/kvstore-server: $(CORE_OBJS) $(SERVER_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/kvstore-bench: $(CORE_OBJS) $(BENCH_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
