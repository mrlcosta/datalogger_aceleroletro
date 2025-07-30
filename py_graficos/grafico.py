# Script para gerar gráficos dos dados do sensor MPU6050
# Este script lê arquivos CSV gerados pelo datalogger e cria gráficos de análise

import numpy as np
import matplotlib.pyplot as plt

# Configurações do arquivo de entrada
input_file = 'sensor_data1.csv'  # Nome do arquivo CSV a ser analisado
x_axis_type = 0                  # 0 = número da amostra, 1 = tempo em segundos

# Caminho completo do arquivo
file_path = rf'{input_file}'

# Lê os nomes das colunas do arquivo CSV
with open(file_path, 'r') as f:
    column_names = f.readline().strip().split(',')

# Carrega os dados do arquivo CSV (ignora a primeira linha que contém os nomes)
sensor_data = np.loadtxt(file_path, delimiter=',', skiprows=1)
x_values = sensor_data[:, x_axis_type]  # Valores do eixo X

# Cores para os diferentes gráficos
plot_colors = ['b', 'g', 'r', 'c', 'm', 'y']  # Azul, Verde, Vermelho, Ciano, Magenta, Amarelo

# Cria uma figura com 2 linhas e 3 colunas de gráficos
fig, axes = plt.subplots(2, 3, figsize=(15, 8))
axes = axes.flatten()  # Converte para array 1D para facilitar o acesso

# Gera os 6 gráficos (aceleração X, Y, Z e giroscópio X, Y, Z)
for i in range(6):
    y_column = i + 2  # Índice da coluna Y (pula as colunas de amostra e tempo)
    
    # Plota os dados com marcadores e linhas
    axes[i].plot(x_values, sensor_data[:, y_column], color=plot_colors[i], marker='o', linestyle='-')
    axes[i].grid()  # Adiciona grade ao gráfico
    axes[i].set_ylabel('Amplitude')  # Rótulo do eixo Y
    axes[i].set_title(f'{column_names[y_column]} x {column_names[x_axis_type]}')  # Título do gráfico
    
    # Define o rótulo do eixo X baseado no tipo selecionado
    if x_axis_type == 0:
        axes[i].set_xlabel("Sample number")  # Número da amostra
    else:
        axes[i].set_xlabel("Time (s)")  # Tempo em segundos

# Ajusta o layout para evitar sobreposição
plt.tight_layout()

# Exibe os gráficos
plt.show()