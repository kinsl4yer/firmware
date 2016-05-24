/**************************************************************************
 * Copyright 2015 by Elco Jacobs, BrewPi
 * Copyright 2014 by Petr Gargulak. eGUI Community.
 * Copyright 2009-2013 by Petr Gargulak. Freescale Semiconductor, Inc.
 *
 ***************************************************************************
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License Version 3
 * or later (the "LGPL").
 *
 * As a special exception, the copyright holders of the eGUI project give you
 * permission to link the eGUI sources with independent modules to produce an
 * executable, regardless of the license terms of these independent modules,
 * and to copy and distribute the resulting executable under terms of your
 * choice, provided that you also meet, for each linked independent module,
 * the terms and conditions of the license of that module.
 * An independent module is a module which is not derived from or based
 * on this library.
 * If you modify the eGUI sources, you may extend this exception
 * to your version of the eGUI sources, but you are not obligated
 * to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License
 * and the GNU Lesser General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************//*!
*
* @file      d4dlcdhw_spi_spark_8b.c
*
* @author    Elco Jacobs
*
* @version   0.0.1.0
*
* @date      Feb-2015
*
* @brief     D4D driver - spi_8b hardware lcd driver for spark platform - source c file
*
******************************************************************************/

extern "C" {
#include "d4d.h"            // include of all public items (types, function etc) of D4D driver
#include "common_files/d4d_lldapi.h"     // include non public low level driver interface header file (types, function prototypes, enums etc. )
#include "common_files/d4d_private.h"    // include the private header file that contains perprocessor macros as D4D_MK_STR
}
#include "application.h"

// identification string of driver - must be same as name D4DTCH_FUNCTIONS structure + "_ID"
// it is used for enable the code for compilation
#define d4dlcdhw_spi_spark_8b_ID 1


// compilation enable preprocessor condition
// the string d4dtch_spi_spark_8b_ID must be replaced by define created one line up
#if (D4D_MK_STR(D4D_LLD_LCD_HW) == d4dlcdhw_spi_spark_8b_ID)

// include of low level driver header file
// it will be included into whole project only in case that this driver is selected in main D4D configuration file
#include "low_level_drivers/LCD/lcd_hw_interface/spi_spark_8bit/d4dlcdhw_spi_spark_8b.h"

/******************************************************************************
 * Macros
 ******************************************************************************/
#if D4D_COLOR_SYSTEM != D4D_COLOR_SYSTEM_RGB565
#error The eGUI low level driver "d4dlcdhw_spi_spark_8b" not supported selected type of D4D_COLOR_SYSTEM. To run this driver just select D4D_COLOR_SYSTEM_RGB565.
#endif
/******************************************************************************
 * Internal function prototypes
 ******************************************************************************/

static unsigned char D4DLCDHW_Init_Spi_Spark_8b(void);
static unsigned char D4DLCDHW_DeInit_Spi_Spark_8b(void);
static void D4DLCDHW_SendDataWord_Spi_Spark_8b(unsigned short value);
static void D4DLCDHW_SendCmdWord_Spi_Spark_8b(unsigned short cmd);
static unsigned short D4DLCDHW_ReadDataWord_Spi_Spark_8b(void);
static unsigned short D4DLCDHW_ReadCmdWord_Spi_Spark_8b(void);
static unsigned char D4DLCDHW_PinCtl_Spi_Spark_8b(D4DLCDHW_PINS pinId, D4DHW_PIN_STATE setState);
static void D4DLCD_FlushBuffer_Spi_Spark_8b(D4DLCD_FLUSH_MODE mode);
static void D4DLCDHW_Delay_Spi_Spark_8b(unsigned short period);


/**************************************************************//*!
  *
  * Global variables
  *
  ******************************************************************/

// the main structure that contains low level driver api functions
// the name of this structure is used for recognizing of configured low level driver of whole D4D
// so this name has to be used in main configuration header file of D4D driver to enable this driver
extern "C" const D4DLCDHW_FUNCTIONS d4dlcdhw_spi_spark_8b ={
    D4DLCDHW_Init_Spi_Spark_8b,
    D4DLCDHW_SendDataWord_Spi_Spark_8b,
    D4DLCDHW_SendCmdWord_Spi_Spark_8b,
    D4DLCDHW_ReadDataWord_Spi_Spark_8b,
    D4DLCDHW_ReadCmdWord_Spi_Spark_8b,
    D4DLCDHW_PinCtl_Spi_Spark_8b,
    D4DLCD_FlushBuffer_Spi_Spark_8b,
    D4DLCDHW_DeInit_Spi_Spark_8b,
};
/**************************************************************//*!
  *
  * Local variables
  *
  ******************************************************************/

#define SCREEN_DATA_BUFFER_SIZE 320

