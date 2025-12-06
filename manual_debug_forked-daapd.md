# 手动运行和调试 forked-daapd 容器指南

## 1. 启动交互式容器

首先，确定你的镜像名称。镜像名称格式为：`xpg-{月份}-{日}-forked-daapd`

例如，如果是12月5日构建的镜像：
```bash
IMAGE_NAME="xpg-12-5-forked-daapd"
```

启动交互式容器：
```bash
docker run -it --name forked-daapd-debug --cap-add=SYS_PTRACE --security-opt seccomp=unconfined $IMAGE_NAME /bin/bash
```

参数说明：
- `-it`: 交互式终端
- `--name forked-daapd-debug`: 给容器起个名字，方便后续使用
- `--cap-add=SYS_PTRACE`: 允许使用调试工具（如 gdb）
- `--security-opt seccomp=unconfined`: 解除安全限制，方便调试

## 2. 在容器内启动必要的服务

forked-daapd 需要 dbus 和 avahi-daemon 服务：

```bash
# 启动 dbus
sudo /etc/init.d/dbus start

# 启动 avahi-daemon
sudo /etc/init.d/avahi-daemon start

# 验证服务状态
sudo /etc/init.d/dbus status
sudo /etc/init.d/avahi-daemon status
```

## 3. 运行 forked-daapd（正常模式）

进入工作目录并运行：

```bash
cd /home/ubuntu/experiments

# 直接运行 forked-daapd
./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

参数说明：
- `-d 0`: 调试级别 0（最详细）
- `-c ./forked-daapd.conf`: 指定配置文件
- `-f`: 前台运行（不后台运行）

## 4. 使用 GDB 调试 forked-daapd

### 4.1 启动 GDB

```bash
cd /home/ubuntu/experiments

# 使用 gdb 启动 forked-daapd
gdb --args ./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

### 4.2 常用 GDB 命令

在 GDB 提示符下：

```gdb
# 设置断点（例如在 main 函数）
(gdb) break main

# 运行程序
(gdb) run

# 继续执行
(gdb) continue

# 单步执行
(gdb) step

# 查看变量
(gdb) print variable_name

# 查看堆栈
(gdb) backtrace

# 查看所有线程
(gdb) info threads

# 切换到线程
(gdb) thread 2

# 退出 GDB
(gdb) quit
```

## 5. 使用 Valgrind 检测内存问题

```bash
cd /home/ubuntu/experiments

# 使用 valgrind 检测内存泄漏
valgrind --leak-check=full --show-leak-kinds=all ./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

## 6. 检查服务器是否能收到消息

### 6.1 检查端口是否在监听

在容器内检查服务器是否在监听端口 3689：

```bash
# 方法1: 使用 netstat
netstat -tlnp | grep 3689

# 方法2: 使用 ss
ss -tlnp | grep 3689

# 方法3: 使用 lsof
sudo lsof -i :3689

# 方法4: 使用 nc (netcat) 检查端口是否开放
nc -zv 127.0.0.1 3689
```

### 6.2 使用 curl 发送 HTTP 请求测试

**在容器内测试：**

```bash
# 基本 GET 请求
curl -v http://127.0.0.1:3689/

# 发送自定义 HTTP 请求
curl -v -X GET http://127.0.0.1:3689/ HTTP/1.1

# 发送带自定义头的请求
curl -v -H "User-Agent: TestClient" http://127.0.0.1:3689/

# 查看详细响应（包括响应头）
curl -i http://127.0.0.1:3689/
```

**从宿主机测试（需要端口映射）：**

```bash
# 如果启动容器时使用了 -p 3689:3689
curl -v http://localhost:3689/

# 或者使用容器IP
CONTAINER_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' forked-daapd-debug)
curl -v http://$CONTAINER_IP:3689/
```

### 6.3 使用 netcat (nc) 手动发送 HTTP 请求

```bash
# 连接到服务器并发送 HTTP 请求
echo -e "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc 127.0.0.1 3689

# 或者交互式发送
nc 127.0.0.1 3689
# 然后输入：
# GET / HTTP/1.1
# Host: localhost
# Connection: close
# 
# (空行表示请求结束，按两次回车)
```

### 6.4 使用 telnet 测试连接

```bash
# 使用 telnet 连接（如果已安装）
telnet 127.0.0.1 3689

# 连接后输入 HTTP 请求：
# GET / HTTP/1.1
# Host: localhost
# Connection: close
# 
# (空行结束请求)
```

### 6.5 使用 tcpdump 抓包查看网络流量

在容器内（需要 root 权限或使用 sudo）：

```bash
# 抓取所有到端口 3689 的流量
sudo tcpdump -i any -n port 3689 -v

# 抓取并保存到文件
sudo tcpdump -i any -n port 3689 -w /tmp/forked-daapd.pcap

