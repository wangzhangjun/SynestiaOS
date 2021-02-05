/*
    Part of the Raspberry-Pi Bare Metal Tutorials
    https://www.valvers.com/rpi/bare-metal/
    Copyright (c) 2013-2018, Brian Sidebotham

    This software is licensed under the MIT License.
    Please see the LICENSE file included with this software.

*/

#include <raspi2/aux.h>
#include <raspi2/rpi-gpio.h>

static aux_t* auxillary = (aux_t*)AUX_BASE;


aux_t* RPI_GetAux( void )
{
    return auxillary;
}

/* Define the system clock frequency in MHz for the baud rate calculation.
   This is clearly defined on the BCM2835 datasheet errata page:
   http://elinux.org/BCM2835_datasheet_errata */


void RPI_AuxMiniUartInit( int baud, int bits )
{
    /* As this is a mini uart the configuration is complete! Now just
       enable the uart. Note from the documentation in section 2.1.1 of
       the ARM peripherals manual:

       If the enable bits are clear you will have no access to a
       peripheral. You can not even read or write the registers */
    auxillary->ENABLES = AUX_ENA_MINIUART;

    auxillary->MU_IER = 0;

    /* Disable flow control,enable transmitter and receiver! */
    auxillary->MU_CNTL = 0;

    /* Decide between seven or eight-bit mode */
    if( bits == 8 )
        auxillary->MU_LCR = AUX_MULCR_8BIT_MODE;
    else
        auxillary->MU_LCR = 0;

    auxillary->MU_MCR = 0;

    /* Disable all interrupts from MU and clear the fifos */
    auxillary->MU_IER = 0;

    auxillary->MU_IIR = 0xC6;

    /* Transposed calculation from Section 2.2.1 of the ARM peripherals manual */
    auxillary->MU_BAUD = ( SYSFREQ / ( 8 * baud ) ) - 1;

    /* Setup GPIO 14 and 15 as alternative function 5 which is UART 1 TXD/RXD. These need to be
       set before enabling the UART */
    RPI_SetGpioPinFunction( RPI_GPIO14, FS_ALT5 );
    RPI_SetGpioPinFunction( RPI_GPIO15, FS_ALT5 );

    /* See the requirements in the GPIO section of the timing requirements of the GPIO controller.
       Who knows why 150 cycles is mentioned - what if we're running at 1500MHz as opposed to
       500MHz ? */
    RPI_GetGpio()->GPPUD = 0;
    for( volatile int i=0; i<150; i++ ) { }
    RPI_GetGpio()->GPPUDCLK0 = ( 1 << 14 );
    for( volatile int i=0; i<150; i++ ) { }
    RPI_GetGpio()->GPPUDCLK0 = 0;

    /* Disable flow control,enable transmitter and receiver! */
    auxillary->MU_CNTL = AUX_MUCNTL_TX_ENABLE;
}


void RPI_AuxMiniUartWrite( char c )
{
    /* Wait until the UART has an empty space in the FIFO */
    while( ( auxillary->MU_LSR & AUX_MULSR_TX_EMPTY ) == 0 ) { }

    /* Write the character to the FIFO for transmission */

    if(c == '\n')
        RPI_AuxMiniUartWrite('\r');
    auxillary->MU_IO = c;
}

void RPI_AuxPuts(char * s, int len)
{
    int i;
    for (i = 0;i < len; i++)
       RPI_AuxMiniUartWrite(s[i]);
}

void aux_uart_print(const char *str) {
    while (*str) {
        RPI_AuxMiniUartWrite(*str);
        str++;
    }
}