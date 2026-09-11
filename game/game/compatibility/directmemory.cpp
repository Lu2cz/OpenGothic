#include "directmemory.h"

#include <zenkit/DaedalusScript.hh>
#include <Tempest/Log>

#include <cassert>
#include <charconv>
#include <limits>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "world/focus.h"
#include "gothic.h"
#include "gamemusic.h"
#include "game/serialize.h"
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
  for(auto& i:focusVobs)
    if(i.native==&focus)
      return i.address;

  auto* cls = vm.find_symbol_by_name("OCMOBLOCKABLE");
  if(cls==nullptr || cls->class_size()==0)
    return 0;

  const auto address = mem32.alloc(cls->class_size(),"focused OCMOBLOCKABLE");
  focusVobs.push_back({&focus,address});
  return address;
  }

void DirectMemory::setNpcFocus(Npc& npc, Interactive* focus, int pickLockProgress) {
  npc.handle().focus_vob = 0;
  if(focus==nullptr)
    return;

  const auto address = focusVob(*focus);
  auto* bitfield = vm.find_symbol_by_name("OCMOBLOCKABLE.BITFIELD");
  if(address==0 || bitfield==nullptr)
    return;

  const auto at = address + ptr32_t(bitfield->offset_as_member());
  const int32_t state = mem32.readInt(at);
  mem32.writeInt(at,(state & int32_t(3)) | (pickLockProgress<<2) | (focus->isLocked() ? 1 : 0));
  npc.handle().focus_vob = int32_t(address);
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

void DirectMemory::save(Serialize& out) {
  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_PERSISTENCE_SAVE_FAILURE")!=nullptr)
    throw std::runtime_error("Injected compatibility save failure");
  out.setEntry("game/compatibility");
  // ponytail: a complete virtual-memory snapshot requires identical scripts and
  // mapping ABI. Bump this version when changing the native memory layout.
  out.write(uint32_t(1),scriptFingerprint,scriptVariables,scriptSymbols,ASMINT_InternalStack,musicThemePtr);
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
  }

void DirectMemory::load(Serialize& in) {
  resetMusicZone();
  focusVobs.clear();
  if(!in.setEntry("game/compatibility")) {
    restoreQuestCallbacks = true; // older saves have no heap to restore
    return;
    }
  uint32_t version = 0;
  uint64_t fingerprint = 0;
  in.read(version,fingerprint);
  if(version!=1 || fingerprint!=scriptFingerprint)
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
    if(type<Mem32::Type::firstScriptReference || !boundTypes.insert(type).second)
      throw std::runtime_error("Invalid compatibility binding type");
    bindReference(ref,context,type);
    if((context || !ref->is_member()) && scriptReferences.contains({context,id}))
      throw std::runtime_error("Duplicate compatibility reference");
    // Distinct deleted native objects can resolve to null. Keep each address
    // bound to a tombstone, including across subsequent saves.
    scriptReferences.emplace(std::make_pair(context,id),ptr);
  }
  mem32.validateCallbacks();
  if(auto* lastMob = vm.find_symbol_by_name("G_PICKLOCK.LASTMOB"))
    lastMob->set_int(0); // Native vob addresses are not valid after a reload.
  restoreQuestCallbacks = false;
  if(std::getenv("OPENGOTHIC_PERSISTENCE_PROBE")!=nullptr)
    persistenceProbeRoot = uint32_t(vm.find_symbol_by_name("MEM_INFOBOX.RES")->get_int());
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
    auto type = Mem32::Type(uint32_t(Mem32::Type::firstScriptReference)+uint32_t(scriptReferences.size())+1);
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
      type = Mem32::Type(uint32_t(Mem32::Type::firstScriptReference)+uint32_t(scriptReferences.size())+1);
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

  auto p = mem32.pin(&ASMINT_CallTarget, sizeof(ASMINT_CallTarget), "ASMINT_CallTarget");
  if(auto sym = vm.find_symbol_by_name("ASMINT_CallTarget")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(p));
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
      auto it = scriptReferences.find({context,ref->index()});
      if(it==scriptReferences.end())
        it = scriptReferences.emplace(std::make_pair(context,ref->index()),0);
      auto& ptr = it->second;
      if(ptr==0) {
        const auto type = Mem32::Type(uint32_t(Mem32::Type::firstScriptReference)+uint32_t(scriptReferences.size()));
        bindReference(ref,context,type);
        ptr = mem32.alloc(uint32_t(ref->count())*stride,type);
        }
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
  mem32.free(Mem32::ptr32_t(ptr));
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

  std::span<zenkit::DaedalusSymbol> params = vm.find_parameters_for_function(&func);
  if(params.size()>0)
    Log::d("");
  //vm.call_function(sym);
  //zenkit::StackGuard guard {&vm, func.rtype()};
  vm.unsafe_call(&func);
  // The opt-in regression uses the original TIMER_SETPAUSE script as a callback.
  // Count each dispatch, including multiple calls in one frame, then undo the pause.
  if(persistenceProbeRoot!=0 && func.name()=="TIMER_SETPAUSE") {
    auto* argument = vm.find_symbol_by_name("TIMER_SETPAUSE.ON");
    if(uint32_t(argument->get_int())==persistenceProbeRoot) {
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
  const int ZCVIEW__OPEN       = 8023040;
  const int ZCVIEW__CLOSE      = 8023600;
  const int ZCVIEW_TOP         = 8021904;
  const int ZCVIEW__SETSIZE    = 8026016;
  const int zCVIEW__MOVE       = 8025824;
  const int ZCVIEW__INSERTBACK = 8020272;
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
    });

  cpu.register_thiscall(ZCVIEW__OPEN, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Open - unable to resolve address");
      return;
      }
    Log::e("LeGo: zCView__Open");
    });

  cpu.register_thiscall(ZCVIEW__CLOSE, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Close - unable to resolve address");
      return;
      }
    });

  cpu.register_thiscall(ZCVIEW_TOP, [this](ptr32_t ptr) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__Top - unable to resolve address");
      return;
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
    view->VPOSX  = x;
    view->VPOSY  = y;
    });

  cpu.register_thiscall(ZCVIEW__INSERTBACK, [this](ptr32_t ptr, std::string img) {
    auto view = mem32.deref<zCView>(ptr);
    if(view==nullptr) {
      Log::e("LeGo: zCView__InsertBack - unable to resolve address");
      return;
      }
    Log::e("LeGo: zCView__InsertBack: ", img);
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
  cpu.register_thiscall(ZCFONTMAN__LOAD, [](ptr32_t ptr, std::string font) {
    Log::e("LeGo: zCFontMan__Load");
    return 1234;
    });
  cpu.register_thiscall(ZCFONTMAN__GETFONT, [](ptr32_t ptr, int handle) {
    Log::e("LeGo: zCFontMan__GetFont");
    return handle;
    });
  }

void DirectMemory::tickUi(uint64_t dt) {
  if(auto* vScreen = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    //TODO: report correct screen size
    vScreen->PSIZEX = 800;
    vScreen->PSIZEY = 600;
    }

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
  if(vm.find_symbol_by_name("SPELL_LOGIC_PICKLOCK")!=nullptr &&
     vm.find_symbol_by_name("SPL_PICKLOCK")!=nullptr) {
    // The script implements this using oCNpc/oCMobLockable memory and x86 hooks.
    // Use native targeting/locks while leaving casting and scroll use to Npc.
    vm.override_function("SPELL_LOGIC_PICKLOCK", [this, target=static_cast<Interactive*>(nullptr)](int mana) mutable -> int {
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
        target = focus;
        return SPL_NEXTLEVEL;
        }
      if(target!=focus)
        return SPL_SENDSTOP;
      if(mana%required!=0)
        return SPL_RECEIVEINVEST;
      world.sendPassivePerc(*npc,*npc,*npc,PERC_ASSESSUSEMOB);
      npc->emitSoundEffect("PICKLOCK_SUCCESS",2500,true);
      // ponytail: progress is per cast; partial conventional lockpicking and its
      // hybrid achievement need shared per-lock progress before they can combine.
      if(size_t(mana/required)>=focus->pickLockCode().size()) {
        focus->setAsCracked(true);
        npc->changeAttribute(ATR_MANA,-required,false);
        if(auto msg = vm.find_symbol_by_name("PRINT_PICKLOCK_UNLOCK"))
          Gothic::inst().onPrint(msg->get_string());
        Gothic::inst().emitGlobalSound("MFX_PICKLOCK_CAST");
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
