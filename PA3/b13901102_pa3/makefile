# 指定編譯器
CXX = g++

# 編譯參數
# -Isrc 確保能 include "src" 裡的 header
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -Isrc

# 設定路徑
SRC_DIR = src
BIN_DIR = bin
TARGET = $(BIN_DIR)/router

# 抓取 src 資料夾下的所有 .cpp 檔案
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

# 將 .cpp 路徑轉換為對應的 .o 路徑 (放在 bin 資料夾下)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BIN_DIR)/%.o)

# 預設目標
all: $(TARGET)

# 連結: 將所有 .o 檔連結成執行檔
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# 編譯: 將 src/%.cpp 編譯成 bin/%.o
$(BIN_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清除編譯檔案
clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean