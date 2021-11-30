#include <math.h>
#include <stdint.h>

#include <msp430.h>

#define FONT_COLUMNS 9

typedef struct {
    char name;
    uint8_t shape[FONT_COLUMNS]; // 9x6
} Symbol;

extern const Symbol SYMBOLS[];
extern const unsigned int SYMBOLS_COUNT;
const Symbol SYMBOLS[] = {{'+', {0, 0, 0, 0x20, 0x70, 0x20, 0, 0, 0} }, {'-', {0, 0, 0, 0, 0xF0, 0, 0, 0, 0} }, { '0', {0xF0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xF0}
}, {'1', {0x20, 0x60, 0xA0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xF0} }, { '2', {0xF0, 0x90, 0x10, 0x10, 0x10, 0x20, 0x40, 0x80, 0xF0} }, { '3', {0xF0, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10, 0xF0}
}, { '4', {0x90, 0x90, 0x90, 0x90, 0xF0, 0x10, 0x10, 0x10, 0x10} }, { '5', {0xF0, 0x80, 0x80, 0x80, 0xF0, 0x10, 0x10, 0x10, 0xF0} }, { '6', {0xF0, 0x80, 0x80, 0x80, 0xF0, 0x90, 0x90, 0x90, 0xF0}
}, {'7', {0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10} }, { '8', {0xF0, 0x90, 0x90, 0x90, 0xF0, 0x90, 0x90, 0x90, 0xF0} }, {'9', {0xF0, 0x90, 0x90, 0x90, 0xF0, 0x10, 0x10, 0x10, 0xF0}
},
};

const unsigned int SYMBOLS_COUNT = sizeof(SYMBOLS) / sizeof(Symbol);

#define FRAME_PAGES 8
#define FRAME_COLUMNS 102

// set column CA=[0, 131]
#define LCD_CMD_COL_LOW  0x00 // | CA[3..0]
#define LCD_CMD_COL_HIGH 0x10 // | CA[7..4]

#define LCD_CMD_PWR 0x28
#define LCD_CMD_PWR__AMP 0x01
#define LCD_CMD_PWR__REG 0x02
#define LCD_CMD_PWR__REP 0x04

#define LCD_CMD_SCROLL 0x40 // | SL[5..0]
#define LCD_CMD_PAGE 0xB0 // | PA[3..0]

#define LCD_CMD_BRIGHT 0x20 // | PC[5..3]
#define LCD_CMD_CONTRAST_1 0x81
#define LCD_CMD_CONTRAST_2 0x00 // | PM[5..0]

#define LCD_CMD_ALL_NO 0xA4

#define LCD_CMD_INV_NO 0xA6

#define LCD_CMD_EN_ON 0xAE | 0x01

#define LCD_CMD_COL_ORDER_NORMAL 0xA0

#define LCD_CMD_ROW_ORDER_NORMAL 0xC8

#define LCD_CMD_PWR_OFFSET_1_9 0xA2

#define LCD_CMD_EXT_1 0xFA
#define LCD_CMD_EXT_2 0x10
#define LCD_CMD_EXT_2__TEMP       0x80
#define LCD_CMD_EXT_2__COL_CYCLE  0x02
#define LCD_CMD_EXT_2__PAGE_CYCLE 0x01

static float axis_to_g(unsigned char value);


// defines

#define CD              BIT6
#define CS              BIT4

void Dogs102x6_writeCommand(uint8_t* sCmd, uint8_t i)
{
    P7OUT &= ~CS;
    P5OUT &= ~CD;//режим команды  включить

    while (i)
    {
        while (!(UCB1IFG & UCTXIFG));

        UCB1TXBUF = *sCmd; // буфер передачи

        sCmd++;
        i--;
    }

    while (UCB1STAT & UCBUSY);
    // Dummy read to empty RX buffer and clear any overrun conditions
    UCB1RXBUF;//буфер приемника

    P7OUT |= CS;
}

void Dogs102x6_writeData(uint8_t* sData, uint8_t i)
{
    P7OUT &= ~CS;//выбор устройства какой-то  непонятно какой
    P5OUT |= CD;//режим данных включить

    while (i)//пока не запишутся все переданные в функиию данные
    {
        while (!(UCB1IFG & UCTXIFG));

        UCB1TXBUF = *sData;

        sData++;
        i--;
    }

    while (UCB1STAT & UCBUSY);
    // Dummy read to empty RX buffer and clear any overrun conditions
    UCB1RXBUF;

    P7OUT |= CS;
}

