#include "directmemory.h"

#include <zenkit/DaedalusScript.hh>
#include <Tempest/Log>

#include <cassert>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "world/focus.h"
#include "gothic.h"
#include "gamemusic.h"
#include "game/serialize.h"
#include "resources.h"
#include "utils/gthfont.h"
#undef zError // miniz's zlib alias conflicts with the Gothic structure

using namespace Tempest;
using namespace Compatibility;

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

static int32_t floatBitsToInt(float f) {
  int32_t i = 0;
  std::memcpy(&i, &f, sizeof(i));
  return i;
  }

static float intBitsToFloat(int32_t i) {
  float f = 0;
  std::memcpy(&f, &i, sizeof(f));
  return f;
  }

void DirectMemory::memory_instance::set_int(const zenkit::DaedalusSymbol& sym, uint16_t index, int32_t value) {
  if(sym.name()=="ZCARRAY.NUMINARRAY" && value>1024)
    Log::d("");
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sizeof(int32_t));
  owner.mem32.writeInt(addr, value);
  }

int32_t DirectMemory::memory_instance::get_int(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sizeof(int32_t));
  int32_t v = owner.mem32.readInt(addr);
  return v;
  }

void DirectMemory::memory_instance::set_float(const zenkit::DaedalusSymbol& sym, uint16_t index, float value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sizeof(int32_t));
  owner.mem32.writeInt(addr, *reinterpret_cast<const int32_t*>(&value));
  }

float DirectMemory::memory_instance::get_float(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index * sizeof(int32_t));
  int32_t v = owner.mem32.readInt(addr);

  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
  }

void DirectMemory::memory_instance::set_string(const zenkit::DaedalusSymbol& sym, uint16_t index, std::string_view value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sizeof(zString));
  if(auto* s = owner.mem32.deref<zString>(addr))
    owner.memAssignString(*s, value);
  }

const std::string& DirectMemory::memory_instance::get_string(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index*sizeof(zString));

  if(auto zs = owner.mem32.deref<const zString>(addr)) {
    static std::string ret;
    owner.memFromString(ret, *zs);
    return ret;
    }

  Log::d("TODO: memory_instance::get_string ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


DirectMemory::DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm) : gameScript(owner), vm(vm), cpu(*this, mem32) {
  // Virtual pointers include bytecode/symbol addresses: accept snapshots only
  // for the same script, including string literals and original constants.
  scriptFingerprint = 14695981039346656037ull;
  auto hash = [this](uint64_t value) {
    for(unsigned i=0;i<8;++i) {
      scriptFingerprint = (scriptFingerprint ^ (value & 255))*1099511628211ull;
      value >>= 8;
      }
    };
  for(auto& s : vm.symbols()) {
    for(auto c : s.name()) hash(uint8_t(c));
    hash(0); hash(uint32_t(s.type())); hash(s.count()); hash(s.address());
    if(s.is_member()) { hash(s.offset_as_member()); continue; }
    if(s.is_const()) {
      for(uint16_t i=0;i<s.count();++i) {
        if(s.type()==zenkit::DaedalusDataType::INT) hash(uint32_t(s.get_int(i)));
        if(s.type()==zenkit::DaedalusDataType::FLOAT) hash(uint32_t(floatBitsToInt(s.get_float(i))));
        if(s.type()==zenkit::DaedalusDataType::STRING) {
          for(auto c : s.get_string(i)) hash(uint8_t(c));
          hash(0);
          }
        }
      }
    }
  for(uint32_t pc=0;pc<vm.size();) {
    auto i = vm.instruction_at(pc);
    hash(uint8_t(i.op)); hash(i.address); hash(i.symbol); hash(uint32_t(i.immediate)); hash(i.index);
    pc += i.size;
    }
  if(auto v = vm.find_symbol_by_name("Ikarus_Version")) {
    const int version = v->type()==zenkit::DaedalusDataType::INT ? v->get_int() : 0;
    Log::i("DMA mod detected: Ikarus v", version);
    }
  if(auto v = vm.find_symbol_by_name("LeGo_Version")) {
    const char* version = v->type()==zenkit::DaedalusDataType::STRING ? v->get_string().c_str() : "LeGo";
    Log::i("DMA mod detected: ", version);
    }

  setupFunctionTable();
  setupIkarusLoops();
  setupEngineText();
  setupEngineMemory();

  // Inline ASM
  vm.override_function("ASMINT_Init",           [this]() { ASMINT_Init(); });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [this]() { ASMINT_CallMyExternal(); });

  // Pointers
  setupMemoryFunctions();

  // Dynamic calls by id's
  setupDirectFunctions();

  setupWinapiFunctions();

  setupUtilityFunctions();

  // LeGo hooks
  setupHookEngine();

  setupMathFunctions();
  setupStringsFunctions();

  setupzCParserFunctions();

  setupInitFileFunctions();
  setupUiFunctions();
  setupFontFunctions();
  setupNpcFunctions();
  setupWorldFunctions();
  setupMusicFunctions();
  setupKmLibFunctions();

  // various
  vm.override_function("MEM_PrintStackTrace", [this](){ memPrintstacktraceImplementation(); });
  vm.override_function("MEM_SendToSpy",       [this](int cat, std::string_view msg){ return memSendToSpy(cat,msg); });
  vm.override_function("MEM_ReplaceFunc",     [this](zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func){ memReplaceFunc(dest, func); });
  //
  vm.override_function("MEMINT_SetupExceptionHandler",   [](){ });
  vm.override_function("MEMINT_ReplaceSlowFunctions",    [](){ });
  //vm.override_function("MEMINT_ReplaceLoggingFunctions", [](){ });
  vm.override_function("MEM_InitStatArrs",               [](){ });
  vm.override_function("MEM_InitRepeat",                 [](){ });
  vm.override_function("MEM_GetCommandLine",             [](){ return std::string(""); });

  auto safeOverrideFunction = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };

  safeOverrideFunction("_RENDER_INIT", []() {
    // unclear how exactly it should behave - need to find testing sample
    Log::e("not implemented call [_RENDER_INIT]");
    });
  safeOverrideFunction("PRINT_FIXPS", [](){
    // function patches asm code of zCView::PrintTimed* to fix text coloring - we can ignore it
    });

  // override for now: need to expose way-net system and provide dma-access for npc
  safeOverrideFunction("TELEPORTNPCTOWP", [this](int npcId, std::string_view wpName){
    auto wp = gameScript.world().findPoint(wpName,false);
    if(wp==nullptr) {
      Log::d("TeleportNpcToWP: invalid waypoint: ", wpName);
      return;
      }
    auto npcRef = this->vm.find_symbol_by_index(uint32_t(npcId))->get_instance();
    if(npcRef==nullptr || npcRef->user_ptr==nullptr)
      return;
    auto& npc = *reinterpret_cast<Npc*>(npcRef->user_ptr);
    npc.clearGoTo();
    npc.setPosition (wp->position());
    npc.setDirection(wp->direction());
    });
  //
  safeOverrideFunction("LOG_MOVETOTOP", [](std::string_view topic) {
    // need to have a projection of 'OCLOGMANAGER_PTR' into mem32
    Log::e("not implemented call [LOG_MOVETOTOP]");
    });

  // Disable some high-level functions, until basic stuff is done
  auto dummyfy = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };
  dummyfy("WRITENOP",    [](int,int){}); // hook-related mess
  dummyfy("MEM_SETKEYS", [](std::string_view,int,int){});
  dummyfy("INIT_QUIVERS_ALWAYS", [](){}); // requires sorted npc list
  // Register only inside DMA compatibility and after fingerprinting the original
  // raw member offset. Ordinary mods and existing v1 snapshot identity stay intact.
  if(vm.find_symbol_by_name("OCNPC.FOCUS_VOB"))
    vm.register_member("OCNPC.FOCUS_VOB",&zenkit::INpc::focus_vob);
  }

bool DirectMemory::isRequired(zenkit::DaedalusScript& vm) {
  return
      vm.find_symbol_by_name("Ikarus_Version") != nullptr &&
      vm.find_symbol_by_name("MEM_InitAll") != nullptr &&
      vm.find_symbol_by_name("MEM_ReadInt") != nullptr &&
      vm.find_symbol_by_name("MEM_WriteInt") != nullptr &&
      vm.find_symbol_by_name("_@") != nullptr &&
      vm.find_symbol_by_name("_^") != nullptr;
  }

auto DirectMemory::focusVob(Interactive& focus) -> ptr32_t {
  if(focus.lockAddress!=0)
    return focus.lockAddress;

  auto* cls = vm.find_symbol_by_name("OCMOBLOCKABLE");
  if(cls==nullptr || cls->class_size()==0)
    return 0;

  // ponytail: retain one small block per touched lock until session destruction;
  // reclaim only with script-reference tracking. Deleted locks leave zeroed
  // bytes, and new native objects never inherit their virtual address.
  focus.lockAddress = mem32.alloc(cls->class_size(),"focused OCMOBLOCKABLE");
  return focus.lockAddress;
  }

void DirectMemory::setNpcFocus(Npc& npc, Interactive* focus, int pickLockProgress) {
  clearNpcFocus(npc);
  if(focus==nullptr)
    return;

  const auto address = focusVob(*focus);
  auto* bitfield = vm.find_symbol_by_name("OCMOBLOCKABLE.BITFIELD");
  if(address==0 || bitfield==nullptr)
    return;

  const auto at = address + ptr32_t(bitfield->offset_as_member());
  // Original oCMobLockable: bit 0 locked, bit 1 auto-open, bits 2..31 progress.
  // Native locks do not auto-open. Derive every bit; never keep stale lock state.
  mem32.writeInt(at,int32_t(uint32_t(pickLockProgress)<<2) | (focus->isLocked() ? 1 : 0));
  npc.handle().focus_vob = int32_t(address);
  }

void DirectMemory::setNpcFocus(Npc& npc, Npc* focus) {
  if(focus==nullptr) {
    clearNpcFocus(npc);
    return;
    }
  const auto address = npcVob(*focus);
  if(address==0) {
    clearNpcFocus(npc);
    return;
    }
  npc.handle().focus_vob = int32_t(address);
  }

DirectMemory::ptr32_t DirectMemory::npcVob(Npc& npc) {
  auto* vtable = vm.find_symbol_by_name("OCNPC_VTBL");
  if(vtable==nullptr)
    return 0;
  auto instance = npc.handlePtr();
  // Existing bindings are invalidated by native world removal. Only a newly
  // encountered target needs the linear world-membership check.
  if(focusNpcAddress.find(instance)==focusNpcAddress.end() && !isLiveNpc(instance))
    return 0;
  auto [it,inserted] = focusNpcAddress.emplace(instance,0);
  if(inserted) {
    it->second = mem32.alloc(4,"focused OCNPC");
    focusNpc.emplace(it->second,instance);
    }
  mem32.writeInt(it->second,vtable->get_int());
  return it->second;
  }

void DirectMemory::clearNpcFocus(Npc& npc) {
  if(auto address = uint32_t(npc.handle().focus_vob)) {
    auto* cls = vm.find_symbol_by_name("OCMOBLOCKABLE");
    if(cls && mem32.isAllocation(address,cls->class_size(),"focused OCMOBLOCKABLE"))
      if(auto* bytes = mem32.derefv(address,cls->class_size()))
        std::memset(bytes,0,cls->class_size());
    }
  npc.handle().focus_vob = 0;
  }

void DirectMemory::invalidateNpcFocus(Npc& npc) {
  clearNpcFocus(npc);
  auto it = focusNpcAddress.find(npc.handlePtr());
  if(it==focusNpcAddress.end())
    return;
  const auto ptr = it->second;
  // Retain the zeroed allocation: scripts may still hold its virtual address.
  mem32.writeInt(ptr,0);
  focusNpc.erase(ptr);
  focusNpcAddress.erase(it);
  auto& world = gameScript.world();
  for(uint32_t id=0;auto* observer=world.npcById(id);++id)
    if(uint32_t(observer->handle().focus_vob)==ptr)
      observer->handle().focus_vob = 0;
  }

bool DirectMemory::isLiveNpc(const std::shared_ptr<zenkit::INpc>& npc) const {
  auto& world = gameScript.world();
  const auto id = world.npcId(static_cast<Npc*>(npc->user_ptr));
  auto* live = world.npcById(id);
  return live!=nullptr && live->handlePtr()==npc;
  }

void DirectMemory::pruneFocusNpcs() {
  for(auto it=focusNpcAddress.begin(); it!=focusNpcAddress.end();) {
    auto npc = it->first.lock();
    if(npc && isLiveNpc(npc)) {
      ++it;
      continue;
      }
    mem32.writeInt(it->second,0);
    focusNpc.erase(it->second);
    it = focusNpcAddress.erase(it);
    }
  }

