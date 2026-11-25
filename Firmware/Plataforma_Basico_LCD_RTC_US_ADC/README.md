# Plataforma_Basico_LCD_RTC_US_ADC

**Básico: LCD + RTC + ultrassom + ADC**

Primeiro teste da plataforma: LCD 1602, relógio DS3231 (I2C), sensor HC-SR04, LDR e dois potenciômetros. Um LED pisca em PC13 como "batimento" do sistema.

- Microcontrolador: STM32F103C8T6 (Blue Pill), ARM Cortex-M3, HSE de 8 MHz → 72 MHz
- Linguagem: C (gnu11), somente registradores, sem HAL e sem `printf`
- Código-fonte: `Sources/main.c` (648 linhas, arquivo único)
- Projeto: STM32CubeIDE com CMake (`CMakeLists.txt`, `CMakePresets.json`)

## Pinagem

| Pino | Função | Periférico |
|---|---|---|
| PA3 | LDR | ADC1 canal 3 |
| PA4–PA7 | LCD 1602: DB4–DB7 | GPIO |
| PB0, PB1 | Potenciômetros B0 e B1 | ADC1 canais 8 e 9 |
| PB6, PB7 | I2C1: SCL, SDA (DS3231, 0x68) | I2C1 |
| PB8, PB9 | HC-SR04: TRIG, ECHO | GPIO + TIM4 |
| PB12, PB14 | LCD 1602: RS, E | GPIO |
| PC13 | LED de batimento (500 ms) | GPIO |

Veja também [docs/PINAGEM.md](../docs/PINAGEM.md).

## Telas do LCD 1602

| Tela | Conteúdo |
|---|---|
| 0 | Hora, temperatura e data (DS3231) |
| 1 | Distância (HC-SR04) |
| 2 | LDR |
| 3 | Potenciômetros B0 e B1 |

## Temporização

Escalonamento cooperativo: um laço em `main()` compara `agora_ms()` com o instante da última execução de cada tarefa.

| Tarefa | Período |
|---|---|
| LED de batimento (PC13) | 500 ms |
| HC-SR04 | 200 ms |
| ADC (LDR, B0, B1) | 100 ms |
| Leitura do RTC | 500 ms |
| Atualização do LCD | 500 ms |

## Funções

