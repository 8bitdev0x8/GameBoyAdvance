#pragma once
#include <Arduino.h>
#include <hardware/sync.h>

// CPU-observed access cost including cache, loop overhead and live HDMI IRQs.
// These numbers are not raw PSRAM-chip or QMI-wire latency.
struct MemoryBenchmarkResult {
  uint32_t sequentialReadNs,dependentReadNs,sequentialWriteNs,scatteredWriteNs;
};
static volatile uint32_t memoryBenchmarkSink;
static MemoryBenchmarkResult __no_inline_not_in_flash_func(benchmarkMemory)(volatile uint32_t* data,uint32_t words) {
  constexpr uint32_t operations=131072;
  const uint32_t mask=words-1;
  // Full-period permutation for a power-of-two window; chasing stored links
  // forces each read to depend on the preceding read.
  for(uint32_t i=0;i<words;++i)data[i]=(i*1664525u+1013904223u)&mask;
  __dsb();
  uint32_t sum=0,index=0;
  uint32_t start=time_us_32();
  for(uint32_t i=0;i<operations;++i)sum^=data[i&mask];
  __dsb();const uint32_t seqRead=time_us_32()-start;
  memoryBenchmarkSink=sum;
  start=time_us_32();
  for(uint32_t i=0;i<operations;++i)index=data[index];
  __dsb();const uint32_t depRead=time_us_32()-start;
  memoryBenchmarkSink=index;
  start=time_us_32();
  for(uint32_t i=0;i<operations;++i)data[i&mask]=i;
  __dsb();const uint32_t seqWrite=time_us_32()-start;
  start=time_us_32();index=0;
  for(uint32_t i=0;i<operations;++i) {
    index=(index*1664525u+1013904223u)&mask;
    data[index]=i;
  }
  __dsb();const uint32_t scatteredWrite=time_us_32()-start;
  return {uint32_t(uint64_t(seqRead)*1000/operations),uint32_t(uint64_t(depRead)*1000/operations),
          uint32_t(uint64_t(seqWrite)*1000/operations),uint32_t(uint64_t(scatteredWrite)*1000/operations)};
}
