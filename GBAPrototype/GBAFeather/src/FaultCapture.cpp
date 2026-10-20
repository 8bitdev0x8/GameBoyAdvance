#include <Arduino.h>
#include "FaultCapture.h"
#include "PcProfilerFormat.h"
#include <hardware/exception.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/watchdog.h>
#include <hardware/sync.h>

extern "C" void __no_inline_not_in_flash_func(gbaFaultCaptureFrame)(
    const uint32_t* frame,uint32_t excReturn) {
  const uint32_t cfsr=*reinterpret_cast<volatile uint32_t*>(0xe000ed28u);
  const uint32_t hfsr=*reinterpret_cast<volatile uint32_t*>(0xe000ed2cu);
  const uint32_t sfsr=*reinterpret_cast<volatile uint32_t*>(0xe000ede4u)&0xffu;
  uint32_t pc=0;
  // Reuse the tested secure Cortex-M33 default-frame validator. Do not
  // dereference an out-of-SRAM or unrecognized exception frame.
  const bool valid=gbaProfileFramePc(frame,excReturn,0x20000000u,0x20082000u,pc);
  watchdog_hw->scratch[3]=cfsr;
  watchdog_hw->scratch[5]=valid?pc:0u;
  watchdog_hw->scratch[6]=valid?frame[5]:excReturn;
  watchdog_hw->scratch[7]=uint32_t(reinterpret_cast<uintptr_t>(frame));
  // HFSR uses bits 31,30,1; bits 0/2/3 carry fault/core/frame-valid flags,
  // and bits 8..15 retain SFSR on this Secure Cortex-M33 firmware.
  watchdog_hw->scratch[2]=1u|(hfsr&0xc0000002u)|((sio_hw->cpuid&1u)<<2u)|(valid?8u:0u)|(sfsr<<8u);
  __dsb();
  while(true)__asm volatile("nop");
}

// Preserve the interrupted SP before any compiler prologue changes MSP.
extern "C" __attribute__((naked,used,section(".time_critical.gba_fault_irq"))) void gbaFaultIrq() {
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "ldr r2, =gbaFaultCaptureFrame\n"
    "bx r2\n");
}

void gbaInstallFaultCapture() {
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION,gbaFaultIrq);
}

// Link with --wrap=abort so allocation/assertion failures retain their caller
// instead of ending at newlib's generic _exit breakpoint. This is an ordinary
// call, not an exception frame; flag bit 4 distinguishes the record.
extern "C" __attribute__((noreturn,noinline,section(".time_critical.gba_abort_capture"))) void __wrap_abort() {
  const uint32_t caller=uint32_t(reinterpret_cast<uintptr_t>(
      __builtin_extract_return_addr(__builtin_return_address(0))));
  uint32_t sp;__asm volatile("mrs %0, msp":"=r"(sp));
  watchdog_hw->scratch[2]=1u|16u|((sio_hw->cpuid&1u)<<2u);
  watchdog_hw->scratch[3]=0;
  watchdog_hw->scratch[5]=caller&~1u;
  watchdog_hw->scratch[6]=caller;
  watchdog_hw->scratch[7]=sp;
  __dsb();
  // Startup stages already arm the watchdog; also cover earlier runtime
  // failures before the application has armed it.
  if(!(watchdog_hw->ctrl&WATCHDOG_CTRL_ENABLE_BITS))watchdog_enable(1000,false);
  while(true)__asm volatile("nop");
}