static uint8_t tx_buffer[2][SCREEN_DATA_BUFFER_SIZE];
/**
 * The index of the currently active buffer. This buffer is written to by calls to
 */
static int8_t active_buffer_idx = 0;

/**
 * The byte offset in the active buffer where the next byte can be written to.
 */
static uint16_t active_buffer_offset = 0;

/**
 * The buffer that is currently being read by DMA. When -1, no buffer is being read.
 */
static volatile int8_t dma_buffer_idx = -1;

/**
 * Determines if there is data to send.
 */
inline uint16_t hasPendingDataToSend()
{
	return active_buffer_offset;
}

/**
 * Waits for the asynchronous transfer to complete.
 */
inline void waitForTransferToComplete()
{
    HAL_SPI_TransferStatus st;
    do {
      HAL_SPI_DMA_Transfer_Status(HAL_SPI_INTERFACE1, &st);
    } while(st.transfer_ongoing);
}

/**
 * Notification that the DMA transfer was complete.
 */
void transferComplete()
{
	D4DLCD_DEASSERT_CS;
	dma_buffer_idx = -1;
}

inline void scheduleTransfer(int8_t tx_buffer_idx, uint16_t length)
{
	waitForTransferToComplete();
	D4DLCD_ASSERT_CS;
#if 1 // DMA
	dma_buffer_idx = tx_buffer_idx;
	SPI.transfer(tx_buffer[tx_buffer_idx], NULL, length, transferComplete);
#else
	while (length-->0) {
		SPI.transfer(*data++);
	}
	transferComplete();
#endif
}

/**
 * Ensures any pending data to send to the device is flushed asynchronously.
 * To wait for the data to be flushed, call waitForTransferComplete().
 */
inline void flushData()
{
	if (hasPendingDataToSend())
	{
		scheduleTransfer(active_buffer_idx, active_buffer_offset);
		int8_t new_active_idx = (active_buffer_idx + 1) & 0x1;

		if (new_active_idx==dma_buffer_idx){
		    // is this even possible with a 2 line buffer?
		    // scheduleTransfer waits for the previous transfer to complete.
			waitForTransferToComplete();
		}
		active_buffer_offset = 0;
		active_buffer_idx = new_active_idx;
	}
}



/**************************************************************//*!
  *
  * Functions bodies
  *
  ******************************************************************/


//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_Init_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function is used for initialization of this low level driver
//
// PARAMETERS:  none
//
// RETURNS:     result: 1 - Success
//                      0 - Failed
//-----------------------------------------------------------------------------

static unsigned char D4DLCDHW_Init_Spi_Spark_8b(void) {
#ifdef D4DLCD_DISPLAY_MCU_USER_INIT
    D4DLCD_DISPLAY_MCU_USER_INIT
#endif

    D4DLCD_DEASSERT_CS;
    D4DLCD_ASSERT_DC;

    D4DLCD_INIT_CS;
    D4DLCD_INIT_DC;

    // Serial clock cycle is min 150ns from ILI93841 datasheet, which equals 6.7 MHz
    // But touch screen driver (XPT2046) needs 200ns low, 200ns high.
    // 1 /( 72 MHz / 29) = 403 ns. Prescaler of 32 gives a bit of margin.
    SPI.setClockDivider(
#if PLATFORM_ID==0
    SPI_CLOCK_DIV32
#elif PLATFORM_ID==6
    SPI_CLOCK_DIV64
#else
#error Unknown platform
#endif    
    );

    
    SPI.begin(D4DLCD_CS);

    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    
    D4DLCD_DEASSERT_RESET;
    D4DLCDHW_Delay_Spi_Spark_8b(5);
    D4DLCD_ASSERT_RESET;
    D4DLCDHW_Delay_Spi_Spark_8b(20);
    D4DLCD_DEASSERT_RESET;
    D4DLCDHW_Delay_Spi_Spark_8b(150);

    return 1;
}


//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_DeInit_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function is used for deinitialization of this low level driver
//
// PARAMETERS:  none
//
// RETURNS:     result: 1 - Success
//                      0 - Failed
//-----------------------------------------------------------------------------

static unsigned char D4DLCDHW_DeInit_Spi_Spark_8b(void) {
    return 0;
}

//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_SendDataWord_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function send the one 16 bit variable into LCD
//
// PARAMETERS:  unsigned short value    variable to send
//
// RETURNS:     none
//-----------------------------------------------------------------------------

static void D4DLCDHW_SendDataWord_Spi_Spark_8b(unsigned short value) {
#if 1
	tx_buffer[active_buffer_idx][active_buffer_offset++] = value;

	if (active_buffer_offset>=SCREEN_DATA_BUFFER_SIZE)
	{
		flushData();
	}


#else

	D4DLCD_ASSERT_CS;
    // Send data byte
    SPI.transfer(value);

    D4DLCD_DEASSERT_CS;
#endif
}

