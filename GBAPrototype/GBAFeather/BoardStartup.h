#pragma once
#include <hardware/clocks.h>
#include <hardware/structs/qmi.h>
#include <hardware/regs/qmi.h>
#include <hardware/sync.h>
static void __no_inline_not_in_flash_func(safeCpuAndRamClock)() {
  uint32_t irq=save_and_disable_interrupts();
  // Restore the HDMI library's CPU speed, retaining the proven PSRAM SCK:
  // 240 MHz / 8 = 30 MHz. RXDELAY has only 3 bits: use its latest legal sample
  // (7 half system cycles), never let an out-of-range value spill into bit 11.
  // 30 * 64 system clocks = 8 us maximum select; 1 + ceil(8/2) cycles
  // provides >= 18 ns deselect. The independent HSTX PLL stays unchanged.
  static_assert(((7u<<QMI_M1_TIMING_RXDELAY_LSB)&~QMI_M1_TIMING_RXDELAY_BITS)==0);
  qmi_hw->m[1].timing=(1u<<QMI_M1_TIMING_COOLDOWN_LSB)|
    (QMI_M1_TIMING_PAGEBREAK_VALUE_1024<<QMI_M1_TIMING_PAGEBREAK_LSB)|
    (30u<<QMI_M1_TIMING_MAX_SELECT_LSB)|
    (1u<<QMI_M1_TIMING_MIN_DESELECT_LSB)|
    (7u<<QMI_M1_TIMING_RXDELAY_LSB)|(8u<<QMI_M1_TIMING_CLKDIV_LSB);
  // Apply the slower memory divisor before any CPU speed increase.
  (void)*reinterpret_cast<volatile uint32_t*>(0x11000000u);
  __dsb();
  clock_configure(clk_sys,CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,480000000,240000000);
  __dmb();restore_interrupts(irq);
}