| Função | Linha | Descrição |
|---|---|---|
| `clock_init` | 61 | Liga o HSE (8 MHz), configura PLL ×9 (72 MHz), APB1 /2 (36 MHz) e 2 wait states da Flash. Se o HSE não partir, continua a 8 MHz. Configura o SysTick a 1 kHz (sem interrupção) e habilita o contador de ciclos DWT. |
| `delay_ms` | 91 | Atraso bloqueante em ms, pelo flag COUNTFLAG do SysTick. |
| `delay_us` | 97 | Atraso bloqueante em µs, pelo contador de ciclos DWT_CYCCNT. |
| `agora_ms` | 105 | Milissegundos desde o início, acumulados a partir do DWT_CYCCNT. Deve ser chamada a menos de ~59 s de intervalo (estouro de 32 bits a 72 MHz); o laço principal a chama a cada volta. |
| `lcd_pulso_e` | 129 | Pulso no pino E (enable) do HD44780. |
| `lcd_nibble` | 136 | Coloca 4 bits em DB4–DB7 (um único acesso ao BSRR) e pulsa E. |
| `lcd_byte` | 147 | Envia um byte (comando ou dado, conforme RS) em dois nibbles. |
| `lcd_cmd` | 156 | Envia um comando; limpar (0x01) e home (0x02) esperam 2 ms. |
| `lcd_init` | 162 | Configura os pinos e executa a sequência de inicialização em modo de 4 bits (2 linhas, 5×8, display ligado, sem cursor). |
| `lcd_linha` | 188 | Escreve uma linha (0 ou 1) de 16 colunas, completando com espaços. |
| `adc_init` | 203 | Habilita o ADC1, coloca os pinos em modo analógico, ADCCLK = PCLK2/6 = 12 MHz, amostragem de 239,5 ciclos e calibração. |
| `adc_uma` | 226 | Uma conversão por software (SWSTART) com timeout. Retorna 0xFFFF em erro. |
| `adc_media` | 234 | Seleciona o canal, descarta a 1ª conversão (troca de canal) e retorna a média de n amostras. |
| `i2c_config` | 256 | Configura o I2C1 em modo padrão, 100 kHz (CCR = PCLK1/200 kHz) e TRISE. |
| `i2c_recupera` | 266 | Reset por software (SWRST) do I2C1 e reconfiguração; usada após timeout. |
| `i2c_init` | 273 | Habilita os clocks, coloca PB6/PB7 em função alternativa open-drain e configura o I2C1. |
| `i2c_sr1` | 281 | Espera um flag do registrador SR1, com timeout. |
| `i2c_livre` | 288 | Espera o barramento ficar livre (BUSY = 0), com timeout. |
| `i2c_start_end` | 295 | Gera START, envia o endereço e espera ADDR ou NACK (AF). |
| `i2c_ler_reg` | 309 | Lê n ≥ 3 bytes a partir de um registrador do escravo, seguindo a sequência do STM32F1 para 3 ou mais bytes (NACK e STOP antes do último). Recupera o barramento em caso de timeout. |
| `bcd` | 350 | Converte BCD para binário. |
| `ds3231_ler` | 352 | Lê 19 bytes (0x00–0x12) do DS3231 (endereço 0x68): hora, data, flag OSF e temperatura (resolução de 0,25 °C). Valida as faixas dos campos. |
| `us_init` | 382 | Configura o TIM4 a 1 MHz (PSC = HCLK/1 MHz − 1), PB8 como saída (TRIG) e PB9 como entrada (ECHO). |
| `us_medir` | 395 | Pulso de 10 µs no TRIG, mede a largura do ECHO e calcula mm = largura × 343 / 2000. Faixa válida: 20 a 4000 mm; timeout de 30 ms em cada espera. |
| `ps` | 422 | Copia uma string para o buffer e devolve o novo ponteiro. |
| `pu` | 424 | Escreve um inteiro sem sinal em decimal. |
| `p2` | 432 | Escreve um número com 2 dígitos (zeros à esquerda). |
| `p4` | 439 | Escreve um número com 4 dígitos (zeros à esquerda). |
| `p_volts` | 448 | Escreve uma tensão em mV no formato "1.65V". |
| `p_temp` | 458 | Escreve uma temperatura em centésimos de °C no formato "25.2C". |
| `tarefa_adc` | 489 | Lê cada canal (média de 32 amostras) e aplica um filtro passa-baixas IIR, y += (x − y)/8, em ponto fixo ×16. |
| `mostra_rtc` | 504 | Tela do RTC: hora, temperatura e data; "!" se o oscilador parou ou a data é inválida; mensagem de erro de I2C quando falha. |
| `mostra_dist` | 533 | Tela do HC-SR04: distância em cm e largura do eco em µs; mensagens de erro (sem eco, sem objeto, fora da faixa). |
| `mostra_ldr` | 555 | Tela do LDR: valor bruto (0–4095), tensão e percentual. |
| `mostra_pot_linha` | 577 | Escreve uma linha com valor bruto e tensão de um potenciômetro. |
| `mostra_pots` | 592 | Tela dos dois potenciômetros (B0 e B1). |
| `main` | 601 | Inicializa os periféricos, mostra a tela de abertura por 1,5 s e executa o laço cooperativo: LED PC13 (500 ms), HC-SR04 (200 ms), ADC (100 ms), RTC (500 ms) e LCD (500 ms). |

## Tamanho do binário

Compilado com `arm-none-eabi-gcc` 13.3.1, `-Wall -Werror`, sem otimização (build Debug).

| Região | Uso | Ocupação |
|---|---|---|
| Flash (text + data) | 6676 bytes | 10.2% de 64 kB |
| RAM (data + bss) | 1640 bytes | 8.0% de 20 kB (sem contar pilha e heap) |

## Compilar e gravar

Veja o [README geral](../README.md#compilar-e-gravar).

## Observações

- O comentário do código diz que a tela troca a cada 3 s, mas a expressão `(agora / 12000U) % 4U` troca a cada **12 s**. Para 3 s, use `agora / 3000U`.
- O cabeçalho do arquivo informa "NAO testado fisicamente".