void DirectMemory::resetWorldReferences() {
  for(const auto& [ptr,npc]:focusNpc)
    mem32.writeInt(ptr,0);
  focusNpc.clear();
  focusNpcAddress.clear();
  for(auto it=scriptReferences.begin(); it!=scriptReferences.end();) {
    auto context = it->first.first;
    if(!dynamic_cast<zenkit::INpc*>(context.get()) && !dynamic_cast<zenkit::IItem*>(context.get())) {
      ++it;
      continue;
      }
    auto* ref = vm.find_symbol_by_index(it->first.second);
    const auto ptr = it->second;
    if(ref!=nullptr && ptr!=0) {
      const uint32_t stride = ref->type()==zenkit::DaedalusDataType::STRING ? sizeof(zString) : 4;
      const uint32_t size = ((uint32_t(ref->count())*stride+Mem32::memAlign-1)/Mem32::memAlign)*Mem32::memAlign;
      bindReference(ref,nullptr,mem32.regionType(ptr,size));
      }
    // Keep the virtual block as a deleted-native tombstone. A recreated world
    // object must receive a fresh mapping rather than inherit this one.
    auto node = scriptReferences.extract(it++);
    node.key().first.reset();
    scriptReferences.insert(std::move(node));
    }
  if(auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB"))
    lastMob->set_int(0);
  }

static uint64_t inventorySignature(const Npc& npc) {
  uint64_t result = 0;
  for(auto item=npc.inventory().iterator(Inventory::T_Inventory); item.isValid(); ++item)
    result = (result*1000003) ^ (uint64_t(item->clsId())<<32) ^ item.count();
  return result;
  }

auto DirectMemory::nativeReference(zenkit::DaedalusSymbol* ref, std::shared_ptr<zenkit::DaedalusInstance> context) -> ptr32_t {
  if(ref==nullptr)
    throw std::runtime_error("Invalid native reference probe");
  auto it = scriptReferences.find({context,ref->index()});
  if(it!=scriptReferences.end())
    return it->second;
  const auto type = mem32.nextScriptReferenceType();
  bindReference(ref,context,type);
  const uint32_t stride = ref->type()==zenkit::DaedalusDataType::STRING ? uint32_t(sizeof(zString)) : 4;
  return scriptReferences.emplace(std::make_pair(std::move(context),ref->index()),
                                  mem32.alloc(uint32_t(ref->count())*stride,type))->second;
  }

void DirectMemory::beginWorldTransitionProbe(Npc& npc, Interactive& lock) {
  // A prior stage has already observed its restored recurring callback. Do not
  // carry that test callback into the next transition's persistence check.
  if(worldProbeRecurringTimer!=0) {
    vm.call_function("FF_REMOVE",int32_t(vm.find_symbol_by_name("TIMER_SETPAUSE")->index()));
    worldProbeRecurringTimer = 0;
    Log::i("[WORLD_PROBE] inherited_callback_removed=1");
    }
  auto* ref = vm.find_symbol_by_name("C_NPC.AIVAR");
  auto* itemValue = vm.find_symbol_by_name("C_ITEM.VALUE");
  worldProbeReference = nativeReference(ref,npc.handlePtr());
  mem32.writeInt(worldProbeReference+99*4,0x1234);
  if(mem32.readInt(worldProbeReference+99*4)!=0x1234)
    throw std::runtime_error("World-transition probe could not bind native reference");
  worldProbeItems[0] = worldProbeItems[1] = 0;
  uint32_t firstClass = 0;
  for(auto item=npc.inventory().iterator(Inventory::T_Inventory); item.isValid() && worldProbeItems[1]==0; ++item) {
    const auto cls = uint32_t(item->clsId());
    if(cls==firstClass)
      continue;
    const auto n = worldProbeItems[0]==0 ? 0u : 1u;
    worldProbeItems[n] = nativeReference(itemValue,const_cast<Item&>(*item).handlePtr());
    firstClass = cls;
    }
  if(worldProbeItems[0]==0 || worldProbeItems[1]==0 || worldProbeItems[0]==worldProbeItems[1])
    throw std::runtime_error("World-transition probe needs two inventory item bindings");

  setNpcFocus(npc,&lock,int(lock.lockpickProgress()));
  auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB");
  lastMob->set_int(npc.handle().focus_vob);
  clearNpcFocus(npc);

  worldProbeTimer = mem32.alloc(64);
  mem32.writeInt(worldProbeTimer+24,int32_t(gameScript.tickCount()));
  mem32.writeInt(worldProbeTimer+28,vm.find_symbol_by_name("_TIMER_PAUSED")->get_int());
  worldProbeRecurringTimer = mem32.alloc(64);
  mem32.writeInt(worldProbeRecurringTimer+24,int32_t(gameScript.tickCount()));
  mem32.writeInt(worldProbeRecurringTimer+28,vm.find_symbol_by_name("_TIMER_PAUSED")->get_int());
  worldProbeDispatches = 0;
  worldProbeRecurringDispatches = 0;
  worldProbeRecurringLastElapsed = 0;
  worldProbeInventory = inventorySignature(npc);
  auto* cb = vm.find_symbol_by_name("TIMER_SETPAUSE");
  vm.call_function("FF_APPLYEXTDATAGT",int32_t(cb->index()),1000,1,int32_t(worldProbeTimer));
  vm.call_function("FF_APPLYEXTDATAGT",int32_t(cb->index()),250,-1,int32_t(worldProbeRecurringTimer));
  Log::i("[WORLD_PROBE] source native_ref=1 inventory_refs=2 inventory=",worldProbeInventory," lock_address=",lock.lockAddress," pending=1 recurring=1");
  }

void DirectMemory::checkWorldTransitionProbe(Npc& npc, Interactive* returnedLock) {
  if(worldProbeReference==0 || worldProbeTimer==0 || worldProbeRecurringTimer==0 || worldProbeDispatches!=1 ||
     worldProbeRecurringDispatches<2 ||
     mem32.readInt(worldProbeReference+99*4)!=0)
    throw std::runtime_error("World-transition reference/callback regression");
  if(inventorySignature(npc)!=worldProbeInventory)
    throw std::runtime_error("World-transition lost player inventory");
  auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB");
  if(lastMob->get_int()!=0)
    throw std::runtime_error("World-transition retained stale lock focus");
  if(returnedLock!=nullptr) {
    setNpcFocus(npc,returnedLock,int(returnedLock->lockpickProgress()));
    if(npc.handle().focus_vob==0)
      throw std::runtime_error("World-transition could not bind returned lock");
    Log::i("[WORLD_PROBE] returned_lock_progress=",returnedLock->lockpickProgress(),
           " lock_address=",returnedLock->lockAddress);
    clearNpcFocus(npc);
    }
  auto* ref = vm.find_symbol_by_name("C_NPC.AIVAR");
  auto* itemValue = vm.find_symbol_by_name("C_ITEM.VALUE");
  const auto root = mem32.alloc(56,"World transition probe");
  mem32.writeInt(root,0x57545031); // WTP1
  mem32.writeInt(root+4,int32_t(worldProbeReference));
  if(worldProbeItems[0]==0 || worldProbeItems[1]==0 || worldProbeItems[0]==worldProbeItems[1])
    throw std::runtime_error("World-transition lost inventory tombstones");
  mem32.writeInt(root+8,int32_t(worldProbeItems[0]));
  mem32.writeInt(root+12,int32_t(worldProbeItems[1]));
  const auto liveHero = nativeReference(ref,npc.handlePtr());
  std::array<ptr32_t,2> liveItems = {};
  std::array<uint32_t,2> itemClasses = {};
  for(auto item=npc.inventory().iterator(Inventory::T_Inventory); item.isValid() && liveItems[1]==0; ++item) {
    const auto cls = uint32_t(item->clsId());
    if(cls==itemClasses[0])
      continue;
    const auto n = liveItems[0]==0 ? 0u : 1u;
    liveItems[n] = nativeReference(itemValue,const_cast<Item&>(*item).handlePtr());
    itemClasses[n] = cls;
    }
  if(liveItems[0]==0 || liveItems[1]==0)
    throw std::runtime_error("World-transition lost live inventory bindings");
  const std::array<ptr32_t,6> addresses = {worldProbeReference,worldProbeItems[0],worldProbeItems[1],liveHero,liveItems[0],liveItems[1]};
  for(size_t i=0;i<addresses.size();++i)
    for(size_t r=0;r<i;++r)
      if(addresses[i]==addresses[r])
        throw std::runtime_error("World-transition reused a native reference address");
  mem32.writeInt(root+16,int32_t(liveHero));
  mem32.writeInt(root+20,int32_t(liveItems[0]));
  mem32.writeInt(root+24,int32_t(liveItems[1]));
  const auto checkLive = [this,root](ptr32_t address, uint32_t offset) {
    const auto value = mem32.readInt(address);
    mem32.writeInt(address,value^0x55aa55aa);
    if(mem32.readInt(address)!=(value^0x55aa55aa))
      throw std::runtime_error("World-transition live native reference write failed");
    mem32.writeInt(address,value);
    mem32.writeInt(root+offset,value);
    };
  checkLive(liveHero+99*4,28);
  checkLive(liveItems[0],32);
  checkLive(liveItems[1],36);
  mem32.writeInt(root+40,int32_t(worldProbeRecurringTimer));
  mem32.writeInt(root+44,int32_t(itemClasses[0]));
  mem32.writeInt(root+48,int32_t(itemClasses[1]));
  vm.find_symbol_by_name("MEM_INFOBOX.RES")->set_int(int32_t(root));
  Log::i("[WORLD_PROBE] destroyed_ref=0 inventory=",worldProbeInventory,
         " callback_dispatches=",worldProbeDispatches," recurring_dispatches=",worldProbeRecurringDispatches,
         " stale_focus=0 restart_bindings=ready");
  }

void DirectMemory::verifyWorldTransitionProbe(Npc& npc) {
  const auto root = uint32_t(vm.find_symbol_by_name("MEM_INFOBOX.RES")->get_int());
  if(!mem32.isAllocation(root,56,"World transition probe") || mem32.readInt(root)!=0x57545031 ||
     worldProbeRecurringTimer!=uint32_t(mem32.readInt(root+40)) || worldProbeRecurringDispatches<2)
    throw std::runtime_error("World-transition restart callback regression");
  const std::array<ptr32_t,3> old = {uint32_t(mem32.readInt(root+4)),uint32_t(mem32.readInt(root+8)),uint32_t(mem32.readInt(root+12))};
  if(mem32.readInt(old[0]+99*4)!=0 || mem32.readInt(old[1])!=0 || mem32.readInt(old[2])!=0)
    throw std::runtime_error("World-transition restart retained a tombstone");
  auto* firstItem = npc.getItem(uint32_t(mem32.readInt(root+44)));
  auto* secondItem = npc.getItem(uint32_t(mem32.readInt(root+48)));
  if(firstItem==nullptr || secondItem==nullptr)
    throw std::runtime_error("World-transition restart lost inventory item binding");
  auto* ref = vm.find_symbol_by_name("C_NPC.AIVAR");
  auto* itemValue = vm.find_symbol_by_name("C_ITEM.VALUE");
  const std::array<std::shared_ptr<zenkit::DaedalusInstance>,3> context = {npc.handlePtr(),firstItem->handlePtr(),secondItem->handlePtr()};
  const std::array<ptr32_t,3> live = {uint32_t(mem32.readInt(root+16))+99*4,uint32_t(mem32.readInt(root+20)),uint32_t(mem32.readInt(root+24))};
  const std::array<int32_t,3> expected = {mem32.readInt(root+28),mem32.readInt(root+32),mem32.readInt(root+36)};
  for(size_t i=0;i<live.size();++i) {
    if(mem32.readInt(live[i])!=expected[i])
      throw std::runtime_error("World-transition restart live native reference read failed");
    mem32.writeInt(live[i],expected[i]^0x55aa55aa);
    const auto actual = i==0 ? ref->get_int(99,context[i].get()) : itemValue->get_int(0,context[i].get());
    if(actual!=(expected[i]^0x55aa55aa))
      throw std::runtime_error("World-transition restart live native reference write failed");
    mem32.writeInt(live[i],expected[i]);
    }
  vm.call_function("FF_REMOVE",int32_t(vm.find_symbol_by_name("TIMER_SETPAUSE")->index()));
  worldProbeRecurringTimer = 0;
  Log::i("[WORLD_PROBE] restart_bindings=1 recurring_dispatches=",worldProbeRecurringDispatches);
  }

void DirectMemory::probeNpcFocus(Npc& npc, bool restored) {
  auto check = [](bool ok) { if(!ok) throw std::runtime_error("NPC focus lifetime regression"); };
  auto& world = gameScript.world();
  auto* storage = vm.find_symbol_by_name("MEM_INFOBOX.RES");
  auto isNpc = [&](ptr32_t ptr) { return vm.call_function<int>("HLP_IS_OCNPC",int32_t(ptr))!=0; };
  if(restored) {
    const auto root = uint32_t(storage->get_int());
    check(mem32.isAllocation(root,16,"NPC focus probe") && mem32.readInt(root)==0x464f4331);
    for(uint32_t off:{4u,8u}) {
      const auto ptr = uint32_t(mem32.readInt(root+off));
      check(!isNpc(ptr) && mem_ptrtoinst(ptr)==nullptr);
      }
    const auto ptr = uint32_t(mem32.readInt(root+12));
    auto handle = std::dynamic_pointer_cast<zenkit::INpc>(mem_ptrtoinst(ptr));
    check(isNpc(ptr) && handle && isLiveNpc(handle));
    auto* target = static_cast<Npc*>(handle->user_ptr);
    setNpcFocus(npc,target);
    check(uint32_t(npc.handle().focus_vob)==ptr);
    world.removeNpc(*target);
    check(npc.handle().focus_vob==0 && !isNpc(ptr) && mem_ptrtoinst(ptr)==nullptr);
    Log::i("[NPC_FOCUS] restart bindings=1 tombstones=1 removed=1");
    return;
    }
  const auto cls = vm.find_symbol_by_name("RAZOR_ARMORED")->index();
  auto* a = world.addNpc(cls,npc.position()+Tempest::Vec3(200,0,0));
  auto* b = world.addNpc(cls,npc.position()+Tempest::Vec3(400,0,0));
  check(a && b);
  auto retained = a->handlePtr();
  setNpcFocus(npc,a);
  const auto first = uint32_t(npc.handle().focus_vob);
  check(first && isNpc(first) && mem_ptrtoinst(first)==retained);
  check(mem32.isAllocation(first,4,"focused OCNPC") &&
        !mem32.isAllocation(first+4,4,"focused OCNPC") &&
        !mem32.isAllocation(first,16,"focused OCNPC") &&
        !mem32.isAllocation(first,4,"wrong type"));
  setNpcFocus(npc,b);
  const auto second = uint32_t(npc.handle().focus_vob);
  check(second && second!=first && isNpc(second) && mem_ptrtoinst(second)==b->handlePtr());
  setNpcFocus(npc,static_cast<Npc*>(nullptr));
  check(npc.handle().focus_vob==0);
  world.removeNpc(*a);
  // Check before any subsequent target selection or save can prune mappings.
  check(!isNpc(first) && mem_ptrtoinst(first)==nullptr && retained->user_ptr==a);
  setNpcFocus(npc,a);
  check(npc.handle().focus_vob==0);
  setNpcFocus(npc,b);
  world.removeNpc(*b);
  check(npc.handle().focus_vob==0 && !isNpc(second) && mem_ptrtoinst(second)==nullptr);
  auto* c = world.addNpc(cls,npc.position()+Tempest::Vec3(600,0,0));
  check(c!=nullptr);
  setNpcFocus(npc,c);
  const auto third = uint32_t(npc.handle().focus_vob);
  check(third && third!=first && third!=second && mem_ptrtoinst(third)==c->handlePtr());
  // Simulate allocator address reuse while the old script handle survives.
  retained->user_ptr = c;
  const bool reused = !isNpc(first) && mem_ptrtoinst(first)==nullptr && mem_ptrtoinst(third)==c->handlePtr();
  retained->user_ptr = a;
  check(reused);
  const auto root = mem32.alloc(16,"NPC focus probe");
  mem32.writeInt(root,0x464f4331);
  mem32.writeInt(root+4,int32_t(first));
  mem32.writeInt(root+8,int32_t(second));
  mem32.writeInt(root+12,int32_t(third));
  storage->set_int(int32_t(root));
  clearNpcFocus(npc);
  Log::i("[NPC_FOCUS] targets=1 null=1 retained_handle=1 removed=1 reuse=1");
  // Exercise installed view destruction before drawing or a new constructor
  // can hide stale registration. Reuse the exact virtual address as raw data.
  const auto priorOrder = uiViewOrder;
  const auto priorViews = uiViews.size();
  const int viewHandle = vm.call_function<int>("VIEW_CREATE",0,0,100,100);
  check(uiViewOrder.size()==priorOrder.size()+1 && uiViews.size()==priorViews+1);
  const auto viewPtr = uiViewOrder.back();
  vm.call_function("VIEW_SETTEXTURE",viewHandle,std::string_view("BOSSBAR.TGA"));
  vm.call_function("VIEW_OPEN",viewHandle);
  check(uiViews.at(viewPtr).texture=="BOSSBAR.TGA");
  vm.call_function("DELETE",viewHandle); // Same handle destructor/free path as BAR_DELETE.
  auto absent = [&]() { return !uiViews.contains(viewPtr) && uiViews.size()==priorViews && uiViewOrder==priorOrder; };
  if(!absent()) throw std::runtime_error("View free retained UI registration");
  check(mem32.alloc(viewPtr,sizeof(zCView),"non-view reuse probe")==viewPtr);
  mem32.writeInt(viewPtr,0x52415731);
  if(!absent()) throw std::runtime_error("Non-view reuse retained UI registration");
  mem32.free(viewPtr);
  Log::i("[VIEW_REUSE] installed_delete=1 unregistered=1 raw_address_reuse=1 constructor_calls=0");
  const int releasedHandle = vm.call_function<int>("VIEW_CREATE",0,0,100,100);
  const auto releasedPtr = uiViewOrder.back();
  vm.call_function("VIEW_OPEN",releasedHandle);
  const auto validHandleId = int32_t(vm.find_symbol_by_name("HLP_ISVALIDHANDLE")->index());
  const auto dynamicValid = [&] {
    vm.call_function("MEM_CallByID",validHandleId);
    return vm.pop_int();
    };
  vm.push_int(releasedHandle);
  check(dynamicValid()==1);
  vm.push_int(0);
  check(dynamicValid()==0);
  auto* argument = vm.find_symbol_by_name("BAR_DELETE.BAR");
  const auto priorArgument = argument->get_int();
  argument->set_int(releasedHandle);
  vm.push_reference(argument);
  check(dynamicValid()==1);
  argument->set_int(priorArgument);
  vm.push_instance(npc.handlePtr());
  check(dynamicValid()==0);
  Log::i("[DYNAMIC_CALL] integer=1 zero=1 reference=1 instance_to_int_zero=1");
  vm.call_function("VIEW_DELETE",releasedHandle);
  if(uiViews.contains(releasedPtr) || uiViewOrder!=priorOrder)
    throw std::runtime_error("View destructor retained UI registration");
  check(vm.call_function<int>("HLP_ISVALIDHANDLE",releasedHandle)==0);
  check(mem32.isAllocation(releasedPtr,sizeof(zCView),""));
  check(mem32.deref<zCView>(releasedPtr)->ISOPEN==0 && mem32.deref<zCView>(releasedPtr)->ISCLOSED!=0);
  // RELEASE drops a handle, not allocation ownership. The explicit pointer
  // owner can still destroy/free it; that must not double-free the allocation.
  vm.call_function("VIEWPTR_DELETE",int32_t(releasedPtr));
  check(!mem32.isAllocation(releasedPtr,sizeof(zCView),""));
  check(mem32.alloc(releasedPtr,sizeof(zCView),"released non-view reuse")==releasedPtr);
  check(!uiViews.contains(releasedPtr) && uiViewOrder==priorOrder);
  mem32.free(releasedPtr);
  Log::i("[VIEW_REUSE] destructor_unregistered=1 release_keeps_allocation=1 owner_free=1 raw_reuse=1");
  }

void DirectMemory::probeLockFocus(Npc& npc, Interactive& lock, bool restored) {
  auto check = [](bool ok) { if(!ok) throw std::runtime_error("Lock focus regression failed"); };
  const auto offset = vm.find_symbol_by_name("OCMOBLOCKABLE.BITFIELD")->offset_as_member();
  const bool cracked = lock.isCracked();
  check(npc.handle().focus_vob==0);
  npc.handle().focus_vob = 1; // Unknown/non-lock pointer must not be dereferenced.
  setNpcFocus(npc,nullptr,0);
  check(npc.handle().focus_vob==0);
  if(restored) {
    check(lock.lockAddress!=0);
    check(mem32.readInt(lock.lockAddress+offset)==0);
    check(uint32_t(vm.find_symbol_by_name("G_PICKLOCK.LASTMOB")->get_int())==lock.lockAddress);
    Log::i("[LOCK_PROBE] restored ownership=1 stale_bytes=0");
    }
  lock.setAsCracked(false);
  setNpcFocus(npc,&lock,2);
  const auto first = uint32_t(npc.handle().focus_vob);
  check(mem32.readInt(first+offset)==9);
  lock.setAsCracked(true);
  setNpcFocus(npc,&lock,2);
  check(uint32_t(npc.handle().focus_vob)==first && mem32.readInt(first+offset)==8);
  setNpcFocus(npc,nullptr,0);
  check(npc.handle().focus_vob==0 && mem32.readInt(first+offset)==0);
  if(!restored) {
    // Simulate a new object at the exact same native address: object-local
    // metadata is reset on construction. Old script pointers must not alias it.
    lock.lockAddress = 0;
    setNpcFocus(npc,&lock,0);
    check(uint32_t(npc.handle().focus_vob)!=first && mem32.readInt(first+offset)==0);
    }
  Interactive* second = nullptr;
  for(uint32_t id=0;auto* mob=gameScript.world().mobsiById(id);++id)
    if(mob!=&lock && (mob->isContainer() || mob->isDoor())) { second=mob; break; }
  check(second!=nullptr);
  const auto current = lock.lockAddress;
  setNpcFocus(npc,second,0);
  check(uint32_t(npc.handle().focus_vob)!=current && mem32.readInt(current+offset)==0);
  clearNpcFocus(npc);
  check(npc.handle().focus_vob==0 && mem32.readInt(second->lockAddress+offset)==0);
  lock.setAsCracked(cracked);
  Log::i("[LOCK_PROBE] focus locked_unlocked=1 null=1 target_change=1 address_reuse=",!restored," cleared=1");
  }

void DirectMemory::saveReference(Serialize& out, const std::shared_ptr<zenkit::DaedalusInstance>& instance) {
  auto& w = gameScript.world();
  if(!instance) {
    out.write(uint8_t(0));
    } else if(auto m = dynamic_cast<memory_instance*>(instance.get())) {
    out.write(uint8_t(1),m->address);
    } else if(auto npc = dynamic_cast<zenkit::INpc*>(instance.get())) {
    auto id = w.npcId(static_cast<Npc*>(npc->user_ptr));
    auto* live = w.npcById(id);
    out.write(uint8_t(2),live && live->handlePtr()==instance ? id : uint32_t(-1));
    } else if(auto item = dynamic_cast<zenkit::IItem*>(instance.get())) {
    const auto id = w.itmId(item);
    if(id!=uint32_t(-1)) {
      out.write(uint8_t(3),id);
      return;
      }
    for(uint32_t i=0;i<w.npcCount();++i) {
      auto* itm = w.npcById(i)->getItem(item->symbol_index());
      if(itm && itm->handlePtr().get()==item) {
        out.write(uint8_t(4),i,uint32_t(item->symbol_index()));
        return;
        }
      }
    out.write(uint8_t(0)); // deleted native item
    } else {
    auto* sym = vm.find_symbol_by_index(instance->symbol_index());
    if(!sym || sym->get_instance()!=instance)
      throw std::runtime_error("Unsupported persistent native instance");
    out.write(uint8_t(5),sym->index());
    }
  }

auto DirectMemory::loadReference(Serialize& in) -> std::shared_ptr<zenkit::DaedalusInstance> {
  uint8_t kind = 0;
  uint32_t id = 0;
  in.read(kind);
  if(kind==0) return nullptr;
  in.read(id);
  auto& w = gameScript.world();
  if(kind==1) return std::make_shared<memory_instance>(*this,id);
  if(kind==2) {
    auto* npc = w.npcById(id);
    return npc ? npc->handlePtr() : nullptr;
    }
  if(kind==3) {
    auto* item = w.itmById(id);
    return item ? item->handlePtr() : nullptr;
    }
  if(kind==4) {
    uint32_t cls = 0;
    in.read(cls);
    auto* npc = w.npcById(id);
    auto* item = npc ? npc->getItem(cls) : nullptr;
    return item ? item->handlePtr() : nullptr;
    }
  if(kind==5) {
    auto* sym = vm.find_symbol_by_index(id);
    if(sym && sym->get_instance()) return sym->get_instance();
    }
  throw std::runtime_error("Invalid persistent native reference");
  }

void DirectMemory::logBuffProbeState(const char* phase) {
  if(std::getenv("OPENGOTHIC_BUFF_PROBE")!=nullptr) {
    auto* buff = vm.find_symbol_by_name("BUFF_SPEED");
    auto* end  = vm.find_symbol_by_name("LCBUFF._ENDTIME");
    auto* hero = gameScript.world().player();
    const int handle = buff && hero ? vm.call_function<int>("BUFF_HAS",hero->handlePtr(),int32_t(buff->index())) : 0;
    auto value = handle ? vm.call_function<std::shared_ptr<zenkit::DaedalusTransientInstance>>("GET",handle) : nullptr;
    const int timer = vm.call_function<int>("TIMERGT");
    Log::i("[BUFF_UI] ",phase," snapshot handle=",handle," timer=",timer,
           " remaining=",end && value ? end->get_int(0,value.get())-timer : 0);
    }
  }

void DirectMemory::save(Serialize& out) {
  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_PERSISTENCE_SAVE_FAILURE")!=nullptr)
    throw std::runtime_error("Injected compatibility save failure");
  logBuffProbeState("save");
  pruneFocusNpcs();
  out.setEntry("game/compatibility");
  // ponytail: a complete virtual-memory snapshot requires identical scripts and
  // mapping ABI. Bump this version when changing the native memory layout.
  out.write(uint32_t(4),scriptFingerprint,scriptVariables,scriptSymbols,ASMINT_InternalStack,musicThemePtr);
  std::vector<uint32_t> values, instances;
  for(auto& s : vm.symbols()) {
    if(s.is_member()) continue;
    const auto t = s.type();
    if(s.count()>0 && ((s.is_const() && (t==zenkit::DaedalusDataType::INT || t==zenkit::DaedalusDataType::FLOAT || t==zenkit::DaedalusDataType::STRING)) ||
                      (t==zenkit::DaedalusDataType::FUNCTION && !s.is_const())))
      values.push_back(s.index());
    if(t==zenkit::DaedalusDataType::INSTANCE && dynamic_cast<memory_instance*>(vm.find_symbol_by_index(s.index())->get_instance().get()))
      instances.push_back(s.index());
    }
  out.write(uint32_t(values.size()));
  for(auto id : values) {
    auto& s = *vm.find_symbol_by_index(id);
    out.write(id);
    for(uint16_t i=0;i<s.count();++i) {
      if(s.type()==zenkit::DaedalusDataType::STRING) out.write(s.get_string(i));
      else if(s.type()==zenkit::DaedalusDataType::FLOAT) out.write(s.get_float(i));
      else out.write(s.get_int(i));
      }
    }
  out.write(uint32_t(instances.size()));
  for(auto id : instances) {
    out.write(id);
    saveReference(out,vm.find_symbol_by_index(id)->get_instance());
    }
  mem32.save(out);
  out.write(uint32_t(scriptReferences.size()));
  for(const auto& [ref,ptr] : scriptReferences) {
    out.write(ref.second,ptr);
    saveReference(out,ref.first);
    }
  out.write(uint32_t(fontNames.size()));
  for(const auto& [handle,name] : fontNames)
    out.write(handle,name);
  out.write(uint32_t(uiViewOrder.size()));
  for(auto ptr : uiViewOrder) {
    auto it = uiViews.find(ptr);
    if(it==uiViews.end())
      throw std::runtime_error("Invalid compatibility view order");
    out.write(ptr,it->second.texture);
    }
  std::vector<std::pair<ptr32_t,std::shared_ptr<zenkit::DaedalusInstance>>> focus;
  for(const auto& [ptr,npc]:focusNpc)
    if(auto instance=npc.lock(); instance && isLiveNpc(instance))
      focus.emplace_back(ptr,std::move(instance));
  out.write(uint32_t(focus.size()));
  for(const auto& [ptr,npc]:focus) {
    out.write(ptr);
    saveReference(out,npc);
    }
  std::vector<Interactive*> locks;
  auto& world = gameScript.world();
  for(uint32_t id=0;auto* lock=world.mobsiById(id);++id)
    if(lock->lockAddress!=0 || lock->lockProgress!=0)
      locks.push_back(lock);
  out.write(uint32_t(locks.size()));
  for(auto* lock:locks)
    out.write(world.mobsiId(lock),lock->lockAddress,uint32_t(lock->lockProgress));
  }

void DirectMemory::load(Serialize& in) {
  resetMusicZone();
  if(!in.setEntry("game/compatibility")) {
    if(auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB"))
      lastMob->set_int(0);
    restoreQuestCallbacks = true; // older saves have no heap to restore
    return;
    }
  uint32_t version = 0;
  uint64_t fingerprint = 0;
  in.read(version,fingerprint);
  if((version!=1 && version!=2 && version!=3 && version!=4) || fingerprint!=scriptFingerprint)
    throw std::runtime_error("Incompatible script/compatibility snapshot");
  uint32_t variables = 0, symbols = 0;
  in.read(variables,symbols,ASMINT_InternalStack,musicThemePtr);
  if(variables!=scriptVariables || symbols!=scriptSymbols)
    throw std::runtime_error("Incompatible script memory addresses");
  uint32_t count = 0;
  in.read(count);
  if(count>vm.symbols().size()) throw std::runtime_error("Invalid compatibility symbol count");
  std::unordered_set<uint32_t> seen;
  for(uint32_t n=0;n<count;++n) {
    uint32_t id = 0;
    in.read(id);
    auto* s = vm.find_symbol_by_index(id);
    if(!seen.insert(id).second || !s || s->is_member() || s->count()==0 ||
       (!s->is_const() && s->type()!=zenkit::DaedalusDataType::FUNCTION) ||
       !(s->type()==zenkit::DaedalusDataType::INT || s->type()==zenkit::DaedalusDataType::FLOAT ||
         s->type()==zenkit::DaedalusDataType::STRING || (s->type()==zenkit::DaedalusDataType::FUNCTION && !s->is_const())))
      throw std::runtime_error("Invalid compatibility symbol");
    for(uint16_t i=0;i<s->count();++i) {
      if(s->type()==zenkit::DaedalusDataType::STRING) {
        uint32_t size = 0;
        in.read(size);
        if(size>16*1024*1024) throw std::runtime_error("Invalid compatibility string size");
        std::string v(size,'\0');
        in.readBytes(v.data(),size);
        s->set_string(v,i);
        }
      else if(s->type()==zenkit::DaedalusDataType::FLOAT) { float v; in.read(v); s->set_float(v,i); }
      else { int32_t v; in.read(v); s->set_int(v,i); }
      }
    }
  in.read(count);
  if(count>vm.symbols().size()) throw std::runtime_error("Invalid compatibility instance count");
  seen.clear();
  for(uint32_t n=0;n<count;++n) {
    uint32_t id = 0;
    in.read(id);
    auto* s = vm.find_symbol_by_index(id);
    if(!seen.insert(id).second || !s || s->is_member() || s->type()!=zenkit::DaedalusDataType::INSTANCE)
      throw std::runtime_error("Invalid compatibility instance");
    auto instance = loadReference(in);
    if(!dynamic_cast<memory_instance*>(instance.get()))
      throw std::runtime_error("Invalid compatibility instance binding");
    s->set_instance(std::move(instance));
    }
  mem32.load(in,[this](std::string_view name,uint32_t size) -> void* {
    if(name=="ASMINT_CallTarget" && size==sizeof(ASMINT_CallTarget)) return &ASMINT_CallTarget;
    return nullptr;
    });
  focusNpc.clear();
  focusNpcAddress.clear();
  ASMINT_CallTargetPtr = mem32.pinAddress("ASMINT_CallTarget");
  scriptReferences.clear();
  in.read(count);
  if(count>100000) throw std::runtime_error("Invalid compatibility reference count");
  std::unordered_set<Mem32::Type> boundTypes;
  for(uint32_t n=0;n<count;++n) {
    uint32_t id = 0, ptr = 0;
    in.read(id,ptr);
    auto context = loadReference(in);
    auto* ref = vm.find_symbol_by_index(id);
    if(!ref || !(ref->type()==zenkit::DaedalusDataType::INT || ref->type()==zenkit::DaedalusDataType::FLOAT || ref->type()==zenkit::DaedalusDataType::STRING))
      throw std::runtime_error("Invalid compatibility binding");
    const uint32_t stride = ref->type()==zenkit::DaedalusDataType::STRING ? sizeof(zString) : 4;
    const uint32_t size = ((uint32_t(ref->count())*stride+Mem32::memAlign-1)/Mem32::memAlign)*Mem32::memAlign;
    auto type = mem32.regionType(ptr,size);
    const bool unique = boundTypes.insert(type).second;
    if(type<Mem32::Type::firstScriptReference || !unique) {
      throw std::runtime_error("Invalid compatibility binding type");
      }
    bindReference(ref,context,type);
    if((context || !ref->is_member()) && scriptReferences.contains({context,id}))
      throw std::runtime_error("Duplicate compatibility reference");
    // Distinct deleted native objects can resolve to null. Keep each address
    // bound to a tombstone, including across subsequent saves.
    scriptReferences.emplace(std::make_pair(context,id),ptr);
    }
  mem32.validateCallbacks();
  uiViews.clear();
  uiViewOrder.clear();
  fontNames.clear();
  nextFontHandle = 0x10000000;
  if(version>=3) {
    in.read(count);
    if(count>10000) throw std::runtime_error("Invalid compatibility font count");
    for(uint32_t n=0;n<count;++n) {
      uint32_t handle = 0;
      std::string name;
      in.read(handle,name);
      if(handle<0x10000000 || handle>=0x80000000 || name.size()>1024 || !fontNames.emplace(handle,std::move(name)).second)
        throw std::runtime_error("Invalid compatibility font");
      nextFontHandle = std::max(nextFontHandle,handle+1);
      }
    in.read(count);
    if(count>10000) throw std::runtime_error("Invalid compatibility view count");
    for(uint32_t n=0;n<count;++n) {
      uint32_t ptr = 0;
      std::string texture;
      in.read(ptr,texture);
      if(texture.size()>1024 || !mem32.isAllocation(ptr,sizeof(zCView),"") ||
         !uiViews.emplace(ptr,UiView{std::move(texture)}).second)
        throw std::runtime_error("Invalid compatibility view");
      uiViewOrder.push_back(ptr);
      }
    if(version>=4) {
      in.read(count);
      if(count>10000) throw std::runtime_error("Invalid compatibility focus count");
      auto* vtable = vm.find_symbol_by_name("OCNPC_VTBL");
      for(uint32_t n=0;n<count;++n) {
        uint32_t ptr = 0;
        in.read(ptr);
        auto npc = std::dynamic_pointer_cast<zenkit::INpc>(loadReference(in));
        if(vtable==nullptr || npc==nullptr || !isLiveNpc(npc) || !mem32.isAllocation(ptr,4,"focused OCNPC") ||
           mem32.readInt(ptr)!=vtable->get_int() || !focusNpc.emplace(ptr,npc).second ||
           !focusNpcAddress.emplace(npc,ptr).second)
          throw std::runtime_error("Invalid compatibility focus binding");
        }
      }
    }
  if(version>=2) {
    in.read(count);
    if(count>100000) throw std::runtime_error("Invalid compatibility lock count");
    seen.clear();
    std::unordered_set<uint32_t> addresses;
    for(uint32_t n=0;n<count;++n) {
      uint32_t id=0,address=0,progress=0;
      in.read(id,address,progress);
      auto* lock = gameScript.world().mobsiById(id);
      auto* cls = vm.find_symbol_by_name("OCMOBLOCKABLE");
      if(!lock || !seen.insert(id).second || progress>lock->pickLockCode().size() ||
         (address!=0 && (!addresses.insert(address).second || !cls ||
                        !mem32.isAllocation(address,cls->class_size(),"focused OCMOBLOCKABLE"))))
        throw std::runtime_error("Invalid compatibility lock binding");
      lock->lockAddress = address;
      lock->lockProgress = progress;
      }
    }
  // v1 had no lock ownership metadata. Its old allocations remain harmless
  // script data; none are rebound to new native objects by guessing addresses.
  if(version==1)
    if(auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB"))
      lastMob->set_int(0);
  // Older UI snapshots predate native resize dispatch. Seed LeGo's missing
  // previous metrics from the saved viewport/HP convention, before tickUi
  // replaces those metrics; its normal callback then rescales existing bars.
  auto* screen = vm.find_symbol_by_name("PRINT_SCREEN");
  auto* barX = vm.find_symbol_by_name("_BAR_SCREEN_X");
  auto* barY = vm.find_symbol_by_name("_BAR_SCREEN_Y");
  auto* barScale = vm.find_symbol_by_name("_BAR_SCALING");
  auto* hpBar = mem32.deref<oCViewStatusBar>(memGame.HPBAR);
  if(auto* mode = std::getenv("OPENGOTHIC_BOSS_UI_PROBE"); mode && std::string_view(mode).starts_with("legacy-"))
    Log::i("[LEGACY_UI] version=",version," views=",uiViews.size()," fonts=",fontNames.size(),
           " screen=",screen ? screen->get_int(0) : 0,",",screen ? screen->get_int(1) : 0,
           " bar_screen=",barX ? barX->get_int() : 0,",",barY ? barY->get_int() : 0);
  if(!uiViews.empty() && screen && screen->count()>=2 && barX && barY && barScale && hpBar &&
     barX->get_int()==0 && screen->get_int(0)>0 && screen->get_int(1)>0 && hpBar->VSIZEX>0) {
    barX->set_int(screen->get_int(0));
    barY->set_int(screen->get_int(1));
    const auto unitWidth = std::max(1l,std::lround(180.f*8192.f/float(screen->get_int(0))));
    barScale->set_int(floatBitsToInt(float(hpBar->VSIZEX)/float(unitWidth)));
    }
  restoreQuestCallbacks = false;
  if(std::getenv("OPENGOTHIC_PERSISTENCE_PROBE")!=nullptr)
    persistenceProbeRoot = uint32_t(vm.find_symbol_by_name("MEM_INFOBOX.RES")->get_int());
  if(std::getenv("OPENGOTHIC_WORLD_PROBE")!=nullptr) {
    const auto root = uint32_t(vm.find_symbol_by_name("MEM_INFOBOX.RES")->get_int());
    if(mem32.isAllocation(root,56,"World transition probe") && mem32.readInt(root)==0x57545031) {
      worldProbeRecurringTimer = uint32_t(mem32.readInt(root+40));
      worldProbeRecurringDispatches = 0;
      worldProbeRecurringLastElapsed = 0;
      }
    }
  logBuffProbeState("load");
  Log::i("[COMPATIBILITY] Restored virtual heap and script bindings");
  }

void DirectMemory::probePersistence(bool finish) {
  const auto mode = std::string_view(std::getenv("OPENGOTHIC_PERSISTENCE_PROBE"));
  const bool seed = mode=="seed", completed = mode=="completed";
  auto* root = vm.find_symbol_by_name("MEM_INFOBOX.RES");
  if(!finish && seed) {
    auto ptr = mem32.alloc(64);
    auto child = mem32.alloc(8);
    mem32.writeInt(child,0x7654321);
    child = mem32.realloc(child,128);
    mem32.writeInt(ptr,0x1234abcd);
    mem32.writeInt(ptr+8,int32_t(child));
    mem32.writeInt(ptr+12,vm.find_symbol_by_name("_TIMER_PAUSED")->get_int());
    zString str = {};
    memAssignString(str,"Archolos persistent string");
    *static_cast<zString*>(mem32.derefv(ptr+32,sizeof(zString))) = str;
    auto* ref = vm.find_symbol_by_name("C_NPC.AIVAR");
    auto context = gameScript.world().player()->handlePtr();
    auto type = mem32.nextScriptReferenceType();
    bindReference(ref,context,type);
    auto mapped = mem32.alloc(uint32_t(ref->count())*4,type);
    scriptReferences.emplace(std::make_pair(context,ref->index()),mapped);
    mem32.writeInt(ptr+16,int32_t(mapped));
    // Two removed objects must retain distinct null bindings after reload.
    auto* itemValue = vm.find_symbol_by_name("C_ITEM.VALUE");
    for(uint32_t off : {20u,28u}) {
      auto* gold = vm.find_symbol_by_name("ITMI_GOLD");
      auto original = gold->get_instance();
      auto removed = vm.allocate_instance<zenkit::IItem>(gold);
      gold->set_instance(original);
      type = mem32.nextScriptReferenceType();
      bindReference(itemValue,removed,type);
      auto address = mem32.alloc(4,type);
      scriptReferences.emplace(std::make_pair(removed,itemValue->index()),address);
      mem32.writeInt(ptr+off,int32_t(address));
      }
    root->set_int(int32_t(ptr));
    }
  auto ptr = uint32_t(root->get_int());
  persistenceProbeRoot = ptr;
  if(seed && finish) {
    auto cb = vm.find_symbol_by_name("TIMER_SETPAUSE");
    mem32.writeInt(ptr+24,int32_t(gameScript.tickCount()));
    vm.call_function("FF_APPLYEXTDATAGT",int32_t(cb->index()),15000,1,int32_t(ptr));
    }
  if(mem32.readInt(ptr)!=0x1234abcd || mem32.readInt(uint32_t(mem32.readInt(ptr+8)))!=0x7654321)
    throw std::runtime_error("Persistence lost linked heap objects");
  std::string str;
  memFromString(str,*mem32.deref<zString>(ptr+32));
  if(str!="Archolos persistent string") throw std::runtime_error("Persistence lost string");
  auto* ref = vm.find_symbol_by_name("C_NPC.AIVAR");
  auto context = gameScript.world().player()->handlePtr();
  auto mapped = uint32_t(mem32.readInt(ptr+16));
  auto value = ref->get_int(99,context.get());
  if(mem32.readInt(mapped+99*4)!=value) throw std::runtime_error("Persistence lost native array read");
  mem32.writeInt(mapped+99*4,value^0x1234);
  if(ref->get_int(99,context.get())!=(value^0x1234)) throw std::runtime_error("Persistence lost native array write");
  mem32.writeInt(mapped+99*4,value);
  for(uint32_t off : {20u,28u}) {
    auto address = uint32_t(mem32.readInt(ptr+off));
    if(mem32.readInt(address)!=0) throw std::runtime_error("Persistence lost deleted-object binding");
    if(!seed) {
      mem32.writeInt(address,1234);
      if(mem32.readInt(address)!=0) throw std::runtime_error("Persistence wrote a deleted object");
      }
    }
  auto count = mem32.readInt(ptr+4);
  if(count!=((completed || (finish && !seed)) ? 1 : 0))
    throw std::runtime_error("Persistence callback count/timing mismatch");
  Log::i("[PERSISTENCE_PROBE] phase=",finish ? "finish" : "start"," count=",count);
  }

void DirectMemory::tick(uint64_t dt) {
  frameDt = dt;
  memGame.TIMESTEP = floatBitsToInt(float(dt));
  if(restoreQuestCallbacks) {
    restoreQuestCallbacks = false;
    // Legacy saves restore script globals, but lack allocated callback handles.
    // Re-register the mod's recurring quest dispatcher without rerunning world
    // startup or resetting NPC routines. Lost one-shots cannot be recovered.
    if(auto init = vm.find_symbol_by_name("INIT_QUESTSEVENTSMANAGER")) {
      vm.call_function("MEM_InitAll");
      vm.call_function(init);
      }
    // LeGo registers this recurring animation update during its own load hook.
    // Without it, newly created fades never finish or call their continuation.
    if(auto anim = vm.find_symbol_by_name("_ANIM8_FFLOOP")) {
      if(vm.find_symbol_by_name("FF_APPLYONCEGT")!=nullptr)
        vm.call_function("FF_APPLYONCEGT",int32_t(anim->index()));
      }
    }

  //TODO: propper hook-engine
  if(auto* sym = vm.find_symbol_by_name("_FF_Hook")) {
    vm.call_function(sym);
    }
  frameDt = 0;

  tickUi(dt);
  updateMusic();
  }

void DirectMemory::setupKmLibFunctions() {
  if(vm.find_symbol_by_name("KMLIB_INITIALIZEGAMESTART")==nullptr)
    return;
  // Native initialization owns audio, menus, console and save files. The original
  // initializers patch Windows code, start platform clients/crash reporting, and
  // look up version symbols; INIT_ALWAYS itself performs the script migrations.
  for(auto name : {"KMLIB_INITIALIZEMENU", "KMLIB_INITIALIZEGAMESTART"})
    if(vm.find_symbol_by_name(name)!=nullptr)
      vm.override_function(name, []() {});

  if(vm.find_symbol_by_name("KMLIB_INITIALIZEALWAYS")!=nullptr)
    vm.override_function("KMLIB_INITIALIZEALWAYS", [this]() { resetMusicZone(); });

  const auto validKey = [](std::string_view key) {
    return !key.empty() && key.size()<=128 &&
           key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")==std::string_view::npos;
    };
  // Local profile progress, deliberately outside saves: loading an earlier save
  // must not roll back achievement counters. No Steam/GOG/Discord publishing.
  if(vm.find_symbol_by_name("GAMESERVICES_GETSTAT")!=nullptr)
    vm.override_function("GAMESERVICES_GETSTAT", [validKey](std::string_view name) {
      return validKey(name) ? Gothic::settingsGetI("KMLIB_STATS",name) : -1;
      });
  if(vm.find_symbol_by_name("GAMESERVICES_INCREMENTSTAT")!=nullptr)
    vm.override_function("GAMESERVICES_INCREMENTSTAT", [validKey](std::string_view name, int amount) {
      if(!validKey(name))
        return;
      const int64_t value = int64_t(Gothic::settingsGetI("KMLIB_STATS",name))+amount;
      Gothic::settingsSetI("KMLIB_STATS",name,int(std::clamp<int64_t>(value,0,std::numeric_limits<int32_t>::max())));
      Gothic::inst().flushSettings();
      });
  if(vm.find_symbol_by_name("GAMESERVICES_UNLOCKACHIEVEMENT")!=nullptr)
    vm.override_function("GAMESERVICES_UNLOCKACHIEVEMENT", [validKey](std::string_view name) {
      if(!validKey(name) || Gothic::settingsGetI("KMLIB_ACHIEVEMENTS",name)!=0)
        return;
      Gothic::settingsSetI("KMLIB_ACHIEVEMENTS",name,1);
      Gothic::inst().flushSettings();
      });
  }

void DirectMemory::setupMusicFunctions() {
  if(vm.find_symbol_by_name("KMLIB_INITIALIZEGAMESTART")==nullptr)
    return;
  for(auto name : {"MUSIC_CURRENTOVERRIDE", "MUSIC_OVERRIDETRACK", "MUSIC_DISABLEOVERRIDE",
                   "MUSICTRACK.FILENAME", "MUSICTRACK.LOOPOFFSETDURATION",
                   "MUSICTRACK.FADEOUTSLIDEDURATION", "MUSICTRACK.FADEINSLIDEDURATION",
                   "MUSICZONE.DAY", "MUSICZONE.NIGHT", "MUSICZONE.DAYFIGHT", "MUSICZONE.NIGHTFIGHT"})
    if(vm.find_symbol_by_name(name)==nullptr)
      return;
  musicOverride = vm.find_symbol_by_name("MUSIC_CURRENTOVERRIDE");
  if(vm.find_symbol_by_name("ONZONEMUSICCHANGEDHOOK")!=nullptr && vm.find_symbol_by_name("EDX")!=nullptr)
    musicThemePtr = mem32.pin(&musicThemeString,sizeof(musicThemeString),"music zone theme");
  vm.register_as_opaque("MUSICTRACK");
  vm.register_as_opaque("MUSICZONE");
  // Keep the script global authoritative: it is already serialized in saves.
  // Playback happens on the world tick, never on the asynchronous load thread.
  vm.override_function("MUSIC_OVERRIDETRACK", [this](int track) { musicOverride->set_int(track); });
  vm.override_function("MUSIC_DISABLEOVERRIDE", [this]() { musicOverride->set_int(0); });
  }

bool DirectMemory::setMusicZone(std::string_view zone, uint8_t tags) {
  if(musicOverride==nullptr)
    return false;
  auto sym = vm.find_symbol_by_name(zone);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
    return false;
  auto cls = sym;
  while(cls!=nullptr && cls->type()!=zenkit::DaedalusDataType::CLASS)
    cls = vm.find_symbol_by_index(cls->parent());
  if(cls==nullptr || cls->name()!="MUSICZONE")
    return false;
  if(musicZoneName!=zone) {
    musicZone = vm.init_opaque_instance(sym);
    musicZoneName = zone;
    }
  musicTags = tags;
  notifyMusicZone();
  updateMusic();
  return true;
  }

void DirectMemory::resetMusicZone() {
  musicZone.reset();
  musicZoneName.clear();
  musicThemeName.clear();
  musicTrack = 0;
  }

void DirectMemory::notifyMusicZone() {
  if(musicThemePtr==0)
    return;
  // This hook observes the original engine theme name, including time/combat
  // suffixes, even while audio is muted or a story track overrides the zone.
  std::string name = musicZoneName + ((musicTags&GameMusic::Ngt)!=0 ? "_NGT_" : "_DAY_");
  name += (musicTags&GameMusic::Fgt)!=0 ? "FGT" : (musicTags&GameMusic::Thr)!=0 ? "THR" : "STD";
  if(name==musicThemeName)
    return;
  musicThemeName = name;
  memAssignString(musicThemeString,name);
  auto& edx = *vm.find_symbol_by_name("EDX");
  struct RestoreRegister {
    zenkit::DaedalusSymbol& symbol;
    int32_t value;
    ~RestoreRegister() { symbol.set_int(value); }
    } restore{edx,edx.get_int()};
  edx.set_int(int32_t(musicThemePtr));
  vm.call_function("ONZONEMUSICCHANGEDHOOK");
  }

void DirectMemory::updateMusic() {
  if(musicOverride==nullptr)
    return;
  int32_t track = musicOverride->get_int();
  if(track<=0 && musicZone!=nullptr) {
    const bool night = (musicTags&GameMusic::Ngt)!=0;
    const bool fight = (musicTags&(GameMusic::Fgt|GameMusic::Thr))!=0;
    auto field = fight ? (night ? "MUSICZONE.NIGHTFIGHT" : "MUSICZONE.DAYFIGHT") :
                         (night ? "MUSICZONE.NIGHT" : "MUSICZONE.DAY");
    track = vm.find_symbol_by_name(field)->get_int(0,musicZone.get());
    }
  if(track<=0)
    return;
  auto sym = vm.find_symbol_by_index(uint32_t(track));
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INSTANCE)
    return;
  auto cls = sym;
  while(cls!=nullptr && cls->type()!=zenkit::DaedalusDataType::CLASS)
    cls = vm.find_symbol_by_index(cls->parent());
  if(cls==nullptr || cls->name()!="MUSICTRACK")
    return;
  auto instance = sym->get_instance();
  if(instance==nullptr)
    instance = vm.init_opaque_instance(sym);
  GameMusic::FileTheme theme;
  theme.file = vm.find_symbol_by_name("MUSICTRACK.FILENAME")->get_string(0,instance.get());
  if(theme.file.empty())
    return;
  theme.file += ".ogg";
  const auto duration = [&](const char* name) {
    return uint64_t(std::max(0,vm.find_symbol_by_name(name)->get_int(0,instance.get())));
    };
  theme.loopOverlap = duration("MUSICTRACK.LOOPOFFSETDURATION");
  theme.fadeIn = duration("MUSICTRACK.FADEINSLIDEDURATION");
  theme.fadeOut = duration("MUSICTRACK.FADEOUTSLIDEDURATION");
  if(track!=musicTrack && std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_MUSIC_PROBE")!=nullptr)
    Log::i("[MUSIC_PROBE] select zone=",musicZoneName," tags=",int(musicTags),
           " override=",musicOverride->get_int()," track=",sym->name()," file=",theme.file);
  musicTrack = track;
  GameMusic::inst().setMusic(theme);
  }

void DirectMemory::eventPlayAni(std::string_view ani) {
  // oCNPC__EV_PlayAni -> _AI_FUNCTION_EVENT
  //TODO: propper hook-engine
  if(ani.find("CALL ")!=0)
    return;
  //Log::d("LeGo::eventPlayAni: ", ani);
#if 0

  if(auto* sym = vm.find_symbol_by_name("_AI_FUNCTION_EVENT")) {
    vm.push_string(ani);
    vm.call_function(sym);
    // vm.unsafe_jump(sym->address()+0x1b); // jump over asm-related stuff
    return;
    }
  return;
#endif

  auto toInt = [](std::string_view ss) {
    int32_t i  = 0;
    auto err = std::from_chars(ss.data(), ss.data()+ss.size(), i).ec;
    if(err!=std::errc())
      return 0;
    return i;
    };

  auto toStr = [](std::string_view ss) {
    std::string ret;
    ret.reserve(ss.size());
    for(size_t i=0; i<ss.size(); ++i) {
      if(ss[i]=='\\' && i+1<ss.size()) {
        ++i;
        if(ss[i]=='_') {
          ret.push_back(' ');
          continue;
          }
        }
      ret.push_back(ss[i]);
      }
    return ret;
    };

  auto v   = ani.substr(5);
  auto tok = v.substr(0, v.find(' '));
  v = v.substr(v.find(' ')+1);

  std::string_view args[10];
  for(int i=0; i<10; ++i) {
    size_t n = v.find(' ');
    args[i] = v.substr(0, n);
    if(n==std::string_view::npos)
      break;
    v = v.substr(n+1);
    }

  if(tok.size()>0 && std::isdigit(tok[0])) {
    auto fncID = toInt(args[0]);
    if(auto* sym = vm.find_symbol_by_index(uint32_t(fncID))) {
      vm.call_function(sym);
      }
    return;
    }
  if(tok=="I") {
    auto fncID = toInt(args[1]);
    if(auto* sym = vm.find_symbol_by_index(uint32_t(fncID))) {
      vm.call_function(sym, toInt(args[0]));
      }
    return;
    }
  if(tok=="S") {
    auto fncID = toInt(args[1]);
    if(auto* sym = vm.find_symbol_by_index(uint32_t(fncID))) {
      //NOTE: need support in ZenKit, to use call_function with string&&
      std::string arg0 = toStr(args[0]);
      vm.call_function(sym, std::string_view(arg0));
      }
    return;
    }
  if(tok=="SS") {
    auto fncID = toInt(args[2]);
    if(auto* sym = vm.find_symbol_by_index(uint32_t(fncID))) {
      std::string arg0 = toStr(args[0]);
      std::string arg1 = toStr(args[1]);
      vm.call_function(sym, std::string_view(arg0), std::string_view(arg1));
      }
    return;
    }
  if(tok=="NSII") {
    auto fncID = toInt(args[4]);
    auto npcID = toInt(args[0]);
    auto npcS  = vm.find_symbol_by_index(uint32_t(npcID));
    auto inst  = (npcS!=nullptr && npcS->type()==zenkit::DaedalusDataType::INSTANCE) ? npcS->get_instance() : nullptr;
    if(auto* sym = vm.find_symbol_by_index(uint32_t(fncID))) {
      (void)sym;
      //NOTE: need support in ZenKit, to use call_function for externals
      // vm.call_function(sym, inst, args[1], float(toInt(args[2])), float(toInt(args[3])));
      }
    return;
    }

  Log::d("Ikarus: skip ai-call: ", ani);
  }

void DirectMemory::setupFunctionTable() {
  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(i.address()==0)
      continue;
    symbolsByAddress.push_back(&i);
    }

  std::sort(symbolsByAddress.begin(), symbolsByAddress.end(), [](const zenkit::DaedalusSymbol* l, const zenkit::DaedalusSymbol* r){
    return l->address() < r->address();
    });
  }

void DirectMemory::setupIkarusLoops() {
  // ## Traps
  if(auto end = vm.find_symbol_by_name("END")) {
    end->set_access_trap_enable(true);
    }
  if(auto break_ = vm.find_symbol_by_name("BREAK")) {
    break_->set_access_trap_enable(true);
    }
  if(auto continue_ = vm.find_symbol_by_name("CONTINUE")) {
    continue_->set_access_trap_enable(true);
    }

  auto end       = vm.find_symbol_by_name("END");
  auto rep       = vm.find_symbol_by_name("REPEAT");
  auto whl       = vm.find_symbol_by_name("WHILE");
  auto continue_ = vm.find_symbol_by_name("CONTINUE");

  uint32_t endIdx  = (end!=nullptr       ? end->index()       : uint32_t(-1));
  uint32_t cntIdx  = (continue_!=nullptr ? continue_->index() : uint32_t(-1));
  uint32_t repAddr = (rep!=nullptr       ? rep->address()     : uint32_t(-1));
  uint32_t whlAddr = (whl!=nullptr       ? whl->address()     : uint32_t(-1));

  // auto cont = vm.find_symbol_by_name("CONTINUE");

  // ## Control-flow ##
  vm.override_function("repeat",   [this](zenkit::DaedalusVm& vm) { return repeat(vm);    });
  vm.override_function("while",    [this](zenkit::DaedalusVm& vm) { return while_(vm);    });
  vm.override_function("mem_goto", [this](zenkit::DaedalusVm& vm) { return mem_goto(vm);  });
  vm.register_access_trap([this](zenkit::DaedalusSymbol& i) { return loop_trap(&i); });

  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    }

  struct LoopDecr {
    uint32_t pc      = 0;
    uint32_t backJmp = 0;
    };

  std::vector<LoopDecr> loopStk;
  for(uint32_t i=0; i<vm.size();) {
    const auto pc = i;
    auto instr = vm.instruction_at(i);
    i += instr.size;

    if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==endIdx) {
      auto loop = loopStk.back();
      loopBacktrack[loop.pc] = i; // loop exit
      loopBacktrack[pc]      = loop.backJmp;
      loopStk.pop_back();
      }
    else if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==cntIdx) {
      if(!loopStk.empty()) {
        auto func = findSymbolByAddress(pc); (void)func;
        auto loop = loopStk.back();
        loopBacktrack[pc] = loop.pc;
        }
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL && (instr.address==repAddr || instr.address==whlAddr)) {
      LoopDecr d;
      if(instr.address==whlAddr) {
        d.backJmp = traceBackExpression(vm, pc);
        d.pc      = pc;
        }
      else if(instr.address==repAddr) {
        d.pc      = pc;
        d.backJmp = pc;
        }
      loopStk.push_back(d);
      }
    }
  }

