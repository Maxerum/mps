#include <msp430.h>

int change_timer = -1;
int s1_expected_state = 0;
int s2_expected_state = 0;
int led_state = 0;
int current_timer = 0;
int s2_running = 0;
int s1_running = 0;

void set_timers()
{
    TA1CTL = TACLR;
    TA1CTL |= TASSEL__ACLK |ID__4|MC__STOP;
    TA1CCTL0 |= CCIE;

    TA2CTL = TACLR;
    TA2CTL |= TASSEL__ACLK |MC__CONTINUOUS |ID__1;
    TA2CCTL0 &= ~CCIE;
    TA2CCTL1 &= ~CCIE;

    SFRIE1 |= WDTIE;
}


void leb_blinking(void)
{
    //all leds on
    switch(led_state){
    case 0:
        P1OUT |= BIT0;
        P8OUT |= BIT1;
        P8OUT |= BIT2;
        break;
    case 1:
        P1OUT &= ~BIT0;
        break;
    case 2:
        P8OUT &= ~BIT1;
        break;
    case 3:
        P8OUT &= ~BIT2;
        break;
    }

    led_state += 1;
    //stop timers
    TA1CTL &= ~MC__UP;
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer

    switch (current_timer) {
       case 0://TA1
           TA1R = 0;
           switch(led_state){
                case 1:
                    TA1CCR0 = 800;
                    break;
                case 2:
                    TA1CCR0 = 1600;
                    break;
                case 3:
                    TA1CCR0 = 3200;
                    break;
           }
           TA1CTL |= TASSEL__ACLK |ID__4|MC__UP|TACLR;
           break;
       case 1://WDT
           switch(led_state){
               case 1:
                    UCSCTL5 = 0;
                    UCSCTL5 = DIVS__4;
                    WDTCTL = WDTPW |WDTSSEL__SMCLK |WDTTMSEL|WDTCNTCL|WDTIS__32K;
                    break;
               case 2:
                    UCSCTL5 = 0;
                    UCSCTL5 = DIVS__8;
                    WDTCTL = WDTPW |WDTSSEL__SMCLK |WDTTMSEL|WDTCNTCL|WDTIS__32K;
                    break;
               case 3:
                  UCSCTL2 = 0;
                  UCSCTL3 = 0;
                  UCSCTL5 = DIVS__1;
                  UCSCTL2 |= FLLD__2; // FLLD = 2
                  UCSCTL3 |= SELREF__REFOCLK;// FLLREFCLK = 32768
                  UCSCTL3 |= FLLREFDIV__1;
                  UCSCTL2 |= FLLN1|FLLN2|FLLN5;//38
                  WDTCTL = WDTPW |WDTSSEL__ACLK|WDTTMSEL|WDTCNTCL |WDTIS__8192;
//                    WDTCTL = WDTPW |WDTSSEL__SMCLK |WDTTMSEL|WDTCNTCL|WDTIS__32K;
//                    UCSCTL5 = DIVS__16;
                    break;
           }
           break;
    }
}


#pragma vector = TIMER1_A0_VECTOR
__interrupt void led_blinking_ta(void) {
    TA1CTL &= ~MC__UP;

    leb_blinking();
}

#pragma vector = WDT_VECTOR
__interrupt void led_blinking_wdt(void) {
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer

    leb_blinking();
}
//s1 interrupt
#pragma vector = PORT1_VECTOR
__interrupt void first_button_interrupt()
{
    unsigned char button_1_pressed = (P1IES&BIT7) ? 1 : 0; // if the button is pressed - 1, else - 0
    s1_expected_state = button_1_pressed;

    if (!s1_running)
    {
        s1_running = 1;
        TA2CCR0 = TA2R + 4000; // The TAxR register is the count of Timer_A
        TA2CCTL0 |= CCIE; // Capture/compare interrupt enable for S1
    }

    P1IFG &= ~BIT7;
    P1IES ^= BIT7;
}
//s2 interrupt
#pragma vector = PORT2_VECTOR
__interrupt void second_button_interrupt() {

    unsigned char button_2_pressed = (P2IES&BIT2) ? 1 : 0; // if the button is pressed - 1, else - 0
    s2_expected_state = button_2_pressed;

    if (!s2_running)
    {
       s2_running = 1;
       TA2CCR1 = TA2R + 4000; // The TAxR register is the count of Timer_A
       TA2CCTL1 |= CCIE; // Capture/compare interrupt enable for S2
    }

    P2IFG &= ~BIT2;
    P2IES ^= BIT2;
}

#pragma vector = TIMER2_A0_VECTOR
__interrupt void timer_s1(void) {
    TA2CCTL0 &= ~CCIE; // disable interrupt for S1

    // check if button still in expected state
    unsigned char button_1_state = ((P1IN&BIT7) ? 0 : 1); // if the button is pressed - 1, else - 0

    if (button_1_state != s1_expected_state)
    {
        s1_running = 0;
        return;
    }
    if (s1_expected_state){
        led_state = 0;
        leb_blinking();
    }
    else if (!s1_expected_state){
        led_state = 0;
        leb_blinking();
    }
    s1_running = 0;
}

#pragma vector = TIMER2_A1_VECTOR
__interrupt void timer_s2(void) {
    TA2CCTL1 &= ~CCIE;

    unsigned char button_2_state = ((P2IN&BIT2) ? 0 : 1);

    if (button_2_state != s2_expected_state)
    {
        s2_running = 0;
        return;
    }

    if (s2_expected_state)
        switch (change_timer) {
            case 0:
            case 1:
                current_timer = change_timer;
                break;
            case -1:
                current_timer = current_timer ? 0 : 1;
                break;
            }
    s2_running = 0;
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
    __bis_SR_register(GIE); // for enable interrupts

    //s1
    P1DIR &= ~BIT7;
    P1OUT |= BIT7;
    P1REN |= BIT7;
    P1IE  |= BIT7;
    P1IFG &= ~BIT7;
    P1IES |= BIT7;

    // s2
    P2DIR &= ~BIT2;
    P2OUT |= BIT2;
    P2REN |= BIT2;
    P2IE  |= BIT2;
    P2IFG &= ~BIT2;
    P2IES |= BIT2;

    //leds 1-3 and led 5
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;

    P8DIR |= BIT1;
    P8OUT &= ~BIT1;

    P8DIR |= BIT2;
    P8OUT &= ~BIT2;

    P1OUT &= ~BIT5;
    P1DIR |= BIT5;
    P1SEL |= BIT5;

    set_timers();

    TA0CTL = TASSEL_1 | ID__4 | MC__UP | TACLR; // set source to ACLK, divider to 4, count to up and clear A0 counter

    TA0CCTL4 = (OUTMOD_6);  // toggle-set mode
    TA0CCR0 = 4000;        //0,5 s is a period
    TA0CCR4 = 800; // 0.1 is a duty cycle

    __bis_SR_register(LPM0_bits); // put the controller into the mode LPM0
    __no_operation();
    return 0;
}

