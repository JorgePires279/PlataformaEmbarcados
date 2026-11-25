# Arquitetura do firmware

Todos os projetos seguem a mesma organização em um único arquivo `Sources/main.c`, com as seções na ordem abaixo. Não há HAL, interrupções nem RTOS: o programa acessa os registradores diretamente (RM0008) e usa *polling*.

## Camadas

```
main()  ── laço cooperativo (escalonador)
  │
  ├─ tarefas periódicas   tarefa_botoes, tarefa_adc, tarefa_esp
  ├─ telas do LCD         mostra_rtc, mostra_dist, mostra_ldr, ...
  ├─ drivers              lcd_*, adc_*, i2c_*, ds3231_ler, us_*, dht_*, botoes_*, leds_*, io_*, uart_*, g_*
  └─ base                 clock_init, delay_ms/us, agora_ms, gpio_modo/nivel
```

## Base

- **Clock** (`clock_init`): HSE de 8 MHz × PLL 9 = 72 MHz; APB1 = 36 MHz; Flash com 2 wait states. Se o cristal não partir, o sistema segue no HSI (8 MHz) e continua funcionando, mais lento.
- **Tempo**: o SysTick roda a 1 kHz sem interrupção e serve a `delay_ms()`. O contador de ciclos DWT (`DWT_CYCCNT`) serve a `delay_us()` e a `agora_ms()`, que acumula milissegundos. `agora_ms()` precisa ser chamada ao menos a cada ~59 s (o contador de 32 bits estoura em 2^32 / 72 MHz); o laço principal a chama a cada volta.
- **GPIO** (`gpio_modo`, `gpio_nivel`): acesso genérico aos 4 bits CNF/MODE de qualquer pino e escrita atômica por BSRR/BRR.

## Escalonador cooperativo

Em `main()`, depois da inicialização, um laço infinito lê `agora_ms()` e, para cada tarefa, compara com o instante da última execução:

```c
while (1) {
    agora = agora_ms();   /* uint32_t */
    if ((agora - t_rtc) >= 500U) {
        t_rtc = agora;
        rtc_st = ds3231_ler(&rtc);
    }
    if ((agora - t_dht) >= 2000U) {
        t_dht = agora;
        st = dht_ler(&u, &t);
    }
    ...
}
```

A subtração em `uint32_t` dispensa tratamento de estouro. Como as tarefas não são interrompidas, o tempo de cada uma deve ser curto. As funções que bloqueiam por mais tempo são `dht_ler()` (cerca de 24 ms, a cada 2 s), `us_medir()` (até 30 ms por espera; tipicamente menos de 25 ms) e `botoes_ler()` (5 ms de debounce a cada 20 ms).

## Drivers

| Módulo | Detalhes |
|---|---|
| LCD 1602 | HD44780 em 4 bits, escrita apenas (RW ligado a GND). `lcd_nibble()` altera DB4–DB7 em um único acesso ao BSRR. As linhas são montadas em buffers de 16 caracteres por `ps`, `pu`, `p2`, `p4`, `p_volts` e `p_temp`, sem `printf`. |
| ADC | ADC1 por software, 239,5 ciclos de amostragem a 12 MHz. `adc_media()` descarta a primeira conversão após trocar de canal. `tarefa_adc()` faz média de 32 amostras e filtro IIR `y += (x − y)/8` em ponto fixo ×16. |
| I2C / DS3231 | I2C1 a 100 kHz, por *polling* com timeout em todas as esperas e recuperação por SWRST. `i2c_ler_reg()` segue a sequência do STM32F1 para leituras de 3 ou mais bytes. `ds3231_ler()` lê os registradores 0x00–0x12 de uma vez e valida os campos. |
| HC-SR04 | TIM4 a 1 MHz. Pulso de 10 µs no TRIG e medição da largura do ECHO; distância em mm = µs × 343 / 2000. Faixa válida: 20 a 4000 mm. |
| DHT11 | Protocolo de 1 fio temporizado pelo TIM4: início de 20 ms, resposta, 40 bits e checksum. |
| Matriz 3×3 | Varredura por linhas (saída em 0) e leitura de colunas com pull-up. Debounce por duas leituras separadas por 5 ms. |
| LEDs | JTAG desligado e SWD mantido para liberar PA15, PB3 e PB4. |
| E/S isoladas | PB10/PB11 começam como entrada. Só viram saída depois de uma confirmação do usuário (T6 na tela SAÍDAS). |
| USART1 e ESP-01 | 8N1 por *polling*; `tarefa_esp()` procura o baud rate com comandos `AT` e depois monitora o módulo. |
| JLX ST7567 | SPI1 em modo mestre, fonte 6×8 em `charset[]`, 21 colunas por linha. |

## Interface do usuário

O LCD 1602 mostra uma tela por vez; a tela muda sozinha com o tempo (ver o README de cada projeto). O firmware completo tem 8 telas, sendo duas interativas (SAÍDAS e ENTRADAS). O tempo de cada tela e a regra de troca ficam na seção "controle de página" do laço principal.

## Como acrescentar um periférico

1. Escreva `xxx_init()` (clock, pinos, registradores) e uma função de leitura que retorne um código de erro.
2. Chame `xxx_init()` em `main()`, antes do laço.
3. Acrescente uma tarefa periódica no laço (variável `t_xxx` e período).
4. Acrescente uma função `mostra_xxx()` e uma entrada no `switch` de páginas, ajustando `N_PAGINAS`.
5. Compile: `-Wall -Werror` acusa variáveis e funções não usadas.
