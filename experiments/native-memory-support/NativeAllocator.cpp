#include "NativeAllocator.h"
#include <limits>
#include <new>
namespace NativeMemory {
namespace {
bool powerOfTwo(size_t v){return v && !(v&(v-1));}
bool add(size_t a,size_t b,size_t& result){if(b>std::numeric_limits<size_t>::max()-a)return false;result=a+b;return true;}
size_t maximum(size_t a,size_t b){return a>b?a:b;}
}
bool Allocator::configure(Config c,Backend b){
  if(stats_.active || c.internalBudget>sizeof(pool_) || (c.internalBudget && c.internalBudget<sizeof(Block)) ||
     c.internalBudget%alignof(Block) || !b.allocate || !b.release)return false;
  config_=c;backend_=b;stats_={};free_=c.internalBudget ? ::new(static_cast<void*>(pool_)) Block{c.internalBudget,nullptr} : nullptr;initialized_=true;return true;
}
void* Allocator::internalRaw(size_t wanted,size_t& charged){
  size_t rounded;if(!add(wanted,alignof(Block)-1,rounded))return nullptr;
  rounded&=~size_t(alignof(Block)-1);rounded=maximum(rounded,sizeof(Block));
  Block** link=&free_;
  while(*link){Block* b=*link;if(b->bytes>=rounded){
    const size_t rest=b->bytes-rounded;Block* next=b->next;
    if(rest>=sizeof(Block)){
      auto* tail=::new(static_cast<void*>(reinterpret_cast<unsigned char*>(b)+rounded)) Block{rest,next};*link=tail;charged=rounded;
    }else{charged=b->bytes;*link=next;}
    return b;
  }link=&b->next;}
  return nullptr;
}
void Allocator::releaseInternal(void* raw,size_t charged){
  const auto address=reinterpret_cast<uintptr_t>(raw);Block** link=&free_;
  while(*link && reinterpret_cast<uintptr_t>(*link)<address)link=&(*link)->next;
  auto* b=::new(raw) Block{charged,*link};*link=b;
  if(b->next && reinterpret_cast<unsigned char*>(b)+b->bytes==reinterpret_cast<unsigned char*>(b->next)){
    b->bytes+=b->next->bytes;b->next=b->next->next;
  }
  Block* previous=nullptr;for(Block* cur=free_;cur && cur!=b;cur=cur->next)previous=cur;
  if(previous && reinterpret_cast<unsigned char*>(previous)+previous->bytes==reinterpret_cast<unsigned char*>(b)){
    previous->bytes+=b->bytes;previous->next=b->next;
  }
}
uintptr_t Allocator::cookie(void* user,const Header& h){return reinterpret_cast<uintptr_t>(user)^reinterpret_cast<uintptr_t>(h.raw)^h.bytes^h.alignment^(h.charged<<1)^h.rawBytes^(uintptr_t(h.origin)*0x9e3779b9u)^uintptr_t(Magic);}
void* Allocator::allocate(size_t bytes,size_t alignment){
  if(!initialized_ || !bytes || !powerOfTwo(alignment)){++stats_.invalidRequests;return nullptr;}
  const size_t effective=maximum(alignment,alignof(Header));size_t wanted;
  if(!add(bytes,sizeof(Header),wanted)||!add(wanted,effective-1,wanted)){++stats_.invalidRequests;return nullptr;}
  size_t charged=0;void* raw=nullptr;uint32_t origin=0;
  if(bytes<=config_.smallLimit){raw=internalRaw(wanted,charged);if(!raw && !config_.spillSmallToPsram){++stats_.budgetFailures;return nullptr;}}
  if(!raw){
    if(wanted>config_.psramBudget || stats_.psramCharged>config_.psramBudget-wanted){++stats_.budgetFailures;return nullptr;}
    raw=backend_.allocate(backend_.context,wanted);
    if(!raw){++stats_.backendFailures;return nullptr;}
    charged=wanted;origin=1;
  }
  const uintptr_t base=reinterpret_cast<uintptr_t>(raw);uintptr_t first;
  if(base>std::numeric_limits<uintptr_t>::max()-sizeof(Header) ||
     (first=base+sizeof(Header))>std::numeric_limits<uintptr_t>::max()-(effective-1)){
    if(origin)backend_.release(backend_.context,raw);else releaseInternal(raw,charged);
    ++stats_.invalidRequests;return nullptr;
  }
  const uintptr_t address=(first+effective-1)&~uintptr_t(effective-1);void* user=reinterpret_cast<void*>(address);
  auto* h=::new(reinterpret_cast<void*>(address-sizeof(Header))) Header{Magic,origin,raw,charged,bytes,alignment,charged,0};h->cookie=cookie(user,*h);
  ++stats_.allocations;++stats_.active;stats_.peakActive=maximum(stats_.peakActive,stats_.active);
  if(origin){stats_.psramPayload+=bytes;stats_.psramCharged+=charged;stats_.psramPeak=maximum(stats_.psramPeak,stats_.psramCharged);++stats_.psramAllocations;if(bytes<=config_.smallLimit)++stats_.smallSpills;}
  else{stats_.internalPayload+=bytes;stats_.internalCharged+=charged;stats_.internalPeak=maximum(stats_.internalPeak,stats_.internalCharged);++stats_.internalAllocations;}
  return user;
}
bool Allocator::release(void* user,size_t bytes,size_t alignment){
  if(!user)return true;
  // Like free/GlobalAlloc::dealloc this requires a live pointer returned by this
  // allocator. Arbitrary/dangling pointers cannot be made safe by a header test.
  if(!initialized_ || reinterpret_cast<uintptr_t>(user)%alignof(Header)){++stats_.badFrees;return false;}
  auto* h=reinterpret_cast<Header*>(static_cast<unsigned char*>(user)-sizeof(Header));
  if(h->magic!=Magic || h->bytes!=bytes || h->alignment!=alignment || h->origin>1 || h->cookie!=cookie(user,*h)){
    ++stats_.badFrees;return false;
  }
  const Header saved=*h;h->magic=0;
  --stats_.active;++stats_.frees;
  if(saved.origin){stats_.psramPayload-=saved.bytes;stats_.psramCharged-=saved.charged;backend_.release(backend_.context,saved.raw);}
  else{stats_.internalPayload-=saved.bytes;stats_.internalCharged-=saved.charged;releaseInternal(saved.raw,saved.charged);}
  return true;
}
bool Allocator::ownsInternal(const void* p)const{const uintptr_t value=reinterpret_cast<uintptr_t>(p),base=reinterpret_cast<uintptr_t>(pool_);return value>=base && value-base<config_.internalBudget;}
size_t Allocator::largestInternalFree()const{size_t largest=0;for(auto* b=free_;b;b=b->next)largest=maximum(largest,b->bytes);return largest;}
}
