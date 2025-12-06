# MAB 默认启用配置说明

## 概述

已将所有 xpgfuzz 相关的脚本配置为默认启用 Multi-Armed Bandit (MAB) 算法（`-b` 选项）。

## 修改的文件

### 1. 核心代码文件
- `xpgfuzz/afl-fuzz.c`: 添加了 `-b` 命令行选项支持
- `xpgfuzz/chat-llm.c`: 实现了 MAB 反馈机制
- `xpgfuzz/chat-llm.h`: 添加了相关函数声明

### 2. 执行脚本（已更新所有 9 个协议）

所有 `benchmark/subjects/*/run.sh` 文件都已更新，为 xpgfuzz 自动添加 `-b` 选项：

1. ✅ `benchmark/subjects/FTP/ProFTPD/run.sh`
2. ✅ `benchmark/subjects/FTP/PureFTPD/run.sh`
3. ✅ `benchmark/subjects/FTP/LightFTP/run.sh`
4. ✅ `benchmark/subjects/FTP/BFTPD/run.sh`
5. ✅ `benchmark/subjects/HTTP/Lighttpd1/run.sh`
6. ✅ `benchmark/subjects/SMTP/Exim/run.sh`
7. ✅ `benchmark/subjects/RTSP/Live555/run.sh`
8. ✅ `benchmark/subjects/SIP/Kamailio/run.sh`
9. ✅ `benchmark/subjects/DAAP/forked-daapd/run.sh`

## 修改方式

每个 `run.sh` 脚本都添加了条件判断：

```bash
# Add -b option for xpgfuzz to enable MAB by default
if $(strstr $FUZZER "xpgfuzz"); then
    timeout ... /home/ubuntu/${FUZZER}/afl-fuzz ... -b $OPTIONS ...
else
    timeout ... /home/ubuntu/${FUZZER}/afl-fuzz ... $OPTIONS ...
fi
```

## 效果

- **xpgfuzz**: 自动启用 MAB（`-b` 选项）
- **其他 fuzzer** (aflnet, chatafl 等): 不受影响，保持原有行为

## 使用说明

### 通过脚本运行（推荐）

使用现有的脚本运行，MAB 会自动启用：

```bash
# 使用 run.sh 脚本
./run.sh NUM_CONTAINERS TIMEOUT TARGET xpgfuzz
```

### 手动运行

如果需要手动运行，仍然可以显式指定 `-b` 选项：

```bash
./afl-fuzz -i input_dir -o output_dir -N tcp://127.0.0.1/8554 -P RTSP -b -- ./target
```

### 禁用 MAB

如果需要禁用 MAB（不推荐），可以修改 `run.sh` 脚本，移除 `-b` 选项，或者修改代码将默认值改为禁用。

## 验证

可以通过以下命令验证脚本是否正确配置：

```bash
# 检查所有 run.sh 是否包含 MAB 配置
find benchmark/subjects -name "run.sh" -exec grep -l "Add -b option for xpgfuzz" {} \;
```

应该看到 9 个文件都被列出。

## 注意事项

1. **重新构建 Docker 镜像**: 如果使用 Docker，需要重新构建镜像以包含更新后的脚本
2. **向后兼容**: 其他 fuzzer（aflnet, chatafl）的行为不受影响
3. **性能影响**: MAB 算法需要一些时间来学习，初期可能看不到明显效果

## 技术细节

- MAB 使用 UCB1 算法
- 反馈机制在检测到新覆盖率时自动更新
- 支持所有约束类型：INTEGER, STRING, ENUM, IP, PATH, HEX

