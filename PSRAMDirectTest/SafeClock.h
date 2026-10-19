#pragma once
#include <hardware/clocks.h>
#include <hardware/structs/qmi.h>
#include <hardware/regs/qmi.h>
#include <hardware/sync.h>
static void __no_inline_not_in_flash_func(safeCpuAndRamClock)() {
  uint32_t irq=save_and_disable_interrupts();
  clock_configure(clk_sys,CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,480000000,120000000);
  // 120 MHz CPU / 4 = 30 MHz PSRAM. 4 half-cycle receive delay,
  // <= 8 us select, >= 18 ns deselect including divided-clock contribution.
  qmi_hw->m[1].timing=(1u<<QMI_M1_TIMING_COOLDOWN_LSB)|
    (QMI_M1_TIMING_PAGEBREAK_VALUE_1024<<QMI_M1_TIMING_PAGEBREAK_LSB)|
    (15u<<QMI_M1_TIMING_MAX_SELECT_LSB)|
    (1u<<QMI_M1_TIMING_MIN_DESELECT_LSB)|
    (4u<<QMI_M1_TIMING_RXDELAY_LSB)|(4u<<QMI_M1_TIMING_CLKDIV_LSB);
  __dmb();restore_interrupts(irq);
}
