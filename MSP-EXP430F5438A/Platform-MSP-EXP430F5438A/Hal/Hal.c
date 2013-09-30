/*
 * ============ Platform Configuration for MSP-EXP430F5438A LaunchPad ============
 *
 * The Hal manages the hardware connections between the TI MSP430 (known in the comments below as the EAP)
 * and the Anaren A2541R24x10Gx BLE module (known in the comments below as the MCM).  In addition it manages
 * connections to other devices used in the application (e.g. LEDs, buttons, timers, etc).
 *
 * Note that for clarity's sake, we will denote ports on the EAP as PX.X (with a decimal point) and ports on the
 * MCM as PX_X with an underscore.
 *
 * There are four mandatory connections between the EAP microcontroller and the MCM BLE module.
 *
 *   - EAP_RX             -- Connection between the EAP UART's RXD and the MCM UART's P0_3/TXD signals
 *   - EAP_TX             -- Connection between the EAP UART's TXD and the MCM UART's P0_2/RXD signals
 *   - EAP_RX_ACK         -- Connection between an EAP output GPIO to the MCM's P0_4/CTS input signal
 *   - EAP_TX_ACK         -- Connection between an EAP input GPIO to the MCM's P0_5/RTS output signal
 *
 * In addition there are several optional (but recommended) signals that are present on the Anaren A2541R24A-ADB1 BoosterPack
 * board that can be implemented on custom hardware.  These do not require any port connection on the EAP.  They are
 * directly connected to the MCM:
 *
 *   - MCM RESET          -- The MCM Reset signal (see A2541 datasheet for important requirements)
 *   - MCM P0_0/S1        -- The MCM User Input Switch (normally pulled high, GND on closure)
 *   - MCM P1_7/RXD       -- The MCM Admin UART's RXD line -- used for updating firmware and programming the MCM's parameters
 *   - MCM P1_6/TXD       -- The MCM Admin UART's TXD line -- used for updating firmware and programming the MCM's parameters
 *   - MCM P1_5/RTS       -- The MCM Admin UART's RTS line -- used for updating firmware and programming the MCM's parameters
 *   - MCM P1_4/CTS       -- The MCM Admin UART's CTS line -- used for updating firmware and programming the MCM's parameters
 *   - MCM P0_6/LED1      -- The MCM Red Status LED
 *   - MCM P1_0/LED2      -- The MCM Yellow Status LED
 *   - MCM P2_0/LED3      -- The MCM Blue Status LED
 *
 * And further, there are several optional ports used on the EAP microcontroller in this HAL and with these examples.
 *
 *   - EAP CONNECTED_LED  -- An LED connected to the EAP used to signal connection to a BLE central
 *   - EAP LED            -- An LED connected to the EAP used by the user application.
 *   - EAP DEBUG1         -- An EAP port used by the user application for debug
 *   - EAP DEBUG2         -- An EAP port used by the user application for debug
 *   - EAP BUTTON         -- An EAP input port connected to a switch (normally pulled high, GND on closure)
 *
 * To get a full understanding the following documents prove highly useful:
 *
 *   - MSP430F5438A Datasheet:           TI document SLAS655 (rev D at this time)
 *   - MSP430Fx5xx Family Users Guide:   TI document SLAU208 (rev M at this time)
 *   - MSP-EXP430F5438 Users Guide:      TI document SLAU263 (rev H at this time)
 *   - A2541x24x Booster Pack Users Manual:
 *   - A2541x24x Module Users Manual:
 *
 * Ports specifically used in this HAL:
 *
 *   - MSP430 P1.0 -- output -- CONNECTED_LED connected to RED LED1
 *   - MSP430 P1.1 -- output -- LED connected to YELLOW LED2
 *   - MSP430 P1.2 -- input  -- EAP_TX_ACK   connected to A2541 P0_5/RTS# Experimenter RF3-6 through BoosterPack jumper JP4-1/JP4-2
 *   - MSP430 P1.3 -- output -- EAP_RX_ACK   connected to A2541 P0_4/CTS# Experimenter RF3-14 through BoosterPack jumper JP3-1/JP3-2
 *   - MSP430 P2.6 -- input  -- BUTTON connected to Experimenter switch S1
 *   - MSP430 P4.0 -- output -- DEBUG1  used optionally for debugging connected to Experimenter P3,4-5
 *   - MSP430 P4.1 -- output -- DEBUG2  used optionally for debugging connected to Experimenter P3,4-6
 *   - MSP430 P9.4 -- output -- EAP TX UCA2TXD UART RF3-5 connected to A2541 module P0_2/RXD (115200,8N1N)
 *   - MSP430 P9.5 -- input  -- EAP RX UCA2RXD UART RF3-7 connected to A2541 module P0_3/TXD (115200,8N1N)
 *
 * In addition to the above, the MSP430's TimerA1 is used in this HAL.
 *   - TimerA1 is configured in Continuous mode, based on the ACLK -- which is based on the MSP430 VLO running at approximately 12 KHz.
 *   - TA1CCR0 is used to generate a "tick" interrupt (see Hal_tickStart() below).
 *   - TA1CCR1 is used as a watchdog timer on the UART connection between the EAP and the MCM.
 *
 * And finally, in this HAL, the MSP430 clocks are set up as:
 *   - MCLK = SMCLK = 1MHz DCO
 *   - ACLK = XT1 = 32768 Hz
 *
 */

