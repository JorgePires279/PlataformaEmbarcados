# Plataforma_Botoes_LEDs

**Básico + matriz de botões e LEDs**

Acrescenta a matriz 3×3 (6 chaves táteis T1–T6 e 3 chaves de alavanca S1–S3) e os 6 LEDs. O LED de PC13 deixa de existir, pois PC13 passa a ser coluna da matriz.

- Microcontrolador: STM32F103C8T6 (Blue Pill), ARM Cortex-M3, HSE de 8 MHz → 72 MHz
- Linguagem: C (gnu11), somente registradores, sem HAL e sem `printf`
- Código-fonte: `Sources/main.c` (804 linhas, arquivo único)
- Projeto: STM32CubeIDE com CMake (`CMakeLists.txt`, `CMakePresets.json`)

## Pinagem

| Pino | Função | Periférico |
|---|---|---|
| PA0, PA1, PA2 | Matriz de botões, linhas l2, l1, l0 | GPIO |
| PA3 | LDR | ADC1 canal 3 |
| PA4–PA7 | LCD 1602, DB4–DB7 | GPIO |
| PA11, PA12, PA15 | LED1, LED2, LED4 | GPIO |
| PA13, PA14 | SWD (programação) | — |
| PB0 | Potenciômetro B0 | ADC1 canal 8 |
| PB1 | Potenciômetro B1 | ADC1 canal 9 |
| PB3, PB4, PB5 | LED5, LED6, LED3 | GPIO (JTAG desligado) |
| PB6, PB7 | I2C1: SCL, SDA (DS3231, endereço 0x68) | I2C1 |
| PB8 | HC-SR04: TRIG | GPIO |
| PB9 | HC-SR04: ECHO | GPIO |
| PB12 | LCD 1602, RS | GPIO |
| PB14 | LCD 1602, E | GPIO |
| PC13, PC14, PC15 | Matriz de botões, colunas c2, c1, c0 | GPIO (pull-up) |

Veja também [docs/PINAGEM.md](../docs/PINAGEM.md).

## Opções de compilação

Definidas no início de `Sources/main.c`.

| Macro | Padrão | Efeito |
|---|---|---|
| LED_ATIVO_ALTO | 1 | 1 = LED acende com o pino em nível alto; 0 = nível baixo |
| MATRIZ_DEBUG | 1 | 1 = a tela de botões mostra os 9 bits crus da matriz; 0 = mostra S1–S3 |

## Telas do LCD 1602

| Tela | Conteúdo |
|---|---|
| 0 | Hora, temperatura e data |
| 1 | Distância |
| 2 | LDR |
| 3 | Potenciômetros |
| 4 | Botões T1–T6 e S1–S3 |

## Teclas e LEDs

Matriz 3×3: `T2 T1 S1 / T4 T3 S2 / T5 T6 S3`.

| LED | Acende enquanto estiver ativo |
|---|---|
| LED1 (PA11) | T1 ou S1 |
| LED2 (PA12) | T2 ou S1 |
| LED3 (PB5) | T3 ou S2 |
| LED4 (PA15) | T4 ou S2 |
| LED5 (PB3) | T5 ou S3 |
| LED6 (PB4) | T6 ou S3 |

## Temporização

Escalonamento cooperativo: um laço em `main()` compara `agora_ms()` com o instante da última execução de cada tarefa.

| Tarefa | Período |
|---|---|
| Matriz de botões e LEDs | 20 ms |
| HC-SR04 | 200 ms |
| ADC (LDR, B0, B1) | 100 ms |
| Leitura do RTC | 500 ms |
| Atualização do LCD | 500 ms (100 ms na tela 4) |

## Funções

