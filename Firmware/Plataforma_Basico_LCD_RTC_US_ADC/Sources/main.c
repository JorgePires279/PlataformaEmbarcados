/**
 * main.c - FIRMWARE: LCD 1602 + I2C (DS3231) + HC-SR04 + LDR + 2 POTs
 * Plataforma didatica STM32 Blue Pill (STM32F103C8T6) - TCC UNISATC
 * Somente registradores, sem HAL.
 *
 * PINOS
 *   LCD 1602 : RS=PB12  E=PB14  DB4=PA4  DB5=PA5  DB6=PA6  DB7=PA7  (R/W no GND)
 *   I2C1     : SCL=PB6  SDA=PB7  (DS3231, endereco 0x68)
 *   HC-SR04  : TRIG=PB8  ECHO=PB9  (TIM4 a 1 MHz; PB9 tolera 5 V)
 *   ADC1     : LDR=PA3 (canal 3)  POT B0=PB0 (canal 8)  POT B1=PB1 (canal 9)
 *   LED      : PC13 (pisca a cada 500 ms)
 *
 * LCD: pag 0 = hora/temp/data, pag 1 = distancia, pag 2 = LDR, pag 3 = POTs.
 *      Troca a cada 3 s.
 *
 * NAO testado fisicamente.
 */
#include <stdint.h>

/* ========================================================================== */
/*  REGISTRADORES (STM32F103, RM0008)                                          */
/* ========================================================================== */
#define REG32 volatile uint32_t
typedef struct { REG32 CR, CFGR, CIR, APB2RSTR, APB1RSTR, AHBENR, APB2ENR, APB1ENR, BDCR, CSR; } RCC_T;
typedef struct { REG32 ACR, KEYR, OPTKEYR, SR, CR, AR; }                                          FLASH_T;
typedef struct { REG32 CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; }                                     GPIO_T;
typedef struct { REG32 CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR; }        TIM_T;
typedef struct { REG32 SR, CR1, CR2, SMPR1, SMPR2, JOFR1, JOFR2, JOFR3, JOFR4, HTR, LTR,
                       SQR1, SQR2, SQR3, JSQR, JDR1, JDR2, JDR3, JDR4, DR; }                      ADC_T;
typedef struct { REG32 CR1, CR2, OAR1, OAR2, DR, SR1, SR2, CCR, TRISE; }                          I2C_T;
typedef struct { REG32 CTRL, LOAD, VAL, CALIB; }                                                  SYSTICK_T;

#define RCC       ((RCC_T *)      0x40021000UL)
#define FLASH     ((FLASH_T *)    0x40022000UL)
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
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFFUL) | 0xDD000000UL;  /* PB6/PB7: AF open-drain (preserva PB0/PB1) */
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

/* ========================================================================== */
/*  MAIN                                                                       */
/* ========================================================================== */
int main(void)
{
    uint32_t agora, t_led = 0, t_rtc = 0, t_us = 0, t_adc = 0, t_lcd = 0;

    clock_init();
    lcd_init();
    adc_init();
    i2c_init();
    us_init();

    GPIOC->CRH = (GPIOC->CRH & ~(0xFUL << 20)) | (0x2UL << 20);   /* PC13 saida (LED) */
    GPIOC->BSRR = (1UL << 13);

    lcd_linha(0, "TCC1 UNISATC");
    lcd_linha(1, "Plataforma 1.0");
    delay_ms(1500);

    while (1) {
        agora = agora_ms();

        if ((agora - t_led) >= 500U) {
            t_led = agora;
            GPIOC->ODR ^= (1UL << 13);                  /* LED: sistema vivo */
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
        if ((agora - t_lcd) >= 500U) {
            t_lcd = agora;
            switch ((agora / 12000U) % 4U) {             /* troca de pagina a cada 3 s */
            case 0:  mostra_rtc();  break;
            case 1:  mostra_dist(); break;
            case 2:  mostra_ldr();  break;
            default: mostra_pots(); break;
            }
        }
    }
}
