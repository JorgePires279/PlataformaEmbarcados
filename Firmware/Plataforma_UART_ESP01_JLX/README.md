# Plataforma_UART_ESP01_JLX

**UART/ESP-01 + LCD gráfico JLX**

Teste de comunicação serial com um módulo ESP-01 (comandos AT, com busca automática de baud rate) e de um LCD gráfico JLX (ST7567) por SPI1. Usa uma pinagem de LCD 1602 diferente das demais (ver docs/PINAGEM.md).

- Microcontrolador: STM32F103C8T6 (Blue Pill), ARM Cortex-M3, HSE de 8 MHz → 72 MHz
- Linguagem: C (gnu11), somente registradores, sem HAL e sem `printf`
- Código-fonte: `Sources/main.c` (1007 linhas, arquivo único)
- Projeto: STM32CubeIDE com CMake (`CMakeLists.txt`, `CMakePresets.json`)

## Pinagem

| Pino | Função | Periférico |
|---|---|---|
| PA0, PA1 | LCD 1602: RS, E | GPIO |
| PA3 | LDR | ADC1 canal 3 |
| PA4, PA5, PA6, PA7 | JLX: CS, SCK, RST, SDA (MOSI) | SPI1 |
| PA9, PA10 | USART1: TX (→ RX do ESP-01), RX (← TX do ESP-01) | USART1 |
| PB0, PB3, PB4, PB5 | LCD 1602: DB7, DB6, DB5, DB4 | GPIO (JTAG desligado) |
| PB1 | Potenciômetro POT2 | ADC1 canal 9 |
| PB6, PB7 | I2C1: SCL, SDA (DS3231, endereço 0x68) | I2C1 |
| PB8, PB9 | HC-SR04: TRIG, ECHO | GPIO + TIM4 |
| PB12 | JLX: A0 (dados/comando) | GPIO |
| PB13 | Controle do ISL32475 (nível alto desliga o RO, para não disputar o PA10) | GPIO |
| PC13 | LED de batimento (500 ms) | GPIO |

Veja também [docs/PINAGEM.md](../docs/PINAGEM.md).

## Opções de compilação

Definidas no início de `Sources/main.c`.

| Macro | Padrão | Efeito |
|---|---|---|
| USAR_JLX | 1 | 1 = usa o LCD gráfico JLX |
| USAR_ESP | 1 | 1 = executa a tarefa do ESP-01 |
| USAR_HCSR04 | 1 | 1 = executa o HC-SR04 |
| RS485_DESABILITA_RX | 1 | 1 = coloca PB13 em nível alto (RO do ISL32475 desligado) |

## Telas do LCD 1602

| Tela | Conteúdo |
|---|---|
| 0 | Hora, temperatura e data |
| 1 | LDR e POT2 |
| 2 | Distância |
| 3 | ESP-01: contadores OK/ERR, baud rate e erros |

## Temporização

Escalonamento cooperativo: um laço em `main()` compara `agora_ms()` com o instante da última execução de cada tarefa.

| Tarefa | Período |
|---|---|
| LED de batimento (PC13) | 500 ms |
| HC-SR04 | 200 ms |
| ADC (LDR, POT2) | 100 ms (valor exibido atualiza a cada 500 ms) |
| Leitura do RTC | 500 ms |
| ESP-01 (AT) | 2000 ms |
| LCD 1602 | 500 ms |
| Painel JLX | 500 ms |

## Funções

