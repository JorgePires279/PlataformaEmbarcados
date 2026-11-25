/**
 * main.c - FIRMWARE GERAL (arquivo unico, sem HAL, so registradores)
 * Plataforma didatica STM32 Blue Pill (STM32F103C8T6) - TCC UNISATC
 *
 * Tudo roda ao mesmo tempo; o LCD 1602 troca de pagina a cada 3 s:
 *   Pagina 0: hora, temperatura e data do DS3231
 *   Pagina 1: LDR (PA3) e potenciometro (PB1) com tensao
 *   Pagina 2: distancia do HC-SR04
 *   Pagina 3: ESP-01 (AT) pela UART
 * O LCD grafico JLX (ST7567) mostra um painel com tudo (se estiver ligado).
 *
 * PINOS
 *   LCD 1602 : RS=PA0  E=PA1  DB4=PB5  DB5=PB4  DB6=PB3  DB7=PB0   (R/W no GND)
 *   ADC      : LDR=PA3 (canal 3)   POT2=PB1 (canal 9)   [PB0 = DB7 do LCD, indisponivel]
 *   I2C1     : SCL=PB6  SDA=PB7  (DS3231, endereco 0x68)
 *   USART1   : TX=PA9 (-> RX do ESP-01)  RX=PA10 (<- TX do ESP-01)   115200 / 9600 / ...
 *   RS485    : PB13 em nivel alto (ISL_CTRL) para o RO do ISL32475 nao disputar o PA10
 *   HC-SR04  : TRIG=PB8  ECHO=PB9
 *   JLX      : CS=PA4  SCK=PA5  RST=PA6  SDA=PA7  A0=PB12   (SPI1)
 *   LED      : PC13 (pisca a cada 500 ms)
 * Botoes: NAO usados (PA0/PA1 do LCD conflitam com as linhas BL3/BL2 da matriz).
 *
 * NAO testado fisicamente.
 */
#include <stdint.h>

/* ===== Opcoes: 1 = ativo, 0 = desligado ===== */
#define USAR_JLX             1
#define USAR_ESP             1
#define USAR_HCSR04          1
#define RS485_DESABILITA_RX  1

/* ========================================================================== */
/*  REGISTRADORES (STM32F103, RM0008)                                          */
/* ========================================================================== */
#define REG32 volatile uint32_t
typedef struct { REG32 CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR; } RCC_T;
typedef struct { REG32 ACR, KEYR, OPTKEYR, SR, CR, AR; }                                          FLASH_T;
typedef struct { REG32 CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; }                                     GPIO_T;
typedef struct { REG32 CR1, CR2, SR, DR, CRCPR, RXCRCR, TXCRCR, I2SCFGR, I2SPR; }                 SPI_T;
typedef struct { REG32 CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR; }        TIM_T;
typedef struct { REG32 SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4, HTR, LTR,
                       SQR1, SQR2, SQR3, JSQR, JDR1, JDR2, JDR3, JDR4, DR; }                      ADC_T;
typedef struct { REG32 CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE; }                          I2C_T;
typedef struct { REG32 SR, DR, BRR, CR1, CR2, CR3, GTPR; }                                         USART_T;
typedef struct { REG32 EVCR, MAPR, EXTICR1, EXTICR2, EXTICR3, EXTICR4, MAPR2; }                   AFIO_T;
typedef struct { REG32 CTRL, LOAD, VAL, CALIB; }                                                  SYSTICK_T;

#define RCC       ((RCC_T *)      0x40021000UL)
#define FLASH     ((FLASH_T *)    0x40022000UL)
#define AFIO      ((AFIO_T *)     0x40010000UL)
#define GPIOA     ((GPIO_T *)     0x40010800UL)
#define GPIOB     ((GPIO_T *)     0x40010C00UL)
#define GPIOC     ((GPIO_T *)     0x40011000UL)
#define SPI1      ((SPI_T *)      0x40013000UL)
#define USART1    ((USART_T *)    0x40013800UL)
#define TIM4      ((TIM_T *)      0x40000800UL)
#define ADC1      ((ADC_T *)      0x40012400UL)
#define I2C1      ((I2C_T *)      0x40005400UL)
#define SYSTICK   ((SYSTICK_T *)  0xE000E010UL)
#define DEMCR       (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)

#define RCC_APB2ENR_AFIOEN   (1UL << 0)
#define RCC_APB2ENR_IOPAEN   (1UL << 2)
#define RCC_APB2ENR_IOPBEN   (1UL << 3)
#define RCC_APB2ENR_IOPCEN   (1UL << 4)
#define RCC_APB2ENR_ADC1EN   (1UL << 9)
#define RCC_APB2ENR_SPI1EN   (1UL << 12)
#define RCC_APB2ENR_USART1EN (1UL << 14)
#define RCC_APB1ENR_TIM4EN   (1UL << 2)
#define RCC_APB1ENR_I2C1EN   (1UL << 21)

/* ========================================================================== */
/*  CLOCK (72 MHz) E ATRASOS                                                   */
/* ========================================================================== */
static uint32_t g_hclk_hz  = 8000000UL;
static uint32_t g_pclk1_hz = 8000000UL;

__attribute__((weak)) void SystemInit(void) { }   /* o startup chama; o clock e feito em clock_init() */

static void clock_init(void)
{
    uint32_t t, ppre1;

    RCC->CR |= (1UL << 16);                                   /* HSEON */
    for (t = 0; t < 200000UL; t++) if (RCC->CR & (1UL << 17)) break;

    if (RCC->CR & (1UL << 17)) {
        FLASH->ACR = 0x12;                                    /* prefetch + 2 wait states */
        RCC->CFGR = (1UL << 16) | (7UL << 18) | (4UL << 8);   /* PLL = HSE x9, APB1 = /2 */
        RCC->CR |= (1UL << 24);                               /* PLLON */
        for (t = 0; t < 200000UL; t++) if (RCC->CR & (1UL << 25)) break;
        if (RCC->CR & (1UL << 25)) {
            RCC->CFGR = (RCC->CFGR & ~3UL) | 2UL;             /* SYSCLK = PLL */
            for (t = 0; t < 200000UL; t++) if (((RCC->CFGR >> 2) & 3UL) == 2UL) break;
            g_hclk_hz = 72000000UL;
        }
    }
    ppre1 = (RCC->CFGR >> 8) & 7UL;
    g_pclk1_hz = (ppre1 >= 4UL) ? (g_hclk_hz >> (ppre1 - 3UL)) : g_hclk_hz;

    SYSTICK->LOAD = (g_hclk_hz / 1000UL) - 1UL;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = 5UL;                                      /* SysTick 1 ms, sem interrupcao */

    DEMCR |= (1UL << 24);                                     /* contador de ciclos (DWT) */
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1UL;
}