#include "Hal.h"
#include "Em_Message.h"

#include <msp430.h>


/* -------- INTERNAL FEATURES -------- */

#define LED_CONFIG()                (P1DIR |= BIT1)
#define LED_ON()                    (P1OUT |= BIT1)
#define LED_OFF()                   (P1OUT &= ~BIT1)
#define LED_READ()                  (P1OUT & BIT1)
#define LED_TOGGLE()                (P1OUT ^= BIT1)

#define CONNECTED_LED_CONFIG()      (P1DIR |= BIT0)
#define CONNECTED_LED_ON()          (P1OUT |= BIT0)
#define CONNECTED_LED_OFF()         (P1OUT &= ~BIT0)

#define BUTTON_CONFIG()             (P2DIR &= ~BIT6, P2REN |= BIT6, P2OUT |= BIT6, P2IES |= BIT6);
#define BUTTON_ENABLE()             (P2IFG &= ~BIT6, P2IE |= BIT6)
#define BUTTON_PRESSED()            (!(P2IN & BIT6))
#define BUTTON_DEBOUNCE_MSECS       100

#define DEBUG1_CONFIG()             (P4DIR |= BIT0)
#define DEBUG1_ON()                 (P4OUT |= BIT0)
#define DEBUG1_OFF()                (P4OUT &= ~BIT0)

#define DEBUG2_CONFIG()             (P4DIR |= BIT1)
#define DEBUG2_ON()                 (P4OUT |= BIT1)
#define DEBUG2_OFF()                (P4OUT &= ~BIT1)

#define EAP_RX_BUF                  UCA2RXBUF
#define EAP_TX_BUF                  UCA2TXBUF

#define EAP_RX_VECTOR               USCI_A2_VECTOR
#define EAP_TX_VECTOR               USCI_A2_VECTOR
#define EAP_TX_ACK_VECTOR           PORT1_VECTOR

#define EAP_RX_ENABLE()             (P9SEL |= BIT5)
#define EAP_RX_DISABLE()            (P9SEL &= ~BIT5)
#define EAP_TX_ENABLE()             (P9SEL |= BIT4)
#define EAP_TX_DISABLE()            (P9SEL &= ~BIT4)

#define EAP_RX_ACK_CONFIG()         (P1DIR |= BIT3)
#define EAP_RX_ACK_SET()            (P1OUT |= BIT3)
#define EAP_RX_ACK_CLR()            (P1OUT &= ~BIT3)

#define EAP_TX_ACK_CONFIG()         (P1DIR &= ~BIT2, P1IES |= BIT2, P1IFG &= ~BIT2, P1IE |= BIT2)
#define EAP_TX_ACK_TST()            (P1IFG & BIT2)
#define EAP_TX_ACK_CLR()            (P1IFG &= ~BIT2)

