#pragma once
#include <stdint.h>
#include <stddef.h>
// Original cache for one verified immutable native image. Single core executor;
// never cache guest bytes or suppress the translated block's existing guard.
// Clear before binding a different image/table. Missing lookups remain missing.
constexpr unsigned nativeCacheBits(size_t n){unsigned bits=0;while(n>1){++bits;n>>=1;}return bits;}
template<size_t Sets,size_t Ways=2,class Address=uint32_t> struct NativeLookupCache {
 static_assert(Sets>=2 && !(Sets&(Sets-1)));
 static_assert(Ways==2||Ways==4);
 struct Entry {uint32_t key=0;Address address=0;};
 Entry entries[Sets][Ways]{};
 uint8_t victim[Sets]{};
 static size_t index(uint32_t key){return ((key^(key>>16))*0x9e3779b1u)>>(32-nativeCacheBits(Sets));}
 void clear(){for(auto& set:entries)for(auto& e:set)e={};for(auto& v:victim)v=0;}
 template<class Resolve> Address lookup(uint32_t key,Resolve resolve){
  const size_t s=index(key);
  for(size_t way=0;way<Ways;++way)if(entries[s][way].address&&entries[s][way].key==key){victim[s]=uint8_t((way+1)%Ways);return entries[s][way].address;}
  const Address address=resolve(key);if(!address)return 0;
  size_t way=0;while(way<Ways&&entries[s][way].address)++way;if(way==Ways)way=victim[s];
  entries[s][way]={key,address};victim[s]=uint8_t((way+1)%Ways);return address;
 }
};