// variables
static uint8_t frame[FRAME_PAGES][FRAME_COLUMNS];
#define LED5_OFF

static uint8_t initCmds[] = {
    LCD_CMD_SCROLL,
    LCD_CMD_COL_ORDER_NORMAL,
    LCD_CMD_ROW_ORDER_NORMAL,
    LCD_CMD_ALL_NO,
    LCD_CMD_INV_NO,
    LCD_CMD_PWR_OFFSET_1_9,
    LCD_CMD_PWR | LCD_CMD_PWR__AMP | LCD_CMD_PWR__REG | LCD_CMD_PWR__REP,
    LCD_CMD_BRIGHT | 0x07, //0x27
    LCD_CMD_CONTRAST_1,
    LCD_CMD_CONTRAST_2 | 0x10, // 0x10
    LCD_CMD_EXT_1,
    LCD_CMD_EXT_2 | LCD_CMD_EXT_2__TEMP,
    LCD_CMD_EN_ON,
};

static const int initCmdsCount = sizeof(initCmds) / sizeof(uint8_t);

static inline void set_page(uint8_t page)
{
    uint8_t cmd[1];
    cmd[0] = LCD_CMD_PAGE | page;
    Dogs102x6_writeCommand(cmd, 1);
}

static inline void set_column(uint8_t column)
{
    uint8_t cmd[2];
    cmd[0] = LCD_CMD_COL_LOW | (column & 0x0F);
    cmd[1] = LCD_CMD_COL_HIGH | ((column & 0xF0) >> 4);
    Dogs102x6_writeCommand(cmd, 2);
}

void lcd_sync(int columns)
{
    uint8_t page, column;
//    const int offset = columns ? (FRAME_COLUMNS - FONT_COLUMNS * columns) : 0;
    for (page = 0; page < FRAME_PAGES; page++)
    {
        set_page(7 - page);
        set_column(30);

        for (column = FRAME_COLUMNS  - 1; column > 0; column--)
        {
            Dogs102x6_writeData(&frame[page][column], 1);
        }
    }
}

void lcd_clear(void)
{
    int page, column;
    for (page = 0; page < FRAME_PAGES; page++)
    {
        for (column = 0; column < FRAME_COLUMNS; column++)
        {
            frame[page][column] = 0;
        }
    }
}

void lcd_init(void)
{
    P4SEL |= BIT1; //устанавливаем на периферию
    P4SEL |= BIT3; //устанавливаем на периферию

    P5DIR |= BIT6; //устанавливаем направление пина на выход
    P5DIR |= BIT7;

    P7DIR |= BIT4;
    P7DIR |= BIT6;

    P5OUT |= BIT7; // чтобы не было сброса
    P5OUT &= ~BIT6; // для отправки команд
    P7OUT |= BIT4; // если 0, то устройство выбрано
    P7OUT |= BIT6; // включение подсветки

    // configure SPI
    UCB1CTL1 = UCSWRST // USCI Software Reset(разрешение программного сброса)
           | UCSSEL__SMCLK; // Clock Source: SMCLK
    UCB1CTL0 = UCSYNC // Sync-Mode  0:UART-Mode / 1:SPI-Mode Режим: синхронный - 1
            | UCMST //  Sync. Mode: Master Select Режим: Master
            | UCMSB  // Порядок передачи:- MSB(старший значащий бит)
            | UCMODE_0 // Sync. Mode: USCI Mode: 0 (Синхронный режим: 00 – 3pin SPI)
            | UCCKPL; // Sync. Mode: Clock Polarity
    UCB1BR0 = 0x01; // UCA0BR0 - младший байт делителя частоты
    UCB1BR1 = 0; // старший байт делителя частоты
    UCB1CTL1 &= ~UCSWRST; // USCI Software Reset(запрет программного сброса)

    // send init commands
    Dogs102x6_writeCommand(initCmds, initCmdsCount);

    lcd_clear();
    lcd_sync(0);
}

void lcd_draw_symbol(char symbol, int position, int offset)
{
    if (position >= FRAME_PAGES)
        return;

    int page = position;

    int s;
    for (s = 0; s < SYMBOLS_COUNT; s++)
    {
        if (SYMBOLS[s].name == symbol)
        {
            break;
        }
    }

    if (s == SYMBOLS_COUNT) // not found
        return;

    const Symbol* S = &SYMBOLS[s];

    int i;
    for (i = 0; i < sizeof(S->shape); i++)
    {
        frame[page][FRAME_COLUMNS-1-i-offset] |= S->shape[i];
    }
}