#define EAP_RX_INT_CLR()            (UCA2IFG &= ~UCRXIFG)
#define EAP_RX_INT_ENABLE()         (UCA2IE |= UCRXIE)
#define EAP_TX_INT_CLR()            (UCA2IFG &= ~UCTXIFG)
#define EAP_TX_INT_DISABLE()        (UCA2IE &= ~UCTXIE)
#define EAP_TX_INT_ENABLE()         (UCA2IE |= UCTXIE)
#define EAP_TX_INT_TST()			(UCA2IFG & UCTXIFG)

#define MCLK_TICKS_PER_MS 			1000L
#define ACLK_TICKS_PER_SECOND       32768L
#define UART_WATCHDOG_PERIOD        (ACLK_TICKS_PER_SECOND * 250) / 1000

#define UART_WATCH_DISABLE()        (TA1CCTL1 = 0)                                              // Turn off CCR1 Interrupt
#define UART_WATCH_ENABLE()         (TA1CCR1 = TA1R + UART_WATCHDOG_PERIOD, TA1CCTL1 = CCIE)    // Set CCR1, and Enable CCR1 Interrupt

#ifdef __TI_COMPILER_VERSION__
#define UNUSED_VECTORS				DMA_VECTOR, RTC_VECTOR, SYSNMI_VECTOR, \
									TIMER0_A0_VECTOR, TIMER0_A1_VECTOR, TIMER0_B0_VECTOR, TIMER0_B1_VECTOR, \
									UNMI_VECTOR, USCI_A0_VECTOR, USCI_A1_VECTOR, USCI_A3_VECTOR, \
									USCI_B0_VECTOR, USCI_B1_VECTOR, USCI_B2_VECTOR, USCI_B3_VECTOR, \
									ADC12_VECTOR, WDT_VECTOR
#endif

#ifdef __GNUC__
#define DINT()                      (__disable_interrupt())
#define EINT()                      (__enable_interrupt())
#define INTERRUPT
#define SLEEP()                     (_BIS_SR(LPM3_bits + GIE))
#define WAKEUP()                    (_BIC_SR_IRQ(LPM3_bits))
#endif

#ifdef __TI_COMPILER_VERSION__
#define DINT()                      (_disable_interrupt())
#define EINT()                      (_enable_interrupt())
#define INTERRUPT interrupt
#define SLEEP()						(__bis_SR_register(LPM3_bits + GIE))
#define WAKEUP()					(__bic_SR_register_on_exit(LPM3_bits))
#endif

#define NUM_HANDLERS 3

#define BUTTON_HANDLER_ID      0
#define TICK_HANDLER_ID        1
#define DISPATCH_HANDLER_ID    2

static void buttonHandler(void);
static void postEvent(uint8_t handlerId);

static Hal_Handler appButtonHandler;
static volatile uint16_t handlerEvents = 0;
static uint16_t clockTick = 0;
static Hal_Handler handlerTab[NUM_HANDLERS];


/* -------- APP-HAL INTERFACE -------- */

void Hal_buttonEnable(Hal_Handler handler) {
    handlerTab[BUTTON_HANDLER_ID] = buttonHandler;
    appButtonHandler = handler;
    BUTTON_CONFIG();
    Hal_delay(100);
    BUTTON_ENABLE();
}

void Hal_connected(void) {
    CONNECTED_LED_ON();
}

void Hal_debugOn(uint8_t line) {
    switch (line) {
    case 1:
        DEBUG1_ON();
        break;
    case 2:
        DEBUG2_ON();
    }
}

void Hal_debugOff(uint8_t line) {
    switch (line) {
    case 1:
        DEBUG1_OFF();
        break;
    case 2:
        DEBUG2_OFF();
    }
}

void Hal_debugPulse(uint8_t line) {
    switch (line) {
    case 1:
        DEBUG1_ON();
        DEBUG1_OFF();
        break;
    case 2:
        DEBUG2_ON();
        DEBUG2_OFF();
    }
}

void Hal_delay(uint16_t msecs) {
    while (msecs--) {
	        __delay_cycles(MCLK_TICKS_PER_MS);
	    }
}

void Hal_disconnected(void) {
    CONNECTED_LED_OFF();
}