static void delay_ms(uint32_t ms)
{
    (void)SYSTICK->CTRL;
    while (ms--) { while (!(SYSTICK->CTRL & (1UL << 16))) { } }
}

static void delay_us(uint32_t us)
{
    uint32_t ciclos = us * (g_hclk_hz / 1000000UL);
    uint32_t t0 = DWT_CYCCNT;
    while ((DWT_CYCCNT - t0) < ciclos) { }
}

/* Milissegundos desde o inicio (acumula o contador de ciclos; chamar a cada < 50 s) */
static uint32_t agora_ms(void)
{
    static uint32_t ciclos_ant = 0, resto = 0, ms_total = 0;
    uint32_t por_ms = g_hclk_hz / 1000UL;
    uint32_t c = DWT_CYCCNT;

    resto += c - ciclos_ant;
    ciclos_ant = c;
    ms_total += resto / por_ms;
    resto %= por_ms;
    return ms_total;
}

/* ========================================================================== */
/*  LCD 1602 (HD44780, 4 bits)  RS=PA0 E=PA1 DB4=PB5 DB5=PB4 DB6=PB3 DB7=PB0   */
/* ========================================================================== */
#define LCD_RS    (1UL << 0)    /* PA0 */
#define LCD_E     (1UL << 1)    /* PA1 */
#define LCD_DB4   (1UL << 5)    /* PB5 */
#define LCD_DB5   (1UL << 4)    /* PB4 */
#define LCD_DB6   (1UL << 3)    /* PB3 */
#define LCD_DB7   (1UL << 0)    /* PB0 */
#define LCD_DADOS (LCD_DB4 | LCD_DB5 | LCD_DB6 | LCD_DB7)

static void lcd_pulso_e(void)
{
    GPIOA->BRR = LCD_E;   delay_us(2);
    GPIOA->BSRR = LCD_E;  delay_us(2);
    GPIOA->BRR = LCD_E;   delay_us(50);
}

static void lcd_nibble(uint8_t n)
{
    uint32_t set = 0, rst = 0;
    if (n & 0x01) set |= LCD_DB4; else rst |= LCD_DB4;
    if (n & 0x02) set |= LCD_DB5; else rst |= LCD_DB5;
    if (n & 0x04) set |= LCD_DB6; else rst |= LCD_DB6;
    if (n & 0x08) set |= LCD_DB7; else rst |= LCD_DB7;
    GPIOB->BSRR = set | (rst << 16);
    lcd_pulso_e();
}

static void lcd_byte(uint8_t v, uint8_t dado)
{
    if (dado) GPIOA->BSRR = LCD_RS; else GPIOA->BRR = LCD_RS;
    delay_us(2);
    lcd_nibble((uint8_t)(v >> 4));
    lcd_nibble((uint8_t)(v & 0x0F));
    delay_us(50);
}

static void lcd_cmd(uint8_t c)
{
    lcd_byte(c, 0);
    if (c == 0x01 || c == 0x02) delay_ms(2);
}

static void lcd_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    AFIO->MAPR = (AFIO->MAPR & ~(7UL << 24)) | (2UL << 24);   /* JTAG off, SWD on: libera PB3/PB4 */

    GPIOA->CRL = (GPIOA->CRL & ~((0xFUL << 0) | (0xFUL << 4))) | (0x2UL << 0) | (0x2UL << 4);
    GPIOB->CRL = (GPIOB->CRL & ~((0xFUL << 0) | (0xFUL << 12) | (0xFUL << 16) | (0xFUL << 20)))
               | (0x2UL << 0) | (0x2UL << 12) | (0x2UL << 16) | (0x2UL << 20);

    GPIOA->BRR = LCD_RS | LCD_E;
    GPIOB->BRR = LCD_DADOS;

    delay_ms(50);
    lcd_nibble(0x03);  delay_ms(5);
    lcd_nibble(0x03);  delay_ms(2);
    lcd_nibble(0x03);  delay_ms(2);
    lcd_nibble(0x02);  delay_ms(2);          /* modo de 4 bits */

    lcd_cmd(0x28);                            /* 4 bits, 2 linhas, 5x8 */
    lcd_cmd(0x08);                            /* display off */
    lcd_cmd(0x01);                            /* limpa */
    lcd_cmd(0x06);                            /* incrementa cursor */
    lcd_cmd(0x0C);                            /* display on, sem cursor */
}

static void lcd_linha(uint8_t linha, const char *s)       /* 16 colunas, completa com espacos */
{
    uint8_t i;
    lcd_cmd((uint8_t)(0x80U | (linha ? 0x40U : 0x00U)));
    for (i = 0; i < 16; i++) lcd_byte((uint8_t)(*s ? *s++ : ' '), 1);
}

/* ========================================================================== */
/*  ADC1: LDR (PA3, canal 3) e POT2 (PB1, canal 9)                             */
/* ========================================================================== */
#define CANAL_LDR   3U
#define CANAL_POT   9U
#define VREF_MV     3300UL

static void adc_init(void)
{
    uint32_t t;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_ADC1EN;

    GPIOA->CRL &= ~(0xFUL << (4U * 3U));                      /* PA3 analogico */
    GPIOB->CRL &= ~(0xFUL << (4U * 1U));                      /* PB1 analogico (PB0 e do LCD) */

    RCC->CFGR = (RCC->CFGR & ~(3UL << 14)) | (2UL << 14);     /* ADCCLK = PCLK2/6 = 12 MHz */
    ADC1->SMPR2 |= (7UL << (3U * CANAL_LDR)) | (7UL << (3U * CANAL_POT));   /* 239,5 ciclos */
    ADC1->SQR1 = 0;
    ADC1->SQR3 = CANAL_LDR;

    ADC1->CR2 = (1UL << 0) | (1UL << 20) | (7UL << 17);       /* ADON, EXTTRIG, EXTSEL = software */
    delay_ms(2);
    ADC1->CR2 |= (1UL << 3);                                  /* RSTCAL */
    for (t = 0; t < 100000UL && (ADC1->CR2 & (1UL << 3)); t++) { }
    ADC1->CR2 |= (1UL << 2);                                  /* CAL */
    for (t = 0; t < 100000UL && (ADC1->CR2 & (1UL << 2)); t++) { }
}