void DirectMemory::setupEngineMemory() {
  mem32.setCallbackR(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::acquire); });
  mem32.setCallbackW(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::release); });

  mem32.setCallbackR(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::release);
    });
  mem32.setCallbackR(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::release);
    });

  const ptr32_t BAD_BUILTIN_PTR                = 0xBAD40000;
  const ptr32_t GothicFirstInstructionAddress  = 4198400;
  const ptr32_t MEMINT_oGame_Pointer_Address   = 11208836; //0xAB0884
  const ptr32_t MEMINT_zTimer_Address          = 10073044; //0x99B3D4
  const ptr32_t MEMINT_gameMan_Pointer_Address = 9185624;  //0x8C2958
  const ptr32_t ZFACTORY                       = 9276912;
  const ptr32_t ContentParserAddress           = 11223232;
  const ptr32_t ZFONTMAN                       = 11221460;
  const ptr32_t OCNPCFOCUS__FOCUSLIST_G2       = 11208440;

  mem32.pin(&versionHint,   GothicFirstInstructionAddress,  4, "MEMINT_ReportVersionCheck");
  mem32.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,   4, "oGame*");
  mem32.pin(&gameman_Ptr,   MEMINT_gameMan_Pointer_Address, 4, "GameMan*");
  mem32.pin(&zTimer,        MEMINT_zTimer_Address,          sizeof(zTimer), "zTimer");
  mem32.pin(&zFactory_Ptr,  ZFACTORY,                       4, "zFactory*");
  mem32.pin(&fontMan_Ptr,   ZFONTMAN,                       4, "zCFontMan*");
  mem32.pin(ContentParserAddress, sizeof(zCParser), Mem32::Type::zCParser);
  mem32.pin(&focusList, OCNPCFOCUS__FOCUSLIST_G2, sizeof(focusList), "OCNPCFOCUS__FOCUSLIST_G2");

  const ptr32_t INGAME_MENU_INSTANCE = 8980576;
  mem32.pin(menuName, INGAME_MENU_INSTANCE, sizeof(menuName)-1, "MENU_NAME");
  // Mods can declare a separate pause menu; legacy initialization may fail to
  // apply the runtime patch. Keep the mapped buffer writable by scripts.
  std::string_view initialMenu = ::menuMain;
  if(auto sym = vm.find_symbol_by_name("INGAME_MENU_INSTANCE")) {
    if(sym->type()==zenkit::DaedalusDataType::STRING && sym->is_const() && !sym->is_member()) {
      const auto& name = sym->get_string();
      if(!name.empty() && name.size()<sizeof(menuName))
        initialMenu = name;
      }
    }
  std::memcpy(menuName, initialMenu.data(), initialMenu.size());

  const ptr32_t ZERRPTR = 9231568;
  mem32.alloc(ZERRPTR, sizeof(Compatibility::zError));

  const ptr32_t LODENABLED = 8596020;
  mem32.alloc(LODENABLED, 4, "LODENABLED"); // can always ignore that

  const ptr32_t AMBIENTVOBSENABLED = 9079488;
  mem32.alloc(AMBIENTVOBSENABLED, 4, "AMBIENTVOBSENABLED");

  const ptr32_t GAME_HOLDTIME_ADDRESS = 11208840;
  mem32.alloc(GAME_HOLDTIME_ADDRESS, 4, "GAME_HOLDTIME_ADDRESS"); // need to investigate how exactly it affects game

  if(auto p = mem32.deref<std::array<ptr32_t,6>>(OCNPCFOCUS__FOCUSLIST_G2)) {
    gameScript.focusMage();
    (*p)[5] = BAD_BUILTIN_PTR; //TODO: pick-lock spells
    }

  // Trialog: need to implement reinterpret_cast to avoid in script-exception
  const ptr32_t SPAWN_INSERTRANGE = 9153744;
  mem32.pin(&spawnRange, SPAWN_INSERTRANGE, sizeof(spawnRange), "spawnRange");

  // Storage for local variables, so script may address them thru pointers
  scriptVariables = mem32.alloc(uint32_t(vm.symbols().size() * sizeof(ScriptVar)),    Mem32::Type::zCParser_variables);
  scriptSymbols   = mem32.alloc(uint32_t(vm.symbols().size() * sizeof(zCPar_Symbol)), Mem32::Type::zCPar_Symbol);

  // Built-in data without assumed address
  oGame_Pointer = mem32.pin(&memGame, sizeof(memGame), "oGame");
  memGame._ZCSESSION_WORLD = mem32.alloc(sizeof(oWorld));
  memGame.WLDTIMER         = BAD_BUILTIN_PTR;
  memGame.TIMESTEP         = 1; // used as boolean in anim8

  auto& mem_world = *mem32.deref<oWorld>(memGame._ZCSESSION_WORLD);
  mem_world.WAYNET       = BAD_BUILTIN_PTR; //TODO: add implement some proxy to waynet
  mem_world.VOBLIST_NPCS = 0; //BAD_BUILTIN_PTR; // zCListSort*

  gameman_Ptr = mem32.alloc(sizeof(GameMgr));
  if(auto v = vm.find_symbol_by_name("CurrSymbolTableLength")) {
    v->set_int(int(vm.symbols().size()));
    }

  fontMan_Ptr = mem32.alloc(sizeof(zCFontMan));

  // ## UI data
  memGame.HPBAR    = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.MANABAR  = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.FOCUSBAR = mem32.alloc(sizeof(oCViewStatusBar));

  memGame._ZCSESSION_VIEWPORT = mem32.alloc(sizeof(zCView));
  memGame.ARRAY_VIEW[0] = mem32.alloc(sizeof(zCView));

  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->VALUE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->VALUE_BAR = mem32.alloc(sizeof(zCView));
  }

