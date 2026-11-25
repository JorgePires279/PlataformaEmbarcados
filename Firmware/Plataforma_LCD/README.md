# Plataforma_Basico_LCD

**Básico: LCD 1602 + GPIO + SysTick**

Primeiro teste de funcionamento da plataforma didática utilizando o microcontrolador STM32F103C8T6. O firmware realiza a inicialização e o controle do display LCD 1602 em modo paralelo de 4 bits, o acionamento do LED integrado da Blue Pill e a apresentação de um contador incremental no display.

- Microcontrolador: STM32F103C8T6 (Blue Pill), ARM Cortex-M3.
- Frequência de operação prevista: 8 MHz.
- Linguagem: C, utilizando somente registradores.
- Biblioteca: `stdint.h`.
- Código desenvolvido sem HAL e sem `printf`.
- Código-fonte: `Sources/main.c`.
- Ambiente de desenvolvimento: STM32CubeIDE.
- Display: LCD 1602, compatível com HD44780.

## Pinagem

| Pino | Função | Periférico |
|---|---|---|
| PA4 | LCD – DB4 | GPIO |
| PA5 | LCD – DB5 | GPIO |
| PA6 | LCD – DB6 | GPIO |
| PA7 | LCD – DB7 | GPIO |
| PB12 | LCD – RS | GPIO |
| PB14 | LCD – E (Enable) | GPIO |
| PC13 | LED integrado da Blue Pill | GPIO |

Os pinos de comunicação com o LCD são configurados como saídas digitais push-pull de 2 MHz.

O pino R/W do LCD deve permanecer conectado ao GND, considerando que o firmware realiza somente operações de escrita.

## Telas do LCD 1602

### Tela de inicialização

```
TCC STM32
LCD 1602 OK
```

### Tela de funcionamento

```
TCC STM32
Tempo: 0001 s
```

A primeira linha permanece fixa, enquanto a segunda apresenta um contador incremental de quatro dígitos.

## Registradores utilizados

| Registrador | Endereço | Função |
|---|---|---|
| RCC_APB2ENR | 0x40021018 | Habilitação dos clocks dos GPIOs |
| GPIOA_CRL | 0x40010800 | Configuração dos pinos PA0–PA7 |
| GPIOA_ODR | 0x4001080C | Controle das saídas do GPIOA |
| GPIOB_CRH | 0x40010C04 | Configuração dos pinos PB8–PB15 |
| GPIOB_ODR | 0x40010C0C | Controle das saídas do GPIOB |
| GPIOC_CRH | 0x40011004 | Configuração dos pinos PC8–PC15 |
| GPIOC_ODR | 0x4001100C | Controle das saídas do GPIOC |
| SYST_CSR | 0xE000E010 | Controle e estado do SysTick |
| SYST_RVR | 0xE000E014 | Valor de recarga do SysTick |
| SYST_CVR | 0xE000E018 | Valor atual do SysTick |

## Temporização

A temporização é realizada pelo SysTick do processador ARM Cortex-M3, utilizando atrasos bloqueantes e a leitura da flag COUNTFLAG.

Para uma frequência de 8 MHz, o registrador de recarga é configurado com o valor 7999, correspondendo a um intervalo nominal de 1 ms.

| Tarefa | Temporização |
|---|---|
| Estabilização inicial do LCD | 50 ms |
| Pulso de Enable em nível alto | 1 ms |
| Pulso de Enable em nível baixo | 1 ms |
| Atraso adicional após envio de byte | 2 ms |
| Alternância do LED PC13 | Atraso de 500 ms por iteração |
| Incremento do contador | Uma vez por iteração |
| Atualização do LCD | Uma vez por iteração |

O programa utiliza um laço principal sequencial, sem interrupções e sem escalonamento cooperativo.

## Funções

| Função | Descrição |
|---|---|
| `delay_ms()` | Gera atrasos bloqueantes em milissegundos utilizando o SysTick e a flag COUNTFLAG. |
| `gpio_init()` | Habilita os clocks dos GPIOs A, B e C, configura os pinos como saídas push-pull e inicializa seus estados. |
| `lcd_pulse_enable()` | Gera um pulso no pino E do LCD, permitindo a transferência das informações. |
| `lcd_write_nibble()` | Envia quatro bits de informação pelos pinos PA4, PA5, PA6 e PA7. |
| `lcd_write_byte()` | Envia um byte ao LCD, dividido em dois grupos de quatro bits, controlando o pino RS. |
| `lcd_command()` | Envia comandos de configuração e controle ao LCD. |
| `lcd_data()` | Envia caracteres para apresentação no LCD. |
| `lcd_init()` | Executa a sequência de inicialização do LCD em modo de quatro bits. |
| `lcd_set_cursor()` | Posiciona o cursor na linha e coluna desejadas. |
| `lcd_print()` | Percorre uma string e envia seus caracteres individualmente ao LCD. |
| `main()` | Inicializa os periféricos, apresenta a mensagem inicial, alterna o LED PC13 e atualiza o contador no display. |

## Comandos utilizados no LCD

| Comando | Função |
|---|---|
| 0x03 | Sequência inicial de configuração |
| 0x02 | Seleção do modo de quatro bits durante a inicialização |
| 0x28 | Interface de quatro bits, duas linhas e matriz 5×8 |
| 0x0C | Display ligado e cursor desligado |
| 0x01 | Limpeza do display |
| 0x06 | Incremento automático do cursor |
| 0x80 | Definição do endereço DDRAM para posicionamento do cursor |

## Funcionamento do firmware

1. Habilita os clocks dos GPIOs A, B e C.
2. Configura os pinos de comunicação com o LCD.
3. Configura o LED integrado da Blue Pill.
4. Inicializa o LCD em modo de quatro bits.
5. Apresenta a mensagem inicial da plataforma.
6. Inicia o laço principal de execução.
7. Alterna o estado do LED PC13.
8. Executa um atraso de 500 ms.
9. Incrementa o contador.
10. Atualiza a segunda linha do LCD.
11. Repete continuamente o processo.

## Compilar e gravar

1. Abrir o projeto no STM32CubeIDE.
2. Verificar a seleção do microcontrolador STM32F103C8T6.
3. Compilar o projeto utilizando a opção Build Project.
4. Conectar o programador ST-Link à Blue Pill.
5. Realizar a gravação do firmware utilizando Run ou Debug.
6. Verificar o funcionamento do LCD e do LED PC13.

## Observações

- O firmware realiza o acesso direto aos registradores do microcontrolador, sem utilização da biblioteca HAL.
- A temporização pressupõe uma frequência de operação de 8 MHz.
- O LED integrado da Blue Pill é ativo em nível baixo.
- O LCD utiliza comunicação paralela de quatro bits.
- O contador é apresentado com quatro dígitos, de 0000 a 9999.
- O contador é incrementado a cada iteração do laço principal, não correspondendo precisamente a segundos reais.
- Os comentários internos da função `lcd_write_nibble()` apresentam uma inversão na indicação dos pinos, porém as definições e operações do código utilizam PA4 como DB4 e PA7 como DB7.
- O RTC, o ADC, o sensor ultrassônico, o LDR e os potenciômetros não estão implementados nesta versão.
- O tamanho do binário e o consumo de memória Flash e RAM deverão ser verificados após a compilação.
- O funcionamento físico deverá ser validado na plataforma didática.

## Objetivo

Validar individualmente o funcionamento do LCD 1602, das saídas digitais e da temporização por SysTick, servindo como base para a implementação dos demais periféricos da plataforma didática de sistemas embarcados.