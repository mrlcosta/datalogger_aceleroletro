import numpy as np
import matplotlib.pyplot as plt

data_file = 'sensor_log1.csv'
x_axis_choice = 0

file_path = rf'{data_file}'

with open(file_path, 'r') as input_file:
    column_names = input_file.readline().strip().split(',')

raw_data = np.loadtxt(file_path, delimiter=',', skiprows=1)
x_values = raw_data[:, x_axis_choice]

plot_colors = ['b', 'g', 'r', 'c', 'm', 'y']

figure, axes = plt.subplots(2, 3, figsize=(15, 8))
axes = axes.flatten()

for plot_index in range(6):
    y_data_index = plot_index + 2
    axes[plot_index].plot(x_values, raw_data[:, y_data_index], color=plot_colors[plot_index], marker='o', linestyle='-')
    axes[plot_index].grid()
    axes[plot_index].set_ylabel('Amplitude')
    axes[plot_index].set_title(f'{column_names[y_data_index]} x {column_names[x_axis_choice]}')
    if x_axis_choice == 0:
        axes[plot_index].set_xlabel("Sample count")
    else:
        axes[plot_index].set_xlabel("Time (s)")

plt.tight_layout()
plt.show()