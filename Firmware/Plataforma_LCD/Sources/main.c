
#include <stdint.h>

/* ================= REGISTRADORES RCC ================= */

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018U)

/* ================= GPIOA ================= */

#define GPIOA_CRL   (*(volatile uint32_t *)0x40010800U)
#define GPIOA_ODR   (*(volatile uint32_t *)0x4001080CU)

/* ================= GPIOB ================= */

#define GPIOB_CRH   (*(volatile uint32_t *)0x40010C04U)
#define GPIOB_ODR   (*(volatile uint32_t *)0x40010C0CU)

/* ================= GPIOC ================= */

#define GPIOC_CRH   (*(volatile uint32_t *)0x40011004U)
#define GPIOC_ODR   (*(volatile uint32_t *)0x4001100CU)

/* ================= SYSTICK ================= */

#define SYST_CSR    (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR    (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR    (*(volatile uint32_t *)0xE000E018U)

/* ================= PINOS DO LCD ================= */

/* Controle */
#define LCD_RS      (1U << 12)  /* PB12 */
#define LCD_E       (1U << 14)  /* PB14 */

/* Dados: mapeamento confirmado pelo usuário */
#define LCD_DB7     (1U << 7)   /* PA7 */
#define LCD_DB6     (1U << 6)   /* PA6 */
#define LCD_DB5     (1U << 5)   /* PA5 */
#define LCD_DB4    (1U << 4)   /* PA4 */

#define LCD_DATA_MASK (LCD_DB4 | LCD_DB5 | LCD_DB6 | LCD_DB7)

/* LED onboard da Blue Pill */
#define LED_PC13    (1U << 13)

/* ================= TEMPORIZAÇÃO ================= */

/*
 * Esta configuração pressupõe SysTick operando a 8 MHz.
 * Se o projeto estiver configurado para outra frequência,
 * ajuste o valor de SYST_RVR.
 */

void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        SYST_CSR = 0;
        SYST_RVR = 7999U;
        SYST_CVR = 0;
        SYST_CSR = (1U << 2) | (1U << 0);

        while ((SYST_CSR & (1U << 16)) == 0)
        {
            /* Aguarda 1 ms */
        }

        SYST_CSR = 0;
    }
}

/* ================= GPIO ================= */

void gpio_init(void)
{
    /* Habilita clocks de GPIOA, GPIOB e GPIOC */
RCC_APB2ENR |= (1U << 2) | (1U << 3) | (1U << 4);

    /* PA4, PA5, PA6 e PA7: saída push-pull, 2 MHz */
    GPIOA_CRL &= ~(
        (0xFU << 16) |
        (0xFU << 20) |
        (0xFU << 24) |
        (0xFU << 28)
    );

    GPIOA_CRL |= (
        (0x2U << 16) |
        (0x2U << 20) |
        (0x2U << 24) |
        (0x2U << 28)
    );

    /* PB12 e PB14: saída push-pull, 2 MHz */
    GPIOB_CRH &= ~(
        (0xFU << 16) |
        (0xFU << 24)
    );

    GPIOB_CRH |= (
        (0x2U << 16) |
        (0x2U << 24)
    );

    /* PC13: saída push-pull, 2 MHz */
    GPIOC_CRH &= ~(0xFU << 20);
    GPIOC_CRH |=  (0x2U << 20);

    /* Inicializa sinais do LCD em nível baixo */
    GPIOB_ODR &= ~(LCD_RS | LCD_E);
    GPIOA_ODR &= ~LCD_DATA_MASK;

    /* LED desligado: PC13 é ativo em nível baixo */
    GPIOC_ODR |= LED_PC13;
}

/* ================= LCD: PULSO ENABLE ================= */

void lcd_pulse_enable(void)
{
    GPIOB_ODR |= LCD_E;
    delay_ms(1);

    GPIOB_ODR &= ~LCD_E;
    delay_ms(1);
}

/* ================= LCD: ENVIO DE 4 BITS ================= */

void lcd_write_nibble(uint8_t nibble)
{
    uint32_t data = 0;

    /* bit 0 -> PA7 (DB4) */
    if (nibble & 0x01U) data |= LCD_DB4;

    /* bit 1 -> PA6 (DB5) */
    if (nibble & 0x02U) data |= LCD_DB5;

    /* bit 2 -> PA5 (DB6) */
    if (nibble & 0x04U) data |= LCD_DB6;

    /* bit 3 -> PA4 (DB7) */
    if (nibble & 0x08U) data |= LCD_DB7;

    GPIOA_ODR = (GPIOA_ODR & ~LCD_DATA_MASK) | data;

    lcd_pulse_enable();
}

/* ================= LCD: ENVIO DE BYTE ================= */

void lcd_write_byte(uint8_t value, uint8_t is_data)
{
    if (is_data)
        GPIOB_ODR |= LCD_RS;
    else
        GPIOB_ODR &= ~LCD_RS;

    /* Envia os 4 bits superiores primeiro */
    lcd_write_nibble((value >> 4) & 0x0FU);

    /* Depois envia os 4 bits inferiores */
    lcd_write_nibble(value & 0x0FU);

    delay_ms(2);
}

void lcd_command(uint8_t command)
{
    lcd_write_byte(command, 0);
}

void lcd_data(uint8_t data)
{
    lcd_write_byte(data, 1);
}

/* ================= LCD: INICIALIZAÇÃO ================= */

void lcd_init(void)
{
    /* Aguarda estabilização da alimentação */
    delay_ms(50);

    GPIOB_ODR &= ~LCD_RS;

    /* Sequência de inicialização para modo de 4 bits */
    lcd_write_nibble(0x03);
    delay_ms(5);

    lcd_write_nibble(0x03);
    delay_ms(2);

    lcd_write_nibble(0x03);
    delay_ms(2);

    lcd_write_nibble(0x02);
    delay_ms(2);

    /* Interface 4 bits, 2 linhas, matriz 5x8 */
    lcd_command(0x28);

    /* Display ligado, cursor desligado */
    lcd_command(0x0C);

    /* Limpa o display */
    lcd_command(0x01);
    delay_ms(3);

    /* Incrementa cursor, sem deslocar display */
    lcd_command(0x06);
}

/* ================= LCD: POSICIONAMENTO ================= */

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0)
        address = 0x00U + col;
    else
        address = 0x40U + col;

    lcd_command(0x80U | address);
}

/* ================= LCD: TEXTO ================= */

void lcd_print(const char *text)
{
    while (*text)
    {
        lcd_data((uint8_t)*text);
        text++;
    }
}

/* ================= PROGRAMA PRINCIPAL ================= */

int main(void)
{
    uint32_t seconds = 0;

    gpio_init();

    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("TCC STM32");

    lcd_set_cursor(1, 0);
    lcd_print("LCD 1602 OK");

    while (1)
    {
        /* LED PC13: alterna a cada 500 ms */
        GPIOC_ODR ^= LED_PC13;
        delay_ms(500);

        seconds++;

        /* Atualiza a segunda linha a cada segundo */
        lcd_set_cursor(1, 0);
        lcd_print("Tempo: ");

        /* Exibe os segundos em até 4 dígitos */
        lcd_data((seconds / 1000U) % 10U + '0');
        lcd_data((seconds / 100U) % 10U + '0');
        lcd_data((seconds / 10U) % 10U + '0');
        lcd_data(seconds % 10U + '0');

        lcd_print(" s   ");
    }

    return 0;
}

