/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2018, Atmel Corporation
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

/**
 * \file
 *
 * Implementation of memories configuration on board.
 *
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "board.h"
#include "board_timer.h"
#include "timer.h"
#include "trace.h"

#include "irq/irq.h"
#include "gpio/pio.h"
#include "peripherals/pmc.h"
#include "extram/smc.h"
#include "peripherals/wdt.h"

#include "extram/ddram.h"

#include "arm/mmu_cp15.h"
#include "mm/l1cache.h"

#include "board_support.h"

/*----------------------------------------------------------------------------
 *        Local constants
 *----------------------------------------------------------------------------*/

static const char* board_name = BOARD_NAME;

/*----------------------------------------------------------------------------
 *        Local variables
 *----------------------------------------------------------------------------*/

SECTION(".region_ddr") ALIGNED(16384) static uint32_t tlb[4096];

/*----------------------------------------------------------------------------
 *        Local functions
 *----------------------------------------------------------------------------*/

#define SMC_CS_NUMBER SMC_CS
static void smc_nand_configure_sam9xx6(uint8_t bus_width)
{
	SMC->SMC_CS_NUMBER[NAND_EBI_CS].SMC_SETUP =
		SMC_SETUP_NWE_SETUP(0) |
		SMC_SETUP_NCS_WR_SETUP(0) |
		SMC_SETUP_NRD_SETUP(0) |
		SMC_SETUP_NCS_RD_SETUP(0);

	SMC->SMC_CS_NUMBER[NAND_EBI_CS].SMC_PULSE =
		SMC_PULSE_NWE_PULSE(1) |
		SMC_PULSE_NCS_WR_PULSE(2) |
		SMC_PULSE_NRD_PULSE(1) |
		SMC_PULSE_NCS_RD_PULSE(2);

	SMC->SMC_CS_NUMBER[NAND_EBI_CS].SMC_CYCLE =
		SMC_CYCLE_NWE_CYCLE(2) |
		SMC_CYCLE_NRD_CYCLE(2);

#ifdef SMC_TIMINGS_NFSEL
	SMC->SMC_CS_NUMBER[NAND_EBI_CS].SMC_TIMINGS =
		SMC_TIMINGS_TCLR(3) |
		SMC_TIMINGS_TADL(27) |
		SMC_TIMINGS_TAR(3) |
		SMC_TIMINGS_TRR(6) |dff
		SMC_TIMINGS_TWB(5) |
		SMC_TIMINGS_NFSEL;
#endif

	SMC->SMC_CS_NUMBER[NAND_EBI_CS].SMC_MODE =
		SMC_MODE_READ_MODE |
		SMC_MODE_WRITE_MODE |
                SMC_MODE_EXNW_MODE_FROZEN |
		((bus_width == 8 ) ? SMC_MODE_DBW_BIT_8 : SMC_MODE_DBW_BIT_16) |
		SMC_MODE_TDF_CYCLES(0);
}

/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

const char* get_board_name(void)
{
	return board_name;
}

void board_cfg_clocks(void)
{
	struct _pmc_plla_cfg plla_config = {
		.mul = 199,
		.div = 3,
		.count = 0x3f,
	};

#if defined(BOARD_PMC_PLLA_MUL) && defined(BOARD_PMC_PLLA_DIV)
	plla_config.mul = BOARD_PMC_PLLA_MUL;
	plla_config.div = BOARD_PMC_PLLA_DIV;
#else
	switch (pmc_get_main_oscillator_freq()) {
	case 24000000:
		plla_config.mul = 40;
		plla_config.div = 1;
		break;
	case 16000000:
		plla_config.mul = 61;
		plla_config.div = 1;
		break;
	case 12000000:
		plla_config.mul = 82;
		plla_config.div = 1;
		break;
	}
#endif
	pmc_switch_mck_to_main();
	pmc_set_mck_prescaler(PMC_MCKR_PRES_CLOCK);
//	pmc_set_mck_divider(PMC_MCKR_MDIV_EQ_PCK);
	//pmc_disable_plla();
	pmc_select_external_osc(false);
	pmc_configure_plla(&plla_config);
	pmc_set_mck_divider(PMC_MCKR_MDIV_PCK_DIV3);
//	pmc_set_mck_plladiv2(true);
	pmc_set_mck_prescaler(PMC_MCKR_PRES_CLOCK);
	pmc_switch_mck_to_pll();
}

