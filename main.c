#include <msp430.h>

int clkEnabled = 0;
int lpm3_enabled = 0;

#pragma vector = PORT1_VECTOR
__interrupt void first_button_interrupt() {
    P1IFG &= ~BIT7;
    if(!(P1IN&BIT7)){
        if(!lpm3_enabled){
            __bis_SR_register_on_exit(LPM3_bits);
        }
        else{
            __bic_SR_register_on_exit(LPM3_bits);
        }
        lpm3_enabled = lpm3_enabled ? 0 : 1;
    }
    __delay_cycles(1000);

    return;
}

#pragma vector = PORT2_VECTOR
__interrupt void second_button_interrupt()
{
    P2IFG &= ~BIT2;

    if(!(P2IN&BIT2)){
        if (!clkEnabled) {
            UCSCTL4 &= 0;
            UCSCTL5 &= 0;

            UCSCTL4 |= SELM__REFOCLK;
            UCSCTL5 |= DIVM__2;
        }
        else
        {
            UCSCTL4 &= 0;
            UCSCTL5 &= 0;

            UCSCTL4 |= SELM__DCOCLKDIV;
            UCSCTL5 |= DIVM__8;
        }
        clkEnabled = clkEnabled ? 0 : 1;
    }

  __delay_cycles(100);

    return;
}



void setup_buttons() {
    P1DIR &= ~BIT7;
    P1REN |= BIT7;
    P1OUT |= BIT7;

    P2DIR &= ~BIT2;
    P2REN |= BIT2;
    P2OUT |= BIT2;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;


    P7DIR |= BIT7;
    P7SEL |= BIT7;

    setup_buttons();

    //button 1 interrupt settings
    P1IES |= BIT7;
    P1IE |= BIT7;
    P1IFG &= ~BIT7;

    //button2 interrupt settings

    P2IES |= BIT2;
    P2IE |= BIT2;
    P2IFG &= ~BIT2;

    __bis_SR_register(GIE); // enable interrupts

    UCSCTL1 &= 0;
    UCSCTL2 &= 0;
    UCSCTL3 &= 0;
    UCSCTL4 &= 0;
    UCSCTL5 &= 0;

    UCSCTL1 |= DCORSEL_0;  // 0.07  - 1.7 MHz

    UCSCTL2 |= FLLN1|FLLN2|FLLN3; // FLLN = 14
    UCSCTL2 |= FLLD__2; // FLLD = 2

    UCSCTL3 |= SELREF__REFOCLK;// FLLREFCLK = 32768
    UCSCTL3 |= FLLREFDIV__1; // FLLREFDIV = 1

    UCSCTL4 |= SELM__DCOCLK;
    UCSCTL5 |= DIVM__8;

    // 980 000 = rllrefclk/fllrefdiv * (FLLN + 1) * FLLD
    //980 000 = 983 040

    __no_operation();

    return 0;
}
