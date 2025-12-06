检查 xpgfuzz 中 MAB 算法是否已启用。搜索代码中的相关实现和配置。


[2 tools called]


检查 MAB 算法的实际使用情况，确认是否已启用：


[3 tools called]


检查 MAB 反馈更新是否被调用：


[2 tools called]


检查 afl-fuzz.c 中是否调用了 MAB 反馈更新函数：


[2 tools called]


检查 afl-fuzz.c 中调用 `mutate_value_by_constraint` 的位置，确认是否更新了 MAB 奖励：


[2 tools called]


检查 `save_if_interesting` 函数，确认是否在那里更新 MAB 奖励：


[2 tools called]


查看 `save_if_interesting` 函数的完整实现：


[1 tool called]


## MAB算法状态

**部分启用**：MAB 已实现并在变异算子选择中使用，但反馈更新未集成。

### 已实现的部分

1. MAB 核心实现（`chat-llm.c`）：
   - UCB1 算法实现
   - 为 INTEGER、STRING、ENUM 三种约束类型分别创建 MAB 实例
   - `mab_select_arm()` 用于选择变异算子

2. 在变异操作中使用（`chat-llm.c` 第2028-2041行、2093-2106行、2199-2210行）：
   ```c
   // 使用MAB选择变异算子
   multi_armed_bandit_t *mab = get_mab_for_constraint(CONSTRAINT_INTEGER);
   if (mab) {
       mutation_type = (int)mab_select_arm(mab);
   }
   ```

### 缺失的部分

反馈更新未集成：`mab_update_last_mutation_reward()` 函数已定义，但在 `afl-fuzz.c` 中未被调用。

- 在 `save_if_interesting()` 中检测到新覆盖时未调用
- 在 `fuzz_one()` 中执行测试用例后未调用

### 影响

- MAB 会进行初始探索（未尝试过的算子会被优先选择）
- 无法根据历史表现学习，无法优化选择策略
- 效果等同于随机选择，无法发挥 MAB 的自适应优势

### 建议

需要在 `save_if_interesting()` 或 `fuzz_one()` 中添加反馈更新调用，才能让 MAB 算法完全启用并发挥作用。

需要我帮您添加反馈更新的集成代码吗？