| Função | Linha | Descrição |
|---|---|---|
| `clock_init` | 83 | Liga o HSE (8 MHz), configura PLL ×9 (72 MHz), APB1 /2 (36 MHz) e 2 wait states da Flash. Se o HSE não partir, continua a 8 MHz. Configura o SysTick a 1 kHz (sem interrupção) e habilita o contador de ciclos DWT. |
| `delay_ms` | 113 | Atraso bloqueante em ms, pelo flag COUNTFLAG do SysTick. |
| `delay_us` | 119 | Atraso bloqueante em µs, pelo contador de ciclos DWT_CYCCNT. |
| `agora_ms` | 127 | Milissegundos desde o início, acumulados a partir do DWT_CYCCNT. Deve ser chamada a menos de ~59 s de intervalo (estouro de 32 bits a 72 MHz); o laço principal a chama a cada volta. |
| `lcd_pulso_e` | 151 | Pulso no pino E (enable) do HD44780. |
| `lcd_nibble` | 158 | Coloca 4 bits em DB4–DB7 (um único acesso ao BSRR) e pulsa E. |
| `lcd_byte` | 169 | Envia um byte (comando ou dado, conforme RS) em dois nibbles. |
| `lcd_cmd` | 178 | Envia um comando; limpar (0x01) e home (0x02) esperam 2 ms. |
| `lcd_init` | 184 | Configura os pinos e executa a sequência de inicialização em modo de 4 bits (2 linhas, 5×8, display ligado, sem cursor). |
| `lcd_linha` | 210 | Escreve uma linha (0 ou 1) de 16 colunas, completando com espaços. |
| `adc_init` | 224 | Habilita o ADC1, coloca os pinos em modo analógico, ADCCLK = PCLK2/6 = 12 MHz, amostragem de 239,5 ciclos e calibração. |
| `adc_uma` | 245 | Uma conversão por software (SWSTART) com timeout. Retorna 0xFFFF em erro. |
| `adc_media` | 253 | Seleciona o canal, descarta a 1ª conversão (troca de canal) e retorna a média de n amostras. |
| `i2c_config` | 275 | Configura o I2C1 em modo padrão, 100 kHz (CCR = PCLK1/200 kHz) e TRISE. |
| `i2c_recupera` | 285 | Reset por software (SWRST) do I2C1 e reconfiguração; usada após timeout. |
| `i2c_init` | 292 | Habilita os clocks, coloca PB6/PB7 em função alternativa open-drain e configura o I2C1. |
| `i2c_sr1` | 300 | Espera um flag do registrador SR1, com timeout. |
| `i2c_livre` | 307 | Espera o barramento ficar livre (BUSY = 0), com timeout. |
| `i2c_start_end` | 314 | Gera START, envia o endereço e espera ADDR ou NACK (AF). |
| `i2c_ler_reg` | 328 | Lê n ≥ 3 bytes a partir de um registrador do escravo, seguindo a sequência do STM32F1 para 3 ou mais bytes (NACK e STOP antes do último). Recupera o barramento em caso de timeout. |
| `bcd` | 369 | Converte BCD para binário. |
| `ds3231_ler` | 371 | Lê 19 bytes (0x00–0x12) do DS3231 (endereço 0x68): hora, data, flag OSF e temperatura (resolução de 0,25 °C). Valida as faixas dos campos. |
| `uart_baud` | 401 | Calcula o BRR do USART1 para o baud rate pedido. |
| `uart_init` | 403 | USART1 8N1 por polling: PA9 função alternativa push-pull (TX), PA10 entrada com pull-up (RX). |
| `uart_flush` | 413 | Descarta bytes pendentes no receptor. |
| `uart_envia` | 418 | Envia uma string e espera o fim da transmissão (TC). |
| `uart_recebe` | 424 | Recebe até max bytes, com timeout total e tempo ocioso entre bytes (medidos pelo DWT); conta erros de recepção em uart_erros. |
| `us_init` | 454 | Configura o TIM4 a 1 MHz (PSC = HCLK/1 MHz − 1), PB8 como saída (TRIG) e PB9 como entrada (ECHO). |
| `us_medir` | 467 | Pulso de 10 µs no TRIG, mede a largura do ECHO e calcula mm = largura × 343 / 2000. Faixa válida: 20 a 4000 mm; timeout de 30 ms em cada espera. |
| `spi_envia` | 598 | Envia n bytes pelo SPI1 e espera o barramento ficar livre. |
| `g_cmd` | 606 | Envia um comando ao ST7567 (CS baixo, A0 baixo). |
| `g_dados` | 613 | Envia dados de vídeo ao ST7567 (A0 alto). |
| `g_cursor` | 620 | Posiciona o cursor: página (linha de 8 pixels) e coluna. |
| `g_limpa` | 628 | Apaga as 8 páginas do display. |
| `g_init` | 638 | Configura SPI1 (mestre, /16), faz o reset e envia a sequência de inicialização do ST7567. |
| `g_char` | 660 | Desenha um caractere 6×8 (fonte em charset[]), opcionalmente invertido. |
| `g_linha` | 671 | Escreve uma linha de 21 caracteres no display gráfico. |
| `ps` | 682 | Copia uma string para o buffer e devolve o novo ponteiro. |
| `pu` | 684 | Escreve um inteiro sem sinal em decimal. |
| `p2` | 692 | Escreve um número com 2 dígitos (zeros à esquerda). |
| `p4` | 699 | Escreve um número com 4 dígitos (zeros à esquerda). |
| `p_volts` | 708 | Escreve uma tensão em mV no formato "1.65V". |
| `p_temp` | 718 | Escreve uma temperatura em centésimos de °C no formato "25.2C". |
| `contem` | 729 | Procura uma substring em um buffer de bytes. |
| `exibidos` | 745 |  |
| `tarefa_adc` | 761 | Lê cada canal (média de 32 amostras) e aplica um filtro passa-baixas IIR, y += (x − y)/8, em ponto fixo ×16. |
| `tarefa_esp` | 780 | Procura o baud rate do ESP-01 enviando "AT" (115200, 9600, 57600, 38400, 19200). Ao achar "OK", lê a versão (AT+GMR). Depois confere o "AT" a cada ciclo; após 5 falhas seguidas volta a procurar. |
| `lcd_pagina` | 820 | Desenha a página atual (0 a 3) no LCD 1602: RTC, LDR/POT, HC-SR04 e ESP-01. |
| `painel_jlx` | 893 | Desenha no display gráfico um painel com hora, temperatura, LDR, POT, distância e estado do ESP-01. |
| `main` | 949 | Inicializa os periféricos e executa o laço cooperativo: LED (500 ms), HC-SR04 (200 ms), ADC (100 ms), RTC (500 ms), ESP-01 (2 s), LCD 1602 (500 ms) e painel JLX (500 ms). |

## Tamanho do binário

Compilado com `arm-none-eabi-gcc` 13.3.1, `-Wall -Werror`, sem otimização (build Debug).

| Região | Uso | Ocupação |
|---|---|---|
| Flash (text + data) | 10732 bytes | 16.4% de 64 kB |
| RAM (data + bss) | 1680 bytes | 8.2% de 20 kB (sem contar pilha e heap) |

## Compilar e gravar

Veja o [README geral](../README.md#compilar-e-gravar).

## Observações

- A pinagem do LCD 1602 aqui (RS = PA0, E = PA1, DB4–DB7 = PB5, PB4, PB3, PB0) é diferente da dos demais projetos; PA0/PA1 conflitam com as linhas da matriz de botões, por isso os botões não são usados.
- PB0 é DB7 do LCD, então só o potenciômetro em PB1 (POT2) é lido.
- O LCD gráfico usa PA4–PA7 (SPI1), que na placa são compartilhados com o LCD 1602 por jumpers.
- O cabeçalho do arquivo informa "NAO testado fisicamente".
