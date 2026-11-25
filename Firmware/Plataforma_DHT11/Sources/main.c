/**
 * main.c - FIRMWARE: LCD 1602 + I2C (DS3231) + HC-SR04 + LDR + 2 POTs + matriz 3x3 + 6 LEDs + DHT11
 * Plataforma didatica STM32 Blue Pill (STM32F103C8T6) - TCC UNISATC
 * Somente registradores, sem HAL.
 *
 * PINOS
 *   LCD 1602 : RS=PB12  E=PB14  DB4=PA4  DB5=PA5  DB6=PA6  DB7=PA7  (R/W no GND)
 *   I2C1     : SCL=PB6  SDA=PB7  (DS3231, endereco 0x68)
 *   HC-SR04  : TRIG=PB8  ECHO=PB9  (TIM4 a 1 MHz; PB9 tolera 5 V)
 *   DHT11    : DATA=PB15  (precisa de pull-up de 4,7k..10k para 3V3)
 *   ADC1     : LDR=PA3 (canal 3)  POT B0=PB0 (canal 8)  POT B1=PB1 (canal 9)
 *   Matriz   : linhas PA2,PA1,PA0 (l0..l2)  colunas PC15,PC14,PC13 (c0..c2, pull-up interno)
 *              T2 T1 S1 / T4 T3 S2 / T5 T6 S3
 *   LEDs     : LED1=PA11 LED2=PA12 LED3=PB5 LED4=PA15 LED5=PB3 LED6=PB4
 *              (JTAG desligado, SWD mantido, para liberar PA15/PB3/PB4)
 *
 * Funcao dos LEDs (aceso enquanto o botao/chave estiver ativo):
 *   LED1 = T1 ou S1    LED2 = T2 ou S1
 *   LED3 = T3 ou S2    LED4 = T4 ou S2
 *   LED5 = T5 ou S3    LED6 = T6 ou S3
 * LCD: pag 0 = hora/temp/data, pag 1 = distancia, pag 2 = LDR, pag 3 = POTs,
 *      pag 4 = botoes (T1..T6 e S1..S3), pag 5 = DHT11. Troca a cada 3 s.
 *
 * NAO testado fisicamente.
 */
#include <stdint.h>

/* ===== Opcoes ===== */
#define LED_ATIVO_ALTO  1   /* 1 = LED acende com pino em nivel alto; 0 = nivel baixo */
#define MATRIZ_DEBUG    1   /* 1 = pag. de botoes mostra os 9 bits crus da matriz; 0 = mostra S1..S3 */

/* ========================================================================== */
/*  REGISTRADORES (STM32F103, RM0008)                                          */
/* ========================================================================== */
#define REG32 volatile uint32_t
typedef struct { REG32 CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR; } RCC_T;
typedef struct { REG32 ACR, KEYR, OPTKEYR, SR, CR, AR; }                                          FLASH_T;
typedef struct { REG32 CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; }                                     GPIO_T;
typedef struct { REG32 EVCR, MAPR, EXTICR1, EXTICR2, EXTICR3, EXTICR4, MAPR2; }                   AFIO_T;
typedef struct { REG32 CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR; }        TIM_T;
typedef struct { REG32 SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4, HTR, LTR,
                       SQR1, SQR2, SQR3, JSQR, JDR1, JDR2, JDR3, JDR4, DR; }                      ADC_T;
typedef struct { REG32 CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE; }                          I2C_T;
typedef struct { REG32 CTRL, LOAD, VAL, CALIB; }                                                  SYSTICK_T;

#define RCC       ((RCC_T *)      0x40021000UL)
#define FLASH     ((FLASH_T *)    0x40022000UL)
#define AFIO      ((AFIO_T *)     0x40010000UL)
#define GPIOA     ((GPIO_T *)     0x40010800UL)
#define GPIOB     ((GPIO_T *)     0x40010C00UL)
#define GPIOC     ((GPIO_T *)     0x40011000UL)
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
/*  LCD 1602 (HD44780, 4 bits)  RS=PB12 E=PB14 DB4..DB7=PA4..PA7               */
/* ========================================================================== */
#define LCD_RS    (1UL << 12)   /* PB12 */
#define LCD_E     (1UL << 14)   /* PB14 */
#define LCD_DB4   (1UL << 4)    /* PA4  */
#define LCD_DB5   (1UL << 5)    /* PA5  */
#define LCD_DB6   (1UL << 6)    /* PA6  */
#define LCD_DB7   (1UL << 7)    /* PA7  */
#define LCD_DADOS (LCD_DB4 | LCD_DB5 | LCD_DB6 | LCD_DB7)

