# Pinagem

Pinagem da plataforma conforme o firmware. Vale para os projetos `Plataforma_Basico_LCD_RTC_US_ADC`, `Plataforma_Botoes_LEDs`, `Plataforma_DHT11` e `Plataforma_Firmware_Completo`; o projeto `Plataforma_UART_ESP01_JLX` usa outra pinagem, descrita no final.

## Pinagem padrão (firmware completo)

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

## Matriz de botões 3×3

Linha `l` (0 a 2) = PA(2 − l); coluna `c` (0 a 2) = PC(15 − c). As linhas não selecionadas ficam em entrada flutuante; as colunas usam o pull-up interno (a placa não tem pull-up externo). Uma tecla pressionada leva a coluna a 0 quando sua linha está ativa (em 0).

| | c0 (PC15) | c1 (PC14) | c2 (PC13) |
|---|---|---|---|
| **l0 (PA2)** | T2 | T1 | S1 |
| **l1 (PA1)** | T4 | T3 | S2 |
| **l2 (PA0)** | T5 | T6 | S3 |

O bit `l×3 + c` da máscara retornada por `botoes_ler()` fica em 1 para a tecla pressionada. A tabela `tac_bit[] = { 1, 0, 4, 3, 6, 7 }` converte T1 a T6 para o bit correspondente; S1, S2 e S3 são os bits 2, 5 e 8.

## JTAG e SWD

`leds_init()` escreve `AFIO_MAPR.SWJ_CFG = 010` (JTAG desligado, SWD ativo). Isso libera PA15, PB3 e PB4 (LED4, LED5 e LED6) e mantém PA13/PA14 para o ST-Link. Como o JTAG fica desligado, `leds_init()` deve ser chamada antes de configurar esses pinos.

## Pinos compartilhados por jumpers

| Pinos | Funções disponíveis |
|---|---|
| PA4–PA7 | LCD 1602 (DB4–DB7) ou SPI1 (NSS, SCK, MISO, MOSI) |
| PB10, PB11 | Entradas isoladas E1/E2 ou saídas Q1/Q2 (transistores NPN) |
| PB13 | Controle de direção do ISL32475 (RS485) ou linha de reset do módulo RFID |



