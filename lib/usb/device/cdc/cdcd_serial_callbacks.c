/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2015, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 *      Headers
 *---------------------------------------------------------------------------*/

#include "usb/device/cdc/cdcd_serial.h"

#include <stdint.h>

/*---------------------------------------------------------------------------
 *         Default callback functions
 *---------------------------------------------------------------------------*/

/**
 * Invoked when the CDC LineCoding is requested to changed
 * \param port          Port number.
 * \param p_line_coding   Pointer to new LineCoding settings.
 * \return USBRC_SUCCESS if ready to receive the line coding.
 */
extern WEAK uint8_t cdcd_serial_line_coding_is_to_change(
	CDCLineCoding *line_coding)
{
	/* Accept any of linecoding settings */
	line_coding = line_coding;
	return USBRC_SUCCESS;
}

/**
 * Invoked when the CDC ControlLineState is changed
 * \param port  Port number.
 * \param dtr   New dtr value.
 * \param rts   New rts value.
 */
extern WEAK void cdcd_serial_control_line_state_changed(
		uint8_t dtr, uint8_t rts)
{
	/* Do nothing */
	dtr = dtr;
	rts = rts;
}