static void lcd_pulso_e(void)
{
    GPIOB->BRR  = LCD_E;  delay_us(2);
    GPIOB->BSRR = LCD_E;  delay_us(2);
    GPIOB->BRR  = LCD_E;  delay_us(50);
}

static void lcd_nibble(uint8_t n)
{
    uint32_t set = 0, rst = 0;
    if (n & 0x01) set |= LCD_DB4; else rst |= LCD_DB4;
    if (n & 0x02) set |= LCD_DB5; else rst |= LCD_DB5;
    if (n & 0x04) set |= LCD_DB6; else rst |= LCD_DB6;
    if (n & 0x08) set |= LCD_DB7; else rst |= LCD_DB7;
    GPIOA->BSRR = set | (rst << 16);
    lcd_pulso_e();
}

static void lcd_byte(uint8_t v, uint8_t dado)
{
    if (dado) GPIOB->BSRR = LCD_RS; else GPIOB->BRR = LCD_RS;
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
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    /* PA4..PA7: saida push-pull 2 MHz (preserva PA0..PA3) */
    GPIOA->CRL = (GPIOA->CRL & 0x0000FFFFUL) | 0x22220000UL;
    /* PB12 e PB14: saida push-pull 2 MHz */
    GPIOB->CRH = (GPIOB->CRH & ~((0xFUL << 16) | (0xFUL << 24)))
               | (0x2UL << 16) | (0x2UL << 24);

    GPIOB->BRR = LCD_RS | LCD_E;
    GPIOA->BRR = LCD_DADOS;

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
/*  ADC1: LDR (PA3, canal 3), POT B0 (PB0, canal 8), POT B1 (PB1, canal 9)     */
/* ========================================================================== */
#define CANAL_LDR   3U
#define CANAL_POT0  8U
#define CANAL_POT1  9U
#define VREF_MV     3300UL

static void adc_init(void)
{
    uint32_t t;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_ADC1EN;

    GPIOA->CRL &= ~(0xFUL << (4U * 3U));                      /* PA3 analogico */
    GPIOB->CRL &= ~((0xFUL << 0) | (0xFUL << 4));             /* PB0 e PB1 analogicos */

    RCC->CFGR = (RCC->CFGR & ~(3UL << 14)) | (2UL << 14);     /* ADCCLK = PCLK2/6 = 12 MHz */
    ADC1->SMPR2 |= (7UL << (3U * CANAL_LDR))                  /* 239,5 ciclos */
                 | (7UL << (3U * CANAL_POT0))
                 | (7UL << (3U * CANAL_POT1));
    ADC1->SQR1 = 0;                                           /* 1 conversao */
    ADC1->SQR3 = CANAL_LDR;

    ADC1->CR2 = (1UL << 0) | (1UL << 20) | (7UL << 17);       /* ADON, EXTTRIG, EXTSEL = software */
    delay_ms(2);
    ADC1->CR2 |= (1UL << 3);                                  /* RSTCAL */
    for (t = 0; t < 100000UL && (ADC1->CR2 & (1UL << 3)); t++) { }
    ADC1->CR2 |= (1UL << 2);                                  /* CAL */
    for (t = 0; t < 100000UL && (ADC1->CR2 & (1UL << 2)); t++) { }
}

static uint16_t adc_uma(void)                                 /* 0xFFFF = erro */
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
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFFUL) | 0xDD000000UL;  /* PB6/PB7: AF open-drain (preserva PB0..PB5) */
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
    TIM4->PSC = (g_hclk_hz / 1000000UL) - 1UL;                /* contador a 1 MHz (1 us) */
    TIM4->ARR = 0xFFFF;
    TIM4->EGR = 1UL;
    TIM4->CR1 = 1UL;
}