| Função | Linha | Descrição |
|---|---|---|
| `clock_init` | 75 | Liga o HSE (8 MHz), configura PLL ×9 (72 MHz), APB1 /2 (36 MHz) e 2 wait states da Flash. Se o HSE não partir, continua a 8 MHz. Configura o SysTick a 1 kHz (sem interrupção) e habilita o contador de ciclos DWT. |
| `delay_ms` | 105 | Atraso bloqueante em ms, pelo flag COUNTFLAG do SysTick. |
| `delay_us` | 111 | Atraso bloqueante em µs, pelo contador de ciclos DWT_CYCCNT. |
| `agora_ms` | 119 | Milissegundos desde o início, acumulados a partir do DWT_CYCCNT. Deve ser chamada a menos de ~59 s de intervalo (estouro de 32 bits a 72 MHz); o laço principal a chama a cada volta. |
| `lcd_pulso_e` | 143 | Pulso no pino E (enable) do HD44780. |
| `lcd_nibble` | 150 | Coloca 4 bits em DB4–DB7 (um único acesso ao BSRR) e pulsa E. |
| `lcd_byte` | 161 | Envia um byte (comando ou dado, conforme RS) em dois nibbles. |
| `lcd_cmd` | 170 | Envia um comando; limpar (0x01) e home (0x02) esperam 2 ms. |
| `lcd_init` | 176 | Configura os pinos e executa a sequência de inicialização em modo de 4 bits (2 linhas, 5×8, display ligado, sem cursor). |
| `lcd_linha` | 202 | Escreve uma linha (0 ou 1) de 16 colunas, completando com espaços. |
| `adc_init` | 217 | Habilita o ADC1, coloca os pinos em modo analógico, ADCCLK = PCLK2/6 = 12 MHz, amostragem de 239,5 ciclos e calibração. |
| `adc_uma` | 240 | Uma conversão por software (SWSTART) com timeout. Retorna 0xFFFF em erro. |
| `adc_media` | 248 | Seleciona o canal, descarta a 1ª conversão (troca de canal) e retorna a média de n amostras. |
| `i2c_config` | 270 | Configura o I2C1 em modo padrão, 100 kHz (CCR = PCLK1/200 kHz) e TRISE. |
| `i2c_recupera` | 280 | Reset por software (SWRST) do I2C1 e reconfiguração; usada após timeout. |
| `i2c_init` | 287 | Habilita os clocks, coloca PB6/PB7 em função alternativa open-drain e configura o I2C1. |
| `i2c_sr1` | 295 | Espera um flag do registrador SR1, com timeout. |
| `i2c_livre` | 302 | Espera o barramento ficar livre (BUSY = 0), com timeout. |
| `i2c_start_end` | 309 | Gera START, envia o endereço e espera ADDR ou NACK (AF). |
| `i2c_ler_reg` | 323 | Lê n ≥ 3 bytes a partir de um registrador do escravo, seguindo a sequência do STM32F1 para 3 ou mais bytes (NACK e STOP antes do último). Recupera o barramento em caso de timeout. |
| `bcd` | 364 | Converte BCD para binário. |
| `ds3231_ler` | 366 | Lê 19 bytes (0x00–0x12) do DS3231 (endereço 0x68): hora, data, flag OSF e temperatura (resolução de 0,25 °C). Valida as faixas dos campos. |
| `us_init` | 396 | Configura o TIM4 a 1 MHz (PSC = HCLK/1 MHz − 1), PB8 como saída (TRIG) e PB9 como entrada (ECHO). |
| `us_medir` | 409 | Pulso de 10 µs no TRIG, mede a largura do ECHO e calcula mm = largura × 343 / 2000. Faixa válida: 20 a 4000 mm; timeout de 30 ms em cada espera. |
| `linha_ativa` | 442 | Ativa (saída em 0) ou libera (entrada flutuante) uma linha da matriz de botões. |
| `botoes_init` | 455 | Linhas em entrada flutuante; colunas PC13–PC15 como entrada com pull-up interno. |
| `botoes_varre` | 468 | Varredura da matriz 3×3. O bit (linha×3 + coluna) fica em 1 quando a tecla está pressionada. |
| `botoes_ler` | 486 | Debounce: duas varreduras separadas por 5 ms; vale o AND das duas. |
| `led_set` | 502 | Liga ou desliga o LED i (0 a 5), respeitando LED_ATIVO_ALTO. |
| `leds_init` | 509 | Desliga o JTAG e mantém o SWD (libera PA15, PB3 e PB4) e configura as 6 saídas dos LEDs. |
| `tarefa_botoes` | 530 | Lê a matriz (com debounce) e acende cada LED enquanto sua tecla Tn ou a chave Sn correspondente estiver ativa. |
| `ps` | 546 | Copia uma string para o buffer e devolve o novo ponteiro. |
| `pu` | 548 | Escreve um inteiro sem sinal em decimal. |
| `p2` | 556 | Escreve um número com 2 dígitos (zeros à esquerda). |
| `p4` | 563 | Escreve um número com 4 dígitos (zeros à esquerda). |
| `p_volts` | 572 | Escreve uma tensão em mV no formato "1.65V". |
| `p_temp` | 582 | Escreve uma temperatura em centésimos de °C no formato "25.2C". |
| `tarefa_adc` | 613 | Lê cada canal (média de 32 amostras) e aplica um filtro passa-baixas IIR, y += (x − y)/8, em ponto fixo ×16. |
| `mostra_rtc` | 628 | Tela do RTC: hora, temperatura e data; "!" se o oscilador parou ou a data é inválida; mensagem de erro de I2C quando falha. |
| `mostra_dist` | 657 | Tela do HC-SR04: distância em cm e largura do eco em µs; mensagens de erro (sem eco, sem objeto, fora da faixa). |
| `mostra_ldr` | 679 | Tela do LDR: valor bruto (0–4095), tensão e percentual. |
| `mostra_pot_linha` | 701 | Escreve uma linha com valor bruto e tensão de um potenciômetro. |
| `mostra_pots` | 716 | Tela dos dois potenciômetros (B0 e B1). |
| `mostra_botoes` | 722 | Tela das teclas: T1–T6 pressionadas e S1–S3 (ou 9 bits crus da matriz com MATRIZ_DEBUG = 1). |
| `main` | 754 | Inicializa os periféricos e executa o laço cooperativo: matriz/LEDs (20 ms), HC-SR04 (200 ms), ADC (100 ms), RTC (500 ms) e LCD (500 ms; 100 ms na tela de botões). |

## Tamanho do binário

Compilado com `arm-none-eabi-gcc` 13.3.1, `-Wall -Werror`, sem otimização (build Debug).

| Região | Uso | Ocupação |
|---|---|---|
| Flash (text + data) | 7696 bytes | 11.7% de 64 kB |
| RAM (data + bss) | 1640 bytes | 8.0% de 20 kB (sem contar pilha e heap) |

## Compilar e gravar

Veja o [README geral](../README.md#compilar-e-gravar).

## Observações

- PC13 é a coluna c2 da matriz; por isso este projeto não tem o LED de batimento do projeto básico.
- O cabeçalho do arquivo informa "NAO testado fisicamente".