static uint16_t adc_uma(void)
{
    uint32_t t;
    ADC1->CR2 |= (1UL << 22);                                 /* SWSTART */
    for (t = 0; t < 100000UL; t++) if (ADC1->SR & (1UL << 1)) return (uint16_t)(ADC1->DR & 0xFFFU);
    return 0xFFFF;
}

static uint16_t adc_media(uint8_t canal, uint8_t n)           /* 0xFFFF = erro */
{
    uint32_t soma = 0;
    uint8_t i;
    uint16_t v;

    ADC1->SQR3 = canal;
    if (adc_uma() == 0xFFFF) return 0xFFFF;                   /* descarta a 1a (troca de canal) */
    for (i = 0; i < n; i++) {
        v = adc_uma();
        if (v == 0xFFFF) return 0xFFFF;
        soma += v;
    }
    return (uint16_t)(soma / n);
}

/* ========================================================================== */
/*  I2C1 (SCL=PB6, SDA=PB7, 100 kHz) e DS3231 (0x68, somente leitura)          */
/* ========================================================================== */
#define I2C_TIMEOUT  60000UL
typedef enum { I2C_OK = 0, I2C_ERR_TIMEOUT, I2C_ERR_NACK, I2C_ERR_PARAM } I2C_Status;

static void i2c_config(void)
{
    uint32_t mhz = g_pclk1_hz / 1000000UL;
    I2C1->CR1 = 0;
    I2C1->CR2 = mhz;
    I2C1->CCR = g_pclk1_hz / 200000UL;
    I2C1->TRISE = mhz + 1UL;
    I2C1->CR1 = 1UL;                                          /* PE */
}

static void i2c_recupera(void)
{
    I2C1->CR1 |= (1UL << 15);
    I2C1->CR1 &= ~(1UL << 15);
    i2c_config();
}

static void i2c_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFFUL) | 0xDD000000UL;  /* PB6/PB7: AF open-drain */
    i2c_recupera();
}

static uint8_t i2c_sr1(uint32_t m)
{
    uint32_t t;
    for (t = 0; t < I2C_TIMEOUT; t++) if (I2C1->SR1 & m) return 1;
    return 0;
}

static uint8_t i2c_livre(void)
{
    uint32_t t;
    for (t = 0; t < I2C_TIMEOUT; t++) if (!(I2C1->SR2 & (1UL << 1))) return 1;
    return 0;
}

static I2C_Status i2c_start_end(uint8_t b)
{
    uint32_t t, sr1;
    I2C1->CR1 |= (1UL << 8);                                  /* START */
    if (!i2c_sr1(1UL << 0)) return I2C_ERR_TIMEOUT;
    I2C1->DR = b;
    for (t = 0; t < I2C_TIMEOUT; t++) {
        sr1 = I2C1->SR1;
        if (sr1 & (1UL << 1)) { (void)I2C1->SR1; (void)I2C1->SR2; return I2C_OK; }
        if (sr1 & (1UL << 10)) { I2C1->SR1 &= ~(1UL << 10); I2C1->CR1 |= (1UL << 9); return I2C_ERR_NACK; }
    }
    return I2C_ERR_TIMEOUT;
}

static I2C_Status i2c_ler_reg(uint8_t a7, uint8_t reg, uint8_t *buf, uint8_t n)   /* n >= 3 */
{
    I2C_Status st;
    if (n < 3) return I2C_ERR_PARAM;
    if (!i2c_livre()) { i2c_recupera(); return I2C_ERR_TIMEOUT; }

    I2C1->CR1 |= (1UL << 10);                                 /* ACK */
    st = i2c_start_end((uint8_t)(a7 << 1));
    if (st != I2C_OK) { if (st == I2C_ERR_TIMEOUT) i2c_recupera(); return st; }
    if (!i2c_sr1(1UL << 7)) { i2c_recupera(); return I2C_ERR_TIMEOUT; }
    I2C1->DR = reg;
    if (!i2c_sr1(1UL << 2)) { i2c_recupera(); return I2C_ERR_TIMEOUT; }

    I2C1->CR1 |= (1UL << 10);
    st = i2c_start_end((uint8_t)((a7 << 1) | 1U));
    if (st != I2C_OK) { if (st == I2C_ERR_TIMEOUT) i2c_recupera(); return st; }

    while (n > 3) {
        if (!i2c_sr1(1UL << 6)) { i2c_recupera(); return I2C_ERR_TIMEOUT; }
        *buf++ = (uint8_t)I2C1->DR;
        n--;
    }
    if (!i2c_sr1(1UL << 2)) { i2c_recupera(); return I2C_ERR_TIMEOUT; }
    I2C1->CR1 &= ~(1UL << 10);                                /* NACK no ultimo */
    *buf++ = (uint8_t)I2C1->DR;
    I2C1->CR1 |= (1UL << 9);                                  /* STOP */
    *buf++ = (uint8_t)I2C1->DR;
    if (!i2c_sr1(1UL << 6)) { i2c_recupera(); return I2C_ERR_TIMEOUT; }
    *buf = (uint8_t)I2C1->DR;
    i2c_livre();
    I2C1->CR1 |= (1UL << 10);
    return I2C_OK;
}

typedef struct {
    uint8_t  segundo, minuto, hora, dia_semana, dia, mes;
    uint16_t ano;
    int16_t  temp_centi;
    uint8_t  osf, valido;
} DS3231_Dados;

static uint8_t bcd(uint8_t v) { return (uint8_t)(((v >> 4) * 10U) + (v & 0x0FU)); }

