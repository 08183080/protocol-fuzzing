#!/bin/bash

# 测试 forked-daapd 服务器是否能接收消息的脚本
# 使用方法: ./test_forked-daapd.sh [host] [port]

HOST=${1:-"127.0.0.1"}
PORT=${2:-"3689"}

echo "=========================================="
echo "测试 forked-daapd 服务器连接"
echo "目标: $HOST:$PORT"
echo "=========================================="
echo

# 1. 检查端口是否开放
echo "[1/6] 检查端口 $PORT 是否开放..."
if nc -zv $HOST $PORT 2>&1 | grep -q "succeeded"; then
    echo "✓ 端口 $PORT 已开放"
else
    echo "✗ 端口 $PORT 未开放或无法连接"
    echo "  请确保服务器正在运行"
    exit 1
fi
echo

# 2. 检查端口监听状态
echo "[2/6] 检查端口监听状态..."
if netstat -tlnp 2>/dev/null | grep -q ":$PORT "; then
    echo "✓ 端口 $PORT 正在监听"
    netstat -tlnp 2>/dev/null | grep ":$PORT "
else
    echo "⚠ 无法确认端口监听状态（可能需要 root 权限）"
fi
echo

# 3. 使用 curl 发送 GET 请求
echo "[3/6] 发送 HTTP GET 请求..."
echo "请求: GET http://$HOST:$PORT/"
echo "---"
if curl -s -o /dev/null -w "HTTP状态码: %{http_code}\n响应时间: %{time_total}s\n" \
    --connect-timeout 5 \
    http://$HOST:$PORT/; then
    echo "✓ HTTP 请求成功"
else
    echo "✗ HTTP 请求失败"
fi
echo

# 4. 发送详细请求（显示响应头）
echo "[4/6] 发送详细 HTTP 请求（显示响应头）..."
echo "---"
curl -v -s --connect-timeout 5 http://$HOST:$PORT/ 2>&1 | head -20
echo

# 5. 使用 netcat 发送原始 HTTP 请求
echo "[5/6] 使用 netcat 发送原始 HTTP 请求..."
echo "---"
echo -e "GET / HTTP/1.1\r\nHost: $HOST\r\nConnection: close\r\n\r\n" | \
    timeout 5 nc $HOST $PORT 2>&1 | head -10
echo

# 6. 检查日志文件（如果存在）
echo "[6/6] 检查日志文件..."
LOG_FILE="/home/ubuntu/experiments/forked-daapd.log"
if [ -f "$LOG_FILE" ]; then
    echo "✓ 找到日志文件: $LOG_FILE"
    echo "最近 5 行日志:"
    tail -5 "$LOG_FILE" 2>/dev/null || echo "  无法读取日志文件"
else
    echo "⚠ 日志文件不存在: $LOG_FILE"
    echo "  可能路径不同，请手动检查"
fi
echo

echo "=========================================="
echo "测试完成！"
echo "=========================================="
echo
echo "提示："
echo "  - 如果所有测试都通过，说明服务器可以正常接收消息"
echo "  - 如果连接失败，请检查："
echo "    1. 服务器是否正在运行"
echo "    2. 端口是否正确（默认 3689）"
echo "    3. 防火墙是否阻止连接"
echo "  - 查看详细日志: tail -f $LOG_FILE"