void DirectMemory::setupEngineText() {
  // pin functions - CoM and some other mods rewriting assembly for them
  const uint32_t OCNPC__ENABLE_EQUIPBESTWEAPONS = 7626662;
  mem32.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");

  const uint32_t OCNPC__GETNEXTENEMY = 7556941;
  mem32.alloc(OCNPC__GETNEXTENEMY, 48, "OCNPC__GETNEXTENEMY");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE = 7378665;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE, 5, ".text");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP = 7378700;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP, 5, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18 = 7384908;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR_SP18 = 7386812;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR = 7384900;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR = 7386805;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR, 5, ".text");

  const int OCNPCCONTAINER__HANDLEEVENT_ISEMPTY = 7387581;
  mem32.alloc(OCNPCCONTAINER__HANDLEEVENT_ISEMPTY, 5, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ = 7402077;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ, 4, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON = 7402065;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON, 8, ".text");

  const int OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM = 6935573;
  mem32.alloc(OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM, 8, ".text");

  // unknown, code segment used by INIT_RESTOREMOBFIRESTATES in CoM
  const int INIT_RESTOREMOBFIRESTATES_P0 = 7482368;
  const int INIT_RESTOREMOBFIRESTATES_P1 = 7482688;
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P0, 4, ".text");
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P1, 4, ".text");
  }