static I2C_Status ds3231_ler(DS3231_Dados *d)
{
    uint8_t r[19];
    I2C_Status st = i2c_ler_reg(0x68, 0x00, r, sizeof r);
    if (st != I2C_OK) return st;

    d->segundo = bcd(r[0] & 0x7F);
    d->minuto  = bcd(r[1] & 0x7F);
    if (r[2] & 0x40) {
        d->hora = bcd(r[2] & 0x1F);
        d->hora = (r[2] & 0x20) ? (uint8_t)((d->hora % 12U) + 12U) : (uint8_t)(d->hora % 12U);
    } else {
        d->hora = bcd(r[2] & 0x3F);
    }
    d->dia_semana = r[3] & 0x07;
    d->dia = bcd(r[4] & 0x3F);
    d->mes = bcd(r[5] & 0x1F);
    d->ano = (uint16_t)(2000U + bcd(r[6]) + ((r[5] & 0x80) ? 100U : 0U));
    d->osf = (r[0x0F] >> 7) & 1U;
    d->temp_centi = (int16_t)(((int16_t)(int8_t)r[0x11]) * 100 + (int16_t)((r[0x12] >> 6) * 25));
    d->valido = (d->segundo < 60 && d->minuto < 60 && d->hora < 24 &&
                 d->dia >= 1 && d->dia <= 31 && d->mes >= 1 && d->mes <= 12) ? 1U : 0U;
    return I2C_OK;
}

/* ========================================================================== */
/*  UART (USART1: TX=PA9, RX=PA10), 8N1, polling                               */
/* ========================================================================== */
static uint32_t uart_erros;

static void uart_baud(uint32_t baud) { USART1->BRR = (g_hclk_hz + baud / 2U) / baud; }

static void uart_init(uint32_t baud)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFFUL << 4)) | (0xBUL << 4) | (0x8UL << 8);   /* PA9 AF-PP, PA10 entrada pull-up */
    GPIOA->BSRR = (1UL << 10);
    USART1->CR1 = 0; USART1->CR2 = 0; USART1->CR3 = 0;
    uart_baud(baud);
    USART1->CR1 = (1UL << 13) | (1UL << 3) | (1UL << 2);                         /* UE, TE, RE */
}

static void uart_flush(void)
{
    while (USART1->SR & ((1UL << 5) | (1UL << 3))) (void)USART1->DR;
}

static void uart_envia(const char *s)
{
    while (*s) { while (!(USART1->SR & (1UL << 7))) { } USART1->DR = (uint8_t)*s++; }
    while (!(USART1->SR & (1UL << 6))) { }
}

static uint16_t uart_recebe(uint8_t *buf, uint16_t max, uint32_t timeout_ms, uint32_t ocioso_ms)
{
    uint32_t por_ms = g_hclk_hz / 1000UL;
    uint32_t ini = DWT_CYCCNT, ult = ini, agora, sr;
    uint16_t n = 0;
    uint8_t b;

    while (n < max) {
        agora = DWT_CYCCNT;
        sr = USART1->SR;
        if (sr & (1UL << 5)) {
            b = (uint8_t)USART1->DR;
            if (sr & ((1UL << 3) | (1UL << 1) | (1UL << 2) | (1UL << 0))) uart_erros++; else buf[n++] = b;
            ult = DWT_CYCCNT;
        } else if (sr & (1UL << 3)) {
            (void)USART1->DR; uart_erros++;
        } else if (n == 0) {
            if ((agora - ini) >= timeout_ms * por_ms) break;
        } else {
            if ((agora - ult) >= ocioso_ms * por_ms) break;
        }
    }
    return n;
}

/* ========================================================================== */
/*  HC-SR04 (TRIG=PB8, ECHO=PB9, TIM4 a 1 MHz)                                 */
/* ========================================================================== */
typedef enum { US_OK = 0, US_SEM_ECO, US_SEM_OBJETO, US_FORA } US_Status;

static void us_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFFUL)) | 0x42UL;           /* PB8 saida, PB9 entrada flutuante */
    GPIOB->BRR = (1UL << 8);
    TIM4->CR1 = 0;
    TIM4->PSC = (g_hclk_hz / 1000000UL) - 1UL;
    TIM4->ARR = 0xFFFF;
    TIM4->EGR = 1UL;
    TIM4->CR1 = 1UL;
}

static US_Status us_medir(uint16_t *mm, uint16_t *largura_us)
{
    uint16_t t0, dur;
    uint32_t calc;

    GPIOB->BSRR = (1UL << 8);
    t0 = (uint16_t)TIM4->CNT;
    while ((uint16_t)((uint16_t)TIM4->CNT - t0) < 10U) { }
    GPIOB->BRR = (1UL << 8);

    t0 = (uint16_t)TIM4->CNT;
    while (!(GPIOB->IDR & (1UL << 9))) if ((uint16_t)((uint16_t)TIM4->CNT - t0) > 30000U) return US_SEM_ECO;
    t0 = (uint16_t)TIM4->CNT;
    while (GPIOB->IDR & (1UL << 9)) if ((uint16_t)((uint16_t)TIM4->CNT - t0) > 30000U) return US_SEM_OBJETO;
    dur = (uint16_t)((uint16_t)TIM4->CNT - t0);

    *largura_us = dur;
    calc = ((uint32_t)dur * 343UL) / 2000UL;                  /* mm */
    *mm = (uint16_t)calc;
    return (calc < 20UL || calc > 4000UL) ? US_FORA : US_OK;
}

