# Sistema de Aquisição de Dados de Sensores

Um sistema embarcado completo para aquisição e visualização de dados de sensores em tempo real usando Raspberry Pi Pico W com sensor acelerômetro/giroscópio MPU6050.

## Componentes de Hardware

### Sistema Principal
- **Raspberry Pi Pico W** - Microcontrolador principal
- **MPU6050** - Sensor de movimento de 6 eixos (acelerômetro + giroscópio)
- **Display OLED SSD1306** - Display I2C de 128x64 pixels
- **Cartão MicroSD** - Armazenamento de dados via interface SPI

### Interface do Usuário
- **3 Botões** - Primário, Secundário e do Joystick
- **LED RGB** - Indicador de status
- **Sistema de Buzzer Duplo** - Feedback sonoro

### Configuração de Pinos

#### Sensores I2C (MPU6050)
- **SDA**: GPIO 0
- **SCL**: GPIO 1
- **Endereço**: 0x68

#### Display I2C (SSD1306)
- **SDA**: GPIO 14
- **SCL**: GPIO 15
- **Endereço**: 0x3C

#### Cartão SD SPI
- **MISO**: GPIO 16
- **MOSI**: GPIO 19
- **SCK**: GPIO 18
- **CS**: GPIO 17
- **CD**: GPIO 22

#### Interface do Usuário
- **Botão Primário**: GPIO 5
- **Botão Secundário**: GPIO 6
- **Botão do Joystick**: GPIO 22
- **LED RGB Vermelho**: GPIO 13
- **LED RGB Verde**: GPIO 11
- **LED RGB Azul**: GPIO 12
- **Buzzer A**: GPIO 21
- **Buzzer B**: GPIO 10

## Arquitetura de Software

### Aplicação Principal (`embedded_sensor_logger.c`)
A aplicação embarcada principal que fornece:

- **Interface de Sensores**: Aquisição de dados do MPU6050
- **Gerenciamento de Display**: Controle da tela OLED e interface
- **Sistema de Armazenamento**: Montagem, formatação e registro de dados no cartão SD
- **Interface do Usuário**: Manipulação de botões e navegação de menu
- **Feedback Sonoro**: Controle de buzzer para notificações
- **Feedback Visual**: Indicação de status via LED RGB

### Funções Principais

#### Operações de Sensores
- `sensor_reset()` - Inicializar sensor MPU6050
- `sensor_read_data()` - Ler dados de aceleração, giroscópio e temperatura

#### Gerenciamento de Display
- `refresh_screen()` - Atualizar display OLED com interface atual
- Sistema de navegação multi-tela (5 telas principais + tela de gravação)

#### Operações de Armazenamento
- `execute_mount()` - Montar sistema de arquivos do cartão SD
- `execute_unmount()` - Desmontar cartão SD com segurança
- `execute_format()` - Formatar cartão SD
- `store_sensor_data()` - Registrar dados do sensor em arquivo CSV

#### Interface do Usuário
- `gpio_interrupt_handler()` - Manipular eventos de pressionamento de botões
- `activate_sound()` - Gerar feedback sonoro
- `set_light_color()` - Controlar LED RGB
- `blink_light()` - Criar padrões de piscada do LED

### Formato dos Dados

Os dados do sensor são registrados em formato CSV com a seguinte estrutura:
```csv
sample_number,time_s,motion_x,motion_y,motion_z,rotation_x,rotation_y,rotation_z
```

- **sample_number**: Identificador sequencial do ponto de dados
- **time_s**: Tempo decorrido em segundos
- **motion_x/y/z**: Valores de aceleração (eixos X, Y, Z)
- **rotation_x/y/z**: Valores do giroscópio (eixos X, Y, Z)

## Telas da Interface do Usuário

### Tela 1: Status do Sistema
- Inicialização do sistema e estado pronto
- Instruções de navegação

### Tela 2: Gerenciamento do Cartão SD
- Montar/desmontar cartão SD
- Indicação de status e tratamento de erros

### Tela 3: Formatação do Cartão SD
- Funcionalidade de formatação do cartão SD
- Status de progresso e conclusão

### Tela 4: Gravação de Dados
- Iniciar/parar gravação de dados
- Gerenciamento de arquivos e tratamento de erros

### Tela 5: Visualização em Tempo Real
- Exibição de dados do sensor em tempo real
- Gráficos de barras do giroscópio
- Indicadores de pitch/roll

### Tela 6: Progresso da Gravação
- Status de gravação em tempo real
- Tempo decorrido e contagem de amostras
- Visualização ao vivo do sensor

## Visualização de Dados

### Ferramenta de Análise Python (`python_plots/data_visualization.py`)
Uma ferramenta abrangente de análise e visualização de dados que fornece:

