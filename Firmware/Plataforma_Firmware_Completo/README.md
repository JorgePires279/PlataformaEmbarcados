# Plataforma_Firmware_Completo

**Firmware completo**

Reúne todos os periféricos em um único programa: LCD, RTC, ultrassom, LDR, potenciômetros, matriz de botões, LEDs, DHT11, relé, saídas/entradas por transistor/optoacoplador (PB10/PB11) e buzzer opcional. São 8 telas no LCD.

- Microcontrolador: STM32F103C8T6 (Blue Pill), ARM Cortex-M3, HSE de 8 MHz → 72 MHz
- Linguagem: C (gnu11), somente registradores, sem HAL e sem `printf`
- Código-fonte: `Sources/main.c` (1090 linhas, arquivo único)
- Projeto: STM32CubeIDE com CMake (`CMakeLists.txt`, `CMakePresets.json`)

## Pinagem

| Pino | Função | Periférico |
|---|---|---|
| PA0, PA1, PA2 | Matriz de botões, linhas l2, l1, l0 | GPIO |
| PA3 | LDR | ADC1 canal 3 |
| PA4–PA7 | LCD 1602, DB4–DB7 | GPIO |
| PA8 | Relé | GPIO |
| PA11, PA12, PA15 | LED1, LED2, LED4 | GPIO |
| PA13, PA14 | SWD (programação) | — |
| PB0 | Potenciômetro B0 | ADC1 canal 8 |
| PB1 | Potenciômetro B1 | ADC1 canal 9 |
| PB3, PB4, PB5 | LED5, LED6, LED3 | GPIO (JTAG desligado) |
| PB6, PB7 | I2C1: SCL, SDA (DS3231, endereço 0x68) | I2C1 |
| PB8 | HC-SR04: TRIG | GPIO |
| PB9 | HC-SR04: ECHO | GPIO |
| PB10, PB11 | E1/E2 (entradas) ou Q1/Q2 (saídas), conforme jumpers | GPIO |
| PB12 | LCD 1602, RS | GPIO |
| PB14 | LCD 1602, E | GPIO |
| PB15 | DHT11, DATA | GPIO + TIM4 |
| PC13, PC14, PC15 | Matriz de botões, colunas c2, c1, c0 | GPIO (pull-up) |

Veja também [docs/PINAGEM.md](../docs/PINAGEM.md).

## Opções de compilação

Definidas no início de `Sources/main.c`.

| Macro | Padrão | Efeito |
|---|---|---|
| LED_ATIVO_ALTO | 1 | 1 = LED acende com o pino em nível alto |
| MATRIZ_DEBUG | 1 | 1 = a tela de botões mostra os 9 bits crus da matriz |
| RELE_ATIVO_ALTO | 1 | 1 = relé liga com PA8 em nível alto |
| ENTRADA_ATIVA_ALTA | 1 | 1 = E1/E2 ativas em nível alto (pull-down); 0 = nível baixo (pull-up) |
| E_PULA_TELA | 1 | 1 = o LCD pula para a tela ENTRADAS quando E1 ou E2 é acionada |
| USAR_BUZZER | 0 | 1 = habilita o buzzer (ajuste BUZZ_PORTA e BUZZ_PINO) |
| BUZZ_PORTA / BUZZ_PINO | GPIOA / 9 | Porta e pino do buzzer (exemplo, troque pelo pino real) |
| BUZZ_ATIVO_ALTO | 1 | 1 = buzzer soa com o pino em nível alto |

## Telas do LCD 1602

| Tela | Conteúdo |
|---|---|
| 0 | Hora, temperatura e data |
| 1 | Distância |
| 2 | LDR |
| 3 | Potenciômetros |
| 4 | Botões |
| 5 | DHT11 |
| 6 | SAÍDAS: relé, Q1, Q2, buzzer |
| 7 | ENTRADAS: E1 e E2 |

Telas 0 a 5 trocam a cada 3 s; as telas 6 e 7 ficam 10 s e qualquer tecla T estende o tempo. Quando E1 ou E2 é acionada, o LCD pula para a tela 7 (`E_PULA_TELA`).

### Tela SAÍDAS (tela 6)

| Tecla | Ação |
|---|---|
| T1 | Liga/desliga o relé (PA8) |
| T2 | Liga/desliga Q1 (PB10), depois de T6 |
| T3 | Liga/desliga Q2 (PB11), depois de T6 |
| T4 | Liga/desliga o buzzer (se USAR_BUZZER = 1) |
| T5 | Desliga tudo |
| T6 | Confirma que os jumpers estão em SAÍDA e habilita PB10/PB11 como saída |

## Teclas e LEDs

Matriz 3×3: `T2 T1 S1 / T4 T3 S2 / T5 T6 S3`.

| LED | Acende enquanto estiver ativo |
|---|---|
| LED1 (PA11) | T1, S1 ou E1 |
| LED2 (PA12) | T2, S1 ou E2 |
| LED3 (PB5) | T3 ou S2 |
| LED4 (PA15) | T4 ou S2 |
| LED5 (PB3) | T5 ou S3 |
| LED6 (PB4) | T6 ou S3 |

