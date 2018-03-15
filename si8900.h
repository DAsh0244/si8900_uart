/*
 * si8900.h
 * header file for si8900 UART stack.
 * Author: Danyal Ahsanullah
 * Date: 18 May 2017
 *
 * NOTES:
 *  ONLY use ONE or the other of the values: -- define in build config
 *      MAINS_US_ : enables US characterization values (eg: 120Vrms 60Hz)
 *      MAINS_EU_ : enables US characterization values (eg: 220Vrms 50Hz)
 *
 *  ONLY use ONE or the other of the values: -- define in build config
 *      MSP_ : uses MSP430 libs
 *      PIC_ : uses PIC libs
 *
 *      NOTE: PIC is not implemented yet.
 *
 */

// TODO: add std pc support 
// TODO: Unit Tests?
// TODO: abstract out function implementation to be generic and support multiple architectures

 
#ifndef si8900_H_
#define si8900_H_

/*
 * includes
 */
#include <stdint.h>

#ifdef MSP_
    #include <msp430.h>
    #define UART_TX_BUFF UCA0TXBUF
    #define UART_RX_BUFF UCA0RXBUF
    #define UART_TX_IFG  UCTXIFG
    #define UART_RX_IFG  UCRXIFG
    #define UART_IFG_REG UCA0IFG
#elif PIC_
    #error PIC option NOT implemetned
    #define UART_TX_BUFF "NOT IMPLRMENTED"
    #define UART_RX_BUFF "NOT IMPLRMENTED"
    #define UART_TX_IFG  "NOT IMPLRMENTED"
    #define UART_RX_IFG  "NOT IMPLRMENTED"
    #define UART_IFG_REG "NOT IMPLRMENTED"
#else
    #error "No valid hardware option chosen. Check build options. either \"MSP_\" or \"PIC_\" Must be defined."
#endif



/*
 * CMD BYTE CONFIG
 * packet:      1 1 INCH{2} VREF - MODE PGA
 * bit order:   7 6   54     3   2  1    0
 */
typedef  uint8_t si8900_cfg;


/*
 * Macro constants to aid in constructing command bytes combine with :
 *      BITWISE OR  '|'
 *      ADD         '+'
 *
 * A command byte is constructed with 4 base fields:
 *      PGA  : programmable gain value. Either .5 or 1
 *      MODE : ADC conversion mode. Either 'single shot' or 'stream'
 *      REF  : ADC internal refernce. Either VDD or the external Ref pin
 *      INCH : one of the 3 input channels
 *
 * A general purpose cmd byte can be modeled after:
 *      ( (uint8_t)(<PGA_x> | <MODE_x>  | <REF_x> | <INCH_x>) )
 *
 * NOTE: Only select a SINGLE setting for each of the four base fields: PGA, MODE, REF, INCH
 *       Order does not matter, each option is a differing set of bits in the byte
 *       and by combining them you are just setting bits
 *
 * Examples can be seen in the preconfigured general purpose single shot read command bytes
 *
 * For a bitwise view of the command bytes, see the typedef for si8900_cfg
 */
#define PGA_0       ((uint8_t)(0x00u))      // gain of .5
#define PGA_1       ((uint8_t)(0x01u))      // gain of 1
#define MODE_0      ((uint8_t)(0x00u))      // single read return
#define MODE_1      ((uint8_t)(0x02u))      // stream read
#define REF_0       ((uint8_t)(0x00u))      // Ref = VDD
#define REF_1       ((uint8_t)(0x08u))      // Ref = External ref pin
#define INCH_0      ((uint8_t)(0xC0u))      // input channel 0
#define INCH_1      ((uint8_t)(0xD0u))      // input channel 1
#define INCH_2      ((uint8_t)(0xE0u))      // input channel 2


/*
 *  preconfigured general purpose single shot read command bytes
 */
#define GP_SINGLE_READ_0    ((uint8_t)(INCH_0 | REF_0 | MODE_1 | PGA_0))
#define GP_SINGLE_READ_1    ((uint8_t)(INCH_1 | REF_0 | MODE_1 | PGA_0))
#define GP_SINGLE_READ_2    ((uint8_t)(INCH_2 | REF_0 | MODE_1 | PGA_0))


/*
 * macro constants to aid in confirmation and baud_handshakes
 */
#define CAL_BYTE    ((uint8_t)(0xAAu))  // Calibaration byte
#define CONFIRM	    ((uint8_t)(0x55u))  // recieved correctly byte
#define FAILED 	    ((uint8_t)(0xFFu))  // Failure code
#define HAND_SHAKED ((uint8_t)(0x88u))  // Confirmed status for use as indicator to forgo autobaud process


/*
 * macro constants to aid in conversion from ADC value to voltage value
 */
#define SI8900_VCC      3.3
#define SI8900_VREF     2.5
#define SI8900_RES      1024


/*
 * macro constants to use for numeric conversion of readings to values
 */
#ifdef  MAINS_US_   /* use US stds for mains */
#define MAINS_RMS   120
#define MAINS_PEAK  170
#define MAINS_FRQ   60.0
#elif MAINS_EU_     /* use EU stds for mains */
#define MAINS_RMS   220
#define MAINS_PEAK  311
#define MAINS_FRQ   50.0
#else
#error "Option for mains values NOT selected. Must define either \"MAINS_US_\" or \"MAINS_EU_\"."
#endif


/*
 * Conversion macro to pre-calculate conversion rate
 */
#define MAINS_CONV_RATE (SI8900_VCC / SI8900_RES * MAINS_PEAK / SI8900_VREF)


/*
 * yields {1 0 INCH{2} D0-D9{10} 0}
 */
#define PACKET_JOIN(b1,b2) ((uint16_t)(((uint16_t)b1) << 7 | b2))


/*
 * get inch as a uint8_t with lower 2 bits as the inchannel with a value
 * between 0 and 2 inclusive, upper bits are zero padding
 */
#define GET_INCH(packet)    ((uint8_t)((packet & 0x30) >> 4))


/*
 * get reading as a uint16_t with the lower 10 bits being the reading
 * between 0 and 1024 inclusive, upper bits are zero padding
 */
#define GET_READING(packet)	((uint16_t)((packet & 0x0FFE) >> 1))


/*
 * RECIEVE PACKETS - 1 Cmd byte echo, 2 data bytes
 *
 * CMD Echo
 * packet:      1 1 INCH{2} VREF - MODE PGA
 * bit order:   7 6   54     3   2  1    0
 *
 * Data Byte 1
 * packet:      1 0 INCH{2} D9-D6{4}
 * bit order:   7 6   54     3210
 *
 * Data Byte 2
 * packet:      0 D5-D0{6} 0
 * bit order:   7  654321  0
 */
typedef struct si8900_reading{
    si8900_cfg cmd_byte;
    uint8_t inch;
    uint16_t reading;
}si8900_reading;


/*
 * START: Function prototypes / declarations
 */

/*
 * TX/RX commands
 */
uint8_t si8900_auto_baud(void);
void si8900_send_cmd(si8900_cfg);

/*
 * Byte processing
 */
si8900_reading si8900_get_reading(uint8_t*, uint8_t);
si8900_reading si8900_get_reading_oversampled(uint8_t*, uint8_t, uint8_t);

/*
 * internal functions
 */
uint16_t bit_reverse(uint16_t);

/*
 * END: Function prototypes / declarations
 */

#endif /* si8900_H_ */