- **Análise Multi-gráfico**: 6 gráficos simultâneos (grade 2x3)
- **Eixo X Flexível**: Escolha entre contagem de amostras ou tempo
- **Dados Codificados por Cor**: Cores diferentes para cada eixo do sensor
- **Saída Profissional**: Gráficos matplotlib prontos para publicação

### Uso
1. Certifique-se de que o arquivo CSV de dados do sensor está no mesmo diretório
2. Modifique a variável `data_file` para corresponder ao nome do seu arquivo de dados
3. Defina `x_axis_choice` (0 para contagem de amostras, 1 para tempo)
4. Execute o script para gerar gráficos de visualização

## Compilação e Implantação

### Pré-requisitos
- Raspberry Pi Pico SDK
- CMake 3.13 ou superior
- Toolchain ARM GCC

### Processo de Compilação
```bash
mkdir build
cd build
cmake ..
make
```

### Gravação
```bash
cp embedded_sensor_logger.uf2 /media/pico/
```

## Guia de Operação

### Configuração Inicial
1. Ligue o sistema
2. Aguarde a inicialização (LED amarelo)
3. Navegue pelas telas usando o botão Primário (A)
4. Monte o cartão SD usando o botão Secundário (B) na Tela 2

### Gravação de Dados
1. Navegue para a Tela 4 (Gravação de Dados)
2. Pressione o botão Secundário (B) para iniciar a gravação
3. Monitore o progresso na Tela 6
4. Pressione o botão Secundário (B) para parar a gravação

### Análise de Dados
1. Remova o cartão SD e transfira os arquivos CSV para o computador
2. Copie o arquivo de dados para o diretório `python_plots/`
3. Atualize a variável `data_file` em `data_visualization.py`
4. Execute o script Python para gerar gráficos

## Estrutura de Arquivos

```
embedded_sensor_logger/
├── embedded_sensor_logger.c    # Aplicação principal
├── hw_config.c                 # Configuração de hardware
├── CMakeLists.txt              # Configuração de compilação
├── python_plots/
│   ├── data_visualization.py   # Ferramenta de análise de dados
│   └── sensor_log1.csv         # Arquivo de dados de exemplo
└── lib/                        # Dependências de bibliotecas
    ├── ssd1306.c/h            # Driver do display OLED
    └── FatFs_SPI/             # Sistema de arquivos do cartão SD
```

## Especificações Técnicas

### Especificações do Sensor (MPU6050)
- **Acelerômetro**: Faixas de ±2g, ±4g, ±8g, ±16g
- **Giroscópio**: Faixas de ±250, ±500, ±1000, ±2000°/s
- **Temperatura**: -40°C a +85°C
- **Endereço I2C**: 0x68 (configurável)

### Especificações do Display (SSD1306)
- **Resolução**: 128x64 pixels
- **Interface**: I2C
- **Endereço**: 0x3C
- **Contraste**: Ajustável

### Especificações de Armazenamento
- **Formato**: Sistema de arquivos FAT32
- **Formato de Arquivo**: CSV
- **Taxa de Dados**: 10 Hz (configurável)
- **Capacidade**: Até 32GB (SDHC)

## Solução de Problemas

### Problemas Comuns

#### Cartão SD Não Montando
- Verifique o formato do cartão SD (FAT32 necessário)
- Verifique as conexões SPI
- Tente formatar o cartão SD usando a Tela 3

#### Sensor Não Respondendo
- Verifique as conexões I2C (SDA/SCL)
- Verifique a alimentação do MPU6050
- Certifique-se do endereço I2C correto (0x68)

#### Problemas de Display
- Verifique as conexões I2C (GPIO 14/15)
- Verifique o endereço do display (0x3C)
- Certifique-se da alimentação adequada

#### Botão Não Respondendo
- Verifique as conexões GPIO
- Verifique os resistores pull-up
- Verifique problemas de bounce do botão

### Indicadores de Status LED
- **Amarelo**: Sistema inicializando
- **Vermelho**: Cartão SD não montado
- **Verde**: Cartão SD montado e pronto
- **Azul**: Gravando em andamento

## Notas de Desenvolvimento

### Organização do Código
O projeto segue uma arquitetura modular com separação clara de responsabilidades:
- Camada de abstração de hardware
- Gerenciamento de interface do usuário
- Aquisição e armazenamento de dados
- Visualização e análise

### Extensibilidade
O sistema é projetado para fácil extensão:
- Sensores adicionais podem ser adicionados via I2C/SPI
- Novos tipos de display suportados através de abstração de driver
- Formatos de dados podem ser modificados nas funções de armazenamento
- Ferramentas de análise podem ser estendidas com novos tipos de visualização