zenkit::DaedalusNakedCall DirectMemory::repeat(zenkit::DaedalusVm& vm) {
  const int               len = vm.pop_int();
  zenkit::DaedalusSymbol* i   = std::get<zenkit::DaedalusSymbol*>(vm.pop_reference());

  auto rp = vm.instruction_at(vm.pc());
  if(len==0 || i==nullptr) {
    auto jmp = loopBacktrack[vm.pc()];
    vm.unsafe_jump(jmp-rp.size);
    return zenkit::DaedalusNakedCall();
    }
  // Log::i("repeat: ", i->get_int(), " < ", len);
  auto& pl = loopPayload[vm.pc()];
  pl.i   = i;
  pl.len = len;
  i->set_int(0);
  if(len>1024)
    Log::d("");
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::while_(zenkit::DaedalusVm& vm) {
  const int cond = vm.pop_int();
  // Log::i("while: ", cond);
  if(cond!=0) {
    return zenkit::DaedalusNakedCall();
    }
  auto rp  = vm.instruction_at(vm.pc());
  auto jmp = loopBacktrack[vm.pc()];
  vm.unsafe_jump(jmp-rp.size);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::mem_goto(zenkit::DaedalusVm& vm) {
  const int idx = vm.pop_int();

  auto at   = vm.pc();
  auto func = findSymbolByAddress(at);
  if(func==nullptr) {
    Log::e("Ikarus: invalid vm state");
    return zenkit::DaedalusNakedCall();
    }

  auto     rp      = vm.instruction_at(at);
  auto     label   = vm.find_symbol_by_name("mem_label");
  uint32_t lblAddr = (label!=nullptr ? label->address() : uint32_t(-1));
  uint32_t pc      = func->address();

  for(uint32_t i=0, prev=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    if(instr.op==zenkit::DaedalusOpcode::BL && instr.address==lblAddr) {
      auto lId = vm.instruction_at(prev);
      if(lId.immediate==idx) {
        vm.unsafe_jump(pc + instr.size - rp.size);
        return zenkit::DaedalusNakedCall();
        }
      }
    prev = pc;
    pc += instr.size;
    }

  Log::e("Ikarus: invalid goto");
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::loop_trap(zenkit::DaedalusSymbol* i) {
  auto instr = vm.instruction_at(vm.pc());
  if(instr.op != zenkit::DaedalusOpcode::PUSHV)
    return; // Ikarus keywords are always use pushv

  auto end = vm.find_symbol_by_name("END");
  if(end!=nullptr && instr.symbol==end->index()) {
    auto bt = loopBacktrack[vm.pc()];

    if(loopPayload.find(bt)==loopPayload.end()) {
      // while
      vm.unsafe_jump(bt-instr.size);
      return;
      }

    auto& pl = loopPayload[bt];
    if(pl.i==nullptr) {
      Log::e("bad loop end");
      assert(0);
      return;
      }
    const int32_t i = pl.i->get_int();
    pl.i->set_int(i+1);
    if(i+1 < pl.len) {
      const uint32_t BLsize = 5;
      vm.unsafe_jump(bt-instr.size+BLsize);
      } else {
      loopPayload.erase(bt);
      }
    return;
    }

  if(loopBacktrack.find(vm.pc())!=loopBacktrack.end()) {
    Log::e("bad loop trap");
    assert(0);
    }
  }

uint32_t DirectMemory::traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc) {
  /*
   NOTE: can be expression based 'while';
   while(var){}
     PUSHV @var
     BL @while

   while(i < l) {}
     PUSHV @l
     PUSHV @i
     LT
     BL

    handles by MEMINT_TraceParameter in ikarus
  */

  auto func = findSymbolByAddress(pc);
  if(func==nullptr)
    return uint32_t(-1); //error

  // if(func->name()=="LIST_END")
  //   Log::d("");

  std::vector<zenkit::DaedalusInstruction> icodes;
  for(uint32_t i=func->address(); i<pc && i<vm.size();) {
    auto instr = vm.instruction_at(i);
    i += instr.size;
    icodes.push_back(instr);
    }

  if(icodes.empty())
    return uint32_t(-1); //error

  int paramsNeeded = 1;
  int instancesNeed = 0;
  size_t at = icodes.size();
  while((paramsNeeded>0 || instancesNeed>0) && at>0) {
    --at;
    const auto instr = icodes[at];
    if(zenkit::DaedalusOpcode::ADD <= instr.op && instr.op <= zenkit::DaedalusOpcode::DIVMOVI)
      paramsNeeded += 1;
    else if(instr.op==zenkit::DaedalusOpcode::PUSHI) {
      paramsNeeded -= 1;
      }
    else if(instr.op==zenkit::DaedalusOpcode::PUSHV || instr.op==zenkit::DaedalusOpcode::PUSHVI) {
      auto* sym = vm.find_symbol_by_index(instr.symbol);
      if(sym!=nullptr && sym->is_member())
        instancesNeed = 1; // need instance to be set
      paramsNeeded -= 1;
      }
    else if(instr.op==zenkit::DaedalusOpcode::GMOVI) {
      instancesNeed = 0;
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL) {
      auto sym = vm.find_symbol_by_address(instr.address);
      if(sym==nullptr)
        return uint32_t(-1); //error

      paramsNeeded += sym->count();
      if(sym->has_return())
        paramsNeeded -= 1;
      }
    else {
      // non handled
      return uint32_t(-1); //error
      }
    }

  if(paramsNeeded!=0)
    return uint32_t(-1); //error

  pc = func->address();
  for(uint32_t i=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    pc += instr.size;
    }

  return pc;
  }

const zenkit::DaedalusSymbol* DirectMemory::findSymbolByAddress(uint32_t addr) {
  auto fn = std::lower_bound(symbolsByAddress.begin(), symbolsByAddress.end(), addr, [](const zenkit::DaedalusSymbol* l, uint32_t r){
    return l->address()<r;
    });
  //auto id = std::distance(symbolsByAddress.begin(), fn); (void)id;
  if(fn==symbolsByAddress.end())
    return symbolsByAddress.back();
  if((*fn)->address()==addr)
    return *fn;
  if(fn==symbolsByAddress.begin())
    return nullptr;
  --fn;
  return *fn;
  }

std::string_view DirectMemory::demangleAddress(uint32_t addr){
  for(auto& i:vm.symbols()) {
    if(!i.is_const() || i.is_member())
      continue;
    if(i.type()!=zenkit::DaedalusDataType::INT)// && i.type() != zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(ptr32_t(i.get_int())!=addr)
      continue;
    return i.name();
    }
  return "";
  }

std::string_view DirectMemory::menuMain() const {
  return menuName;
  }

void DirectMemory::memoryCallback(zCParser& p, std::memory_order ord) {
  if(ord==std::memory_order::acquire) {
    if(p.symtab_table.ptr==0) {
      // NOTE: initialize once
      // currSymbolTableAddress / currSymbolTableLength
      p.symtab_table.numInArray = int32_t(vm.symbols().size());
      p.symtab_table.numAlloc   = int32_t(nextPot(uint32_t(p.symtab_table.numInArray)));
      p.symtab_table.ptr        = mem32.realloc(p.symtab_table.ptr, uint32_t(p.symtab_table.numAlloc)*uint32_t(sizeof(uint32_t)));      
      auto v = mem32.deref<ptr32_t>(p.symtab_table.ptr);
      for(int32_t i=0; i<p.symtab_table.numInArray; ++i) {
        v[i] = scriptSymbols + uint32_t(i)*uint32_t(sizeof(zCPar_Symbol));
        }

      // TODO: currSortedSymbolTableAddress
      // TODO: currParserStackAddress
      }

    p.stack_stack = 0; //TODO: map insteructions into mem32?

    auto trap = vm.instruction_at(vm.pc());
    p.stack_stackptr = vm.pc() + trap.size;
    }
  else {
    auto instr = vm.instruction_at(vm.pc());

    auto src = findSymbolByAddress(vm.pc() - instr.size);
    auto dst = findSymbolByAddress(p.stack_stackptr);

    if(src!=dst || dst==nullptr) {
      if(src!=nullptr && dst!=nullptr)
        Log::e("FIXME: unsafe jump: `\"", src->name(), "\" -> \"", dst->name(), "\""); else
        Log::e("FIXME: unsafe jump!");
      return;
      }
    vm.unsafe_jump(p.stack_stackptr - instr.size);
    }
  }

void DirectMemory::memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::d("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  auto& str = v.data;
  auto& sym = *vm.find_symbol_by_index(index);
  if(sym.is_member()) {
    Log::e("Ikarus: accessing member symbol (\"", sym.name(), "\")");
    return;
    }

  switch (sym.type()) {
    case zenkit::DaedalusDataType::FLOAT: {
      if(ord==std::memory_order::release) {
        //TODO
        Log::e("Ikarus: unable to write to mapped symbol (\"", sym.name(), "\")");
        } else {
        auto val = sym.get_float();
        str._VTBL = floatBitsToInt(val); // first 4 bytes
        Log::d("VAR: ", sym.name(), " -> ", val);
        }
      break;
      }
    case zenkit::DaedalusDataType::INT: {
      if(ord==std::memory_order::release) {
        sym.set_int(str._VTBL);
        } else {
        auto val = sym.get_int();
        str._VTBL = val; // first 4 bytes
        }
      break;
      }
    case zenkit::DaedalusDataType::STRING: {
      if(ord==std::memory_order::release) {
        //NOTE: since string is composite object, mixing reads and writes can cause bugs
        /*
        std::string dst;
        memFromString(dst, str);
        sym.set_string(dst);
        Log::d("VAR: ", sym.name(), " {", str.ptr, "} <- ", dst);
        */
        Log::e("Ikarus: unable to write to mapped symbol (\"", sym.name(), "\")");
        } else {
        auto& cstr = sym.get_string();
        memAssignString(str, cstr);
        // Log::d("VAR: ", sym.name(), " {", str.ptr, "} -> ", chr);
        }
      break;
      }
    default:
      Log::e("Ikarus: unable to map symbol (\"", sym.name(), "\") to virtual memory");
      break;
    }
  }

void DirectMemory::memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::e("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  if(ord!=std::memory_order::acquire) {
    auto& sym = *vm.find_symbol_by_index(index);
    Log::e("ikarus: write to symbol ", sym.name()," table is not implemented");
    return;
    }

  auto symBitfield = [](zenkit::DaedalusSymbol& sym) {
    int32_t flags = 0;
    if(sym.is_const())
      flags |= zenkit::DaedalusSymbolFlag::CONST;
    if(sym.has_return())
      flags |= zenkit::DaedalusSymbolFlag::RETURN;
    if(sym.is_member())
      flags |= zenkit::DaedalusSymbolFlag::MEMBER;
    if(sym.is_external())
      flags |= zenkit::DaedalusSymbolFlag::EXTERNAL;
    if(sym.is_merged())
      flags |= zenkit::DaedalusSymbolFlag::MERGED;

    int32_t bitfield = 0;
    bitfield |= (sym.count() & 0xFFF);       // 12 bits
    bitfield |= (int32_t(sym.type()) << 12); // 4 bits
    bitfield |= (flags << 16);               // 6 bits
    return bitfield;
    };

  auto symOffset = [](zenkit::DaedalusSymbol& sym) {
    if(sym.is_member()) {
      return int32_t(sym.offset_as_member());
      }
    else if(sym.type()==zenkit::DaedalusDataType::CLASS) {
      // see 'create' in LeGo
      return int32_t(sym.class_size());
      }
    else if(sym.type()==zenkit::DaedalusDataType::FUNCTION) {
      // return int32_t(sym.address());
      return int32_t(sym.rtype());
      }
     // TODO: pointer to value
    return 0;
    };

  auto symContent = [](zenkit::DaedalusSymbol& sym) {
    switch (sym.type()) {
      case zenkit::DaedalusDataType::VOID:
        return 0;
      case zenkit::DaedalusDataType::FLOAT:
        return floatBitsToInt(sym.get_float());
      case zenkit::DaedalusDataType::INT:
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::FUNCTION:
        if(sym.is_const())
          return int32_t(sym.address());
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::STRING:
      case zenkit::DaedalusDataType::CLASS:
      case zenkit::DaedalusDataType::PROTOTYPE:
      case zenkit::DaedalusDataType::INSTANCE:
        //TODO
        return 0;
      }
    return 0;
    };

  auto& sym = *vm.find_symbol_by_index(index);
  memAssignString(s.name, sym.name());
  s.next     = scriptSymbols + uint32_t(index+1)*uint32_t(sizeof(zCPar_Symbol)); // ???
  //---
  s.content  = symContent(sym);
  s.offset   = symOffset(sym);
  //---
  s.bitfield = symBitfield(sym);
  s.filenr   = int32_t(sym.file_index());
  s.line     = int32_t(sym.line_start());
  s.line_anz = int32_t(sym.line_count());
  s.pos_beg  = int32_t(sym.char_start());
  s.pos_anz  = int32_t(sym.char_count());
  //---
  s.parent   = scriptSymbols + uint32_t(sym.parent())*uint32_t(sizeof(zCPar_Symbol));

  // other flags are internal
  // Log::d("Ikarus: accessing symbol (\"", sym.name(), "\")");
  }

void DirectMemory::memAssignString(zString& str, std::string_view cstr) {
  str.ptr = mem32.realloc(str.ptr, uint32_t(cstr.size()+1));
  str.len = int32_t(cstr.size());

  auto* chr  = reinterpret_cast<char*>(mem32.derefv(str.ptr, uint32_t(str.len)));
  std::memcpy(chr, cstr.data(), cstr.length());
  chr[str.len] = '\0';
  }

void DirectMemory::memFromString(std::string& dst, const zString& str) {
  if(str.len<=0) {
    dst.clear();
    return;
    }

  if(const char* chr = reinterpret_cast<const char*>(mem32.deref(str.ptr, uint32_t(str.len)))) {
    dst.resize(size_t(str.len));
    std::memcpy(dst.data(), chr, dst.size());
    }
  }


void DirectMemory::memPrintstacktraceImplementation() {
  static bool enable = true;
  if(!enable)
    return;
  Log::e("[start of stacktrace]");
  vm.print_stack_trace();
  Log::e("[end of stacktrace]");
  }

void DirectMemory::memSendToSpy(int cat, std::string_view msg) {
  Log::d("[zpy]: ", msg);
  }

void DirectMemory::memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func) {
  auto* sf  = func.value;
  auto* sd  = dest.value;
  if(sd->name()=="MEM_SENDTOSPY")
    return;
  if(sd->name()=="MEM_PRINTSTACKTRACE")
    return;
  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());
  }