static US_Status us_medir(uint16_t *mm, uint16_t *largura_us)
{
    uint16_t t0, dur;
    uint32_t calc;

    GPIOB->BSRR = (1UL << 8);                                 /* pulso de 10 us no TRIG */
    t0 = (uint16_t)TIM4->CNT;
    while ((uint16_t)((uint16_t)TIM4->CNT - t0) < 10U) { }
    GPIOB->BRR = (1UL << 8);

    t0 = (uint16_t)TIM4->CNT;                                 /* espera o ECHO subir */
    while (!(GPIOB->IDR & (1UL << 9)))
        if ((uint16_t)((uint16_t)TIM4->CNT - t0) > 30000U) return US_SEM_ECO;
    t0 = (uint16_t)TIM4->CNT;                                 /* mede a largura do ECHO */
    while (GPIOB->IDR & (1UL << 9))
        if ((uint16_t)((uint16_t)TIM4->CNT - t0) > 30000U) return US_SEM_OBJETO;
    dur = (uint16_t)((uint16_t)TIM4->CNT - t0);

    *largura_us = dur;
    calc = ((uint32_t)dur * 343UL) / 2000UL;                  /* mm (343 m/s, ida e volta) */
    *mm = (uint16_t)calc;
    return (calc < 20UL || calc > 4000UL) ? US_FORA : US_OK;
}

/* ========================================================================== */
/*  DHT11 (DATA=PB15) - usa o TIM4 a 1 MHz (iniciado em us_init)               */
/*  Repouso: entrada com pull-up. Inicio: saida open-drain em 0 por 20 ms.     */
/* ========================================================================== */
#define DHT_PINO   15U
#define DHT_BIT    (1UL << DHT_PINO)
#define DHT_SH     28U                                        /* PB15 = bits 31..28 do CRH */

typedef enum { DHT_OK = 0, DHT_SEM_RESP, DHT_TIMEOUT, DHT_CHECKSUM } DHT_Status;

static void dht_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << DHT_SH)) | (0x8UL << DHT_SH);   /* entrada pull-up/down */
    GPIOB->BSRR = DHT_BIT;                                                /* ODR=1 -> pull-up */
}

/* Espera enquanto o pino estiver em 'nivel'. Retorna a duracao em us, ou 0xFFFF se estourar. */
static uint16_t dht_espera(uint8_t nivel, uint16_t max_us)
{
    uint16_t t0 = (uint16_t)TIM4->CNT;
    uint16_t dt;

    while (((GPIOB->IDR >> DHT_PINO) & 1U) == nivel) {
        dt = (uint16_t)((uint16_t)TIM4->CNT - t0);
        if (dt > max_us) return 0xFFFF;
    }
    return (uint16_t)((uint16_t)TIM4->CNT - t0);
}

static DHT_Status dht_ler(uint8_t *umid, uint8_t *temp)
{
    uint8_t d[5] = { 0, 0, 0, 0, 0 };
    uint8_t i;
    uint16_t w;

    /* pulso de inicio: puxa a linha para 0 por 20 ms */
    GPIOB->BRR = DHT_BIT;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << DHT_SH)) | (0x6UL << DHT_SH);   /* saida open-drain 2 MHz */
    delay_ms(20);
    GPIOB->BSRR = DHT_BIT;
    GPIOB->CRH = (GPIOB->CRH & ~(0xFUL << DHT_SH)) | (0x8UL << DHT_SH);   /* solta (pull-up) */

    /* resposta do sensor: ~20-40 us em alto, 80 us em baixo, 80 us em alto */
    if (dht_espera(1, 100) == 0xFFFF) return DHT_SEM_RESP;
    if (dht_espera(0, 120) == 0xFFFF) return DHT_SEM_RESP;
    if (dht_espera(1, 120) == 0xFFFF) return DHT_SEM_RESP;

    /* 40 bits: 50 us em baixo + 26-28 us (bit 0) ou 70 us (bit 1) em alto */
    for (i = 0; i < 40; i++) {
        if (dht_espera(0, 100) == 0xFFFF) return DHT_TIMEOUT;
        w = dht_espera(1, 120);
        if (w == 0xFFFF) return DHT_TIMEOUT;
        d[i / 8U] = (uint8_t)(d[i / 8U] << 1);
        if (w > 40U) d[i / 8U] |= 1U;
    }

    if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return DHT_CHECKSUM;
    *umid = d[0];
    *temp = d[2];
    return DHT_OK;
}

