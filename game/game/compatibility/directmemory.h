#pragma once

#include <zenkit/DaedalusScript.hh>
#include <map>
#include <unordered_map>
#include <vector>

#include "game/gamescript.h"

#include "cpu32.h"
#include "mem32.h"

class Interactive;
class Npc;
namespace Tempest {
class Painter;
}

class DirectMemory {
  public:
    DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm);

    static bool isRequired(zenkit::DaedalusScript& vm);

    auto        demangleAddress(uint32_t addr) -> std::string_view;

    //
    auto        menuMain() const -> std::string_view;

    // hooks
    void        tick(uint64_t dt);
    void        drawUi(Tempest::Painter& p, int width, int height);
    int         focusBarY(int height);
    void        eventPlayAni(std::string_view ani);
    Npc&        dialogSpeaker(Npc& npc);
    bool        setMusicZone(std::string_view zone, uint8_t tags);
    void        setNpcFocus(Npc& npc, Interactive* focus, int pickLockProgress);
    void        setNpcFocus(Npc& npc, Npc* focus);
    void        clearNpcFocus(Npc& npc);
    void        resetWorldReferences();
    void        probeLockFocus(Npc& npc, Interactive& lock, bool restored);
    void        beginWorldTransitionProbe(Npc& npc, Interactive& lock);
    void        checkWorldTransitionProbe(Npc& npc, Interactive* returnedLock);
    void        verifyWorldTransitionProbe(Npc& npc);
    void        probePersistence(bool finish);
    void        save(Serialize& out);
    void        load(Serialize& in);

  private:
    using ptr32_t      = Mem32::ptr32_t;
    using zCParser     = Compatibility::zCParser;
    using zCPar_Symbol = Compatibility::zCPar_Symbol;
    using zCTimer      = Compatibility::zCTimer;
    using oGame        = Compatibility::oGame;
    using zString      = Compatibility::zString;

    struct memory_instance : public zenkit::DaedalusTransientInstance {
      explicit memory_instance(DirectMemory &owner, ptr32_t address) : owner(owner), address(address) {}

      void set_int(zenkit::DaedalusSymbol const& sym, uint16_t index, std::int32_t value) override;
      std::int32_t get_int(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_float(zenkit::DaedalusSymbol const& sym, uint16_t index, float value) override;
      float get_float(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      void set_string(zenkit::DaedalusSymbol const& sym, uint16_t index, std::string_view value) override;
      const std::string& get_string(zenkit::DaedalusSymbol const& sym, uint16_t index) const override;

      DirectMemory& owner;
      ptr32_t       address = 0;
      };

    struct ScriptVar {
      zString data = {};
      };

    GameScript&         gameScript;
    zenkit::DaedalusVm& vm;
    Mem32               mem32;
    Cpu32               cpu;
    uint64_t            scriptFingerprint = 0;

    bool        restoreQuestCallbacks = false;
    ptr32_t     persistenceProbeRoot = 0;
    ptr32_t     worldProbeReference  = 0;
    ptr32_t     worldProbeItems[2]   = {};
    ptr32_t     worldProbeTimer      = 0;
    ptr32_t     worldProbeRecurringTimer = 0;
    uint32_t    worldProbeDispatches = 0;
    uint32_t    worldProbeRecurringDispatches = 0;
    uint32_t    worldProbeRecurringLastElapsed = 0;
    uint64_t    worldProbeInventory  = 0;
    std::weak_ptr<zenkit::DaedalusInstance> triaSelf, triaSpeaker;

    uint32_t    versionHint     = 504628679; // G2
    //int32_t     invMaxItems     = 9;
    zCTimer     zTimer          = {};
    oGame       memGame         = {};
    char        menuName[17]    = {};   // 16 bytes for script + null-charater, invisible to the script
    float       spawnRange      = 1000; // 10 meters, for now

    ptr32_t     oGame_Pointer   = 0;
    ptr32_t     gameman_Ptr     = 0;
    ptr32_t     zFactory_Ptr    = 0;
    ptr32_t     fontMan_Ptr     = 0;
    std::map<std::weak_ptr<zenkit::INpc>,ptr32_t,std::owner_less<>> focusNpcAddress;
    std::unordered_map<ptr32_t,std::weak_ptr<zenkit::INpc>> focusNpc;
    ptr32_t     focusList[6]    = {};

    ptr32_t     scriptVariables = 0;
    ptr32_t     scriptSymbols   = 0;
    std::multimap<std::pair<std::shared_ptr<zenkit::DaedalusInstance>,uint32_t>,ptr32_t> scriptReferences;
    auto        nativeReference(zenkit::DaedalusSymbol* ref, std::shared_ptr<zenkit::DaedalusInstance> context) -> ptr32_t;
    void        bindReference(zenkit::DaedalusSymbol* ref, std::shared_ptr<zenkit::DaedalusInstance> context, Mem32::Type type);
    bool        isLiveNpc(const std::shared_ptr<zenkit::INpc>& npc) const;
    void        pruneFocusNpcs();
    auto        focusVob(Interactive& focus) -> ptr32_t;
    void        saveReference(Serialize& out, const std::shared_ptr<zenkit::DaedalusInstance>& instance);
    auto        loadReference(Serialize& in) -> std::shared_ptr<zenkit::DaedalusInstance>;

    void        setupFunctionTable();
    void        setupIkarusLoops();
    void        setupEngineMemory();
    void        setupEngineText();

    auto        repeat(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        while_(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_goto(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    void        loop_trap(zenkit::DaedalusSymbol* i);

    uint32_t    traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc);
    auto        findSymbolByAddress(uint32_t addr) -> const zenkit::DaedalusSymbol*;

    struct LPayload {
      zenkit::DaedalusSymbol* i   = nullptr;
      int32_t                 len = 0;
      };
    std::unordered_map<uint32_t,LPayload>      loopPayload;
    std::unordered_map<uint32_t,uint32_t>      loopBacktrack;
    std::vector<const zenkit::DaedalusSymbol*> symbolsByAddress;

    // memory mappings
    void        memoryCallback(zCParser& p, std::memory_order ord);
    void        memoryCallback(ScriptVar& v, uint32_t index, std::memory_order ord);
    void        memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord);
    void        memAssignString(zString& str, std::string_view cstr);
    void        memFromString(std::string& dst, const zString& str);

    // stacktrace & logs
    void        memPrintstacktraceImplementation();
    void        memSendToSpy(int cat, std::string_view msg);
    void        memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func);

    ptr32_t     ASMINT_InternalStack = 0;
    ptr32_t     ASMINT_CallTarget    = 0;
    ptr32_t     ASMINT_CallTargetPtr = 0;
    void        ASMINT_Init();
    void        ASMINT_CallMyExternal();

    // pointers
    void        setupMemoryFunctions();
    auto        mem_ptrtoinst(ptr32_t address) -> std::shared_ptr<zenkit::DaedalusInstance>;
    int         mem_insttoptr(int index);
    auto        _takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    int         mem_readint       (int address);
    void        mem_writeint      (int address, int val);
    void        mem_copybytes     (int src, int dst, int size);
    std::string mem_readstring    (int address);
    auto        mem_readstatarr   (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_writestatarr  (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // pointers: MEM_Alloc and MEM_Free ##
    int         mem_alloc  (int amount);
    int         mem_alloc  (int amount, const char* comment);
    void        mem_free   (int address);
    int         mem_realloc(int address, int oldsz, int size);

    // mem-func
    void        setupDirectFunctions();
    int         mem_getfuncidbyoffset(int off);
    void        mem_assigninst       (int index, int ptr);

    void        directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func);
    auto        mem_callbyid (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_callbyptr(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    // ## Direct calls by id/ptr
    auto        memint_stackpushint (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //TODO: MEMINT_StackPushString
    auto        memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        memint_stackpushvar (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //
    auto        memint_popstring    (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    //---
    auto        mem_popintresult   (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;
    auto        mem_popinstresult  (zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall;

    // math
    void        setupMathFunctions();
    static int  mkf(int v);
    static int  truncf(int v);
    static int  roundf(int v);
    static int  addf(int a, int b);
    static int  subf(int a, int b);
    static int  mulf(int a, int b);
    static int  divf(int a, int b);

    // strings
    void        setupStringsFunctions();
    // winapi
    void        setupWinapiFunctions();
    void        setupUtilityFunctions();
    void        setupHookEngine();
    void        setupzCParserFunctions();
    void        setupInitFileFunctions();
    // gothic-ui
    void        setupUiFunctions();
    void        setupFontFunctions();
    void        tickUi(uint64_t dt);
    void        setUiSize(int width, int height);
    struct UiView {
      std::string texture;
      };
    std::unordered_map<ptr32_t,UiView> uiViews;
    std::vector<ptr32_t> uiViewOrder;
    std::unordered_map<ptr32_t,std::string> fontNames;
    ptr32_t     nextFontHandle = 0x10000000;
    int         uiWidth = 800;
    int         uiHeight = 600;
    //
    void        setupNpcFunctions();
    void        setupWorldFunctions();
    void        setupKmLibFunctions();
    void        setupMusicFunctions();
    void        notifyMusicZone();
    void        resetMusicZone();
    void        updateMusic();
    zenkit::DaedalusSymbol* musicOverride = nullptr;
    std::shared_ptr<zenkit::DaedalusInstance> musicZone;
    uint8_t     musicTags = 0;
    int32_t     musicTrack = 0;
    std::string musicZoneName, musicThemeName;
    zString     musicThemeString = {};
    ptr32_t     musicThemePtr = 0;
  };