# 查看保存的包
sudo tcpdump -r /tmp/forked-daapd.pcap -v
```

### 6.6 实时查看日志确认收到消息

在服务器运行的终端，你应该能看到日志输出。或者：

```bash
# 在另一个终端实时查看日志
tail -f /home/ubuntu/experiments/forked-daapd.log

# 或者查看系统日志
tail -f /var/log/forked-daapd.log
```

### 6.7 使用 aflnet-replay 工具测试（如果可用）

如果容器内有 aflnet-replay 工具，可以使用测试用例：

```bash
cd /home/ubuntu/experiments

# 使用 aflnet-replay 重放测试用例
# 格式: aflnet-replay <testcase> <protocol> <port> <timeout> <wait_time>
/home/ubuntu/aflnet/aflnet-replay in-daap/xxx HTTP 3689 100 10000

# 或者如果有 afl-replay
/home/ubuntu/aflnet/afl-replay in-daap/xxx HTTP 3689
```

### 6.8 完整的测试流程示例

**终端1 - 启动服务器：**
```bash
cd /home/ubuntu/experiments
sudo /etc/init.d/dbus start
sudo /etc/init.d/avahi-daemon start
./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

**终端2 - 测试连接（在容器内或从宿主机）：**
```bash
# 检查端口
netstat -tlnp | grep 3689

# 发送测试请求
curl -v http://127.0.0.1:3689/

# 查看日志确认
tail -f /home/ubuntu/experiments/forked-daapd.log
```

**终端3 - 抓包监控（可选）：**
```bash
sudo tcpdump -i any -n port 3689 -v
```

### 6.9 使用自动化测试脚本

项目根目录提供了一个自动化测试脚本 `test_forked-daapd.sh`，可以快速检查服务器状态：

```bash
# 在容器内运行（默认测试 127.0.0.1:3689）
cd /home/ubuntu/experiments
/path/to/test_forked-daapd.sh

# 或者指定主机和端口
/path/to/test_forked-daapd.sh 127.0.0.1 3689

# 从宿主机测试（如果端口已映射）
./test_forked-daapd.sh localhost 3689
```

脚本会自动执行以下检查：
1. 端口连通性测试
2. 端口监听状态检查
3. HTTP GET 请求测试
4. 详细 HTTP 请求（显示响应头）
5. 原始 HTTP 请求（使用 netcat）
6. 日志文件检查

### 6.10 常见问题排查

**问题1: 连接被拒绝 (Connection refused)**
```bash
# 检查服务器是否真的在运行
ps aux | grep forked-daapd

# 检查端口是否监听
netstat -tlnp | grep 3689
```

**问题2: 没有看到日志输出**
```bash
# 检查日志文件权限
ls -l /home/ubuntu/experiments/forked-daapd.log

# 检查配置文件中的日志路径
grep logfile /home/ubuntu/experiments/forked-daapd.conf

# 尝试提高日志级别（在配置文件中）
# loglevel = debug  # 或 spam
```

**问题3: 服务器启动但无法连接**
```bash
# 检查防火墙规则（容器内通常没有防火墙）
sudo iptables -L

# 检查是否有其他进程占用端口
sudo lsof -i :3689
```

## 7. 查看日志

```bash
# 查看 forked-daapd 日志
tail -f /home/ubuntu/experiments/forked-daapd.log

# 或者
tail -f /var/log/forked-daapd.log
```

## 8. 使用 strace 跟踪系统调用

```bash
cd /home/ubuntu/experiments

# 跟踪所有系统调用
strace -f -o /tmp/strace.log ./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f

# 只跟踪网络相关调用
strace -e trace=network -f ./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

## 9. 端口映射（从宿主机访问容器）

如果需要从宿主机访问容器内的服务，启动容器时添加端口映射：

```bash
docker run -it -p 3689:3689 --name forked-daapd-debug --cap-add=SYS_PTRACE --security-opt seccomp=unconfined $IMAGE_NAME /bin/bash
```

然后就可以从宿主机访问：`http://localhost:3689/`

## 10. 快速调试脚本

创建一个调试脚本 `debug.sh`：

```bash
#!/bin/bash
cd /home/ubuntu/experiments

# 启动服务
sudo /etc/init.d/dbus start
sudo /etc/init.d/avahi-daemon start

# 使用 GDB 启动
gdb --args ./forked-daapd/src/forked-daapd -d 0 -c ./forked-daapd.conf -f
```

在容器内运行：
```bash
chmod +x debug.sh
./debug.sh
```

## 注意事项

1. **ASAN 选项**：容器内已设置 ASAN_OPTIONS，如果遇到崩溃，ASAN 会自动检测并报告
2. **权限问题**：确保使用 `ubuntu` 用户运行，该用户有 sudo 权限
3. **配置文件**：配置文件位于 `/home/ubuntu/experiments/forked-daapd.conf`
4. **工作目录**：所有实验文件在 `/home/ubuntu/experiments/` 目录下