void Hal_init(void) {

    /* setup clocks */

    WDTCTL = WDTPW + WDTHOLD;
    P7SEL |= BIT0 | BIT1;                     // Select XT1
    UCSCTL6 &= ~(XT1OFF);                     // XT1 On
    UCSCTL6 |= XCAP_3;                        // Internal load cap
    // Loop until XT1,XT2 & DCO stabilizes
    do
    {
      UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + XT1HFOFFG + DCOFFG);
                                              // Clear XT2,XT1,DCO fault flags
      SFRIFG1 &= ~OFIFG;                      // Clear fault flags
    } while (SFRIFG1&OFIFG);                  // Test oscillator fault flag
    UCSCTL6 &= ~(XT1DRIVE_3);                 // Xtal is now stable, reduce drive
                                              // strength

    /* setup LEDs */

    LED_CONFIG();
    LED_OFF();
    CONNECTED_LED_CONFIG();
    CONNECTED_LED_OFF();

    /* setup debug pins */

    DEBUG1_CONFIG(); DEBUG1_OFF();
    DEBUG2_CONFIG(); DEBUG2_OFF();

    DEBUG1_ON(); DEBUG1_OFF();

    /* setup TimerA1 */
    TA1CTL = TASSEL_1 + MC_2;                 // ACLK, Continuous mode
    UART_WATCH_ENABLE();

    /* setup UART */

    UCA2CTL1 = UCSWRST;

    EAP_RX_ENABLE();
    EAP_TX_ENABLE();

    EAP_RX_ACK_CONFIG();
    EAP_RX_ACK_SET();

    EAP_TX_ACK_CONFIG();

    // suspend the MCM
    EAP_RX_ACK_CLR();

    UCA2CTL1 |= UCSWRST;                      // **Put state machine in reset**
    UCA2CTL1 |= UCSSEL_2;                     // SMCLK
    UCA2BR0 = 9;                              // 1MHz 115200 (see User's Guide)
    UCA2BR1 = 0;                              // 1MHz 115200
    UCA2MCTL |= UCBRS_1 + UCBRF_0;            // Modulation UCBRSx=1, UCBRFx=0
    UCA2CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

    handlerTab[DISPATCH_HANDLER_ID] = Em_Message_dispatch;
}

void Hal_idleLoop(void) {

    EINT();
    for (;;) {

        // atomically read/clear all handlerEvents
        DINT();
        uint16_t events = handlerEvents;
        handlerEvents = 0;

        if (events) {   // dispatch all current events
            EINT();
            uint16_t mask;
            uint8_t id;
            for (id = 0, mask = 0x1; id < NUM_HANDLERS; id++, mask <<= 1) {
                if ((events & mask) && handlerTab[id]) {
                    handlerTab[id]();
                }
            }
        }
        else {          // await more events
            SLEEP();
        }
    }
}

void Hal_ledOn(void) {
    LED_ON();
}

void Hal_ledOff(void) {
    LED_OFF();
}

bool Hal_ledRead(void) {
    return LED_READ();
}

void Hal_ledToggle(void) {
    LED_TOGGLE();
}

void Hal_tickStart(uint16_t msecs, Hal_Handler handler) {
    handlerTab[TICK_HANDLER_ID] = handler;
    clockTick = (ACLK_TICKS_PER_SECOND * msecs) / 1000;
    TA1CCR0 = TA1R + clockTick;                 // Set the CCR0 interrupt for msecs from now.
    TA1CCTL0 = CCIE;                            // Enable the CCR0 interrupt
}


/* -------- SRT-HAL INTERFACE -------- */

uint8_t Em_Hal_lock(void) {
    #ifdef __GNUC__
        uint8_t key = READ_SR & 0x8;
        __disable_interrupt();
        return key;
    #endif
    #ifdef __TI_COMPILER_VERSION__
        uint8_t key = _get_interrupt_state();
        _disable_interrupt();
        return key;
    #endif
}

