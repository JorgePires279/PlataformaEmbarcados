# Plataforma Didática para o Ensino de Sistemas Embarcados – STM32F103C8T6

**Projeto de Trabalho de Conclusão de Curso – Engenharia da Computação**

**Autor:** Jorge Luiz Madeira Pires  
**Orientador:** Cleber Lourenço Izidoro  
**Instituição:** Centro Universitário UniSATC

---

## 1. Sobre o projeto

Este repositório reúne os arquivos e a documentação técnica desenvolvidos no Trabalho de Conclusão de Curso (TCC), cujo objetivo é desenvolver uma plataforma de hardware didática, modular e expansível para auxiliar no ensino prático de Sistemas Embarcados.

A plataforma utiliza como unidade central de processamento o microcontrolador **STM32F103C8T6**, presente na placa de desenvolvimento Blue Pill.

A proposta consiste em integrar diferentes periféricos, sensores, atuadores e interfaces de comunicação em uma única placa de circuito impresso (PCB), permitindo que estudantes realizem experimentos envolvendo programação de microcontroladores, aquisição de sinais, acionamento de dispositivos e comunicação entre módulos eletrônicos.

O projeto busca reduzir a necessidade de montagens individuais em protoboard, facilitando a organização dos experimentos e permitindo maior concentração nas atividades de programação e compreensão dos sistemas eletrônicos.

## 2. Arquitetura da plataforma

A plataforma foi projetada para reunir diferentes recursos utilizados em atividades práticas de Sistemas Embarcados.

### Microcontrolador

- STM32F103C8T6 – Blue Pill.
- Núcleo ARM Cortex-M3 de 32 bits.
- Frequência de operação de até 72 MHz.
- Conversores analógico-digitais (ADC).
- Temporizadores e recursos de PWM.
- Interfaces de comunicação serial.

### Interfaces de comunicação

- **SPI:** PA7 (MOSI), PA6 (MISO), PA5 (SCK) e PA4 (CS).
- **I2C:** PB7 (SDA) e PB6 (SCL).
- **UART:** PA9 (TX) e PA10 (RX).
- **RS-485:** interface diferencial por meio de transceptor dedicado.

A utilização de algumas linhas pode envolver compartilhamento de pinos, conforme a configuração dos circuitos e dos seletores previstos no projeto eletrônico.

### Periféricos e dispositivos

A placa contempla os seguintes recursos:

| Categoria | Componentes |
|---|---|
| Interface visual | Display LCD 16x2 |
| Sensores | HC-SR04, DHT11 e LDR |
| Comunicação | RS-485, SPI, I2C e UART |
| Memória | EEPROM 24AA256 |
| Identificação | Leitor RFID RC522 |
| Temporização | Módulo RTC |
| Entradas | Botões táteis e chaves de seleção |
| Saídas | LEDs, buzzer e relé |
| Entradas isoladas | Optoacopladores 4N25 |
| Alimentação | Reguladores de 5 V e 3,3 V |

A implementação e a compatibilidade de cada periférico devem ser verificadas no esquemático e na documentação correspondente.

## 3. Desenvolvimento eletrônico

O desenvolvimento do circuito eletrônico e da placa de circuito impresso foi realizado utilizando o software **Altium Designer**.

O processo de desenvolvimento compreendeu:

1. Levantamento dos requisitos da plataforma.
2. Seleção dos componentes eletrônicos.
3. Definição das conexões e dos periféricos.
4. Elaboração do esquemático eletrônico.
5. Desenvolvimento do layout da PCB.
6. Organização dos componentes e trilhas.
7. Preparação dos arquivos para fabricação.
8. Fabricação e montagem da placa.
9. Preparação dos procedimentos de verificação e testes.

O projeto foi organizado em blocos funcionais, facilitando a identificação dos circuitos e a compreensão das conexões entre o microcontrolador e os periféricos.

## 4. Organização do repositório

Os arquivos disponibilizados neste repositório destinam-se à documentação e ao acompanhamento do desenvolvimento da plataforma.