/* ========================================================================== */
/*  MATRIZ DE BOTOES 3x3                                                       */
/*  linha l (0..2) -> PA(2-l) ; coluna c (0..2) -> PC(15-c)                    */
/*  Linhas nao selecionadas ficam em entrada flutuante (sem curto entre linhas)*/
/*  Colunas: entrada com pull-up interno (a placa nao tem pull-up externo)     */
/* ========================================================================== */
#define PINO_LINHA(l)   (2U - (l))
#define PINO_COLUNA(c)  (15U - (c))

static void linha_ativa(uint8_t l, uint8_t ativa)
{
    uint32_t pino = PINO_LINHA(l);
    uint32_t sh = 4U * pino;

    if (ativa) {
        GPIOA->BRR = (1UL << pino);                                   /* ODR = 0 antes de virar saida */
        GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << sh)) | (0x2UL << sh);   /* saida PP 2 MHz */
    } else {
        GPIOA->CRL = (GPIOA->CRL & ~(0xFUL << sh)) | (0x4UL << sh);   /* entrada flutuante */
    }
}

static void botoes_init(void)
{
    uint8_t l;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN;

    for (l = 0; l < 3; l++) linha_ativa(l, 0);

    /* PC13, PC14, PC15: entrada com pull-up (CNF=10, MODE=00 -> 0x8; ODR=1 escolhe pull-up) */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFFFUL << 20)) | (0x888UL << 20);
    GPIOC->BSRR = (1UL << 13) | (1UL << 14) | (1UL << 15);
}

static uint16_t botoes_varre(void)                    /* bit (l*3 + c) = 1 -> pressionado */
{
    uint16_t m = 0;
    uint8_t l, c;
    uint32_t idr;

    for (l = 0; l < 3; l++) {
        linha_ativa(l, 1);
        delay_us(30);                                  /* tempo de acomodacao do pull-up */
        idr = GPIOC->IDR;
        linha_ativa(l, 0);
        for (c = 0; c < 3; c++) {
            if (!(idr & (1UL << PINO_COLUNA(c)))) m |= (uint16_t)(1U << (l * 3U + c));
        }
    }
    return m;
}

static uint16_t botoes_ler(void)                       /* com debounce: vale o que persiste em 2 leituras */
{
    uint16_t a = botoes_varre();
    delay_ms(5);
    return (uint16_t)(a & botoes_varre());
}

/* T1..T6 -> bit da matriz:  T2 T1 S1 / T4 T3 S2 / T5 T6 S3 */
static const uint8_t tac_bit[6] = { 1, 0, 4, 3, 6, 7 };

/* ========================================================================== */
/*  6 LEDs: LED1=PA11 LED2=PA12 LED3=PB5 LED4=PA15 LED5=PB3 LED6=PB4           */
/* ========================================================================== */
static GPIO_T *const led_porta[6] = { GPIOA, GPIOA, GPIOB, GPIOA, GPIOB, GPIOB };
static const uint8_t led_pino[6]  = { 11,    12,    5,     15,    3,     4     };

static void led_set(uint8_t i, uint8_t on)
{
    uint32_t bit = 1UL << led_pino[i];
    uint8_t nivel_alto = LED_ATIVO_ALTO ? on : (uint8_t)!on;
    if (nivel_alto) led_porta[i]->BSRR = bit; else led_porta[i]->BRR = bit;
}

static void leds_init(void)
{
    uint8_t i;

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;

    /* JTAG off, SWD on: libera PA15, PB3 e PB4 */
    AFIO->MAPR = (AFIO->MAPR & ~(7UL << 24)) | (2UL << 24);

    for (i = 0; i < 6; i++) led_set(i, 0);             /* apagados antes de virarem saida */

    /* PA11, PA12, PA15: saida PP 2 MHz (CRH) */
    GPIOA->CRH = (GPIOA->CRH & ~((0xFUL << 12) | (0xFUL << 16) | (0xFUL << 28)))
               | (0x2UL << 12) | (0x2UL << 16) | (0x2UL << 28);
    /* PB3, PB4, PB5: saida PP 2 MHz (CRL) */
    GPIOB->CRL = (GPIOB->CRL & ~((0xFUL << 12) | (0xFUL << 16) | (0xFUL << 20)))
               | (0x2UL << 12) | (0x2UL << 16) | (0x2UL << 20);
}

static uint16_t g_botoes;                              /* ultima leitura da matriz (bit l*3+c) */

