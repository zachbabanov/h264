import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.dates import DateFormatter
import numpy as np

desired_plots = [
    {
        'title': 'Accepted Packets',
        'ylabel': 'Packets',
        'columns': ['acceptedPackets'],
        'func': lambda df: df['acceptedPackets'],
        'color': 'C0'
    },
    {
        'title': 'Accepted Blocks',
        'ylabel': 'Blocks',
        'columns': ['acceptedBlocks'],
        'func': lambda df: df['acceptedBlocks'],
        'color': 'C1'
    },
    {
        'title': 'Accepted / Expected Blocks',
        'ylabel': 'Ratio',
        'columns': ['acceptedBlocks', 'expectedBlocks'],
        'func': lambda df: df['acceptedBlocks'] / df['expectedBlocks'].replace(0, np.nan),
        'color': 'C2'
    },
    {
        'title': 'Assembled NALU',
        'ylabel': 'NALU',
        'columns': ['assembledNalu'],
        'func': lambda df: df['assembledNalu'],
        'color': 'C3'
    },
    {
        'title': 'Skipped / Assembled NALU',
        'ylabel': 'Skipped/Assembled',
        'columns': ['skippedNalu', 'assembledNalu'],
        'func': lambda df: df['skippedNalu'] / df['assembledNalu'].replace(0, np.nan),
        'color': 'C4'
    }
]

def read_csv_file(filename):
    try:
        df = pd.read_csv(filename, parse_dates=['Timestamp'])
        return df
    except Exception as e:
        print(f"Ошибка чтения файла: {e}")
        return None

def get_available_plots(df):
    return [p for p in desired_plots if all(col in df.columns for col in p['columns'])]

def setup_figure(plots):
    n = len(plots)
    if n == 0:
        print("Нет доступных графиков для отображения.")
        sys.exit(1)

    ncols = 2
    nrows = (n + ncols - 1) // ncols
    fig, axes = plt.subplots(nrows, ncols, figsize=(6 * ncols, 4 * nrows))
    axes = axes.flatten()
    for i in range(n, len(axes)):
        fig.delaxes(axes[i])
    return fig, axes[:n]

def update_plots(frame, filename, axes, plots):
    df = read_csv_file(filename)
    if df is None or df.empty:
        return

    for ax, plot in zip(axes, plots):
        ax.clear()
        y = plot['func'](df)
        ax.plot(df['Timestamp'], y, marker='.', linestyle='-',
                markersize=3, color=plot.get('color', None), label=plot['ylabel'])
        ax.set_title(plot['title'])
        ax.set_ylabel(plot['ylabel'])
        ax.set_xlabel('Time')
        ax.grid(True, linestyle='--', alpha=0.7)
        ax.legend(loc='best')
        ax.xaxis.set_major_formatter(DateFormatter('%H:%M:%S'))
        fig = ax.get_figure()
        fig.autofmt_xdate()

def main():
    if len(sys.argv) != 2:
        print("Использование: python script.py <путь_к_csv_файлу>")
        sys.exit(1)

    filename = sys.argv[1]
    plt.style.use('seaborn-v0_8-darkgrid')

    df = read_csv_file(filename)
    if df is None:
        sys.exit(1)

    plots = get_available_plots(df)
    if not plots:
        print("В файле нет столбцов, необходимых для построения графиков.")
        sys.exit(1)

    fig, axes = setup_figure(plots)

    ani = animation.FuncAnimation(
        fig, update_plots, fargs=(filename, axes, plots),
        interval=1000, cache_frame_data=False
    )

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