A organização prevista contempla:

- **Documentação:** informações técnicas e orientações de utilização.
- **Esquemáticos:** diagramas elétricos desenvolvidos no Altium Designer.
- **PCB:** arquivos de layout da placa de circuito impresso.
- **Modelo 3D:** representação tridimensional da placa.
- **BOM:** relação de componentes eletrônicos e orçamento.
- **Fabricação:** registros e arquivos relacionados à confecção da PCB.
- **Firmware:** exemplos de programação e configurações dos periféricos, conforme forem disponibilizados.

A disponibilidade de cada arquivo depende da versão publicada no repositório.

## 5. Ferramentas utilizadas

- Altium Designer: desenvolvimento do esquemático e layout da PCB.
- STM32CubeIDE: desenvolvimento e configuração do firmware.
- STM32CubeMX: configuração dos periféricos e pinos do microcontrolador, quando aplicável.
- Git e GitHub: controle de versão e disponibilização dos arquivos do projeto.

## 6. Orçamento do projeto

O orçamento elaborado para o desenvolvimento da plataforma apresenta custo estimado de:

**R$ 213,00 – componentes eletrônicos.**

O levantamento contempla o microcontrolador, circuitos integrados, sensores, atuadores, componentes passivos, conectores e demais elementos necessários à composição da plataforma.

Os valores correspondem à estimativa registrada no orçamento do projeto e podem variar conforme fornecedor, disponibilidade e condições de aquisição.

## 7. Orientações de utilização

Antes de realizar qualquer montagem ou conectar dispositivos externos, recomenda-se:

1. Consultar o esquemático eletrônico.
2. Verificar a identificação dos pinos e conectores.
3. Conferir as tensões de alimentação de cada periférico.
4. Verificar a compatibilidade dos sinais com os níveis lógicos de 3,3 V do STM32.
5. Conferir as conexões de GND e alimentação.
6. Realizar a verificação elétrica antes de energizar a placa.
7. Desenvolver e validar os experimentos individualmente.

**Atenção:** determinados módulos possuem entradas ou saídas que podem operar em 5 V. É necessário verificar a compatibilidade elétrica antes de conectá-los diretamente ao microcontrolador.

## 8. Aplicação didática

A plataforma foi concebida como ferramenta de apoio às atividades práticas da disciplina de Sistemas Embarcados.

Sua utilização permite explorar conceitos como:

- Configuração de entradas e saídas digitais.
- Leitura de sensores analógicos.
- Conversão analógico-digital (ADC).
- Geração de sinais PWM.
- Temporizadores e interrupções.
- Comunicação SPI, I2C e UART.
- Comunicação industrial utilizando RS-485.
- Armazenamento de dados em memória não volátil.
- Acionamento de sensores e atuadores.
- Desenvolvimento de aplicações embarcadas.

A proposta é proporcionar um ambiente de experimentação organizado, no qual os estudantes possam compreender a integração entre hardware e software.

## 9. Estado de desenvolvimento

O repositório acompanha a evolução do projeto, desde a elaboração do circuito eletrônico até a fabricação e montagem da placa.

As etapas de desenvolvimento, montagem e validação devem ser documentadas individualmente, diferenciando os recursos projetados daqueles efetivamente montados e testados.

Novos arquivos, exemplos de firmware e materiais de apoio poderão ser incorporados conforme a evolução do trabalho.

## 10. Finalidade acadêmica

Este projeto foi desenvolvido como parte do Trabalho de Conclusão de Curso de Engenharia da Computação do Centro Universitário UniSATC.

O repositório tem como finalidade documentar o desenvolvimento da plataforma, organizar os arquivos técnicos e possibilitar a consulta e a continuidade dos estudos relacionados ao ensino de Sistemas Embarcados.

**Autor:** Jorge Luiz Madeira Pires  
**Curso:** Engenharia da Computação  
**Orientador:** Cleber Lourenço Izidoro  
**Instituição:** Centro Universitário UniSATC

---

*Repositório desenvolvido para fins acadêmicos e educacionais.*