//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_SendCmdWord_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function send the one 16 bit command into LCD
//
// PARAMETERS:  unsigned short cmd    command to send
//
// RETURNS:     none
//-----------------------------------------------------------------------------

static void D4DLCDHW_SendCmdWord_Spi_Spark_8b(unsigned short cmd) {
	flushData();
    waitForTransferToComplete();

	D4DLCD_ASSERT_DC; // DataCmd := 0
	D4DLCD_ASSERT_CS;
    // Send data byte
    SPI.transfer(cmd);

    D4DLCD_DEASSERT_CS;
    D4DLCD_DEASSERT_DC; // DataCmd := 1
}


//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_ReadDataWord_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function reads the one 16 bit variable from LCD (if this function is supported)
//
// PARAMETERS:  none
//
// RETURNS:     unsigned short - the readed value
//
//-----------------------------------------------------------------------------

static unsigned short D4DLCDHW_ReadDataWord_Spi_Spark_8b(void) {
    /*D4DLCD_DEASSERT_DC;
    D4DLCD_ASSERT_CS;
    digitalWrite(_cs, LOW);
    unsigned short r = SPI.transfer(0x00);
    D4DLCD_DEASSERT_CS;
    return r;*/
    return 0;
}

//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_ReadCmdWord_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: The function reads the one 16 bit command from LCD (if this function is supported)
//
// PARAMETERS:  none
//
// RETURNS:     unsigned short - the readed value
//
//-----------------------------------------------------------------------------

static unsigned short D4DLCDHW_ReadCmdWord_Spi_Spark_8b(void) {
    return 0;
}

//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_PinCtl_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: allows control GPIO pins for LCD control purposes
//
// PARAMETERS:  D4DLCDHW_PINS pinId - Pin identification
//              D4DHW_PIN_STATE setState - Pin action
// RETURNS:     for Get action returns the pin value
//-----------------------------------------------------------------------------

static unsigned char D4DLCDHW_PinCtl_Spi_Spark_8b(D4DLCDHW_PINS pinId, D4DHW_PIN_STATE setState) {
    switch (pinId) {
        case D4DLCD_RESET_PIN:
            switch (setState) {
#if defined(D4DLCD_RESET)
                case D4DHW_PIN_OUT:
                    OUTPUT(D4DLCD_RESET);
                    break;
                case D4DHW_PIN_IN:
                    INPUT(D4DLCD_RESET);
                    break;
                case D4DHW_PIN_SET_1:
                    D4DLCD_DEASSERT_RESET
                    break;
                case D4DHW_PIN_SET_0:
                    D4DLCD_ASSERT_RESET;
                    break;
#endif
            }
            break;
        case D4DLCD_BACKLIGHT_PIN:
            switch (setState) {

#ifdef D4DLCD_BACKLIGHT

                case D4DHW_PIN_OUT:
                    OUTPUT(D4DLCD_BACKLIGHT);
                    break;
                case D4DHW_PIN_IN:
                    INPUT(D4DLCD_BACKLIGHT);
                    break;
                case D4DHW_PIN_SET_1:
                    D4DLCD_DEASSERT_BACKLIGHT
                    break;
                case D4DHW_PIN_SET_0:
                    D4DLCD_ASSERT_BACKLIGHT;
                    break;

#endif

            }
            break;
    }
    return 1;
}

//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCD_FlushBuffer_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: For buffered low level interfaces is used to inform
//              driver the complete object is drawed and pending pixels should be flushed
//
// PARAMETERS:  none
//
// RETURNS:     none
//-----------------------------------------------------------------------------

static void D4DLCD_FlushBuffer_Spi_Spark_8b(D4DLCD_FLUSH_MODE mode) {
    if (true || mode==D4DLCD_FLSH_SCR_END || mode==D4DLCD_FLSH_FORCE) {
    		flushData();
    }
}


//-----------------------------------------------------------------------------
// FUNCTION:    D4DLCDHW_Delay_Spi_Spark_8b
// SCOPE:       Low Level Driver API function
// DESCRIPTION: For do some small delays in ms
//
// PARAMETERS:  period - count of ms
//
// RETURNS:     none
//-----------------------------------------------------------------------------
/**************************************************************************/ /*!
  * @brief   For do some small delays in ms
  * @param   period - 1ms periods time
  * @return  none
  * @note    This function is just used to do some delays of eGUI (just for initialization purposes, not for run)
  *******************************************************************************/


static void D4DLCDHW_Delay_Spi_Spark_8b(unsigned short period){
    delay(period);
}



#endif //(D4D_MK_STR(D4D_LLD_LCD_HW) == d4dlcdhw_spi_8b_ID)