/* ========================================================================== */
/*  LCD grafico JLX (ST7567) por SPI1: CS=PA4 SCK=PA5 RST=PA6 SDA=PA7 A0=PB12   */
/* ========================================================================== */
static const uint8_t charset[][6] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  // 20 space
  { 0x00, 0x00, 0x00, 0x5f, 0x00, 0x00 },  // 21 !
  { 0x00, 0x00, 0x07, 0x00, 0x07, 0x00 },  // 22 "
  { 0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14 },  // 23 #
  { 0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12 },  // 24 $
  { 0x00, 0x23, 0x13, 0x08, 0x64, 0x62 },  // 25 %
  { 0x00, 0x36, 0x49, 0x55, 0x22, 0x50 },  // 26 &
  { 0x00, 0x00, 0x05, 0x03, 0x00, 0x00 },  // 27 '
  { 0x00, 0x00, 0x1c, 0x22, 0x41, 0x00 },  // 28 (
  { 0x00, 0x00, 0x41, 0x22, 0x1c, 0x00 },  // 29 )
  { 0x00, 0x14, 0x08, 0x3e, 0x08, 0x14 },  // 2a *
  { 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08 },  // 2b +
  { 0x00, 0x00, 0x50, 0x30, 0x00, 0x00 },  // 2c ,
  { 0x00, 0x08, 0x08, 0x08, 0x08, 0x08 },  // 2d -
  { 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },  // 2e .
  { 0x00, 0x20, 0x10, 0x08, 0x04, 0x02 },  // 2f /
  { 0x00, 0x3e, 0x51, 0x49, 0x45, 0x3e },  // 30 0
  { 0x00, 0x00, 0x42, 0x7f, 0x40, 0x00 },  // 31 1
  { 0x00, 0x42, 0x61, 0x51, 0x49, 0x46 },  // 32 2
  { 0x00, 0x21, 0x41, 0x45, 0x4b, 0x31 },  // 33 3
  { 0x00, 0x18, 0x14, 0x12, 0x7f, 0x10 },  // 34 4
  { 0x00, 0x27, 0x45, 0x45, 0x45, 0x39 },  // 35 5
  { 0x00, 0x3c, 0x4a, 0x49, 0x49, 0x30 },  // 36 6
  { 0x00, 0x01, 0x71, 0x09, 0x05, 0x03 },  // 37 7
  { 0x00, 0x36, 0x49, 0x49, 0x49, 0x36 },  // 38 8
  { 0x00, 0x06, 0x49, 0x49, 0x29, 0x1e },  // 39 9
  { 0x00, 0x00, 0x36, 0x36, 0x00, 0x00 },  // 3a :
  { 0x00, 0x00, 0x56, 0x36, 0x00, 0x00 },  // 3b ;
  { 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },  // 3c <
  { 0x00, 0x14, 0x14, 0x14, 0x14, 0x14 },  // 3d =
  { 0x00, 0x00, 0x41, 0x22, 0x14, 0x08 },  // 3e >
  { 0x00, 0x02, 0x01, 0x51, 0x09, 0x06 },  // 3f ?
  { 0x00, 0x32, 0x49, 0x79, 0x41, 0x3e },  // 40 @
  { 0x00, 0x7e, 0x11, 0x11, 0x11, 0x7e },  // 41 A
  { 0x00, 0x7f, 0x49, 0x49, 0x49, 0x36 },  // 42 B
  { 0x00, 0x3e, 0x41, 0x41, 0x41, 0x22 },  // 43 C
  { 0x00, 0x7f, 0x41, 0x41, 0x22, 0x1c },  // 44 D
  { 0x00, 0x7f, 0x49, 0x49, 0x49, 0x41 },  // 45 E
  { 0x00, 0x7f, 0x09, 0x09, 0x09, 0x01 },  // 46 F
  { 0x00, 0x3e, 0x41, 0x49, 0x49, 0x7a },  // 47 G
  { 0x00, 0x7f, 0x08, 0x08, 0x08, 0x7f },  // 48 H
  { 0x00, 0x00, 0x41, 0x7f, 0x41, 0x00 },  // 49 I
  { 0x00, 0x20, 0x40, 0x41, 0x3f, 0x01 },  // 4a J
  { 0x00, 0x7f, 0x08, 0x14, 0x22, 0x41 },  // 4b K
  { 0x00, 0x7f, 0x40, 0x40, 0x40, 0x40 },  // 4c L
  { 0x00, 0x7f, 0x02, 0x0c, 0x02, 0x7f },  // 4d M
  { 0x00, 0x7f, 0x04, 0x08, 0x10, 0x7f },  // 4e N
  { 0x00, 0x3e, 0x41, 0x41, 0x41, 0x3e },  // 4f O
  { 0x00, 0x7f, 0x09, 0x09, 0x09, 0x06 },  // 50 P
  { 0x00, 0x3e, 0x41, 0x51, 0x21, 0x5e },  // 51 Q
  { 0x00, 0x7f, 0x09, 0x19, 0x29, 0x46 },  // 52 R
  { 0x00, 0x46, 0x49, 0x49, 0x49, 0x31 },  // 53 S
  { 0x00, 0x01, 0x01, 0x7f, 0x01, 0x01 },  // 54 T
  { 0x00, 0x3f, 0x40, 0x40, 0x40, 0x3f },  // 55 U
  { 0x00, 0x1f, 0x20, 0x40, 0x20, 0x1f },  // 56 V
  { 0x00, 0x3f, 0x40, 0x38, 0x40, 0x3f },  // 57 W
  { 0x00, 0x63, 0x14, 0x08, 0x14, 0x63 },  // 58 X
  { 0x00, 0x07, 0x08, 0x70, 0x08, 0x07 },  // 59 Y
  { 0x00, 0x61, 0x51, 0x49, 0x45, 0x43 },  // 5a Z
  { 0x00, 0x00, 0x7f, 0x41, 0x41, 0x00 },  // 5b [
  { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20 },  // 5c backslash
  { 0x00, 0x00, 0x41, 0x41, 0x7f, 0x00 },  // 5d ]
  { 0x00, 0x04, 0x02, 0x01, 0x02, 0x04 },  // 5e ^
  { 0x00, 0x40, 0x40, 0x40, 0x40, 0x40 },  // 5f _
  { 0x00, 0x00, 0x01, 0x02, 0x04, 0x00 },  // 60 `
  { 0x00, 0x20, 0x54, 0x54, 0x54, 0x78 },  // 61 a
  { 0x00, 0x7f, 0x48, 0x44, 0x44, 0x38 },  // 62 b
  { 0x00, 0x38, 0x44, 0x44, 0x44, 0x20 },  // 63 c
  { 0x00, 0x38, 0x44, 0x44, 0x48, 0x7f },  // 64 d
  { 0x00, 0x38, 0x54, 0x54, 0x54, 0x18 },  // 65 e
  { 0x00, 0x08, 0x7e, 0x09, 0x01, 0x02 },  // 66 f
  { 0x00, 0x0c, 0x52, 0x52, 0x52, 0x3e },  // 67 g
  { 0x00, 0x7f, 0x08, 0x04, 0x04, 0x78 },  // 68 h
  { 0x00, 0x00, 0x44, 0x7d, 0x40, 0x00 },  // 69 i
  { 0x00, 0x20, 0x40, 0x44, 0x3d, 0x00 },  // 6a j
  { 0x00, 0x7f, 0x10, 0x28, 0x44, 0x00 },  // 6b k
  { 0x00, 0x00, 0x41, 0x7f, 0x40, 0x00 },  // 6c l
  { 0x00, 0x7c, 0x04, 0x18, 0x04, 0x78 },  // 6d m
  { 0x00, 0x7c, 0x08, 0x04, 0x04, 0x78 },  // 6e n
  { 0x00, 0x38, 0x44, 0x44, 0x44, 0x38 },  // 6f o
  { 0x00, 0x7c, 0x14, 0x14, 0x14, 0x08 },  // 70 p
  { 0x00, 0x08, 0x14, 0x14, 0x18, 0x7c },  // 71 q
  { 0x00, 0x7c, 0x08, 0x04, 0x04, 0x08 },  // 72 r
  { 0x00, 0x48, 0x54, 0x54, 0x54, 0x20 },  // 73 s
  { 0x00, 0x04, 0x3f, 0x44, 0x40, 0x20 },  // 74 t
  { 0x00, 0x3c, 0x40, 0x40, 0x20, 0x7c },  // 75 u
  { 0x00, 0x1c, 0x20, 0x40, 0x20, 0x1c },  // 76 v
  { 0x00, 0x3c, 0x40, 0x30, 0x40, 0x3c },  // 77 w
  { 0x00, 0x44, 0x28, 0x10, 0x28, 0x44 },  // 78 x
  { 0x00, 0x0c, 0x50, 0x50, 0x50, 0x3c },  // 79 y
  { 0x00, 0x44, 0x64, 0x54, 0x4c, 0x44 },  // 7a z
  { 0x00, 0x00, 0x08, 0x36, 0x41, 0x00 },  // 7b {
  { 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00 },  // 7c |
  { 0x00, 0x00, 0x41, 0x36, 0x08, 0x00 },  // 7d }
  { 0x00, 0x00, 0x0c, 0x12, 0x12, 0x0c },  // 7e ~ Graus, escrever o ~
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }   // 7f
};

