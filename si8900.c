/*
 * si8900.c
 * implementation file for si8900 UART stack.
 * Author: Danyal Ahsanullah
 * Date: 18 May 2017
 */
#include "si8900.h" // includes <stdint.h>

//#include <limits.h>   // needed for CHAR_BIT

/*
 *  name: bit_reverse
 *
 *  desc: takes a 16-bit value and returns a bitwise reversed 16-bit value
 *        useful to convert form MSB to LSB or for packet slicing
 *
 *  args:
 *      uint16_t num: input 16-bit value to reverse the bits of.
 *
 *  return value:
 *      uint16_t reverse: the bit reversed value
 *
 *  example:
 *      uint16_t reversed_bits = bit_reverse(0x0001);
 *      printf("u", reversed_bits == 0x1000) // will eval to 1
 */
uint16_t bit_reverse(uint16_t num)
{
    uint16_t reverse = num; // reverse will be reversed bits of num. First get LSB
    uint8_t size = sizeof(num) * sizeof(uint8_t) - 1; // extra shift needed at end -- CHAR_BIT
    for (num >>= 1; num; num >>= 1)
    {
    reverse <<= 1;
    reverse |= num & 1;
    size--;
    }
    reverse <<= size; // shift when num's highest bits are zero
    return reverse;
}

/*
 *  name: si8900_auto_baud
 *
 *  desc: handles the audo-baud handshake procedure for
 *        communicating to the si8900 via UART
 *      NOTE!!: there is no timeout implemented yet,
 *              so this may indefinitely hang
 *
 *  args:
 *      void
 *
 *  return value:
 *      uint8_t with value 0 on success
 *
 *  example:
 *      if(!si8900_auto_baud())
 *      {
 *          // continue with code execution
 *      }
 *      else
 *      {
 *          // throw error and/or reset
 *      }
 */
uint8_t si8900_auto_baud(void)
{
    char hold_value = 0;
    UART_TX_BUFF = CAL_BYTE; // Send the first timing sample to UART
    // char hold_value = 0; moved to a global for ISR.
    uint8_t Code_receive = 0; // Correct code receive = 0
    uint8_t Code_confirm = 0; // Confirm code receiving = 0
    while ((Code_receive & Code_confirm) == 0)
    { // Establish correct communication and confirm
        while (!(UART_IFG_REG & UART_RX_IFG))  // Response received?
        UART_TX_BUFF = CAL_BYTE;
        hold_value =  UART_RX_BUFF; // read value (clears interrupt flag)
        if ( hold_value == CONFIRM ) // Check two continuous "0x55" receiving
        {
//          UART_TX_BUFF = CAL_BYTE; // resend timing sample byte
            if (Code_receive == 1)
            {
                __no_operation();
                Code_confirm = 1; // Confirm communication established
            }
        Code_receive = 1; // Initial communication established
        }
        else
        {
            UART_TX_BUFF = CAL_BYTE; // resend timing sample byte
            Code_receive = 0; // If receive one in-correctly, clear both
            Code_confirm = 0; // Code_receive & Code_confirm
        }
        UART_TX_BUFF = CAL_BYTE; // resend timing sample byte
    }
    return 0;
}



/*
 *  name: si8900_get_reading
 *
 *  desc: converts the 3 bytes of incoming packet data into a
 *        si8900_reading struct containting the inch and reading value
 *
 *  args:
 *      uint8_t* buffer  : buffer to read the first 3 bytes from
 *      uint8_t ref_byte : references byte to validate buffer
 *                         contains a full response form si8900
 *
 *  return value:
 *      si8900_reading struct containing readign and inch information
 *      returns a structure containing FAILED as both inch and reading
 *      on a failed read
 *
 *  example:
 *      uint8_t buffer [BUFSIZE] = {0};
 *      si8900_reading reading = si8900_get_reading(buffer,GP_SINGLE_READ_0);
 *      if(reading.inch != FAILED)
 *      {
 *          // continue with code execution
 *      }
 *      else
 *      {
 *          // throw error and/or reset
 *      }
 */
si8900_reading si8900_get_reading(uint8_t* buffer, uint8_t ref_byte)
{
    si8900_reading new_read;

    new_read.cmd_byte = *(buffer);
    if(new_read.cmd_byte == ref_byte)
    {
        new_read.inch = GET_INCH(*(buffer + 1));
        uint16_t packet = PACKET_JOIN(*(buffer + 1), *(buffer + 2));
        new_read.reading = GET_READING(packet);
    }
    else
    {
        new_read.inch = FAILED;
        new_read.reading = FAILED;
    }

    return new_read;
}


/*
 *  name: si8900_get_reading_oversampled
 *
 *  desc: performs multiple acquisitions and averages the
 *        reading values to give a mean sample value
 *
 *  args:
 *      uint8_t* buffer     : buffer to read the first 3 bytes from
 *      uint8_t ref_byte    : refernces byte to validate buffer contains a full response form si8900
 *      uint8_t sample_count: how many samples to take and average
 *
 *  return value:
 *      si8900_reading struct containing readign and inch information
 *      returns a structure with FAILED as both inch and reading
 *      on a failed read
 *
 *  example:
 *      uint8_t buffer [BUFSIZE] = {0};
 *      si8900_reading reading = si8900_get_reading_oversampled(buffer, GP_SINGLE_READ_0, 3);
 *                                                                                      // take average of 3 samples
 *      if(reading.inch != FAILED)
 *      {
 *          // continue with code execution
 *      }
 *      else
 *      {
 *          // throw error and/or reset
 *      }
 */
si8900_reading si8900_get_reading_oversampled(uint8_t* buffer, uint8_t ref_byte, uint8_t sample_count)
{
    uint8_t i, entries = 0;
    uint16_t vals = 0;
    si8900_reading temp;
    for (i = 0; i < sample_count; i++)
    {
        temp = si8900_get_reading(buffer, ref_byte);
        if (temp.reading != FAILED)
        {
            vals += temp.reading;
            entries++;
        }
    }
    temp.reading = vals/entries;
    return temp;
}


/*
 *  name: si8900_send_cmd
 *
 *  desc: sends a command byte to the si8900 over uart
 *
 *  args:
 *      si8900_cfg cmd_byte : configuration byte to send to the si8900
 *
 *  return value:
 *      void
 *
 *  example:
 *      si8900_send_cmd(GP_SINGLE_READ_0);
 *
 */
//todo make sure this function actually works --
/// thinking the pulling off the buffer is bad --
/// alternatively remove the check in the get_reading functions
void si8900_send_cmd(si8900_cfg cmd_byte)
{
    uint8_t hold_value;
    do {
        while (!(UART_IFG_REG & UART_TX_IFG)); // wait for TX buffer ready
        UART_TX_BUFF = cmd_byte;               // send command
        while (!(UART_IFG_REG & UART_RX_IFG)); // wait for response
        hold_value = UART_RX_BUFF;
    } while(hold_value != cmd_byte);
}
