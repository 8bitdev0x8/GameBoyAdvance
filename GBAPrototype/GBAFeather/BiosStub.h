#pragma once
#include <vector>
#include <stdint.h>
// Synthetic ARM vector/IRQ trampoline, not a Nintendo BIOS dump.
// SWIs are implemented by the emulator's HLE path.
inline std::vector<uint8_t> makeBiosStub() {
  std::vector<uint8_t> bios(16384,0xff);
  auto put=[&](unsigned a,uint32_t v){for(unsigned i=0;i<4;++i)bios[a+i]=v>>(i*8);};
  for(unsigned a=0;a<32;a+=4)put(a,0xeafffffe); // Trap unexpected vectors.
  put(0x18,0xea000008); // B 0x40 (IRQ)
  put(0x40,0xe92d500f); // PUSH r0-r3,r12,lr
  put(0x44,0xe59fc01c); // LDR r12,[pc,#0x1c] -> 0x68
  put(0x48,0xe59cc000); // LDR r12,[r12] -> game IRQ handler
  put(0x4c,0xe35c0000); // CMP r12,#0
  put(0x50,0x0a000001); // BEQ 0x5c (restore even without a handler)
  put(0x54,0xe1a0e00f); // MOV lr,pc -> return to 0x5c
  put(0x58,0xe12fff1c); // BX r12
  put(0x5c,0xe8bd500f); // POP r0-r3,r12,lr (including SP writeback)
  put(0x60,0xe25ef004); // SUBS pc,lr,#4 (restore CPSR)
  put(0x68,0x03007ffc);
  return bios;
}