static void tarefa_botoes(void)
{
    uint16_t m = botoes_ler();
    uint8_t i;
    uint8_t chave;

    for (i = 0; i < 6; i++) {
        chave = (uint8_t)((m >> ((i / 2U) * 3U + 2U)) & 1U);   /* S1 -> LED1,2  S2 -> LED3,4  S3 -> LED5,6 */
        led_set(i, (uint8_t)((((m >> tac_bit[i]) & 1U) || chave) ? 1U : 0U));
    }
    g_botoes = m;
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

/* ========================================================================== */
/*  ESTADO E TELAS                                                             */
/* ========================================================================== */
static DS3231_Dados rtc;
static I2C_Status   rtc_st = I2C_ERR_TIMEOUT;

static uint16_t  us_mm, us_us;
static US_Status us_st = US_SEM_ECO;
static uint32_t  us_n;

static uint8_t    dht_umid, dht_temp;
static DHT_Status dht_st = DHT_SEM_RESP;
static uint32_t   dht_n;

/* Canais analogicos: indice 0 = LDR, 1 = POT B0, 2 = POT B1 */
#define N_ADC  3U
#define I_LDR  0U
#define I_POT0 1U
#define I_POT1 2U
static const uint8_t adc_canais[N_ADC] = { CANAL_LDR, CANAL_POT0, CANAL_POT1 };
static uint16_t adc_raw[N_ADC];                        /* valores filtrados (0..4095) */
static int32_t  adc_f16[N_ADC];                        /* filtros x16 */
static uint8_t  adc_erro[N_ADC], adc_ini[N_ADC];

static void tarefa_adc(void)
{
    uint8_t i;
    uint16_t a;

    for (i = 0; i < N_ADC; i++) {
        a = adc_media(adc_canais[i], 32);
        adc_erro[i] = (a == 0xFFFF);
        if (adc_erro[i]) continue;
        if (!adc_ini[i]) { adc_f16[i] = (int32_t)a * 16; adc_ini[i] = 1; }
        else             { adc_f16[i] += ((int32_t)a * 16 - adc_f16[i]) / 8; }
        adc_raw[i] = (uint16_t)(adc_f16[i] / 16);
    }
}

static void mostra_rtc(void)
{
    char b[20];
    char *p;

    if (rtc_st != I2C_OK) {
        p = ps(b, "RTC erro ");
        *p++ = (char)('0' + (uint8_t)rtc_st);
        *p = '\0';
        lcd_linha(0, b);
        lcd_linha(1, (rtc_st == I2C_ERR_NACK) ? "sem resp. 0x68" : "barramento I2C");
        return;
    }

    p = p2(b, rtc.hora);    *p++ = ':';
    p = p2(p, rtc.minuto);  *p++ = ':';
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

static void mostra_dist(void)
{
    char b[20];
    char *p;

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
        lcd_linha(0, (us_st == US_SEM_ECO)    ? "Sem eco (fiacao?)" :
                     (us_st == US_SEM_OBJETO) ? "Sem objeto"        : "Fora da faixa");
        p = ps(b, "Leitura #"); p = pu(p, us_n); *p = '\0';
        lcd_linha(1, b);
    }
}

static void mostra_ldr(void)
{
    char b[20];
    char *p;

    if (adc_erro[I_LDR]) {
        lcd_linha(0, "LDR A3: erro ADC");
        lcd_linha(1, "");
        return;
    }
    p = ps(b, "LDR A3: "); p = p4(p, adc_raw[I_LDR]);
    *p = '\0';
    lcd_linha(0, b);

    p = p_volts(b, ((uint32_t)adc_raw[I_LDR] * VREF_MV) / 4095UL);
    *p++ = ' ';
    p = pu(p, ((uint32_t)adc_raw[I_LDR] * 100UL) / 4095UL);
    *p++ = '%';
    *p = '\0';
    lcd_linha(1, b);
}

static void mostra_pot_linha(uint8_t linha, const char *nome, uint8_t i)
{
    char b[20];
    char *p = ps(b, nome);                              /* "B0:" / "B1:" */

    if (adc_erro[i]) p = ps(p, "erro");
    else {
        p = p4(p, adc_raw[i]);
        *p++ = ' ';
        p = p_volts(p, ((uint32_t)adc_raw[i] * VREF_MV) / 4095UL);
    }
    *p = '\0';
    lcd_linha(linha, b);
}

static void mostra_pots(void)
{
    mostra_pot_linha(0, "B0:", I_POT0);
    mostra_pot_linha(1, "B1:", I_POT1);
}

static void mostra_botoes(void)
{
    char b[20];
    char *p;
    uint8_t i;

    p = ps(b, "TAC: ");
    for (i = 0; i < 6; i++)
        *p++ = ((g_botoes >> tac_bit[i]) & 1U) ? (char)('1' + i) : '-';
    *p = '\0';
    lcd_linha(0, b);

    p = b;
    if (MATRIZ_DEBUG) {                                /* 9 bits crus: [l2c2 ... l0c0] */
        p = ps(p, "M:");
        for (i = 9; i > 0; i--) *p++ = ((g_botoes >> (i - 1U)) & 1U) ? '1' : '0';
    } else {                                           /* S1..S3: bits 2, 5 e 8 da matriz */
        for (i = 0; i < 3; i++) {
            *p++ = 'S';
            *p++ = (char)('1' + i);
            *p++ = ':';
            *p++ = ((g_botoes >> (i * 3U + 2U)) & 1U) ? '1' : '0';
            if (i < 2) *p++ = ' ';
        }
    }
    *p = '\0';
    lcd_linha(1, b);
}

static void mostra_dht(void)
{
    char b[20];
    char *p;

    if (dht_st == DHT_OK) {
        p = ps(b, "DHT11 Temp:"); p = pu(p, dht_temp); p = ps(p, "C");
        *p = '\0';
        lcd_linha(0, b);
        p = ps(b, "      Umid:"); p = pu(p, dht_umid); p = ps(p, "%");
        *p = '\0';
        lcd_linha(1, b);
    } else {
        lcd_linha(0, (dht_st == DHT_SEM_RESP) ? "DHT11 sem resp." :
                     (dht_st == DHT_TIMEOUT)  ? "DHT11 timeout"   : "DHT11 checksum");
        p = ps(b, "PB15 leitura #"); p = pu(p, dht_n); *p = '\0';
        lcd_linha(1, b);
    }
}

/* ========================================================================== */
/*  MAIN                                                                       */
/* ========================================================================== */
int main(void)
{
    uint32_t agora, t_rtc = 0, t_us = 0, t_adc = 0, t_lcd = 0, t_bot = 0, t_dht = 0;
    uint8_t pg;
    uint8_t u, t;
    DHT_Status st;

    clock_init();
    leds_init();                                        /* antes do LCD: configura AFIO/JTAG */
    lcd_init();
    adc_init();
    i2c_init();
    us_init();                                          /* inicia o TIM4 (tambem usado pelo DHT11) */
    dht_init();
    botoes_init();

    lcd_linha(0, "TCC1 UNISATC");
    lcd_linha(1, "Plataforma 1.0");
    delay_ms(1500);

    while (1) {
        agora = agora_ms();

        if ((agora - t_bot) >= 20U) {
            t_bot = agora;
            tarefa_botoes();
        }
        if ((agora - t_us) >= 200U) {
            t_us = agora;
            us_st = us_medir(&us_mm, &us_us);
            us_n++;
        }
        if ((agora - t_adc) >= 100U) {
            t_adc = agora;
            tarefa_adc();
        }
        if ((agora - t_rtc) >= 500U) {
            t_rtc = agora;
            rtc_st = ds3231_ler(&rtc);
        }
        if ((agora - t_dht) >= 2000U) {                 /* DHT11: no minimo 1 s entre leituras */
            t_dht = agora;
            st = dht_ler(&u, &t);
            dht_st = st;
            dht_n++;
            if (st == DHT_OK) { dht_umid = u; dht_temp = t; }
        }

        pg = (uint8_t)((agora / 15000U) % 6U);           /* troca de pagina a cada 3 s */
        if ((agora - t_lcd) >= ((pg == 4U) ? 100U : 500U)) {
            t_lcd = agora;
            switch (pg) {
            case 0:  mostra_rtc();    break;
            case 1:  mostra_dist();   break;
            case 2:  mostra_ldr();    break;
            case 3:  mostra_pots();   break;
            case 4:  mostra_botoes(); break;
            default: mostra_dht();    break;
            }
        }
    }
}
