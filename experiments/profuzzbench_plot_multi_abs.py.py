#!/usr/bin/env python3
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import string

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['Noto Sans CJK SC', 'SimHei']  # 两个都写上，哪个有就用哪个
plt.rcParams['axes.unicode_minus'] = False  # 解决负号显示为方块的问题

def main(csv_files, puts, runs, cut_off, step, out_file, fuzzers):
    all_mean_list = []

    # === 读取与计算平均值 ===
    for csv_file, subject in zip(csv_files, puts):
        df = pd.read_csv(csv_file)
        mean_list = []

        for fuzzer in fuzzers:
            fuzzer = fuzzer.lower()
            cov_type = 'b_abs'  # 只画绝对边覆盖率

            df1 = df[(df['subject'] == subject) &
                     (df['fuzzer'] == fuzzer) &
                     (df['cov_type'] == cov_type)]
            if df1.empty:
                continue

            mean_list.append((subject, fuzzer, cov_type, 0, 0.0))
            for time in range(1, cut_off + 1, step):
                cov_total = 0
                run_count = 0
                for run in range(1, runs + 1):
                    df2 = df1[df1['run'] == run]
                    try:
                        start = df2.iloc[0, 0]
                        df3 = df2[df2['time'] <= start + time * 60]
                        cov_total += df3.tail(1).iloc[0, 5]
                        run_count += 1
                    except Exception:
                        print(f"[{subject}] Issue with run {run}, skipping.")
                mean_list.append((subject, fuzzer, cov_type, time, cov_total / max(run_count, 1)))

        mean_df = pd.DataFrame(mean_list, columns=['subject', 'fuzzer', 'cov_type', 'time', 'cov'])
        all_mean_list.append(mean_df)

    # === 合并所有协议 ===
    combined_df = pd.concat(all_mean_list, ignore_index=True)

    # === 绘图布局：每行 2 个图 ===
    n = len(puts)
    ncols = 2
    nrows = (n + 1) // 2
    fig, axes = plt.subplots(nrows, ncols, figsize=(12, 5 * nrows))
    fig.subplots_adjust(hspace=0.4, wspace=0.25)
    fig.suptitle("Edge Coverage (Absolute) Comparison", fontsize=18)

    fontsize_label = 14
    fontsize_tick = 12
    legend_fontsize = 12

    # === 绘制每个协议 ===
    for i, subject in enumerate(puts):
        row, col = divmod(i, ncols)
        ax = axes[row][col] if nrows > 1 else axes[col]

        df_sub = combined_df[combined_df['subject'] == subject]
        for fuzzer in fuzzers:
            fuzzer_lower = fuzzer.lower()
            grp = df_sub[(df_sub['fuzzer'] == fuzzer_lower) &
                         (df_sub['cov_type'] == 'b_abs')]
            if grp.empty:
                continue
            ax.plot(grp['time'], grp['cov'], label=fuzzer, marker='o', markersize=3, linewidth=1.2)

        # 坐标轴与标签
        ax.set_xlabel("时间（小时）", fontsize=fontsize_label)
        ax.set_ylabel("路径覆盖数", fontsize=fontsize_label)
        ax.tick_params(axis='both', labelsize=fontsize_tick)

        # 时间从分钟转小时显示
        x_ticks = ax.get_xticks()
        ax.set_xticklabels([f"{int(x/60)}" for x in x_ticks])

        # 标题和子图标记 (a)(b)(c)...
        sub_label = string.ascii_lowercase[i]
        ax.set_title(f"{subject}\n({sub_label})", fontsize=fontsize_label + 2, pad=10)

        ax.grid(True, linestyle='--', alpha=0.6)
        ax.legend(fontsize=legend_fontsize, loc='lower right')

    # 删除空白子图
    total_axes = nrows * ncols
    if n < total_axes:
        for j in range(n, total_axes):
            row, col = divmod(j, ncols)
            fig.delaxes(axes[row][col])

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig(out_file, dpi=400, bbox_inches='tight')
    print(f"✅ 组合图已保存至 {out_file}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate multi-protocol absolute edge coverage plots.")
    parser.add_argument('-i', '--csv_files', nargs='+', required=True, help="Paths to CSV files for each protocol.")
    parser.add_argument('-p', '--puts', nargs='+', required=True, help="Protocol names corresponding to CSVs.")
    parser.add_argument('-r', '--runs', type=int, required=True, help="Number of runs per experiment.")
    parser.add_argument('-c', '--cut_off', type=int, required=True, help="Cut-off time in minutes.")
    parser.add_argument('-s', '--step', type=int, required=True, help="Time step in minutes.")
    parser.add_argument('-o', '--out_file', type=str, required=True, help="Output figure filename.")
    parser.add_argument('-f', '--fuzzers', nargs='+', required=True, help="List of fuzzers.")
    args = parser.parse_args()

    if len(args.csv_files) != len(args.puts):
        raise ValueError("csv_files 与 puts 数量必须一致！")

    main(args.csv_files, args.puts, args.runs, args.cut_off, args.step, args.out_file, args.fuzzers)