## Temporização

Escalonamento cooperativo: um laço em `main()` compara `agora_ms()` com o instante da última execução de cada tarefa.

| Tarefa | Período |
|---|---|
| Matriz de botões, entradas E1/E2 e LEDs | 20 ms |
| HC-SR04 | 200 ms |
| ADC (LDR, B0, B1) | 100 ms |
| Leitura do RTC | 500 ms |
| DHT11 | 2000 ms |
| Atualização do LCD | 500 ms (100 ms nas telas 4, 6 e 7) |
| Troca de tela | 3 s (telas 0 a 5); 10 s (telas 6 e 7) |

## Funções

| Função | Linha | Descrição |
|---|---|---|
| `clock_init` | 91 | Liga o HSE (8 MHz), configura PLL ×9 (72 MHz), APB1 /2 (36 MHz) e 2 wait states da Flash. Se o HSE não partir, continua a 8 MHz. Configura o SysTick a 1 kHz (sem interrupção) e habilita o contador de ciclos DWT. |
| `delay_ms` | 121 | Atraso bloqueante em ms, pelo flag COUNTFLAG do SysTick. |
| `delay_us` | 127 | Atraso bloqueante em µs, pelo contador de ciclos DWT_CYCCNT. |
| `agora_ms` | 135 | Milissegundos desde o início, acumulados a partir do DWT_CYCCNT. Deve ser chamada a menos de ~59 s de intervalo (estouro de 32 bits a 72 MHz); o laço principal a chama a cada volta. |
| `gpio_modo` | 149 | Configura os 4 bits CNF/MODE de um pino qualquer (CRL ou CRH). |
| `gpio_nivel` | 161 | Escreve o nível de um pino de saída (BSRR/BRR, atômico). |
| `lcd_pulso_e` | 177 | Pulso no pino E (enable) do HD44780. |
| `lcd_nibble` | 184 | Coloca 4 bits em DB4–DB7 (um único acesso ao BSRR) e pulsa E. |
| `lcd_byte` | 195 | Envia um byte (comando ou dado, conforme RS) em dois nibbles. |
| `lcd_cmd` | 204 | Envia um comando; limpar (0x01) e home (0x02) esperam 2 ms. |
| `lcd_init` | 210 | Configura os pinos e executa a sequência de inicialização em modo de 4 bits (2 linhas, 5×8, display ligado, sem cursor). |
| `lcd_linha` | 236 | Escreve uma linha (0 ou 1) de 16 colunas, completando com espaços. |
| `adc_init` | 251 | Habilita o ADC1, coloca os pinos em modo analógico, ADCCLK = PCLK2/6 = 12 MHz, amostragem de 239,5 ciclos e calibração. |
| `adc_uma` | 274 | Uma conversão por software (SWSTART) com timeout. Retorna 0xFFFF em erro. |
| `adc_media` | 282 | Seleciona o canal, descarta a 1ª conversão (troca de canal) e retorna a média de n amostras. |
| `i2c_config` | 304 | Configura o I2C1 em modo padrão, 100 kHz (CCR = PCLK1/200 kHz) e TRISE. |
| `i2c_recupera` | 314 | Reset por software (SWRST) do I2C1 e reconfiguração; usada após timeout. |
| `i2c_init` | 321 | Habilita os clocks, coloca PB6/PB7 em função alternativa open-drain e configura o I2C1. |
| `i2c_sr1` | 329 | Espera um flag do registrador SR1, com timeout. |
| `i2c_livre` | 336 | Espera o barramento ficar livre (BUSY = 0), com timeout. |
| `i2c_start_end` | 343 | Gera START, envia o endereço e espera ADDR ou NACK (AF). |
| `i2c_ler_reg` | 357 | Lê n ≥ 3 bytes a partir de um registrador do escravo, seguindo a sequência do STM32F1 para 3 ou mais bytes (NACK e STOP antes do último). Recupera o barramento em caso de timeout. |
| `bcd` | 398 | Converte BCD para binário. |
| `ds3231_ler` | 400 | Lê 19 bytes (0x00–0x12) do DS3231 (endereço 0x68): hora, data, flag OSF e temperatura (resolução de 0,25 °C). Valida as faixas dos campos. |
| `us_init` | 430 | Configura o TIM4 a 1 MHz (PSC = HCLK/1 MHz − 1), PB8 como saída (TRIG) e PB9 como entrada (ECHO). |
| `us_medir` | 443 | Pulso de 10 µs no TRIG, mede a largura do ECHO e calcula mm = largura × 343 / 2000. Faixa válida: 20 a 4000 mm; timeout de 30 ms em cada espera. |
| `dht_init` | 477 | PB15 como entrada com pull-up interno (repouso do DHT11). |
| `dht_espera` | 485 | Espera enquanto o pino estiver em um nível e retorna a duração em µs (TIM4); 0xFFFF indica timeout. |
| `dht_ler` | 497 | Protocolo de 1 fio do DHT11: pulso de início de 20 ms, resposta do sensor, 40 bits (limiar de 40 µs entre 0 e 1) e checksum. Retorna umidade e temperatura inteiras. |
| `linha_ativa` | 539 | Ativa (saída em 0) ou libera (entrada flutuante) uma linha da matriz de botões. |
| `botoes_init` | 552 | Linhas em entrada flutuante; colunas PC13–PC15 como entrada com pull-up interno. |
| `botoes_varre` | 565 | Varredura da matriz 3×3. O bit (linha×3 + coluna) fica em 1 quando a tecla está pressionada. |
| `botoes_ler` | 583 | Debounce: duas varreduras separadas por 5 ms; vale o AND das duas. |
| `led_set` | 599 | Liga ou desliga o LED i (0 a 5), respeitando LED_ATIVO_ALTO. |
| `leds_init` | 606 | Desliga o JTAG e mantém o SWD (libera PA15, PB3 e PB4) e configura as 6 saídas dos LEDs. |
| `io_aplica` | 641 | Aplica nos pinos o estado desejado de relé, buzzer e Q1/Q2. |
| `io_modo_entrada` | 651 | PB10/PB11 como entrada com pull (modo seguro, padrão). |
| `io_modo_saida` | 662 | PB10/PB11 como saída push-pull, começando em 0. |
| `io_le_entradas` | 673 | Lê E1/E2 (PB10/PB11) quando estão em modo de entrada. |
| `io_init` | 683 | Configura o relé (PA8), o buzzer opcional e coloca PB10/PB11 em modo de entrada. |
| `tarefa_botoes` | 703 | Lê a matriz e as entradas E1/E2, detecta bordas das teclas, executa as ações da tela SAÍDAS (T1 relé, T2 Q1, T3 Q2, T4 buzzer, T5 desliga tudo, T6 confirma jumpers em SAÍDA), registra eventos de E1/E2 e atualiza os LEDs. |
| `ps` | 747 | Copia uma string para o buffer e devolve o novo ponteiro. |
| `pu` | 749 | Escreve um inteiro sem sinal em decimal. |
| `p2` | 757 | Escreve um número com 2 dígitos (zeros à esquerda). |
| `p4` | 764 | Escreve um número com 4 dígitos (zeros à esquerda). |
| `p_volts` | 773 | Escreve uma tensão em mV no formato "1.65V". |
| `p_temp` | 783 | Escreve uma temperatura em centésimos de °C no formato "25.2C". |
| `tarefa_adc` | 818 | Lê cada canal (média de 32 amostras) e aplica um filtro passa-baixas IIR, y += (x − y)/8, em ponto fixo ×16. |
| `mostra_rtc` | 833 | Tela do RTC: hora, temperatura e data; "!" se o oscilador parou ou a data é inválida; mensagem de erro de I2C quando falha. |
| `mostra_dist` | 862 | Tela do HC-SR04: distância em cm e largura do eco em µs; mensagens de erro (sem eco, sem objeto, fora da faixa). |
| `mostra_ldr` | 884 | Tela do LDR: valor bruto (0–4095), tensão e percentual. |
| `mostra_pot_linha` | 906 | Escreve uma linha com valor bruto e tensão de um potenciômetro. |
| `mostra_pots` | 921 | Tela dos dois potenciômetros (B0 e B1). |
| `mostra_botoes` | 927 | Tela das teclas: T1–T6 pressionadas e S1–S3 (ou 9 bits crus da matriz com MATRIZ_DEBUG = 1). |
| `mostra_dht` | 956 | Tela do DHT11: temperatura e umidade; mensagens de erro (sem resposta, timeout, checksum). |
| `mostra_saidas` | 976 | Tela SAÍDAS: estado de relé, Q1, Q2 e buzzer. |
| `mostra_entradas` | 991 | Tela ENTRADAS: estado de E1 e E2. |
| `main` | 1006 | Inicializa os periféricos, mostra a abertura e executa o laço cooperativo com as tarefas da tabela de temporização, além do controle de páginas do LCD (3 s nas telas 0 a 5 e 10 s nas telas 6 e 7). |

## Tamanho do binário

Compilado com `arm-none-eabi-gcc` 13.3.1, `-Wall -Werror`, sem otimização (build Debug).

| Região | Uso | Ocupação |
|---|---|---|
| Flash (text + data) | 10236 bytes | 15.6% de 64 kB |
| RAM (data + bss) | 1664 bytes | 8.1% de 20 kB (sem contar pilha e heap) |

## Compilar e gravar

Veja o [README geral](../README.md#compilar-e-gravar).

## Observações

- PB10/PB11 são entradas (E1/E2) por padrão. Só viram saídas (Q1/Q2) depois que o usuário confirma com T6, na tela SAÍDAS, que os jumpers estão na posição SAÍDA; ao sair da tela voltam a entrada.
- O buzzer vem desabilitado (USAR_BUZZER = 0) porque o pino em BUZZ_PORTA/BUZZ_PINO é apenas um exemplo.
- O cabeçalho do arquivo informa "NAO testado fisicamente".
