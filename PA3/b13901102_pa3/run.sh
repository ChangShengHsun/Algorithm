#!/bin/bash

# 定義顏色
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
CYAN='\033[1;36m'
RESET='\033[0m'

# ==============================================================================
# 0. 自動編譯 (Make)
# ==============================================================================
echo -e "${CYAN}[0/5] Compiling source code...${RESET}"
make clean > /dev/null 2>&1  # 先清除舊的，確保乾淨編譯 (可選)
make

# 檢查編譯是否成功 ($? 為上一個指令的回傳值，0 代表成功)
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Compilation Failed! Please check your code.${RESET}"
    exit 1
fi
echo -e "${GREEN}Compilation Successful!${RESET}\n"

# ==============================================================================
# 1. 執行測試案例
# ==============================================================================
mkdir -p outputs

# 標題列
echo "--------------------------------------------------------------------------------"
printf "%-8s | %-15s | %-12s | %-12s | %-10s\n" "Case" "Status" "Overflow" "Cost" "Time"
echo "--------------------------------------------------------------------------------"

for i in {1..6}; do
    CAP="./inputs/case${i}.cap"
    NET="./inputs/case${i}.net"
    OUT="./outputs/case${i}.route"
    
    # 檢查輸入檔是否存在
    if [ ! -f "$CAP" ]; then
        printf "%-8s | %b%-15s%b | %-12s | %-12s | %-10s\n" "Case $i" "$YELLOW" "NO INPUT" "$RESET" "-" "-" "-"
        continue
    fi

    # 執行 Router 並計時
    # { time cmd; } 2>&1 用於捕捉 time 的輸出
    TIME_LOG=$( { time ./bin/router --cap $CAP --net $NET --out $OUT > /dev/null; } 2>&1 )
    RET_CODE=$?

    # 提取執行時間 (格式如 0m0.123s)
    REAL_TIME=$(echo "$TIME_LOG" | grep "real" | awk '{print $2}')
    if [ -z "$REAL_TIME" ]; then REAL_TIME="-"; fi

    # 預設狀態
    STATUS_TXT="-"
    STATUS_COLOR=$RESET
    OVERFLOW_TXT="-"
    OVERFLOW_COLOR=$RESET
    COST_TXT="-"

    if [ $RET_CODE -ne 0 ]; then
        STATUS_TXT="ROUTER ERROR"
        STATUS_COLOR=$RED
    else
        # 呼叫 Python Evaluator 進行評分
        # 注意：這裡假設 pa3_evaluator.py 在 utilities 資料夾下，或與 run.sh 同層
        # 如果你的 evaluator 檔名是 common_eval.py，請自行修改下行
        EVAL_MSG=$(python3 ./utilities/pa3_evaluator.py $CAP $NET $OUT 2>&1)

        # 判斷結果
        if echo "$EVAL_MSG" | grep -q "All routes are valid" && echo "$EVAL_MSG" | grep -q "All nets are properly connected"; then
            STATUS_TXT="PASS"
            STATUS_COLOR=$GREEN
        else
            STATUS_TXT="FAIL"
            STATUS_COLOR=$RED
        fi

        # 抓取數值
        OVERFLOW_TXT=$(echo "$EVAL_MSG" | grep "Total Overflow" | awk '{print $3}')
        COST_TXT=$(echo "$EVAL_MSG" | grep "Total Cost" | awk '{print $3}')

        # 若 Overflow > 0 顯示黃色警告
        if [[ "$OVERFLOW_TXT" =~ ^[0-9]+$ ]] && [ "$OVERFLOW_TXT" -gt 0 ]; then
            OVERFLOW_COLOR=$YELLOW
        fi
    fi

    # 輸出表格列
    printf "%-8s | %b%-15s%b | %b%-12s%b | %-12s | %-10s\n" "Case $i" "$STATUS_COLOR" "$STATUS_TXT" "$RESET" "$OVERFLOW_COLOR" "$OVERFLOW_TXT" "$RESET" "$COST_TXT" "$REAL_TIME"

done

echo "--------------------------------------------------------------------------------"