#define G_CS     (1UL << 4)     /* PA4  */
#define G_RST    (1UL << 6)     /* PA6  */
#define G_CD     (1UL << 12)    /* PB12 */

static uint8_t g_x, g_y;
static const uint8_t g_zeros[132] = {0};

static void spi_envia(const uint8_t *d, uint16_t n)
{
    while (n--) { while (!(SPI1->SR & (1UL << 1))) { } SPI1->DR = *d++; }
    while (!(SPI1->SR & (1UL << 1))) { }
    while (SPI1->SR & (1UL << 7)) { }
    (void)SPI1->DR; (void)SPI1->SR;
}

static void g_cmd(uint8_t c)
{
    GPIOA->BRR = G_CS; GPIOB->BRR = G_CD;
    spi_envia(&c, 1);
    GPIOA->BSRR = G_CS;
}

static void g_dados(const uint8_t *d, uint16_t n)
{
    GPIOA->BRR = G_CS; GPIOB->BSRR = G_CD;
    spi_envia(d, n);
    GPIOA->BSRR = G_CS;
}

static void g_cursor(uint8_t x, uint8_t y)
{
    g_x = x; g_y = y;
    g_cmd((uint8_t)(0xB0 + (y & 7)));
    g_cmd((uint8_t)(0x10 + (x >> 4)));
    g_cmd((uint8_t)(0x00 + (x & 0x0F)));
}

static void g_limpa(void)
{
    uint8_t p;
    for (p = 0; p < 8; p++) {
        g_cmd((uint8_t)(0xB0 + p)); g_cmd(0x00); g_cmd(0x10);
        g_dados(g_zeros, 132);
    }
    g_cursor(0, 0);
}

static void g_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_SPI1EN;
    GPIOA->BSRR = G_CS | G_RST;
    GPIOB->BRR = G_CD;
    /* PA4 saida, PA5 SCK (AF-PP), PA6 saida (RESET), PA7 MOSI (AF-PP). Preserva PA0..PA3 */
    GPIOA->CRL = (GPIOA->CRL & 0x0000FFFFUL) | 0xB2B20000UL;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << 16)) | (0x2UL << 16);                  /* PB12 saida */

    SPI1->CR1 = (1UL << 2) | (3UL << 3) | (1UL << 9) | (1UL << 8);              /* mestre, /16, SSM, SSI */
    SPI1->CR1 |= (1UL << 6);                                                      /* SPE */

    delay_ms(50);
    GPIOA->BRR = G_RST; delay_ms(5);
    GPIOA->BSRR = G_RST; delay_ms(5);

    g_cmd(0xE2); g_cmd(0xA3); g_cmd(0xAF); g_cmd(0xA0); g_cmd(0xC8); g_cmd(0x22);
    g_cmd(0x81); g_cmd(0x20); g_cmd(0x2F); g_cmd(0x40); g_cmd(0xB0); g_cmd(0x10);
    g_cmd(0x00); g_cmd(0xAF); g_cmd(0xA4);
    g_limpa();
}

static void g_char(char c, uint8_t inv)
{
    uint8_t buf[6], i;
    if ((uint8_t)c < 0x20 || (uint8_t)c > 0x7F) c = '?';
    if (g_x > 122) { if (g_y >= 7) return; g_cursor(0, (uint8_t)(g_y + 1)); }
    for (i = 0; i < 6; i++) buf[i] = inv ? (uint8_t)~charset[(uint8_t)c - 0x20][i] : charset[(uint8_t)c - 0x20][i];
    g_dados(buf, 6);
    g_x = (uint8_t)(g_x + 6);
}

/* Escreve texto na linha y (0..7), coluna de caractere x (0..20); preenche o resto da linha */
static void g_linha(uint8_t y, uint8_t inv, const char *s)
{
    uint8_t col = 0;
    g_cursor(0, y);
    while (col < 21) { g_char(*s ? *s++ : ' ', inv); col++; }
    g_dados(g_zeros, 2);
}

