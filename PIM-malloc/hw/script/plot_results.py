"""Plot the alien cache experiment results into report_figures/.

Reads the per-step CSVs under hw/results/ and writes PNG figures used by
experiment_report.html.

usage: python plot_results.py
"""

import csv
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter  # noqa: E402

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(SCRIPT_DIR, "..", "results")
FIGURES_DIR = os.path.join(SCRIPT_DIR, "..", "..", "report_figures")

STEPS = [
    ("step0_baseline", "Step 0\n原設計\n（已修 bug）"),
    ("step1_owner_lookup", "Step 1\nO(1)\n查 owner"),
    ("step2_owner_batching", "Step 2\n每 owner\n拿一次鎖"),
]
THRESHOLDS = [1, 2, 4, 8, 16, 32, 64]

# NOTE: dark-mode categorical slots, validated against the report surface
SERIES = ["#3987e5", "#d95926", "#199e70", "#c98500"]
MARKERS = ["o", "s", "^", "D"]
SURFACE = "#151d38"
TEXT_PRIMARY = "#eef2ff"
TEXT_SECONDARY = "#a7b0d6"
GRID = "#263055"
BASELINE = "#4a5580"

plt.rcParams.update({
    "font.family": ["Microsoft JhengHei", "Noto Sans CJK TC", "sans-serif"],
    "font.size": 10,
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "axes.edgecolor": BASELINE,
    "axes.labelcolor": TEXT_SECONDARY,
    "axes.titlecolor": TEXT_PRIMARY,
    "axes.titlesize": 12,
    "axes.titleweight": "bold",
    "xtick.color": TEXT_SECONDARY,
    "ytick.color": TEXT_SECONDARY,
    "legend.frameon": False,
    "legend.labelcolor": TEXT_PRIMARY,
    "savefig.facecolor": SURFACE,
    "lines.linewidth": 2,
    "lines.markersize": 6,
})


def read_summary(step, local, pattern):
    suffix = "_scatter" if pattern == "scatter" else ""
    path = os.path.join(RESULTS_DIR, step, f"summary_local{local}{suffix}.csv")
    if not os.path.exists(path):
        return None
    with open(path, newline="") as f:
        return {int(row["threshold"]): row for row in csv.DictReader(f)}


def cycles(rows, threshold):
    return float(rows[threshold]["median_total_cycles"])


def style_axes(ax):
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.tick_params(length=0)


def millions(value, _position=None):
    return f"{value / 1e6:g}M"


def millions_label(value):
    return f"{value / 1e6:.2f}M"


def save(fig, name):
    os.makedirs(FIGURES_DIR, exist_ok=True)
    path = os.path.join(FIGURES_DIR, name)
    fig.savefig(path, dpi=200, bbox_inches="tight", pad_inches=0.25)
    plt.close(fig)
    print("wrote", os.path.normpath(path))


def plot_step_overview():
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.4))
    width = 0.36

    for ax, local in zip(axes, (50, 0)):
        positions = range(len(STEPS))
        for s, (threshold, color) in enumerate(zip((1, 64), SERIES)):
            values = [cycles(read_summary(step, local, "neighbor"), threshold)
                      for step, _ in STEPS]
            offsets = [p + (s - 0.5) * (width + 0.02) for p in positions]
            bars = ax.bar(offsets, values, width, color=color,
                          label=f"threshold {threshold}")
            for bar, value in zip(bars, values):
                ax.text(bar.get_x() + bar.get_width() / 2, value,
                        millions_label(value), ha="center", va="bottom",
                        fontsize=8, color=TEXT_SECONDARY)

        ax.set_xticks(list(positions))
        ax.set_xticklabels([label for _, label in STEPS], fontsize=9)
        ax.yaxis.set_major_formatter(FuncFormatter(millions))
        ax.set_title(f"neighbor，local free {local}%", loc="left")
        style_axes(ax)

    axes[0].set_ylabel("總 DPU cycles（越低越好）")
    axes[0].legend(loc="upper right")
    save(fig, "fig1_step_overview.png")


