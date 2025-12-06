# Protocol Fuzzing 使用文档

本文档说明如何使用协议模糊测试工具，包括镜像构建和测试运行。

## 目录

- [镜像命名规则](#镜像命名规则)
- [容器命名规则](#容器命名规则)
- [构建 Docker 镜像](#构建-docker-镜像)
- [运行 Fuzzing 测试](#运行-fuzzing-测试)
- [使用不同日期的镜像](#使用不同日期的镜像)
- [查看和管理容器](#查看和管理容器)
- [示例场景](#示例场景)
- [常见问题](#常见问题)

## 镜像命名规则

镜像命名格式：`xpg-月份-日-{协议}`

**示例：**
- `xpg-12-5-lighttpd1` - 12月5日构建的 lighttpd1 镜像
- `xpg-12-5-exim` - 12月5日构建的 exim 镜像
- `xpg-12-6-bftpd` - 12月6日构建的 bftpd 镜像

**说明：**
- 日期根据构建时的系统日期自动生成
- 月份和日期都是数字格式（无前导零）

## 容器命名规则

容器命名格式：`xpg-月份-日-{协议}-{fuzzer}-{序号}`

**示例：**
- `xpg-12-6-lighttpd1-xpgfuzz-1` - 12月6日运行的第1个容器
- `xpg-12-6-lighttpd1-xpgfuzz-2` - 12月6日运行的第2个容器
- `xpg-12-6-exim-aflnet-1` - 12月6日运行的 exim + aflnet 容器

**说明：**
- 日期根据运行时的系统日期自动生成
- 序号从1开始，用于区分同一批次运行的多个容器

## 构建 Docker 镜像

### 支持的协议

| 协议 | 脚本文件 | 说明 |
|------|---------|------|
| lighttpd1 | `setup_lighttpd1.sh` | HTTP 协议 |
| bftpd | `setup_bftpd.sh` | FTP 协议 |
| proftpd | `setup_proftpd.sh` | FTP 协议 |
| pure-ftpd | `setup_pureftpd.sh` | FTP 协议 |
| lightftp | `setup_lightftp.sh` | FTP 协议 |
| exim | `setup_exim.sh` | SMTP 协议 |
| live555 | `setup_live555.sh` | RTSP 协议 |
| kamailio | `setup_kamailio.sh` | SIP 协议 |
| forked-daapd | `setup_forked-daapd.sh` | DAAP 协议 |

### 构建命令

```bash
# 构建单个协议的镜像
./setup_lighttpd1.sh

# 构建多个协议的镜像
./setup_lighttpd1.sh
./setup_exim.sh
./setup_bftpd.sh
```

### 构建示例

假设今天是 **12月5日**，运行：
```bash
./setup_lighttpd1.sh
```

会构建镜像：`xpg-12-5-lighttpd1`

### 验证镜像

```bash
# 查看所有镜像
docker images | grep xpg-

# 查看特定日期的镜像
docker images | grep xpg-12-5

# 查看特定协议的镜像
docker images | grep lighttpd1
```

## 运行 Fuzzing 测试

### 基本语法

```bash
./run.sh NUM_CONTAINERS TIMEOUT TARGET FUZZER
```

### 参数说明

| 参数 | 说明 | 示例 |
|------|------|------|
| `NUM_CONTAINERS` | 要运行的容器数量 | `10` |
| `TIMEOUT` | 超时时间（分钟） | `1440` (24小时) |
| `TARGET` | 协议名称 | `lighttpd1`, `exim`, `all` |
| `FUZZER` | Fuzzer名称 | `xpgfuzz`, `aflnet`, `chatafl`, `all` |

### 支持的 Fuzzer

- `xpgfuzz` - XPG Fuzzer
- `aflnet` - AFLNet
- `chatafl` - ChatAFL

### 基本使用示例

```bash
# 运行单个协议和单个fuzzer
./run.sh 10 1440 lighttpd1 xpgfuzz

# 运行多个协议
./run.sh 10 1440 "lighttpd1,exim" xpgfuzz

# 运行多个fuzzer
./run.sh 10 1440 lighttpd1 "xpgfuzz,aflnet"

# 运行所有协议和所有fuzzer
./run.sh 10 1440 all all
```

### 运行示例

假设今天是 **12月6日**，运行：
```bash
./run.sh 10 1440 lighttpd1 xpgfuzz
```

会创建容器：
- `xpg-12-6-lighttpd1-xpgfuzz-1`
- `xpg-12-6-lighttpd1-xpgfuzz-2`
- ...
- `xpg-12-6-lighttpd1-xpgfuzz-10`

使用的镜像：`xpg-12-6-lighttpd1`（当前日期）

## 使用不同日期的镜像

### 场景说明

如果你在12月5日构建了镜像，但想在12月6日运行测试，可以使用 `IMAGE_DATE` 环境变量指定镜像日期。

### 使用方法

```bash
# 方式1：在命令前设置环境变量
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 xpgfuzz

# 方式2：先导出环境变量
export IMAGE_DATE=12-5
./run.sh 10 1440 lighttpd1 xpgfuzz

# 方式3：在同一行设置多个环境变量
IMAGE_DATE=12-5 SKIPCOUNT=5 ./run.sh 10 1440 lighttpd1 xpgfuzz
```

### 日期格式

- 格式：`MM-DD`（月份-日期）
- 示例：`12-5`（12月5日）、`1-15`（1月15日）

### 完整示例

```bash
# 1. 查看12月5日构建的镜像
docker images | grep xpg-12-5

# 2. 使用12月5日的镜像运行测试（12月6日运行）
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 xpgfuzz

# 3. 容器名会使用运行时的日期（12月6日）
#    镜像名会使用指定的日期（12月5日）
```

**结果：**
- 使用的镜像：`xpg-12-5-lighttpd1`（12月5日构建）
- 创建的容器：`xpg-12-6-lighttpd1-xpgfuzz-1`（12月6日运行）

## 查看和管理容器

### 查看容器状态

```bash
# 查看所有容器
docker ps -a

# 查看运行中的容器
docker ps

# 查看特定协议的容器
docker ps -a | grep lighttpd1

# 查看特定日期的容器
docker ps -a | grep xpg-12-6

# 查看特定fuzzer的容器
docker ps -a | grep xpgfuzz
```

### 查看容器日志

```bash
# 查看容器日志
docker logs <container_name>

# 实时查看日志
docker logs -f <container_name>

# 示例
docker logs xpg-12-6-lighttpd1-xpgfuzz-1
```

### 停止和删除容器

```bash
# 停止容器
docker stop <container_name>

# 删除容器
docker rm <container_name>

# 停止并删除容器
docker rm -f <container_name>

# 批量删除特定协议的容器
docker ps -a | grep xpg-12-6-lighttpd1 | awk '{print $1}' | xargs docker rm -f
```

### 查看镜像

```bash
# 查看所有镜像
docker images

# 查看特定日期的镜像
docker images | grep xpg-12-5

# 查看特定协议的镜像
docker images | grep lighttpd1

# 删除镜像
docker rmi <image_name>
```

## 示例场景

### 场景1：完整工作流程

```bash
# 1. 构建镜像（12月5日）
./setup_lighttpd1.sh
./setup_exim.sh

# 2. 验证镜像
docker images | grep xpg-12-5

# 3. 运行测试（12月5日）
./run.sh 10 1440 lighttpd1 xpgfuzz

# 4. 查看容器状态
docker ps -a | grep xpg-12-5-lighttpd1-xpgfuzz
```

### 场景2：使用旧镜像运行新测试

```bash
# 1. 12月5日构建镜像
./setup_lighttpd1.sh

# 2. 12月6日使用12月5日的镜像运行测试
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 xpgfuzz

# 3. 查看结果
docker ps -a | grep xpg-12-6-lighttpd1-xpgfuzz
```

### 场景3：批量测试多个协议

```bash
# 1. 构建所有需要的镜像
./setup_lighttpd1.sh
./setup_exim.sh
./setup_bftpd.sh

# 2. 运行所有协议和所有fuzzer
./run.sh 10 1440 all all

# 3. 查看所有容器
docker ps -a | grep xpg-
```

### 场景4：对比不同fuzzer

```bash
# 使用同一个镜像测试不同的fuzzer
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 xpgfuzz
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 aflnet
IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 chatafl
```

## 常见问题

### Q1: 如何知道镜像是否构建成功？

```bash
# 检查镜像是否存在
docker images | grep xpg-12-5-lighttpd1

# 如果存在，会显示镜像信息
# 如果不存在，说明构建失败，检查构建日志
```

### Q2: 运行测试时提示找不到镜像？

**原因：** 镜像名不匹配

**解决方案：**
1. 检查镜像是否存在：
   ```bash
   docker images | grep xpg-
   ```

2. 如果镜像日期不匹配，使用 `IMAGE_DATE` 指定正确的日期：
   ```bash
   IMAGE_DATE=12-5 ./run.sh 10 1440 lighttpd1 xpgfuzz
   ```

3. 如果镜像不存在，重新构建：
   ```bash
   ./setup_lighttpd1.sh
   ```

### Q3: 容器名冲突怎么办？

**原因：** 同一天多次运行相同协议和fuzzer

**解决方案：**
- 容器名会自动添加序号（-1, -2, ...），不会冲突
- 如果需要，可以手动删除旧容器：
  ```bash
  docker rm -f xpg-12-6-lighttpd1-xpgfuzz-1
  ```

### Q4: 如何查看测试结果？

测试结果会保存在 `benchmark/results-{协议名}/` 目录下：

```bash
# 查看结果目录
ls benchmark/results-lighttpd1/

# 结果文件格式
# out-lighttpd1-xpgfuzz_1.tar.gz
# out-lighttpd1-xpgfuzz_2.tar.gz
# ...
```

### Q5: 如何清理所有容器和镜像？

```bash
# 停止所有容器
docker stop $(docker ps -aq)

# 删除所有容器
docker rm $(docker ps -aq)

# 删除所有镜像（谨慎使用）
docker rmi $(docker images -q)
```

### Q6: 如何查看帮助信息？

```bash
# 查看 run.sh 的使用说明
./run.sh

# 会显示：
# Usage: ./run.sh NUM_CONTAINERS TIMEOUT TARGET FUZZER [IMAGE_DATE]
# ...
```

## 环境变量

| 变量名 | 说明 | 示例 |
|--------|------|------|
| `IMAGE_DATE` | 镜像日期（MM-DD格式） | `12-5` |
| `SKIPCOUNT` | 覆盖率计算间隔 | `1`, `5` |
| `TEST_TIMEOUT` | 测试超时时间（毫秒） | `5000` |

## 注意事项

1. **镜像必须先构建**：运行测试前，确保已使用对应的 setup 脚本构建了镜像
2. **日期格式**：`IMAGE_DATE` 使用 `MM-DD` 格式，不要使用前导零（如 `12-5` 而不是 `12-05`）
3. **容器名唯一性**：容器名使用运行时的日期，即使使用旧镜像，容器名也会反映运行日期
4. **资源管理**：大量容器会消耗系统资源，注意监控系统状态
5. **结果保存**：测试结果会自动保存，容器删除后结果仍然保留

## 联系和支持

如有问题，请检查：
1. Docker 是否正常运行：`docker ps`
2. 镜像是否存在：`docker images`
3. 脚本权限是否正确：`chmod +x *.sh`

