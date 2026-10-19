#pragma once
#include <Arduino.h>
#include <hardware/clocks.h>
#include <hardware/structs/qmi.h>
#include <hardware/regs/qmi.h>
#include <hardware/sync.h>

// The HSTX library changes clk_sys in a global constructor, after the core's
// initial PSRAM setup. Reconfigure CS1 before allocating or accessing PSRAM.
// No changes to the installed Arduino core or Adafruit library are needed.
static void __no_inline_not_in_flash_func(configureVideoPsram)() {
  // Conservative PSRAM clock <= 60 MHz. Preserve the core's page/select fields.
  // This may cost performance; prioritize reliable startup for the first test.
  const uint32_t hz=clock_get_hz(clk_sys);
  uint32_t divisor=(hz+59999999u)/60000000u;
  if(divisor<4)divisor=4;
  const uint32_t maxSelect=hz/8000000u; // <= 8 us, units of 64 sys clocks.
  const int deselectClocks=int((uint64_t(hz)*18u+999999999u)/1000000000u);
  const int hold=deselectClocks-int((divisor+1)/2);
  const uint32_t minDeselect=hold>0?uint32_t(hold):0;
  const uint32_t irq=save_and_disable_interrupts();
  // Update timing only: the core already detected the chip and enabled QPI.
  // Avoid re-entering direct mode or issuing chip commands after runtime init.
  qmi_hw->m[1].timing=(1u<<QMI_M1_TIMING_COOLDOWN_LSB)|
    (QMI_M1_TIMING_PAGEBREAK_VALUE_1024<<QMI_M1_TIMING_PAGEBREAK_LSB)|
    (maxSelect<<QMI_M1_TIMING_MAX_SELECT_LSB)|
    (minDeselect<<QMI_M1_TIMING_MIN_DESELECT_LSB)|
    (divisor<<QMI_M1_TIMING_CLKDIV_LSB)|(divisor<<QMI_M1_TIMING_RXDELAY_LSB);
  __dmb();
  restore_interrupts(irq);
}