void Em_Hal_reset(void) {
    uint8_t key = Em_Hal_lock();
    EAP_RX_ACK_CLR();    // suspend the MCM
    Hal_delay(100);
    EAP_RX_ACK_SET();    // reset the MCM
    Hal_delay(500);
    EAP_RX_INT_CLR();
    EAP_TX_INT_CLR();
    EAP_TX_ACK_CLR();
    EAP_RX_INT_ENABLE();
    Em_Hal_unlock(key);
}

void Em_Hal_startSend() {
    EAP_TX_BUF = Em_Message_startTx();
}

void Em_Hal_unlock(uint8_t key) {
    #ifdef __GNUC__
        __asm__("bis %0,r2" : : "ir" ((uint16_t) key));
    #endif
    #ifdef __TI_COMPILER_VERSION__
        _set_interrupt_state(key);
    #endif
}

void Em_Hal_watchOff(void) {
    UART_WATCH_DISABLE();
}

void Em_Hal_watchOn(void) {
    UART_WATCH_ENABLE();
}


/* -------- INTERNAL FUNCTIONS -------- */

static void buttonHandler(void) {
    Hal_delay(100);
    if (BUTTON_PRESSED() && appButtonHandler) {
        appButtonHandler();
    }
}

static void postEvent(uint8_t handlerId) {
    uint8_t key = Em_Hal_lock();
    handlerEvents |= 1 << handlerId;
    Em_Hal_unlock(key);
}

/* -------- INTERRUPT SERVICE ROUTINES -------- */

#ifdef __GNUC__
	__attribute__((interrupt(PORT2_VECTOR)))
#endif
#ifdef __TI_COMPILER_VERSION__
	#pragma vector=PORT2_VECTOR
#endif
INTERRUPT void buttonIsr(void) {
    postEvent(BUTTON_HANDLER_ID);
    BUTTON_ENABLE();
    WAKEUP();
}

#ifdef __TI_COMPILER_VERSION__
    #pragma vector=UNUSED_VECTORS
INTERRUPT void ISR_trap(void)
{
  /* For debugging purposes, you can trap the CPU & code execution here with an
     infinite loop */
  while (1);
}
#endif

#ifdef __GNUC__
	__attribute__((interrupt(EAP_RX_VECTOR)))
#endif
#ifdef __TI_COMPILER_VERSION__
	#pragma vector=EAP_RX_VECTOR
#endif
INTERRUPT void rxIsr(void) {
  uint8_t b;
  switch(UCA2IV)
  {
  case 2:                                   // Vector 2 - RXIFG
      b = EAP_RX_BUF;
      Em_Message_startRx();
      EAP_RX_ACK_CLR();
      EAP_RX_ACK_SET();
      if (Em_Message_addByte(b)) {
          postEvent(DISPATCH_HANDLER_ID);
      }
      WAKEUP();
    break;
  }
}

#ifdef __GNUC__
	__attribute__((interrupt(TIMER1_A0_VECTOR)))
#endif
#ifdef __TI_COMPILER_VERSION__
	#pragma vector=TIMER1_A0_VECTOR
#endif
INTERRUPT void timerIsr(void) {
    TA1CCR0 += clockTick;
    postEvent(TICK_HANDLER_ID);
    WAKEUP();
}

#ifdef __GNUC__
	__attribute__((interrupt(EAP_TX_ACK_VECTOR)))
#endif
#ifdef __TI_COMPILER_VERSION__
	#pragma vector=EAP_TX_ACK_VECTOR
#endif
INTERRUPT void txAckIsr(void) {
    if (EAP_TX_ACK_TST()) {
        uint8_t b;
        if (Em_Message_getByte(&b)) {
            EAP_TX_BUF = b;
        }
        EAP_TX_ACK_CLR();
    }
    WAKEUP();
}

#ifdef __GNUC__
	__attribute__((interrupt(TIMER1_A1_VECTOR)))
#endif
#ifdef __TI_COMPILER_VERSION__
	#pragma vector=TIMER1_A1_VECTOR
#endif
INTERRUPT void uartWatchdogIsr(void) {
	    switch (TA1IV) {
  case  2:                                  // CCR1
    UART_WATCH_DISABLE();
    Em_Message_restart();
    WAKEUP();
           break;
  }
}
