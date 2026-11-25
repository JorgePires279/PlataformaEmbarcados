# Firmware da plataforma didática STM32 (Blue Pill)

Projetos de firmware da plataforma didática modular para o ensino de Sistemas Embarcados (STM32F103C8T6), TCC de Engenharia da Computação do Centro Universitário UniSATC.

Todos os programas são escritos em C, em um único arquivo-fonte, **somente com acesso direto aos registradores** (RM0008), sem HAL e sem `printf`. Cada projeto é independente e acrescenta periféricos ao anterior, do mais simples ao mais completo.

## Projetos

| Projeto | Conteúdo | Flash |
|---|---|---|
| [`Plataforma_Basico_LCD_RTC_US_ADC`](Plataforma_Basico_LCD_RTC_US_ADC/README.md) | Básico: LCD + RTC + ultrassom + ADC | 6.5 kB |
| [`Plataforma_Botoes_LEDs`](Plataforma_Botoes_LEDs/README.md) | Básico + matriz de botões e LEDs | 7.5 kB |
| [`Plataforma_DHT11`](Plataforma_DHT11/README.md) | Botões e LEDs + DHT11 | 8.4 kB |
| [`Plataforma_UART_ESP01_JLX`](Plataforma_UART_ESP01_JLX/README.md) | UART/ESP-01 + LCD gráfico JLX | 10.5 kB |
| [`Plataforma_Firmware_Completo`](Plataforma_Firmware_Completo/README.md) | Firmware completo | 10.0 kB |

O projeto `Plataforma_Firmware_Completo` é o programa que reúne todos os periféricos da placa. Os outros servem como exemplos progressivos e como roteiro de teste de cada bloco.

## Estrutura de cada projeto

```
Plataforma_xxx/
├── Sources/
│   ├── main.c            firmware (arquivo único)
│   ├── syscalls.c        stubs de newlib gerados pelo STM32CubeIDE
│   └── sysmem.c          _sbrk gerado pelo STM32CubeIDE
├── Startup/
│   └── startup_stm32f103c8tx.s   tabela de vetores e Reset_Handler
├── STM32F103C8TX_FLASH.ld        script de ligação (64 kB Flash, 20 kB RAM)
├── CMakeLists.txt                configuração do CMake
├── CMakePresets.json             presets Debug e Release
├── cubeide-gcc.cmake             toolchain arm-none-eabi-gcc
├── .project / .cproject / .settings / *.launch   arquivos do STM32CubeIDE
└── README.md                     documentação do projeto
```

## Documentação

- [docs/ARQUITETURA.md](docs/ARQUITETURA.md): organização do código, escalonamento cooperativo e descrição de cada módulo
- [docs/PINAGEM.md](docs/PINAGEM.md): pinagem completa e diferenças entre os projetos
- `README.md` em cada projeto: pinagem, telas, temporização, índice de funções e tamanho do binário

## Requisitos

- Placa Blue Pill (STM32F103C8T6) com cristal de 8 MHz (HSE)
- Gravador ST-Link (SWD)
- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) com suporte a CMake, ou `arm-none-eabi-gcc` + CMake + Ninja/Make

## Compilar e gravar

### STM32CubeIDE

1. `File > Import > Existing Projects into Workspace` e selecione a pasta do projeto.
2. Compile com `Project > Build Project` (configuração Debug).
3. Grave com `Run > Debug` (usa o arquivo `* Debug.launch`, via ST-Link).

### Linha de comando

```bash
cd Plataforma_Firmware_Completo
cmake --preset Debug
cmake --build --preset Debug
```

Ou, sem presets:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cubeide-gcc.cmake -S . -B Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Debug
```

O resultado é `Debug/<projeto>.elf`. Para gravar:

```bash
STM32_Programmer_CLI -c port=SWD -w Debug/Plataforma_Firmware_Completo.elf -v -rst
# ou, com OpenOCD:
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program Debug/Plataforma_Firmware_Completo.elf verify reset exit"
```

As opções de compilação do CMake incluem `-Wall -Werror`: qualquer aviso interrompe o build. Todos os projetos compilam sem avisos com o `arm-none-eabi-gcc` 13.3.1.

## Autoria

- Autor: Jorge Luiz Madeira Pires
- Orientador: Cleber Lourenço Izidoro
- Instituição: Centro Universitário UniSATC