void board_cfg_lowlevel(bool clocks, bool ddram, bool mmu)
{
	/* Disable Watchdog */
	wdt_disable();

	/* Disable all PIO interrupts */
	pio_reset_all_it();

	/* Set the external oscillator frequency */
	pmc_set_main_oscillator_freq(BOARD_MAIN_CLOCK_EXT_OSC);

	if (clocks) {
		/* Configure system clocks */
		board_cfg_clocks();
	}

	/* Setup default interrupt handlers */
	irq_initialize();

	/* Configure system timer */
	board_cfg_timer();

	if (ddram) {
		/* Configure DDRAM */
		board_cfg_ddram();
	}

	if (mmu) {
		/* Setup MMU */
//		board_cfg_mmu();
	}
}

void board_cfg_mmu(void)
{
	uint32_t addr;

	if (mmu_is_enabled())
		return;

	/* TODO: some peripherals are configured TTB_SECT_STRONGLY_ORDERED
	   instead of TTB_SECT_SHAREABLE_DEVICE because their drivers have to
	   be verified for correct operation when write-back is enabled */

	/* Reset table entries */
	for (addr = 0; addr < 4096; addr++)
		tlb[addr] = 0;

	/* 0x00000000: SRAM (Remapped) */
	tlb[0x000] = TTB_SECT_ADDR(0x00000000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_CACHEABLE_WB
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0x00100000: ROM */
	tlb[0x001] = TTB_SECT_ADDR(0x00100000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_CACHEABLE_WB
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0x00300000: SRAM0 */
	tlb[0x003] = TTB_SECT_ADDR(0x00300000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_CACHEABLE_WB
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0x00400000: SRAM1 */
	tlb[0x004] = TTB_SECT_ADDR(0x00400000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_SHAREABLE_DEVICE
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

#ifdef CONFIG_HAVE_UDPHS
	/* 0x00500000: UDPHS RAM */
	tlb[0x005] = TTB_SECT_ADDR(0x00500000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_SHAREABLE_DEVICE
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0x00600000: UHP (OHCI) */
	tlb[0x006] = TTB_SECT_ADDR(0x00600000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_SHAREABLE_DEVICE
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0x00700000: UHP (EHCI) */
	tlb[0x007] = TTB_SECT_ADDR(0x00700000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_SHAREABLE_DEVICE
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;
#endif /* CONFIG_HAVE_UDPHS */

	/* 0x10000000: EBI Chip Select 0 */
	for (addr = 0x100; addr < 0x200; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x20000000: EBI Chip Select 1 / DDR CS */
	/* (64MB cacheable, 192MB strongly ordered) */
	for (addr = 0x200; addr < 0x240; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_CACHEABLE_WB
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;
	for (addr = 0x240; addr < 0x300; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x30000000: EBI Chip Select 2 */
	for (addr = 0x300; addr < 0x400; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x40000000: EBI Chip Select 3 */
	for (addr = 0x400; addr < 0x500; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x50000000: EBI Chip Select 4 */
	for (addr = 0x500; addr < 0x600; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x60000000: EBI Chip Select 5 */
	for (addr = 0x600; addr < 0x700; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_SECT_SBO
	                  | TTB_TYPE_SECT;

	/* 0x70000000: QSPI0/1 AESB MEM */
	for (addr = 0x700; addr < 0x800; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
#if defined(VARIANT_QSPI0)
	                  | TTB_SECT_CACHEABLE_WB
#else
	                  | TTB_SECT_STRONGLY_ORDERED
#endif
	                  | TTB_TYPE_SECT;

	/* 0x80000000: SDMMC0 */
	for (addr = 0x800; addr < 0x900; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_TYPE_SECT;

	/* 0x90000000: SDMMC1 */
	for (addr = 0x900; addr < 0xa00; addr++)
		tlb[addr] = TTB_SECT_ADDR(addr << 20)
	                  | TTB_SECT_AP_FULL_ACCESS
	                  | TTB_SECT_DOMAIN(0xf)
	                  | TTB_SECT_STRONGLY_ORDERED
	                  | TTB_TYPE_SECT;

	/* 0xeff00000: OTPC */
	tlb[0xeff] = TTB_SECT_ADDR(0xeff00000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_STRONGLY_ORDERED
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0xf0000000: Peripherals */
	tlb[0xf00] = TTB_SECT_ADDR(0xf0000000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_STRONGLY_ORDERED
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0xf8000000: Peripherals */
	tlb[0xf80] = TTB_SECT_ADDR(0xf8000000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_STRONGLY_ORDERED
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* 0xfff0000: System Controller */
	tlb[0xfff] = TTB_SECT_ADDR(0xfff00000)
	           | TTB_SECT_AP_FULL_ACCESS
	           | TTB_SECT_DOMAIN(0xf)
	           | TTB_SECT_STRONGLY_ORDERED
	           | TTB_SECT_SBO
	           | TTB_TYPE_SECT;

	/* Enable MMU, I-Cache and D-Cache */
	mmu_configure(tlb);
	icache_enable();
	mmu_enable();
	dcache_enable();
}


#define VDDIOM_1V8_OUT_Z_CALN_TYP 4
#define VDDIOM_1V8_OUT_Z_CALP_TYP 10

void board_cfg_matrix_for_ddr(void)
{
	uint32_t cfg;

	cfg = SFR->SFR_CCFG_EBICSA;
	SFR->SFR_CCFG_EBICSA = cfg
	                     | SFR_CCFG_EBICSA_DDR_MP_EN
	                     | SFR_CCFG_EBICSA_EBI_CS1A
	                     | SFR_CCFG_EBICSA_NFD0_ON_D16;
	cfg = SFR->SFR_CAL1;
	cfg &= ~(SFR_CAL1_CALN_M_Msk | SFR_CAL1_CALP_M_Msk);
	cfg |= SFR_CAL1_TEST_M | SFR_CAL1_CALN_M(VDDIOM_1V8_OUT_Z_CALN_TYP) | SFR_CAL1_CALP_M(VDDIOM_1V8_OUT_Z_CALP_TYP);
	SFR->SFR_CAL1 = cfg;
}

void board_cfg_matrix_for_nand(void)
{
}

void board_cfg_ddram(void)
{
#ifdef BOARD_DDRAM_TYPE
	board_cfg_matrix_for_ddr();
	struct _mpddrc_desc desc;
	ddram_init_descriptor(&desc, BOARD_DDRAM_TYPE);
	ddram_configure(&desc);
#endif
}

#ifdef CONFIG_HAVE_NAND_FLASH
void board_cfg_nand_flash(void)
{
#if defined(BOARD_NANDFLASH_PINS) && defined(BOARD_NANDFLASH_BUS_WIDTH)
	const struct _pin pins_nandflash[] = BOARD_NANDFLASH_PINS;
	pio_configure(pins_nandflash, ARRAY_SIZE(pins_nandflash));
	board_cfg_matrix_for_nand();
	smc_nand_configure_sam9xx6(BOARD_NANDFLASH_BUS_WIDTH);
#else
	trace_fatal("Cannot configure NAND: target board has no NAND definitions!");
#endif
}
#endif /* CONFIG_HAVE_NAND_FLASH */

bool board_cfg_sdmmc(uint32_t periph_id)
{
	switch (periph_id) {
	case ID_SDMMC0:
	{
#ifdef BOARD_SDMMC0_PINS
		const struct _pin pins[] = BOARD_SDMMC0_PINS;

		/* Configure SDMMC0 pins */
		pio_configure(pins, ARRAY_SIZE(pins));
		return true;
#else
		trace_fatal("Cannot configure SDMMC0: target board has no SDMMC0 definitions!");
		return false;
#endif
	}
	case ID_SDMMC1:
	{
#ifdef BOARD_SDMMC1_PINS
		const struct _pin pins[] = BOARD_SDMMC1_PINS;

		/* Configure SDMMC1 pins */
		pio_configure(pins, ARRAY_SIZE(pins));
		return true;
#else
		trace_fatal("Cannot configure SDMMC1: target board has no SDMMC1 definitions!");
		return false;
#endif
	}
	default:
		return false;
	}
}