/* ========================================================================== */
/*  TEXTO                                                                      */
/* ========================================================================== */
static char *ps(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *pu(char *p, uint32_t v)
{
    char t[11]; uint8_t n = 0;
    do { t[n++] = (char)('0' + v % 10U); v /= 10U; } while (v);
    while (n) *p++ = t[--n];
    return p;
}

static char *p2(char *p, uint8_t v)
{
    *p++ = (char)('0' + (v / 10U) % 10U);
    *p++ = (char)('0' + v % 10U);
    return p;
}

static char *p4(char *p, uint16_t v)
{
    *p++ = (char)('0' + (v / 1000U) % 10U);
    *p++ = (char)('0' + (v / 100U) % 10U);
    *p++ = (char)('0' + (v / 10U) % 10U);
    *p++ = (char)('0' + v % 10U);
    return p;
}

static char *p_volts(char *p, uint32_t mv)            /* "1.65V" */
{
    *p++ = (char)('0' + (mv / 1000U) % 10U);
    *p++ = '.';
    *p++ = (char)('0' + (mv % 1000U) / 100U);
    *p++ = (char)('0' + (mv % 100U) / 10U);
    *p++ = 'V';
    return p;
}

static char *p_temp(char *p, int16_t t)               /* "-5.2C" / "25.2C" */
{
    if (t < 0) { t = (int16_t)(-t); *p++ = '-'; }
    if (t >= 1000) *p++ = (char)('0' + (t / 1000) % 10);
    *p++ = (char)('0' + (t / 100) % 10);
    *p++ = '.';
    *p++ = (char)('0' + (t % 100) / 10);
    *p++ = 'C';
    return p;
}

static uint8_t contem(const uint8_t *buf, uint16_t n, const char *alvo)
{
    uint16_t i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; alvo[j] && (uint16_t)(i + j) < n && buf[i + j] == (uint8_t)alvo[j]; j++) { }
        if (alvo[j] == '\0') return 1;
    }
    return 0;
}

/* ========================================================================== */
/*  ESTADO DO SISTEMA                                                          */
/* ========================================================================== */
static DS3231_Dados rtc;
static I2C_Status   rtc_st = I2C_ERR_TIMEOUT;

static uint16_t ldr_raw, pot_raw;                      /* valores exibidos (filtrados) */
static int32_t  ldr_f16, pot_f16;                      /* filtros x16 */
static uint8_t  ldr_erro, pot_erro, adc_ini;

static uint16_t us_mm, us_us;
static US_Status us_st = US_SEM_ECO;
static uint32_t us_n;

static const uint32_t esp_vels[] = { 115200UL, 9600UL, 57600UL, 38400UL, 19200UL };
static uint32_t esp_vel, esp_ok, esp_falha, esp_bytes;
static uint8_t  esp_idx, esp_seguidas;
static char     esp_fw[22];

/* ========================================================================== */
/*  TAREFAS                                                                    */
/* ========================================================================== */
static void tarefa_adc(uint32_t ciclo)
{
    uint16_t a;

    a = adc_media(CANAL_LDR, 32);
    ldr_erro = (a == 0xFFFF);
    if (!ldr_erro) { if (!adc_ini) ldr_f16 = (int32_t)a * 16; else ldr_f16 += ((int32_t)a * 16 - ldr_f16) / 8; }

    a = adc_media(CANAL_POT, 32);
    pot_erro = (a == 0xFFFF);
    if (!pot_erro) { if (!adc_ini) pot_f16 = (int32_t)a * 16; else pot_f16 += ((int32_t)a * 16 - pot_f16) / 8; }

    adc_ini = 1;
    if ((ciclo % 5U) == 0U) {                          /* valor exibido atualiza a cada 500 ms */
        ldr_raw = (uint16_t)(ldr_f16 / 16);
        pot_raw = (uint16_t)(pot_f16 / 16);
    }
}

static void tarefa_esp(void)
{
    uint8_t rx[64];
    uint16_t n, i, k = 0;

    if (esp_vel == 0) {                                /* procurando a velocidade */
        uart_baud(esp_vels[esp_idx]);
        uart_flush();
        uart_envia("AT\r\n");
        n = uart_recebe(rx, sizeof rx, 300, 60);
        esp_bytes += n;
        if (contem(rx, n, "OK")) {
            esp_vel = esp_vels[esp_idx];
            esp_ok++;
            esp_seguidas = 0;
            uart_flush();                               /* le a versao do firmware */
            uart_envia("AT+GMR\r\n");
            n = uart_recebe(rx, sizeof rx, 600, 150);
            for (i = 0; i < n && rx[i] != '\n'; i++) { }   /* pula o eco do comando */
            for (i++; i < n && k < 21 && rx[i] != '\r' && rx[i] != '\n'; i++) {
                esp_fw[k++] = (rx[i] >= 0x20 && rx[i] < 0x7F) ? (char)rx[i] : '.';
            }
            esp_fw[k] = '\0';
        } else {
            esp_falha++;
            esp_idx = (uint8_t)((esp_idx + 1U) % (sizeof esp_vels / sizeof esp_vels[0]));
        }
        return;
    }

    uart_flush();
    uart_envia("AT\r\n");
    n = uart_recebe(rx, sizeof rx, 300, 60);
    if (contem(rx, n, "OK")) { esp_ok++; esp_seguidas = 0; }
    else {
        esp_falha++;
        if (++esp_seguidas >= 5U) { esp_vel = 0; esp_fw[0] = '\0'; }   /* perdeu o ESP: volta a procurar */
    }
}