void lcd_draw_int(int value, int offset)
{
        int sign = value >= 0; // выводим знак
        //    lcd_draw_symbol(sign ? '+' : '-', FRAME_PAGES -1, 0);//сместить вывод знака на количество postions и выводить последним

            char digits[5];
            unsigned int positions = 0;

            short value_buffer = value;
            if (value_buffer < 0)
                value_buffer = -value_buffer;

            do {
                unsigned char offset = value_buffer % 10;
                digits[positions] = '0' + offset;
                value_buffer /= 10;
                positions++;
            } while (value_buffer);


            int i;
            for (i = 0; i < positions; i++)
            {
                lcd_draw_symbol(digits[i], FRAME_PAGES -1- i, 0);
            }

            lcd_draw_symbol(sign ? '+' : '-', FRAME_PAGES -1 - positions, 0);
}

long int get_angle(long int projection)
{
    double proj = projection;

    proj = proj > 1 ? 1 : proj < -1 ? -1 : proj;

    volatile double angle = acos(proj);

    angle *= 57.296;

    return (long int)angle;
}

uint8_t accel_writeCommand(uint8_t firstByte, uint8_t secondByte) {
    char indata;

    P3OUT &= ~BIT5;

    indata = UCA0RXBUF;

    while(!(UCA0IFG & UCTXIFG));

    UCA0TXBUF = firstByte;

    while(!(UCA0IFG & UCRXIFG));

    indata = UCA0RXBUF;

    while(!(UCA0IFG & UCTXIFG));

    UCA0TXBUF = secondByte;

    while(!(UCA0IFG & UCRXIFG));

    indata = UCA0RXBUF;

    while(UCA0STAT & UCBUSY);

    P3OUT |= BIT5;

    return indata;
}

#pragma vector = PORT2_VECTOR
__interrupt void accelerometerInterrupt(void) {
       float gx = axis_to_g(accel_writeCommand(0x18, 0));
       float gz = axis_to_g(accel_writeCommand(0x20, 0));
       float gy = axis_to_g(accel_writeCommand(0x1C, 0));

       int format = gz * 10;
       long int angle = get_angle(gz);

       if (gz < 0 || gy < 0) {
           angle = (-1) * angle;
       }

       if (angle >= -150 && angle <= -30) {
           P1OUT |= BIT1;
       } else {
           P1OUT &= ~BIT1;
       }

       lcd_clear();
       lcd_draw_int(format, 0);
       lcd_sync(1);
}

void accel_init(void) {
    P2DIR  &= ~BIT5;
    P2OUT  |=  BIT5;
    P2REN  |=  BIT5;
    P2IE   |=  BIT5;
    P2IES  &= ~BIT5;
    P2IFG  &= ~BIT5;

    P3DIR  |=  BIT5;
    P3OUT  |=  BIT5;

    P2DIR  |=  BIT7;
    P2SEL  |=  BIT7;


    P3DIR  |= (BIT3 | BIT6);
    P3DIR  &= ~BIT4;
    P3SEL  |= (BIT3 | BIT4);
    P3OUT  |= BIT6;
    UCA0CTL1 = UCSSEL_2 | UCSWRST;

    UCA0BR0 = 0x01;
    UCA0BR1 = 0x0;

    UCA0CTL0 = UCCKPH & ~UCCKPL | UCMSB | UCMST | UCSYNC | UCMODE_0;

    UCA0CTL1 &= ~UCSWRST;

    accel_writeCommand(0x04, 0);
    __delay_cycles(1250);

    accel_writeCommand(0x0A, BIT4 | BIT2 |BIT1);
    __delay_cycles(25000);
}

float axis_to_g(unsigned char value)
{
    int minus = 0;
    if (value & 0x80)
    {
        minus = 1;
        value = ~value;
    }

    int mg = ((value & 0x40) ? 4571 : 0) + ((value & 0x20) ? 2286 : 0) + ((value & 0x10) ? 1142 : 0) + ((value & 0x08) ?  571 : 0) + ((value & 0x04) ?  286 : 0) + ((value & 0x02) ?  143 : 0) + ((value & 0x01) ?   71 : 0);

    if (minus)
        mg = -mg;

    float g = mg / 1000.;

    return g;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    __bis_SR_register(GIE); // for enable interrupts

    P1DIR |= BIT1; // LED 4
    P1OUT &= ~BIT1;

    lcd_init();
    accel_init();

    __bis_SR_register(LPM0_bits);;
    __no_operation();
}