def plot_baseline():
    fig, axes = plt.subplots(1, 2, figsize=(11, 3.8))

    for ax, local, color, marker in zip(axes, (50, 0), SERIES, MARKERS):
        rows = read_summary("step0_baseline", local, "neighbor")
        values = [cycles(rows, t) for t in THRESHOLDS]
        ax.plot(THRESHOLDS, values, color=color, marker=marker)

        change = (values[-1] / values[0] - 1) * 100
        ax.text(0.98, 0.06, f"threshold 64 比 1 慢 {change:.1f}%",
                transform=ax.transAxes, ha="right", color=TEXT_PRIMARY,
                fontsize=10)

        ax.set_xscale("log", base=2)
        ax.set_xticks(THRESHOLDS)
        ax.set_xticklabels([str(t) for t in THRESHOLDS])
        ax.yaxis.set_major_formatter(FuncFormatter(millions))
        ax.set_xlabel("alien cache flush threshold")
        ax.set_title(f"Step 0（原設計），neighbor，local free {local}%",
                     loc="left")
        style_axes(ax)

    axes[0].set_ylabel("總 DPU cycles")
    save(fig, "fig2_baseline_threshold.png")


def plot_threshold_curves():
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.2), sharex=True,
                             sharey=True)
    configs = [("neighbor", 50), ("neighbor", 0), ("scatter", 50),
               ("scatter", 0)]

    for ax, (pattern, local) in zip(axes.flat, configs):
        for (step, label), color, marker in zip(STEPS[1:], SERIES, MARKERS):
            rows = read_summary(step, local, pattern)
            values = [cycles(rows, t) for t in THRESHOLDS]
            ax.plot(THRESHOLDS, values, color=color, marker=marker,
                    label=label.replace("\n", " "))

        ax.set_xscale("log", base=2)
        ax.set_xticks(THRESHOLDS)
        ax.set_xticklabels([str(t) for t in THRESHOLDS])
        ax.yaxis.set_major_formatter(FuncFormatter(millions))
        ax.set_title(f"{pattern}，local free {local}%", loc="left")
        style_axes(ax)

    for ax in axes[1]:
        ax.set_xlabel("alien cache flush threshold")
    for ax in axes[:, 0]:
        ax.set_ylabel("總 DPU cycles")

    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=2,
               bbox_to_anchor=(0.5, 1.03))
    fig.tight_layout()
    save(fig, "fig3_threshold_curves.png")


def plot_relative():
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
    configs = [("neighbor", 50), ("neighbor", 0), ("scatter", 50),
               ("scatter", 0)]

    for ax, (step, label) in zip(axes, STEPS[1:]):
        ax.axhline(0, color=BASELINE, linewidth=1)
        for (pattern, local), color, marker in zip(configs, SERIES, MARKERS):
            rows = read_summary(step, local, pattern)
            base = cycles(rows, 1)
            values = [(cycles(rows, t) / base - 1) * 100 for t in THRESHOLDS]
            ax.plot(THRESHOLDS, values, color=color, marker=marker,
                    label=f"{pattern}，local {local}%")

        ax.set_xscale("log", base=2)
        ax.set_xticks(THRESHOLDS)
        ax.set_xticklabels([str(t) for t in THRESHOLDS])
        ax.yaxis.set_major_formatter(
            FuncFormatter(lambda v, _: f"{v:+.0f}%"))
        ax.set_xlabel("alien cache flush threshold")
        ax.set_title(label.replace("\n", " "), loc="left")
        style_axes(ax)

    axes[0].set_ylabel("相對 threshold 1 的 cycles 變化（低於 0 代表較快）")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=4,
               bbox_to_anchor=(0.5, 1.06))
    fig.tight_layout()
    save(fig, "fig4_threshold_relative.png")


if __name__ == "__main__":
    plot_step_overview()
    plot_baseline()
    plot_threshold_curves()
    plot_relative()