static void lcd_pagina(uint8_t pg)
{
    char b[20];
    char *p;

    switch (pg) {
    case 0:                                            /* DS3231 */
        if (rtc_st != I2C_OK) {
            p = ps(b, "RTC erro ");
            *p++ = (char)('0' + (uint8_t)rtc_st);
            *p = '\0';
            lcd_linha(0, b);
            lcd_linha(1, (rtc_st == I2C_ERR_NACK) ? "sem resp. 0x68" : "barramento I2C");
        } else {
            p = p2(b, rtc.hora);   *p++ = ':';
            p = p2(p, rtc.minuto); *p++ = ':';
            p = p2(p, rtc.segundo); *p++ = ' ';
            p = p_temp(p, rtc.temp_centi);
            if (rtc.osf || !rtc.valido) *p++ = '!';
            *p = '\0';
            lcd_linha(0, b);
            p = p2(b, rtc.dia);  *p++ = '/';
            p = p2(p, rtc.mes);  *p++ = '/';
            p = p4(p, rtc.ano);
            *p = '\0';
            lcd_linha(1, b);
        }
        break;

    case 1:                                            /* LDR e POT */
        p = ps(b, "LDR A3:");
        p = ldr_erro ? ps(p, "----") : p4(p, ldr_raw);
        *p = '\0';
        lcd_linha(0, b);
        p = ps(b, "POT B1:");
        p = pot_erro ? ps(p, "----") : p4(p, pot_raw);
        *p++ = ' ';
        p = p_volts(p, ((uint32_t)pot_raw * VREF_MV) / 4095UL);
        *p = '\0';
        lcd_linha(1, b);
        break;

    case 2:                                            /* HC-SR04 */
        if (!USAR_HCSR04) { lcd_linha(0, "HC-SR04 desligado"); lcd_linha(1, ""); break; }
        if (us_st == US_OK) {
            p = ps(b, "Dist: ");
            p = pu(p, us_mm / 10U); *p++ = '.'; p = pu(p, us_mm % 10U);
            p = ps(p, " cm");
            *p = '\0';
            lcd_linha(0, b);
            p = ps(b, "Eco: "); p = pu(p, us_us); p = ps(p, " us");
            *p = '\0';
            lcd_linha(1, b);
        } else {
            lcd_linha(0, (us_st == US_SEM_ECO) ? "Sem eco (fiacao?)" : (us_st == US_SEM_OBJETO) ? "Sem objeto" : "Fora da faixa");
            p = ps(b, "Leitura #"); p = pu(p, us_n); *p = '\0';
            lcd_linha(1, b);
        }
        break;

    default:                                           /* ESP-01 */
        if (!USAR_ESP) { lcd_linha(0, "ESP desligado"); lcd_linha(1, ""); break; }
        p = ps(b, "ESP OK:"); p = pu(p, esp_ok); p = ps(p, " ERR:"); p = pu(p, esp_falha);
        *p = '\0';
        lcd_linha(0, b);
        if (esp_vel) { p = pu(b, esp_vel); p = ps(p, " e:"); p = pu(p, uart_erros); }
        else         { p = ps(b, "buscando rx:"); p = pu(p, esp_bytes); }
        *p = '\0';
        lcd_linha(1, b);
        break;
    }
}

static void painel_jlx(void)
{
    char b[24];
    char *p;

    g_linha(0, 1, "  PLATAFORMA STM32  ");

    if (rtc_st == I2C_OK) {
        p = p2(b, rtc.hora);   *p++ = ':';
        p = p2(p, rtc.minuto); *p++ = ':';
        p = p2(p, rtc.segundo); *p++ = ' ';
        p = p2(p, rtc.dia);    *p++ = '/';
        p = p2(p, rtc.mes);    *p++ = '/';
        p = p2(p, (uint8_t)(rtc.ano % 100U));
        *p = '\0';
        g_linha(1, 0, b);
        p = ps(b, "Temp: "); p = p_temp(p, rtc.temp_centi);
        if (rtc.osf || !rtc.valido) p = ps(p, " (hora?)");
        *p = '\0';
        g_linha(2, 0, b);
    } else {
        g_linha(1, 0, "RTC sem resposta");
        g_linha(2, 0, "");
    }

    p = ps(b, "LDR A3: ");
    p = ldr_erro ? ps(p, "----") : p4(p, ldr_raw);
    *p++ = ' '; p = p_volts(p, ((uint32_t)ldr_raw * VREF_MV) / 4095UL);
    *p = '\0';
    g_linha(3, 0, b);

    p = ps(b, "POT B1: ");
    p = pot_erro ? ps(p, "----") : p4(p, pot_raw);
    *p++ = ' '; p = p_volts(p, ((uint32_t)pot_raw * VREF_MV) / 4095UL);
    *p = '\0';
    g_linha(4, 0, b);

    if (us_st == US_OK) {
        p = ps(b, "Dist: "); p = pu(p, us_mm / 10U); *p++ = '.'; p = pu(p, us_mm % 10U); p = ps(p, " cm");
    } else {
        p = ps(b, "Dist: ---");
    }
    *p = '\0';
    g_linha(5, 0, b);

    p = ps(b, "ESP ");
    p = esp_vel ? pu(p, esp_vel) : ps(p, "busca");
    p = ps(p, " ok:"); p = pu(p, esp_ok);
    *p = '\0';
    g_linha(6, 0, b);
    g_linha(7, 0, esp_fw);
}

/* ========================================================================== */
/*  MAIN                                                                       */
/* ========================================================================== */
int main(void)
{
    uint32_t agora, t_led = 0, t_us = 0, t_adc = 0, t_rtc = 0, t_esp = 0, t_lcd = 0, t_jlx = 0;
    uint32_t ciclo_adc = 0;

    clock_init();
    lcd_init();
    adc_init();
    i2c_init();
    uart_init(115200UL);
    us_init();
    if (USAR_JLX) g_init();

    GPIOC->CRH = (GPIOC->CRH & ~(0xFUL << 20)) | (0x2UL << 20);   /* PC13 saida (LED) */
    GPIOC->BSRR = (1UL << 13);

    if (RS485_DESABILITA_RX) {
        GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << 20)) | (0x2UL << 20);   /* PB13 alto: RO do ISL32475 desligado */
        GPIOB->BSRR = (1UL << 13);
    }

    lcd_linha(0, "TCC UNISATC");
    lcd_linha(1, "Plataforma 1.0");
    delay_ms(1500);

    while (1) {
        agora = agora_ms();

        if ((agora - t_led) >= 500U) {
            t_led = agora;
            GPIOC->ODR ^= (1UL << 13);                  /* LED: sistema vivo */
        }
        if (USAR_HCSR04 && (agora - t_us) >= 200U) {
            t_us = agora;
            us_st = us_medir(&us_mm, &us_us);
            us_n++;
        }
        if ((agora - t_adc) >= 100U) {
            t_adc = agora;
            tarefa_adc(ciclo_adc++);
        }
        if ((agora - t_rtc) >= 500U) {
            t_rtc = agora;
            rtc_st = ds3231_ler(&rtc);
        }
        if (USAR_ESP && (agora - t_esp) >= 2000U) {
            t_esp = agora;
            tarefa_esp();
        }
        if ((agora - t_lcd) >= 500U) {
            t_lcd = agora;
            lcd_pagina((uint8_t)((agora / 3000U) % 4U));   /* troca de pagina a cada 3 s */
        }
        if (USAR_JLX && (agora - t_jlx) >= 500U) {
            t_jlx = agora;
            painel_jlx();
        }
    }
}