void DirectMemory::ASMINT_Init() {
  const int ASMINT_InternalStackSize = 1024;

  if(!ASMINT_InternalStack) {
    ASMINT_InternalStack = mem32.alloc(4 * ASMINT_InternalStackSize);
    }
  if(auto sym = vm.find_symbol_by_name("ASMINT_InternalStack")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(ASMINT_InternalStack));
    }

  if(!ASMINT_CallTargetPtr)
    ASMINT_CallTargetPtr = mem32.pin(&ASMINT_CallTarget, sizeof(ASMINT_CallTarget), "ASMINT_CallTarget");
  if(auto sym = vm.find_symbol_by_name("ASMINT_CallTarget")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(ASMINT_CallTargetPtr));
    }
  }

void DirectMemory::ASMINT_CallMyExternal() {
  auto mem = mem32.deref(ASMINT_CallTarget);
  auto ins = reinterpret_cast<const uint8_t*>(std::get<0>(mem));
  auto len = std::get<1>(mem);

  cpu.exec(ASMINT_CallTarget, ins, len);
  }

void DirectMemory::setupMemoryFunctions() {
  vm.override_function("MEM_GetAddress_Init",  [    ](){ });

  vm.override_function("MEM_PtrToInst",        [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_^",                   [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("MEM_InstToPtr",        [this](int index)             { return mem_insttoptr(index); });
  if(vm.find_symbol_by_name("NPC_FINDBYID"))
    vm.override_function("NPC_FINDBYID",       [this](int id) {
      if(id<=0)
        return int32_t(0);
      auto& world = gameScript.world();
      for(uint32_t n=0; auto* npc=world.npcById(n); ++n)
        if(npc->handle().aivar[89]==id)
          return int32_t(npcVob(*npc));
      return int32_t(0);
      });
  vm.override_function("MEM_GetIntAddress",    [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@",                   [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetStringAddress", [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@s",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetFloatAddress",  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@f",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });

  vm.override_function("MEM_ReadInt",          [this](int address){ return mem_readint(address);                  });
  vm.override_function("MEM_WriteInt",         [this](int address, int val){ mem_writeint(address, val);          });
  vm.override_function("MEM_CopyBytes",        [this](int src, int dst, int size){ mem_copybytes(src, dst, size); });
  vm.override_function("MEM_ReadString",       [this](int sym){ return mem_readstring(sym);           });
  vm.override_function("MEM_ReadStatArr",      [this](zenkit::DaedalusVm& vm){ return mem_readstatarr(vm); });
  vm.override_function("MEM_WriteStatArr",     [this](zenkit::DaedalusVm& vm){ return mem_writestatarr(vm); });

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });
  }

void DirectMemory::bindReference(zenkit::DaedalusSymbol* ref, std::shared_ptr<zenkit::DaedalusInstance> context, Mem32::Type type) {
  const bool string = ref->type()==zenkit::DaedalusDataType::STRING;
  if(string) {
    mem32.setCallbackR(type, [this,ref,context](zString& value, uint32_t i) {
      if(ref->is_member() && !context) { memAssignString(value,""); return; }
      memAssignString(value,ref->get_string(uint16_t(i),context.get()));
      });
    mem32.setCallbackW(type, [this,ref,context](zString& value, uint32_t i) {
      if(ref->is_member() && !context) return;
      std::string text;
      memFromString(text,value);
      ref->set_string(text,uint16_t(i),context.get());
      });
    } else {
    mem32.setCallbackR(type, [ref,context](int32_t& value, uint32_t i) {
      if(ref->is_member() && !context) { value=0; return; }
      value = ref->type()==zenkit::DaedalusDataType::FLOAT ?
        floatBitsToInt(ref->get_float(uint16_t(i),context.get())) : ref->get_int(uint16_t(i),context.get());
      });
    mem32.setCallbackW(type, [ref,context](int32_t& value, uint32_t i) {
      if(ref->is_member() && !context) return;
      if(ref->type()==zenkit::DaedalusDataType::FLOAT)
        ref->set_float(intBitsToFloat(value),uint16_t(i),context.get()); else
        ref->set_int(value,uint16_t(i),context.get());
      });
    }
  }

auto DirectMemory::_takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  if(vm.top_is_reference()) {
    auto [ref, idx, context] = vm.pop_reference();
    const bool string = ref->type()==zenkit::DaedalusDataType::STRING;
    const uint32_t stride = string ? uint32_t(sizeof(zString)) : 4u;
    if(ref->is_member()) {
      if(auto d = dynamic_cast<memory_instance*>(context.get())) {
        vm.push_int(int32_t(d->address+ref->offset_as_member()+uint32_t(idx)*stride));
        return zenkit::DaedalusNakedCall();
        }
      } else {
      context.reset();
      }

    // Map native members and arrays as contiguous Gothic values, not ScriptVar slots.
    if((ref->is_member() || ref->count()>1) &&
       (string || ref->type()==zenkit::DaedalusDataType::INT || ref->type()==zenkit::DaedalusDataType::FLOAT)) {
      if(idx>=ref->count() || (ref->is_member() && context==nullptr)) {
        Log::e("Ikarus: _takeref - invalid member or array reference: ",ref->name());
        vm.push_int(0);
        return zenkit::DaedalusNakedCall();
        }
      // ponytail: retain referenced VM instances until session end; use reclaimable mappings if this grows materially.
      auto ptr = nativeReference(ref,context);
      vm.push_int(int32_t(ptr+uint32_t(idx)*stride));
      return zenkit::DaedalusNakedCall();
      }
    if(idx!=0 || ref->is_member()) {
      Log::e("Ikarus: _takeref - unsupported reference: ",ref->name());
      vm.push_int(0);
      return zenkit::DaedalusNakedCall();
      }

    const uint32_t id  = ref->index();
    const ptr32_t  ptr = scriptVariables + id*ptr32_t(sizeof(ScriptVar));
    if(false && ref->type()==zenkit::DaedalusDataType::STRING) {
      if(auto* s = mem32.deref<zString>(ptr))
        memAssignString(*s, ref->get_string(0));
      ref->set_instance(std::make_shared<memory_instance>(*this, ptr));
      }
    vm.push_int(int32_t(ptr));
    return zenkit::DaedalusNakedCall();
    }

  auto symbol = vm.pop_int();
  auto sym    = vm.find_symbol_by_index(uint32_t(symbol));
  if(sym==nullptr) {
    Log::e("Ikarus: _takeref - unable to resolve symbol");
    vm.push_int(int32_t(0xBAD10000));
    return zenkit::DaedalusNakedCall();
    }

  if(sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    auto inst = sym->get_instance().get();
    if(auto d = dynamic_cast<memory_instance*>(inst)) {
      const ptr32_t ptr = d->address;
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    else if(inst!=nullptr) {
      // HACK: ScriptVar doesn't really have any backing of data
      const ptr32_t ptr = scriptVariables + inst->symbol_index()*ptr32_t(sizeof(ScriptVar));
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    }

  if(sym->type()==zenkit::DaedalusDataType::STRING) {
    Log::e("Ikarus: _takeref - not a memory-instance: ", sym->name());
    vm.push_int(int32_t(0xBAD20000));
    return zenkit::DaedalusNakedCall();
    }

  Log::e("Ikarus: _takeref - not a memory-instance: ", sym->name());
  vm.push_int(int32_t(0xBAD20000));
  return zenkit::DaedalusNakedCall();
  }

std::shared_ptr<zenkit::DaedalusInstance> DirectMemory::mem_ptrtoinst(ptr32_t address) {
  if(address==0)
    Log::d("mem_ptrtoinst: address is null");
  if(auto it=focusNpc.find(address); it!=focusNpc.end()) {
    if(auto npc=it->second.lock())
      return npc;
    mem32.writeInt(address,0);
    }
  if(mem32.isAllocation(address,4,"focused OCNPC"))
    return nullptr;
  if(scriptVariables<=address && address<scriptVariables + ptr32_t(vm.symbols().size())*ptr32_t(sizeof(ScriptVar))) {
    // HACK: need to be consistent with _takeref
    uint32_t sId = (address-scriptVariables)/ptr32_t(sizeof(ScriptVar));
    auto     sym = vm.find_symbol_by_index(sId);
    if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE)
      return sym->get_instance();
    }
  return std::make_shared<memory_instance>(*this, address);
  }

int DirectMemory::mem_insttoptr(int index) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr || sym->type() != zenkit::DaedalusDataType::INSTANCE) {
    Log::e("MEM_InstToPtr: Invalid instance: ", index);
    return 0;
    }

  const std::shared_ptr<zenkit::DaedalusInstance>& inst = sym->get_instance();
  if(auto mem = dynamic_cast<memory_instance*>(inst.get())) {
    return int32_t(mem->address);
    }

  // Log::d("MEM_InstToPtr: not a memory instance");
  // HACK: ScriptVar doesn't really have any backing of data
  const ptr32_t ptr = scriptVariables + inst->symbol_index()*ptr32_t(sizeof(ScriptVar));
  return int32_t(ptr);
  }

int DirectMemory::mem_readint(int address) {
  return mem32.readInt(ptr32_t(address));
  }

void DirectMemory::mem_writeint(int address, int val) {
  mem32.writeInt(uint32_t(address),val);
  }

void DirectMemory::mem_copybytes(int src, int dst, int size) {
  mem32.copyBytes(ptr32_t(src),ptr32_t(dst),ptr32_t(size));
  }

std::string DirectMemory::mem_readstring(int address) {
  if(auto s = mem32.deref<const zString>(ptr32_t(address))) {
    std::string str;
    memFromString(str, *s);
    return str;
    }
  return "";
  }

zenkit::DaedalusNakedCall DirectMemory::mem_readstatarr(zenkit::DaedalusVm& vm) {
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  const int ret = vm.get_int(context, ref, uint16_t(idx+index));

  vm.push_int(ret);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_writestatarr(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  const int  value = vm.pop_int();
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  vm.set_int(context, ref, uint16_t(idx+index), value);
  return zenkit::DaedalusNakedCall();
  }

int DirectMemory::mem_alloc(int amount) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

int DirectMemory::mem_alloc(int amount, const char* comment) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount), comment);
  return int32_t(ptr);
  }

void DirectMemory::mem_free(int ptr) {
  removeUiView(Mem32::ptr32_t(ptr));
  mem32.free(Mem32::ptr32_t(ptr));
  }

void DirectMemory::removeUiView(ptr32_t ptr) {
  if(uiViews.erase(ptr)!=0 && std::getenv("OPENGOTHIC_BOSS_UI_PROBE")!=nullptr)
    Log::i("[BOSS_UI] view removed=",ptr);
  std::erase(uiViewOrder,ptr);
  }

int DirectMemory::mem_realloc(int address, int oldsz, int size) {
  auto ptr = mem32.realloc(Mem32::ptr32_t(address), uint32_t(size));
  return int32_t(ptr);
  }


void DirectMemory::setupDirectFunctions() {
  // Resolve function variables through the VM instead of legacy parser memory.
  vm.override_function("MEM_GetFuncID", [](zenkit::DaedalusFunction fn) {
    return fn.value!=nullptr ? int32_t(fn.value->index()) : -1;
    });
  vm.override_function("MEM_GetFuncIdByOffset", [this](int off) { return mem_getfuncidbyoffset(off); });
  vm.override_function("MEM_AssignInst",        [this](int index, int ptr) { mem_assigninst(index, ptr); });

  vm.override_function("MEM_CallByID",          [this](zenkit::DaedalusVm& vm) { return mem_callbyid(vm);         });
  vm.override_function("MEM_CallByPtr",         [this](zenkit::DaedalusVm& vm) { return mem_callbyptr(vm);        });

  vm.override_function("memint_stackpushint",   [this](zenkit::DaedalusVm& vm) { return memint_stackpushint (vm); });
  vm.override_function("memint_stackpushinst",  [this](zenkit::DaedalusVm& vm) { return memint_stackpushinst(vm); });
  vm.override_function("memint_stackpushvar",   [this](zenkit::DaedalusVm& vm) { return memint_stackpushvar (vm); });

  vm.override_function("memint_popstring",      [this](zenkit::DaedalusVm& vm) { return memint_popstring(vm);     });

  vm.override_function("mem_popintresult",      [this](zenkit::DaedalusVm& vm) { return mem_popintresult(vm);     });
  vm.override_function("mem_popstringresult",   [this](zenkit::DaedalusVm& vm) { return mem_popstringresult(vm);  });
  vm.override_function("mem_popinstresult",     [this](zenkit::DaedalusVm& vm) { return mem_popinstresult(vm);    });
  }

int DirectMemory::mem_getfuncidbyoffset(int off) {
  if(off==0) {
    // MEM_INITALL
    return 0;
    }
  Log::e("TODO: mem_getfuncidbyoffset ", off);
  return 0;
  }

void DirectMemory::mem_assigninst(int index, int ptr) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr) {
    Log::e("MEM_AssignInst: Invalid instance: ",index);
    return;
    }
  sym->set_instance(std::make_shared<memory_instance>(*this, ptr32_t(ptr)));
  }

void DirectMemory::directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func) {
  if(func.type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("Bad unsafe function call");
    return;
    }
  // Sprite maps use native rendering and input; the original callback reads
  // unbound Win32 input memory and zCVob's engine-only transform array.
  if(func.name()=="SPRITEMAP_LOOP")
    return;
  if(std::getenv("OPENGOTHIC_BUFF_PROBE")!=nullptr &&
     (func.name()=="BUFF_SPEED_APPLY" || func.name()=="BUFF_SPEED_REMOVE"))
    Log::i("[BUFF_UI] callback=",func.name());

  std::span<zenkit::DaedalusSymbol> params = vm.find_parameters_for_function(&func);
  if(params.size()==1 && params[0].type()==zenkit::DaedalusDataType::INT && !vm.top_is_reference()) {
    // Gothic's MOVI writes zero for a raw instance argument (DoStack 0x791b71).
    // LeGo FREE can resolve an integer-taking function as a class destructor.
    // ZenKit otherwise consumes the instance, throws, and retains a stale handle.
    int value = 0;
    try {
      value = vm.pop_int();
      }
    catch(const zenkit::DaedalusVmException& error) {
      if(std::string_view(error.what())!="tried to pop_int but frame does not contain a int.")
        throw;
      }
    vm.push_int(value);
    }
  //vm.call_function(sym);
  //zenkit::StackGuard guard {&vm, func.rtype()};
  vm.unsafe_call(&func);
  // The opt-in regression uses the original TIMER_SETPAUSE script as a callback.
  // Count each dispatch, including multiple calls in one frame, then undo the pause.
  if(func.name()=="TIMER_SETPAUSE") {
    auto* argument = vm.find_symbol_by_name("TIMER_SETPAUSE.ON");
    if(worldProbeTimer!=0 && uint32_t(argument->get_int())==worldProbeTimer) {
      ++worldProbeDispatches;
      Log::i("[WORLD_PROBE] callback_dispatch=",worldProbeDispatches,
             " elapsed=",uint32_t(gameScript.tickCount())-uint32_t(mem32.readInt(worldProbeTimer+24)),
             " world=",gameScript.world().name());
      argument->set_int(0);
      vm.find_symbol_by_name("_TIMER_PAUSED")->set_int(mem32.readInt(worldProbeTimer+28));
      }
    if(worldProbeRecurringTimer!=0 && uint32_t(argument->get_int())==worldProbeRecurringTimer) {
      const auto elapsed = uint32_t(gameScript.tickCount())-uint32_t(mem32.readInt(worldProbeRecurringTimer+24));
      if(worldProbeRecurringLastElapsed!=0 && elapsed-worldProbeRecurringLastElapsed<100)
        throw std::runtime_error("World-transition recurring callback duplicated");
      worldProbeRecurringLastElapsed = elapsed;
      ++worldProbeRecurringDispatches;
      Log::i("[WORLD_PROBE] recurring_dispatch=",worldProbeRecurringDispatches,
             " elapsed=",elapsed," world=",gameScript.world().name());
      argument->set_int(0);
      vm.find_symbol_by_name("_TIMER_PAUSED")->set_int(mem32.readInt(worldProbeRecurringTimer+28));
      }
    if(persistenceProbeRoot!=0 && uint32_t(argument->get_int())==persistenceProbeRoot) {
      auto ptr = persistenceProbeRoot;
      auto elapsed = uint32_t(gameScript.tickCount())-uint32_t(mem32.readInt(ptr+24));
      if(elapsed<15000) throw std::runtime_error("Persistence callback fired early");
      mem32.writeInt(ptr+4,mem32.readInt(ptr+4)+1);
      argument->set_int(0);
      vm.find_symbol_by_name("_TIMER_PAUSED")->set_int(mem32.readInt(ptr+12));
      Log::i("[PERSISTENCE_PROBE] fired elapsed=",elapsed);
      }
    }
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyid(zenkit::DaedalusVm& vm) {
  const uint32_t symbId = uint32_t(vm.pop_int());
  auto* sym = vm.find_symbol_by_index(symbId);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", symbId);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyptr(zenkit::DaedalusVm& vm) {
  //FIXME: map function into memory for real!
  const auto address = uint32_t(vm.pop_int());
  auto sym = vm.find_symbol_by_address(address);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%x", address);
    Log::e("MEM_CallByPtr: Provided symbol is not callable (not function, prototype or instance): 0x", buf);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushint(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  int id = vm.pop_int();
  auto sym = vm.find_symbol_by_index(uint32_t(id));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    vm.push_instance(sym->get_instance());
    return zenkit::DaedalusNakedCall();
    }
  vm.push_instance(nullptr);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushvar(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  //NOTE: works by accident with _AI_FUNCTION_EVENT for now
  const int ptr = vm.pop_int(); (void)ptr;
  Log::d("TODO: memint_stackpushvar");
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_popstring(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popintresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popinstresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::setupMathFunctions() {
  vm.override_function("MKF",    [](int i) { return mkf(i); });
  vm.override_function("TRUNCF", [](int i) { return truncf(i); });
  vm.override_function("ROUNDF", [](int i) { return roundf(i); });
  vm.override_function("ADDF",   [](int a, int b) { return addf(a,b); });
  vm.override_function("SUBF",   [](int a, int b) { return subf(a,b); });
  vm.override_function("MULF",   [](int a, int b) { return mulf(a,b); });
  vm.override_function("DIVF",   [](int a, int b) { return divf(a,b); });
  if(vm.find_symbol_by_name("SIN"))
    vm.override_function("SIN",  [](int a) { return floatBitsToInt(std::sin(intBitsToFloat(a))); });
  }

int DirectMemory::mkf(int v) {
  return floatBitsToInt(float(v));
  }

int DirectMemory::truncf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::truncf(ret);
  return int(ret); //floatBitsToInt(ret);
  }

int DirectMemory::roundf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::roundf(ret);
  return int(ret); //floatBitsToInt(ret);
  }

int DirectMemory::addf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a+b);
  }

int DirectMemory::subf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a-b);
  }

int DirectMemory::mulf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a*b);
  }

int DirectMemory::divf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a/b);
  }

void DirectMemory::setupStringsFunctions() {
  // TODO: allow writes to mapped memory
  vm.override_function("STR_SubStr", [](std::string_view str, int start, int count) -> std::string {
    if(start < 0 || (count < 0)) {
      Log::e("STR_SubStr: start and count may not be negative.");
      return "";
      };

    if(size_t(start)>=str.size()) {
      Log::e("STR_SubStr: The desired start of the substring lies beyond the end of the string.");
      return "";
      };

    return std::string(str.substr(size_t(start), size_t(count)));
    });

  //NOTE: native implementation requires coherency protocol
  // const ptr32_t ZSTRING__UPPER_G2 = 4631296;
  vm.override_function("STR_Upper", [](std::string_view str){
    std::string s = std::string(str);
    for(auto& c:s)
      c = char(std::toupper(c));
    return s;
    });

  // LeGo immplementation requires 'rw' access to 'callback memory'
  vm.override_function("SB_TOSTRING", [this]() -> std::string {
    struct StringBuilder {
      ptr32_t ptr;
      int     cln;
      int     CAL;
      };
    const auto _SB_CURRENT = this->vm.find_symbol_by_name("_SB_CURRENT");
    if(_SB_CURRENT==nullptr || _SB_CURRENT->type()!=zenkit::DaedalusDataType::INT)
      return "";

    auto text = mem32.deref<StringBuilder>(ptr32_t(_SB_CURRENT->get_int()));
    if(text->cln<=0 || text->ptr==0)
      return "";
    auto cstr = reinterpret_cast<const char*>(mem32.deref(text->ptr, uint32_t(text->cln)));
    return std::string(cstr, size_t(text->cln));
    });

  const ptr32_t STR_FROMCHAR = 4198592;
  cpu.register_thiscall(STR_FROMCHAR, [this](ptr32_t self, ptr32_t pchr) {
    if(self<scriptVariables)
      return; // error

    const uint32_t id = (self - scriptVariables)/ptr32_t(sizeof(ScriptVar));
    const auto     sx = vm.find_symbol_by_index(id);
    if(sx==nullptr || sx->type()!=zenkit::DaedalusDataType::STRING)
      return;

    auto [ptr, size] = mem32.deref(pchr);
    const char* chr = reinterpret_cast<const char*>(ptr);
    if(chr==nullptr || size==0) {
      sx->set_string("");
      return;
      }
    size_t strsz = 0;
    while(chr[strsz] && strsz<size)
      ++strsz;
    sx->set_string(std::string(chr, strsz));
    });
  }

void DirectMemory::setupWinapiFunctions() {
  const ptr32_t WINAPI__LOADLIBRARY_ptr    = 8577604;
  const ptr32_t WINAPI__GETPROCADDRESS_ptr = 8577688;
  mem32.alloc(WINAPI__LOADLIBRARY_ptr,    4, "WINAPI__LOADLIBRARY*");
  mem32.alloc(WINAPI__GETPROCADDRESS_ptr, 4, "WINAPI__GETPROCADDRESS*");

  const ptr32_t WINAPI__LOADLIBRARY = mem32.alloc(4);
  mem32.writeInt(WINAPI__LOADLIBRARY_ptr, int32_t(WINAPI__LOADLIBRARY));

  const ptr32_t WINAPI__GETPROCADDRESS = mem32.alloc(4);
  mem32.writeInt(WINAPI__GETPROCADDRESS_ptr, int32_t(WINAPI__GETPROCADDRESS));

  cpu.register_stdcall(WINAPI__LOADLIBRARY, [this](ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr)+size);
    Log::d("suppress LoadLibrary: ", name);
    return 0x1;
    });

  cpu.register_stdcall(WINAPI__GETPROCADDRESS, [this](ptr32_t module, ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), reinterpret_cast<const char*>(ptr)+size);
    Log::d("suppress GetProcAddress: ",name);
    return 0x1;
    });

  const ptr32_t GETUSERNAMEA = 8080162;
  cpu.register_stdcall(GETUSERNAMEA, [this](ptr32_t lpBuffer, ptr32_t pcbBuffer){
    std::string_view uname = "OpenGothic";

    const uint32_t max = pcbBuffer ? uint32_t(mem32.readInt(pcbBuffer)) : 0;
    if(auto ptr = reinterpret_cast<char*>(mem32.derefv(lpBuffer, max))) {
      if(max>=uname.size()) {
        std::strncpy(reinterpret_cast<char*>(ptr), "OpenGothic", max);
        ptr[uname.size()] = '\0';
        return 1;
        }
      }
    // buffer is too small
    mem32.writeInt(pcbBuffer, int32_t(uname.size()+1));
    return -1;
    });

  const ptr32_t GETLOCALTIME = 8079184;
  cpu.register_stdcall(GETLOCALTIME, [this](ptr32_t lpSystemTime) {
    struct SystemTime {
      uint16_t wYear;
      uint16_t wMonth;
      uint16_t wDayOfWeek;
      uint16_t wDay;
      uint16_t wHour;
      uint16_t wMinute;
      uint16_t wSecond;
      uint16_t wMilliseconds;
      };

    if(auto ptr = mem32.deref<SystemTime>(lpSystemTime)) {
      const auto        timePoint = std::chrono::system_clock::now();
      const std::time_t time      = std::chrono::system_clock::to_time_t(timePoint);
      const auto        t         = std::localtime(&time);
      const auto        ms        = std::chrono::time_point_cast<std::chrono::milliseconds>(timePoint);

      ptr->wYear         = uint16_t(t->tm_year + 1900);
      ptr->wMonth        = uint16_t(t->tm_mon + 1);
      ptr->wDayOfWeek    = uint16_t(t->tm_wday);
      ptr->wDay          = uint16_t(t->tm_mday);
      ptr->wHour         = uint16_t(t->tm_hour);
      ptr->wMinute       = uint16_t(t->tm_min);
      ptr->wSecond       = uint16_t(t->tm_sec);
      ptr->wMilliseconds = uint16_t(ms.time_since_epoch().count()%1000);
      }
    });
  }

void DirectMemory::setupUtilityFunctions() {
  static auto generateCrcTable = []() {
    std::array<uint32_t, 256> table;
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (size_t j = 0; j < 8; j++) {
        if(c & 1) {
          c = polynomial ^ (c >> 1);
          } else {
          c >>= 1;
          }
        }
      table[i] = c;
      }
    return table;
    };

  const ptr32_t GETBUFFERCRC32_G2 = 6265360;
  cpu.register_cdecl(GETBUFFERCRC32_G2, [this](ptr32_t buf, int bufLen, int unused) {
    if(buf==0 || bufLen<=0)
      return 0;

    const auto  ptr   = reinterpret_cast<const uint8_t*>(mem32.deref(buf,uint32_t(bufLen)));
    static auto table = generateCrcTable();

    uint32_t initial = 0;
    uint32_t c       = initial ^ 0xFFFFFFFF;
    for(int i = 0; i < bufLen; ++i) {
      c = table[(c ^ ptr[i]) & 0xFF] ^ (c >> 8);
      }
    return int32_t(c ^ 0xFFFFFFFF);
    });

  const ptr32_t QSORT_G2 = 8195951;
  cpu.register_thiscall(QSORT_G2, [](ptr32_t base, int numElt, int eltSize, ptr32_t pCmp) {
    Log::e("LeGo: TODO: QSORT_G2(...)");
    });

  const ptr32_t ZERROR__SETTARGET = 4513616;
  cpu.register_thiscall(ZERROR__SETTARGET, [](ptr32_t, int) {
    // nop
    });

  const int SYSGETTIMEPTR_G2 = 5264000;
  cpu.register_cdecl(SYSGETTIMEPTR_G2, [this](){
    return uint32_t(gameScript.tickCount());
    });
  }

void DirectMemory::setupHookEngine() {
  vm.override_function("HookEngineI", [](int address, int oldInstr, zenkit::DaedalusFunction function){
    auto sym  = function.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    Log::e("not implemented call [HookEngineI] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", name, ")");
    });

  const ptr32_t ZCCONSOLE__REGISTER = 7875296;
  cpu.register_thiscall(ZCCONSOLE__REGISTER, [](ptr32_t, ptr32_t pcmd, ptr32_t pdescr) {
    (void)pcmd;
    (void)pdescr;
    // auto sym  = func.value;
    // auto name = sym==nullptr ? "" : sym->name().c_str();
    // Log::e("not implemented call [ZCCONSOLE__REGISTER] (", prefix, " -> ", name, ")");
    });

  // PermMem
  vm.override_function("LOCALS", [this](zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
    auto sym = findSymbolByAddress(vm.pc());
    if(sym!=nullptr) {
      auto sx = vm.find_symbol_by_index(sym->index());
      sx->set_local_variables_enable(true);
      }
    return zenkit::DaedalusNakedCall();
    });
  vm.override_function("FINAL", []() {
    Log::e("LeGo: 'final' is not implemented");
    return 0;
    });
  }


void DirectMemory::setupzCParserFunctions() {
  const ptr32_t ZCPARSER__GETINDEX_G2 = 7943280;
  cpu.register_thiscall(ZCPARSER__GETINDEX_G2, [this](ptr32_t, std::string sym) {
    if(auto s = vm.find_symbol_by_name(sym))
      return int32_t(s->index());
    return -1;
    });

  const ptr32_t ZCPARSER__CREATEINSTANCE = 7942048;
  auto createInstance = [this](int32_t instId, ptr32_t ptr){
    auto *sym = vm.find_symbol_by_index(uint32_t(instId));
    auto *cls = sym;
    if(sym != nullptr && sym->type() == zenkit::DaedalusDataType::INSTANCE) {
      cls = vm.find_symbol_by_index(sym->parent());
      }

    if(sym==nullptr || cls == nullptr) {
      Log::e("LeGo::createInstance invalid symbold id (", instId, ")");
      return 0;
      }

    auto inst = std::make_shared<memory_instance>(*this, ptr);

    auto self = vm.find_symbol_by_name("SELF");
    auto prevSelf = self != nullptr ? self->get_instance() : nullptr;
    auto prevGi   = vm.unsafe_get_gi();
    if(self!=nullptr)
      self->set_instance(inst);

    vm.unsafe_set_gi(inst);
    vm.unsafe_call(sym);

    vm.unsafe_set_gi(prevGi);
    if(self!=nullptr)
      self->set_instance(prevSelf);

    sym->set_instance(inst);
    return int(ptr); // no idea, what return-value suppose to be - unused in LeGo
    };
  cpu.register_thiscall(ZCPARSER__CREATEINSTANCE, [createInstance](ptr32_t, int32_t instId, ptr32_t ptr) {
    return createInstance(instId,ptr);
    });
  if(vm.find_symbol_by_name("Create")!=nullptr) {
    vm.override_function("Create", [this,createInstance](int32_t id) {
      auto cls = this->vm.find_symbol_by_index(uint32_t(id));
      if(cls==nullptr || cls->type()!=zenkit::DaedalusDataType::INSTANCE)
        return 0;
      while(cls!=nullptr && cls->type()!=zenkit::DaedalusDataType::CLASS)
        cls = this->vm.find_symbol_by_index(cls->parent());
      if(cls==nullptr || cls->class_size()==0)
        return 0;
      return createInstance(id,mem32.alloc(cls->class_size()));
      });
    }
  }

void DirectMemory::setupInitFileFunctions() {
  vm.override_function("MEM_GetGothOpt", [](std::string_view sec, std::string_view opt) {
    return std::string(Gothic::inst().settingsGetS(sec, opt));
    });
  vm.override_function("MEM_GetModOpt", [](std::string_view sec, std::string_view opt) {
    Log::e("TODO: mem_getmodopt(", sec, ", ", opt, ")");
    return std::string("");
    });
  vm.override_function("MEM_GothOptSectionExists", [](std::string_view sec) {
    Log::e("TODO: mem_gothoptaectionexists(", sec, ")");
    return false;
    });
  vm.override_function("MEM_GothOptExists", [](std::string_view sec, std::string_view opt) {
    if(sec=="INTERNAL" && opt=="UnionActivated") {
      // Fake Union, so ikarus won't setup crazy workarounds for unpatched G2.
      return true;
      }
    Log::e("TODO: mem_gothoptexists(", sec, ", ", opt, ")");
    return false;
    });
  vm.override_function("MEM_ModOptSectionExists", [](std::string_view sec) {
    Log::e("TODO: mem_modoptsectionexists(", sec, ")");
    return false;
    });
  vm.override_function("MEM_ModOptExists", [](std::string_view sec, std::string_view opt) {
    Log::e("TODO: mem_modoptexists(", sec, ", ", opt, ")");
    return false;
    });
  vm.override_function("MEM_SetGothOpt", [](std::string_view sec, std::string_view opt, std::string_view v) {
    Log::e("TODO: mem_setgothopt(", sec, ", ", opt, ", ", v, ")");
    });
  }

void DirectMemory::setupUiFunctions() {
  if(vm.find_symbol_by_name("PrintS_Ext")!=nullptr) {
    vm.override_function("PrintS_Ext", [](std::string_view msg, int /*color*/) {
      // ponytail: native notices omit LeGo color/fade effects; add a colored overlay for visual parity.
      Gothic::inst().onPrint(msg);
      });
    }

  // https://github.com/Lehona/LeGo/blob/dev/View.d
  const int ZCVIEW__ZCVIEW     = 8017664;
  const int ZCVIEW__DTOR       = 8017856;
  const int ZCVIEW__OPEN       = 8023040;
  const int ZCVIEW__CLOSE      = 8023600;
  const int ZCVIEW_TOP         = 8021904;
  const int ZCVIEW__SETSIZE    = 8026016;
  const int zCVIEW__MOVE       = 8025824;
  const int ZCVIEW__INSERTBACK = 8020272;
  cpu.register_thiscall(ZCVIEW__DTOR, [this](ptr32_t ptr) {
    // Destruction removes rendering state, not allocation ownership. LeGo
    // RELEASE keeps the allocation; CLEAR/VIEWPTR_DELETE free it separately.
    removeUiView(ptr);
    if(auto* view = mem32.deref<zCView>(ptr)) {
      view->ISOPEN = false;
      view->ISCLOSED = true;
      }
    });
  cpu.register_thiscall(ZCVIEW__ZCVIEW, [this](ptr32_t ptr, int x1, int y1, int x2, int y2, int arg) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__zCView - unable to resolve address");
      return;
      }

    view->VPOSX  = x1;
    view->VPOSY  = y1;
    view->VSIZEX = x2-x1;
    view->VSIZEY = y2-y1;
    view->ALPHA  = 255;
    uiViews[ptr] = {};
    std::erase(uiViewOrder,ptr);
    uiViewOrder.push_back(ptr);
    if(std::getenv("OPENGOTHIC_BOSS_UI_PROBE")!=nullptr)
      Log::i("[BOSS_UI] view constructed=",ptr);
    });

  cpu.register_thiscall(ZCVIEW__OPEN, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Open - unable to resolve address");
      return;
      }
    view->ISOPEN = true;
    view->ISCLOSED = false;
    });

  cpu.register_thiscall(ZCVIEW__CLOSE, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Close - unable to resolve address");
      return;
      }
    view->ISOPEN = false;
    view->ISCLOSED = true;
    });

  cpu.register_thiscall(ZCVIEW_TOP, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Top - unable to resolve address");
      return;
      }
    if(uiViews.contains(ptr)) {
      std::erase(uiViewOrder,ptr);
      uiViewOrder.push_back(ptr);
      }
    });

  cpu.register_thiscall(ZCVIEW__SETSIZE, [this](ptr32_t ptr, int x, int y) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__SetSize - unable to resolve address");
      return;
      }

    view->VSIZEX = x;
    view->VSIZEY = y;
    });

  cpu.register_thiscall(zCVIEW__MOVE, [this](ptr32_t ptr, int x, int y) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Move - unable to resolve address");
      return;
      }
    view->VPOSX += x;
    view->VPOSY += y;
    });

  cpu.register_thiscall(ZCVIEW__INSERTBACK, [this](ptr32_t ptr, std::string img) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__InsertBack - unable to resolve address");
      return;
    }
    uiViews[ptr].texture = std::move(img);
    if(std::getenv("OPENGOTHIC_BOSS_UI_PROBE")!=nullptr)
      Log::i("[BOSS_UI] view texture=",uiViews[ptr].texture,
             " virtual_rect=",view->VPOSX,",",view->VPOSY,",",view->VSIZEX,",",view->VSIZEY);
    });

  // ## Textures
  const int ZCTEXTURE__LOAD = 6239904;
  cpu.register_stdcall(ZCTEXTURE__LOAD, [](std::string img, int flag) {
    Log::e("LeGo: zCTexture__Load: ", img);
    return 0;
    });
  }

void DirectMemory::setupFontFunctions() {
  const ptr32_t ZCFONTMAN__LOAD    = 7897808;
  const ptr32_t ZCFONTMAN__GETFONT = 7898288;
  const ptr32_t ZCFONT__GETFONTY   = 7902432;
  const ptr32_t ZCFONT__GETFONTX   = 7902448;
  cpu.register_thiscall(ZCFONTMAN__LOAD, [this](ptr32_t, std::string font) {
    for(const auto& [handle,name] : fontNames)
      if(name==font)
        return int32_t(handle);
    while(nextFontHandle<0x80000000 && fontNames.contains(nextFontHandle))
      ++nextFontHandle;
    if(nextFontHandle>=0x80000000) {
      Log::e("LeGo: zCFontMan__Load - exhausted font handles");
      return 0;
      }
    const auto handle = nextFontHandle++;
    fontNames.emplace(handle,std::move(font));
    return int32_t(handle);
    });
  cpu.register_thiscall(ZCFONTMAN__GETFONT, [](ptr32_t, int handle) {
    return handle;
    });
  cpu.register_thiscall(ZCFONT__GETFONTY, [this](ptr32_t handle) {
    auto font = fontNames.find(handle);
    return font==fontNames.end() ? 0 : Resources::font(font->second,Resources::FontType::Normal,1).pixelSize();
    });
  cpu.register_thiscall(ZCFONT__GETFONTX, [this](ptr32_t handle, std::string text) {
    auto font = fontNames.find(handle);
    if(font==fontNames.end())
      return 0;
    return Resources::font(font->second,Resources::FontType::Normal,1).textSize(text).w;
    });
  }

void DirectMemory::tickUi(uint64_t dt) {
  setUiSize(uiWidth,uiHeight);

  //NOTE: PRINT_EXT
  if(auto* vPrint = mem32.deref<const zCView>(memGame.ARRAY_VIEW[0])) {
    auto* vList = vPrint->TEXTLINES_NEXT!=0 ? mem32.deref<const zCList>(vPrint->TEXTLINES_NEXT) : nullptr;
    while(vList!=nullptr) {
      if(auto* textView = mem32.deref<const zCViewText>(vList->data)) {
        std::string dst; //TODO: avoid reallocations
        memFromString(dst, textView->text);
        // Log::i("zCViewText: ", dst);
        }
      vList = vList->next!=0 ? mem32.deref<const zCList>(vList->next) : 0;
      }
    }
  }

void DirectMemory::setUiSize(int width, int height) {
  uiWidth = std::max(width,1);
  uiHeight = std::max(height,1);
  if(auto* view = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    view->PSIZEX = uiWidth;
    view->PSIZEY = uiHeight;
    }
  if(auto* hpBar = mem32.deref<oCViewStatusBar>(memGame.HPBAR)) {
    // LeGo's authored status-bar width is 180, versus our 200-pixel outer art.
    // Expose their shared responsive layout scale, not the native fill width.
    hpBar->VSIZEX = int32_t(std::lround(180.f*uiBarScale*8192.f/float(uiWidth)));
    }
  }

bool DirectMemory::isSpriteMapOpen() {
  auto* handle = vm.find_symbol_by_name("SPRITEMAP_SPRITEHNDL");
  return handle!=nullptr && handle->get_int()!=0;
  }

bool DirectMemory::closeSpriteMap() {
  if(!isSpriteMapOpen())
    return false;
  vm.call_function("SPRITEMAP_DESTROY");
  return true;
  }

void DirectMemory::drawSpriteMap(Tempest::Painter& p) {
  if(!isSpriteMapOpen())
    return;
  auto textureName = [this](int handle) -> std::string {
    if(handle==0)
      return {};
    auto sprite = vm.call_function<std::shared_ptr<zenkit::DaedalusTransientInstance>>("GET",handle);
    auto* name = vm.find_symbol_by_name("GCSPRITE.TEXTURENAME");
    return sprite && name ? name->get_string(0,sprite.get()) : std::string();
    };
  const auto* map = Resources::loadTexture(textureName(vm.find_symbol_by_name("SPRITEMAP_SPRITEHNDL")->get_int()));
  if(map==nullptr)
    return;
  const float size = float(std::min(uiWidth,uiHeight));
  const float left = (float(uiWidth)-size)*0.5f, top = (float(uiHeight)-size)*0.5f;
  p.setBrush(Tempest::Brush(*map,Tempest::Color(1),Tempest::Painter::Alpha));
  p.drawRect(left,top,size,size,0.f,0.f,float(map->w()),float(map->h()));

  auto* cursorHandle = vm.find_symbol_by_name("SPRITEMAP_SPRITECURSORHNDL");
  auto* hero = gameScript.world().player();
  if(cursorHandle==nullptr || cursorHandle->get_int()==0 || hero==nullptr)
    return;
  const auto* arrow = Resources::loadTexture(textureName(cursorHandle->get_int()));
  if(arrow==nullptr)
    return;
  auto value = [this](const char* name) {
    auto* sym = vm.find_symbol_by_name(name);
    return sym ? intBitsToFloat(sym->get_int()) : 0.f;
    };
  const float minX = value("SPRITEMAP_MINXF"), minZ = value("SPRITEMAP_MINYF");
  const float rangeX = value("SPRITEMAP_DISTXF"), rangeZ = value("SPRITEMAP_DISTYF");
  if(!std::isfinite(rangeX) || !std::isfinite(rangeZ) || rangeX<=0 || rangeZ<=0)
    return;
  const auto pos = hero->position();
  const float u = std::clamp((pos.x-minX)/rangeX,0.f,1.f);
  const float v = std::clamp(1.f-(pos.z-minZ)/rangeZ,0.f,1.f);
  const bool rotate = vm.find_symbol_by_name("SPRITEMAP_ROTATE90")->get_int()!=0;
  const float x = left+(rotate ? 1.f-v : u)*size;
  const float y = top +(rotate ? u : v)*size;
  const auto transform = hero->transform();
  const float angle = std::atan2(transform.at(0,0),transform.at(0,2))+(rotate ? float(M_PI)*0.5f : 0.f);
  const float sine = std::sin(angle), cosine = std::cos(angle);
  const float half = 8.f*size/float(uiHeight);
  const auto vertex = [&](float dx, float dy) {
    return Tempest::PointF(x+cosine*dx-sine*dy,y+sine*dx+cosine*dy);
    };
  const auto a=vertex(-half,-half), b=vertex(half,-half), c=vertex(half,half), d=vertex(-half,half);
  p.setBrush(Tempest::Brush(*arrow,Tempest::Color(1),Tempest::Painter::Alpha));
  p.drawTriangle(a.x,a.y,0,0,b.x,b.y,float(arrow->w()),0,c.x,c.y,float(arrow->w()),float(arrow->h()));
  p.drawTriangle(a.x,a.y,0,0,c.x,c.y,float(arrow->w()),float(arrow->h()),d.x,d.y,0,float(arrow->h()));
  if(std::getenv("OPENGOTHIC_MAP_PROBE")!=nullptr) {
    static int last = 0;
    const int handle = vm.find_symbol_by_name("SPRITEMAP_SPRITEHNDL")->get_int();
    if(last!=handle) {
      last=handle;
      Log::i("[SPRITEMAP] draw texture=",textureName(handle)," rect=",left,",",top,",",size,
             " marker=",x,",",y," heading=",angle," rotated=",rotate);
      }
    }
  }

int DirectMemory::focusBarY(int height) {
  auto* bar = mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR);
  if(bar==nullptr)
    return 10;
  return bar->VPOSY==0 ? 10 : int((int64_t(bar->VPOSY)*height)/8192);
  }

void DirectMemory::drawUi(Tempest::Painter& p, int width, int height, float barScale) {
  const bool resized = uiWidth!=std::max(width,1) || uiHeight!=std::max(height,1) || uiBarScale!=barScale;
  uiBarScale = barScale;
  setUiSize(width,height);
  if(resized) {
    // Native replacement for the installed screen-resolution callbacks. Refresh
    // script metrics first; callbacks may delete/recreate views, before iteration.
    for(auto name : {"PRINT_GETSCREENSIZE", "_BAR_UPDATERESOLUTION", "_BOSSUI_UPDATERESOLUTION"})
      if(auto sym = vm.find_symbol_by_name(name))
        vm.call_function(sym);
    }
  constexpr int virtualSize = 8192;
  const auto toPixel = [](int value, int size) {
    return int((int64_t(value)*size)/virtualSize);
    };

  for(auto it=uiViewOrder.begin(); it!=uiViewOrder.end();) {
    auto state = uiViews.find(*it);
    auto* view = state==uiViews.end() ? nullptr : mem32.deref<zCView>(*it);
    if(view==nullptr) {
      if(state!=uiViews.end())
        uiViews.erase(state);
      it = uiViewOrder.erase(it);
      continue;
      }
    if(view->ISOPEN!=0 && view->ISCLOSED==0 && !state->second.texture.empty()) {
      const auto* texture = Resources::loadTexture(state->second.texture);
      const int x = toPixel(view->VPOSX,uiWidth), y = toPixel(view->VPOSY,uiHeight);
      const int w = toPixel(view->VSIZEX,uiWidth), h = toPixel(view->VSIZEY,uiHeight);
      if(texture!=nullptr && w>0 && h>0) {
        p.setBrush(Tempest::Brush(*texture,Tempest::Color(1,1,1,std::clamp(float(view->ALPHA)/255.f,0.f,1.f)),Tempest::Painter::Alpha));
        p.drawRect(x,y,w,h,0,0,texture->w(),texture->h());
        if(std::getenv("OPENGOTHIC_BUFF_PROBE")!=nullptr && state->second.texture=="ITPO_SPEED2.TGA") {
          static uint8_t loggedAlpha = 0;
          if((view->ALPHA==255 && !(loggedAlpha&1)) || (view->ALPHA<128 && !(loggedAlpha&2))) {
            loggedAlpha |= view->ALPHA==255 ? 1 : 2;
            Log::i("[BUFF_UI] draw texture=",state->second.texture," alpha=",view->ALPHA,
                   " rect=",x,",",y,",",w,",",h);
            }
          }
        if(std::getenv("OPENGOTHIC_BOSS_UI_PROBE")!=nullptr)
          Log::i("[BOSS_UI] draw texture=",state->second.texture," rect=",x,",",y,",",w,",",h);
        }
      }
    ++it;
    }

  auto* view = mem32.deref<const zCView>(memGame.ARRAY_VIEW[0]);
  auto* list = view!=nullptr && view->TEXTLINES_NEXT!=0 ? mem32.deref<const zCList>(view->TEXTLINES_NEXT) : nullptr;
  while(list!=nullptr) {
    if(auto* text = mem32.deref<const zCViewText>(list->data)) {
      std::string value;
      memFromString(value,text->text);
      auto font = text->font>0 ? fontNames.find(uint32_t(text->font)) : fontNames.end();
      if(!value.empty() && font!=fontNames.end()) {
        const auto color = uint32_t(text->colored!=0 ? text->color : -1);
        auto& gfont = Resources::font(font->second,Resources::FontType::Normal,1);
        if(std::getenv("OPENGOTHIC_BOSS_UI_PROBE")!=nullptr)
          Log::i("[BOSS_UI] text=",value," rect=",toPixel(text->posx,uiWidth),",",toPixel(text->posy,uiHeight),",",gfont.textSize(value).w,",",gfont.pixelSize());
        gfont.drawText(p,toPixel(text->posx,uiWidth),toPixel(text->posy,uiHeight)+gfont.pixelSize(),value,
          Color(float((color>>16)&0xFF)/255.f,float((color>>8)&0xFF)/255.f,float(color&0xFF)/255.f,float(color>>24)/255.f));
        }
      }
    list = list->next!=0 ? mem32.deref<const zCList>(list->next) : nullptr;
    }
  }

void DirectMemory::setupNpcFunctions() {
  auto start = vm.find_symbol_by_name("TRIA_STARTEXT");
  auto next = vm.find_symbol_by_name("TRIA_NEXT");
  auto finish = vm.find_symbol_by_name("TRIA_FINISH");
  auto running = vm.find_symbol_by_name("TRIA_RUNNING");
  auto invited = vm.find_symbol_by_name("TRIA_NPCPTR");
  auto count = vm.find_symbol_by_name("TRIA_CPTR");
  if(start!=nullptr && next!=nullptr && finish!=nullptr && running!=nullptr &&
     invited!=nullptr && count!=nullptr && vm.find_symbol_by_name("_TRIA_COPY")!=nullptr) {
    // LeGo swaps oCNpc memory to impersonate speakers. Keep native NPCs intact
    // and capture the intended speaker when each output is queued instead.
    // ponytail: this covers subtitle identity; visual swaps and wait timing remain unsupported.
    vm.override_function("_TRIA_COPY", [](int, int) {});
    vm.override_function("TRIA_STARTEXT", [this,start,running](int turn) {
      // call_function intentionally executes the script body, bypassing this override.
      const bool alreadyRunning = running->get_int()!=0;
      vm.call_function(start,turn);
      if(!alreadyRunning && running->get_int()!=0) {
        triaSelf = vm.global_self()->get_instance();
        triaSpeaker = triaSelf;
        }
      });
    vm.override_function("TRIA_NEXT", [this,next,running,invited,count](std::shared_ptr<zenkit::INpc> npc) {
      vm.call_function(next,npc);
      if(npc==nullptr || running->get_int()==0)
        return;
      for(int i=0;i<count->get_int() && i<invited->count();++i) {
        if(mem_ptrtoinst(ptr32_t(invited->get_int(uint16_t(i))))==npc) {
          triaSpeaker = npc;
          break;
          }
        }
      });
    vm.override_function("TRIA_FINISH", [this,finish]() {
      vm.call_function(finish);
      triaSelf.reset();
      triaSpeaker.reset();
      });
    }

  const ptr32_t OCNPC__GETSLOTITEM = 7544720;
  cpu.register_thiscall(OCNPC__GETSLOTITEM, [](ptr32_t pHero, std::string slot) {
    Log::e("LeGo: OCNPC__GETSLOTITEM(", slot, ")");
    return 0x0;
    });

  const ptr32_t OCNPC__EQUIPWEAPON = 7577648;
  cpu.register_thiscall(OCNPC__EQUIPWEAPON, [](ptr32_t pHero, ptr32_t pItem) {
    Log::e("LeGo: OCNPC__EQUIPWEAPON(", pItem, ")");
    });
  }

Npc& DirectMemory::dialogSpeaker(Npc& npc) {
  if(triaSelf.lock()!=npc.handlePtr())
    return npc;
  auto speaker = std::dynamic_pointer_cast<zenkit::INpc>(triaSpeaker.lock());
  if(speaker==nullptr || speaker->user_ptr==nullptr)
    return npc;
  return *static_cast<Npc*>(speaker->user_ptr);
  }

void DirectMemory::setupWorldFunctions() {
  // The script adds extra time through an unsupported 32-bit world timer.
  if(vm.find_symbol_by_name("SCALETIME"))
    vm.override_function("SCALETIME", [this](int percent) {
      if(auto hold=vm.find_symbol_by_name("HOLDTIME_ACTIVATED"); hold && hold->get_int()!=0)
        return;
      if(auto* session=Gothic::inst().gameSession())
        session->scaleWorldTime(frameDt,percent);
      });
  if(vm.find_symbol_by_name("SPELL_LOGIC_PICKLOCK")!=nullptr &&
     vm.find_symbol_by_name("SPL_PICKLOCK")!=nullptr) {
    // The script implements this using oCNpc/oCMobLockable memory and x86 hooks.
    // Use native targeting/locks while leaving casting and scroll use to Npc.
    vm.override_function("SPELL_LOGIC_PICKLOCK", [this, target=ptr32_t(0), partial=false, invested=0](int mana) mutable -> int {
      auto npc = gameScript.world().player();
      if(npc==nullptr || vm.global_self()->get_instance()!=npc->handlePtr())
        return SPL_SENDSTOP;
      auto& world = npc->world();
      auto focus = world.findFocus(*npc,Focus()).interactive;
      if(focus==nullptr || !focus->isLocked())
        return SPL_SENDSTOP;
      if(focus->pickLockCode().empty()) {
        if(auto msg = vm.find_symbol_by_name("PRINT_NEVEROPEN"))
          Gothic::inst().onPrint(msg->get_string());
        return SPL_SENDSTOP;
        }
      auto cost = vm.find_symbol_by_name("SPL_COST_PICKLOCK");
      const int required = cost!=nullptr ? cost->get_int() : 1;
      if(required<=0 || npc->attribute(ATR_MANA)<required)
        return SPL_SENDSTOP;
      if(mana==0) {
        target = focusVob(*focus);
        partial = focus->lockpickProgress()>0;
        invested = 0;
        return SPL_NEXTLEVEL;
        }
      if(target==0 || target!=focusVob(*focus))
        return SPL_SENDSTOP;
      if(mana%required!=0 || mana<=invested)
        return SPL_RECEIVEINVEST;
      invested = mana;
      world.sendPassivePerc(*npc,*npc,*npc,PERC_ASSESSUSEMOB);
      npc->emitSoundEffect("PICKLOCK_SUCCESS",2500,true);
      if(++focus->lockpickProgress()>=focus->pickLockCode().size()) {
        focus->setAsCracked(true);
        npc->changeAttribute(ATR_MANA,-required,false);
        if(auto msg = vm.find_symbol_by_name("PRINT_PICKLOCK_UNLOCK"))
          Gothic::inst().onPrint(msg->get_string());
        Gothic::inst().emitGlobalSound("MFX_PICKLOCK_CAST");
        if(partial)
          if(auto* achievement = vm.find_symbol_by_name("ACH_34")) {
            vm.call_function<void>("GAMESERVICES_UNLOCKACHIEVEMENT",std::string_view(achievement->get_string()));
            if(std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr)
              Log::i("[LOCK_PROBE] hybrid achievement=1");
            }
        return SPL_SENDCAST;
        }
      return SPL_RECEIVEINVEST;
      });
    }
  const ptr32_t OCWORLD__SEARCHVOBBYNAME_G2 = 7865872;
  cpu.register_thiscall(OCWORLD__SEARCHVOBBYNAME_G2, [](ptr32_t pWorld, std::string vob) {
    Log::e("LeGo: OCWORLD__SEARCHVOBBYNAME_G2(", vob, ")");
    });

  const ptr32_t OCWORLD__SEARCHVOBLISTBYNAME_G2 = 7866048;
  cpu.register_thiscall(OCWORLD__SEARCHVOBLISTBYNAME_G2, [](ptr32_t pWorld, std::string vob, ptr32_t pOutArr) {
    Log::e("LeGo: OCWORLD__SEARCHVOBLISTBYNAME_G2(", vob, ")");
    });
  }
