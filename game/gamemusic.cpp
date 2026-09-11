#include "gamemusic.h"
#include "gothic.h"

#include <Tempest/Sound>
#include <Tempest/Log>
#include <Tempest/Application>
#include <Tempest/MemReader>

#include "game/definitions/musicdefinitions.h"
#include "dmusic/mixer.h"
#include "resources.h"
#include "dmusic.h"

using namespace Tempest;

static constexpr uint16_t SAMPLE_RATE = 44100;

struct GameMusic::MusicProvider : Tempest::SoundProducer {
  using Tempest::SoundProducer::SoundProducer;

  public:
    virtual void stopTheme() {
      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      pendingMusic.reset();
      }

    void setEnabled(bool b) {
      if(enable == b)
        return;

      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      if(b) {
        hasPending = true;
        reloadTheme = true;
        enable.store(true);
        } else {
        enable.store(false);
        stopTheme();
        }
      }

    bool isEnabled() const {
      return enable.load();
      }

    void playTheme(const zenkit::IMusicTheme &theme, GameMusic::Tags tags) {
      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      reloadTheme  = !pendingMusic || pendingMusic->file != theme.file;
      pendingMusic = theme;
      pendingTags  = tags;
      hasPending   = true;
      }

  protected:
    bool updateTheme(zenkit::IMusicTheme& theme, Tags& tags) {
      bool reloadTheme = false;

      std::lock_guard<std::recursive_mutex> guard(pendingSync);
      if(hasPending && pendingMusic && enable.load()) {
        hasPending  = false;
        reloadTheme = this->reloadTheme;
        theme       = this->pendingMusic.value();
        tags        = this->pendingTags;
        }

      this->reloadTheme = false;
      return reloadTheme;
      }

    static zenkit::MusicTransitionEffect computeTransitionEffect(Tags nextTags, Tags currTags, zenkit::MusicTransitionEffect transtype) {
      const int cur  = currTags & (Tags::Std | Tags::Fgt | Tags::Thr);
      const int next = nextTags & (Tags::Std | Tags::Fgt | Tags::Thr);

      //zenkit::MusicTransitionEffect embellishment = transtype;
      zenkit::MusicTransitionEffect embellishment = zenkit::MusicTransitionEffect::NONE;
      if(next == Tags::Std) {
        if(cur != Tags::Std)
          embellishment = zenkit::MusicTransitionEffect::BREAK;
        }
      else if(next == Tags::Fgt) {
        if(cur == Tags::Thr)
          embellishment = zenkit::MusicTransitionEffect::FILL;
        }
      else if(next == Tags::Thr) {
        if(cur == Tags::Fgt)
          embellishment = zenkit::MusicTransitionEffect::NONE;
        }

      return embellishment;
      }

  private:
    std::atomic_bool     enable{true};

    std::recursive_mutex pendingSync;
    bool                 hasPending  = false;
    bool                 reloadTheme = false;
    Tags                 pendingTags = Tags::Day;
    std::optional<zenkit::IMusicTheme> pendingMusic;

  protected:
    Tags                 currentTags = Tags::Day;
  };

struct GameMusic::OpenGothicMusicProvider : GameMusic::MusicProvider {
  using GameMusic::MusicProvider::MusicProvider;

  void renderSound(int16_t *out, size_t n) override {
    if(!isEnabled()) {
      std::memset(out, 0, n * sizeof(int32_t) * 2);
      return;
      }
    updateTheme();
    mix.mix(out, n);
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    Tags                tags;
    if(!GameMusic::MusicProvider::updateTheme(theme, tags))
      return;

    if(theme.file.empty()) {
      stopTheme();
      return;
      }

    try {
      if(/*reloadTheme*/true) {
        Dx8::PatternList p = Resources::loadDxMusic(theme.file);

        Dx8::Music m;
        m.addPattern(p);

        auto effect = computeTransitionEffect(tags, currentTags, theme.transtype);
        Dx8::DMUS_EMBELLISHT_TYPES em = computeEmbellishment(effect);
        mix.setMusic(m, em);
        currentTags = tags;
        }
      mix.setMusicVolume(theme.vol);
      }
    catch (std::runtime_error &) {
      Log::e("unable to load sound: \"", theme.file, "\"");
      stopTheme();
      }
    catch (std::bad_alloc &) {
      Log::e("out of memory for sound: \"", theme.file, "\"");
      stopTheme();
      }
    }

  void stopTheme() override {
    GameMusic::MusicProvider::stopTheme();
    }

  static Dx8::DMUS_EMBELLISHT_TYPES computeEmbellishment(zenkit::MusicTransitionEffect ef) {
    switch (ef) {
      case zenkit::MusicTransitionEffect::UNKNOWN:
      case zenkit::MusicTransitionEffect::NONE:
        return Dx8::DMUS_EMBELLISHT_NORMAL;
      case zenkit::MusicTransitionEffect::GROOVE:
        return Dx8::DMUS_EMBELLISHT_NORMAL;
      case zenkit::MusicTransitionEffect::FILL:
        return Dx8::DMUS_EMBELLISHT_FILL;
      case zenkit::MusicTransitionEffect::BREAK:
        return Dx8::DMUS_EMBELLISHT_BREAK;
      case zenkit::MusicTransitionEffect::INTRO:
        return Dx8::DMUS_EMBELLISHT_INTRO;
      case zenkit::MusicTransitionEffect::END:
      case zenkit::MusicTransitionEffect::END_AND_INTO:
        return Dx8::DMUS_EMBELLISHT_END;
      }
    return Dx8::DMUS_EMBELLISHT_NORMAL;
    }

  private:
    Dx8::Mixer mix;
  };

static std::pair<DmTiming, DmEmbellishmentType> computeEmbellishmentAndTiming(const zenkit::MusicTransitionEffect effect, const zenkit::MusicTransitionType type) {
  DmEmbellishmentType embellishment = DmEmbellishment_NONE;
  switch (effect) {
    case zenkit::MusicTransitionEffect::UNKNOWN:
    case zenkit::MusicTransitionEffect::NONE:
      embellishment = DmEmbellishment_NONE;
      break;
    case zenkit::MusicTransitionEffect::GROOVE:
      embellishment = DmEmbellishment_GROOVE;
      break;
    case zenkit::MusicTransitionEffect::FILL:
      embellishment = DmEmbellishment_FILL;
      break;
    case zenkit::MusicTransitionEffect::BREAK:
      embellishment = DmEmbellishment_BREAK;
      break;
    case zenkit::MusicTransitionEffect::INTRO:
      embellishment = DmEmbellishment_INTRO;
      break;
    case zenkit::MusicTransitionEffect::END:
      embellishment = DmEmbellishment_END;
      break;
    case zenkit::MusicTransitionEffect::END_AND_INTO:
      embellishment = DmEmbellishment_END_AND_INTRO;
      break;
    }

  DmTiming timing = DmTiming_MEASURE;
  switch (type) {
    case zenkit::MusicTransitionType::UNKNOWN:
    case zenkit::MusicTransitionType::MEASURE:
      timing = DmTiming_MEASURE;
      break;
    case zenkit::MusicTransitionType::IMMEDIATE:
      timing = DmTiming_INSTANT;
      break;
    case zenkit::MusicTransitionType::BEAT:
      timing = DmTiming_BEAT;
      break;
    }

  return std::make_pair(timing, embellishment);
  }

struct GameMusic::GothicKitMusicProvider : GameMusic::MusicProvider {
  GothicKitMusicProvider(uint16_t rate, uint16_t channels) : GameMusic::MusicProvider(rate, channels) {
    /*
    Dm_setRandomNumberGenerator([](void*) -> uint32_t {
      return 0;
      }, nullptr);
    */

    DmResult rv = DmPerformance_create(&performance, rate);
    if(rv != DmResult_SUCCESS) {
      Log::e("Unable to create DmPerformance object. Out of memory?");
      }
    }

  ~GothicKitMusicProvider() override {
    DmPerformance_release(performance);
    }

  void renderSound(int16_t *out, size_t n) override {
    if(!isEnabled()) {
      std::memset(out, 0, n * sizeof(int32_t) * 2);
      return;
      }
    updateTheme();
    DmPerformance_renderPcm(performance, out, n * 2, DmRenderOptions(DmRender_SHORT | DmRender_STEREO));
    }

  void updateTheme() {
    zenkit::IMusicTheme theme;
    Tags                tags;
    if(!GameMusic::MusicProvider::updateTheme(theme, tags))
      return;

    if(theme.file.empty()) {
      stopTheme();
      return;
      }

    auto effect = computeTransitionEffect(tags, currentTags, theme.transtype);
    auto [timing, embellishment] = computeEmbellishmentAndTiming(effect, theme.transsubtype);

    DmSegment* sgt = Resources::loadMusicSegment(theme.file.c_str());
    DmResult rv = DmPerformance_playTransition(performance, sgt, embellishment, timing);
    if(rv != DmResult_SUCCESS) {
      Log::e("Failed to play theme: ", theme.file);
      stopTheme();
      }

    DmPerformance_setVolume(performance, theme.vol);
    DmSegment_release(sgt);
    currentTags = tags;
    }

  void stopTheme() override {
    GameMusic::MusicProvider::stopTheme();
    DmPerformance_playTransition(performance, nullptr, DmEmbellishment_NONE, DmTiming_INSTANT);
    }

  DmEmbellishmentType computeEmbellishment(Tags nextTags, Tags currTags, zenkit::MusicTransitionEffect transtype) const {
    const int cur  = currTags & (Tags::Std | Tags::Fgt | Tags::Thr);
    const int next = nextTags & (Tags::Std | Tags::Fgt | Tags::Thr);

    DmEmbellishmentType embellishment = DmEmbellishment_NONE;
    switch (transtype) {
      case zenkit::MusicTransitionEffect::UNKNOWN:
      case zenkit::MusicTransitionEffect::NONE:
        embellishment = DmEmbellishment_NONE;
        break;
      case zenkit::MusicTransitionEffect::GROOVE:
        embellishment = DmEmbellishment_GROOVE;
        break;
      case zenkit::MusicTransitionEffect::FILL:
        embellishment = DmEmbellishment_FILL;
        break;
      case zenkit::MusicTransitionEffect::BREAK:
        embellishment = DmEmbellishment_BREAK;
        break;
      case zenkit::MusicTransitionEffect::INTRO:
        embellishment = DmEmbellishment_INTRO;
        break;
      case zenkit::MusicTransitionEffect::END:
        embellishment = DmEmbellishment_END;
        break;
      case zenkit::MusicTransitionEffect::END_AND_INTO:
        embellishment = DmEmbellishment_END_AND_INTRO;
        break;
      }

    if(next == Tags::Std) {
      if(cur != Tags::Std)
        embellishment = DmEmbellishmentType::DmEmbellishment_BREAK;
      }
    else if(next == Tags::Fgt) {
      if(cur == Tags::Thr)
        embellishment = DmEmbellishmentType::DmEmbellishment_FILL;
      }
    else if(next == Tags::Thr) {
      if(cur == Tags::Fgt)
        embellishment = DmEmbellishmentType::DmEmbellishment_NONE;
      }

    return embellishment;
    }

  private:
    DmPerformance *performance = nullptr;
  };

static constexpr int PROVIDER_UNINITIALIZED = -1;
static constexpr int PROVIDER_OPENGOTHIC = 0;
// static constexpr int PROVIDER_GOTHICKIT = 1;

GameMusic *GameMusic::instance = nullptr;

GameMusic::GameMusic() {
  instance = this;
  provider = PROVIDER_UNINITIALIZED;

  Gothic::inst().onSettingsChanged.bind(this, &GameMusic::setupSettings);
  setupSettings();
  }

GameMusic::~GameMusic() {
  fileTails.clear();
  fileSound = SoundEffect();
  sound = SoundEffect();
  instance = nullptr;
  Gothic::inst().onSettingsChanged.ubind(this, &GameMusic::setupSettings);
  }

GameMusic &GameMusic::inst() {
  return *instance;
  }

GameMusic::Tags GameMusic::mkTags(GameMusic::Tags daytime, GameMusic::Tags mode) {
  return Tags(daytime | mode);
  }

void GameMusic::setEnabled(bool e) {
  enabled = e;
  impl->setEnabled(e && fileTheme.file.empty());
  tick();
  }

bool GameMusic::isEnabled() const {
  return enabled;
  }

void GameMusic::setMusic(GameMusic::Music m) {
  const char *clsTheme = "";
  switch (m) {
    case GameMusic::SysLoading:
      clsTheme = "SYS_Loading";
      break;
    }
  if(auto theme = Gothic::musicDef()[clsTheme])
    setMusic(*theme, GameMusic::mkTags(GameMusic::Std, GameMusic::Day));
  }

void GameMusic::setMusic(const zenkit::IMusicTheme &theme, Tags tags) {
  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_MUSIC_PROBE")!=nullptr &&
     currentMusic.theme.file!=theme.file)
    Log::i("[MUSIC_PROBE] legacy theme=",theme.file);
  fileTheme = {};
  requestedFile.clear();
  pendingFile = false;
  fileTails.clear();
  fileSound = SoundEffect();
  fileBuffer.reset();
  currentMusic.theme = theme;
  currentMusic.tags  = tags;
  impl->playTheme(currentMusic.theme, currentMusic.tags);
  impl->setEnabled(enabled);
  }

void GameMusic::setMusic(const FileTheme& theme) {
  if(theme.file==requestedFile)
    return;
  requestedFile = theme.file;
  nextFileTheme = theme;
  pendingFile = true;
  }

void GameMusic::loadFileMusic() {
  if(fileLoad.valid() && fileLoad.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    try {
      auto buffer = fileLoad.get();
      if(buffer.isEmpty() || buffer.timeLength()==0)
        throw std::runtime_error("Empty music stream");
      if(loadingFileTheme.file==requestedFile) {
        pendingFile = false;
        // Transition with the outgoing track's fade, as KmLib does.
        if(!fileSound.isEmpty()) {
          const auto now = Application::tickCount();
          const auto fade = fileFadeIn==0 ? 1.f : std::min(1.f,float(now-fileStarted)/float(fileFadeIn));
          fileTails.push_back({std::move(fileSound), now, fileTheme.fadeOut, fade, true});
          }
        fileSound = SoundEffect();
        fileTheme = nextFileTheme;
        fileBuffer.emplace(std::move(buffer));
        impl->setEnabled(false);
        startFileMusic(false);
        }
      }
    catch(const std::exception& e) {
      Log::e("Unable to load music ",loadingFileTheme.file,": ",e.what());
      }
    }
  if(!pendingFile || fileLoad.valid())
    return;
  pendingFile = false;
  if(requestedFile==fileTheme.file)
    return;
  loadingFileTheme = nextFileTheme;
  // Own the input before dispatch: decoding never holds the Resources lock or
  // accesses the VFS while another world is loading or shutting down.
  auto bytes = Resources::getFileData(requestedFile);
  fileLoad = std::async(std::launch::async, [bytes=std::move(bytes), name=requestedFile]() {
    const auto started = Application::tickCount();
    Tempest::MemReader input(bytes.data(),bytes.size());
    Tempest::Sound buffer(input);
    if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_MUSIC_PROBE")!=nullptr)
      Log::i("[MUSIC_PROBE] decode file=",name," ms=",Application::tickCount()-started," background=1");
    return buffer;
    });
  }

void GameMusic::startFileMusic(bool loop) {
  if(loop && !fileSound.isEmpty())
    fileTails.push_back({std::move(fileSound), 0, 0, 1.f, false});
  fileSound = device.load(*fileBuffer);
  fileStarted = Application::tickCount();
  fileFadeIn = loop ? 0 : fileTheme.fadeIn;
  fileSound.setVolume(enabled ? volume : 0.f);
  if(!loop && fileTheme.fadeIn>0)
    fileSound.setVolume(0.f);
  fileSound.play();
  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_MUSIC_PROBE")!=nullptr)
    Log::i("[MUSIC_PROBE] playing file=",fileTheme.file," length=",fileSound.timeLength(),
           " overlap=",fileTheme.loopOverlap," loop=",loop);
  }

void GameMusic::tick() {
  loadFileMusic();
  if(fileTheme.file.empty())
    return;
  const auto length = fileSound.timeLength();
  const auto overlap = fileTheme.loopOverlap<length ? fileTheme.loopOverlap : 0;
  if(fileSound.isFinished() || fileSound.currentTime()>=length-overlap)
    startFileMusic(true);
  const auto now = Application::tickCount();
  const float gain = enabled ? volume : 0.f;
  float fadeIn = fileFadeIn==0 ? 1.f : std::min(1.f, float(now-fileStarted)/float(fileFadeIn));
  fileSound.setVolume(gain*fadeIn);
  for(size_t i=0; i<fileTails.size();) {
    auto& tail = fileTails[i];
    const auto elapsed = now-tail.start;
    if(tail.sound.isFinished() || (tail.fade && elapsed>=tail.duration)) {
      fileTails.erase(fileTails.begin()+int(i));
      continue;
      }
    const float fade = tail.fade ? 1.f-float(elapsed)/float(tail.duration) : 1.f;
    tail.sound.setVolume(gain*tail.volume*fade);
    ++i;
    }
  }

void GameMusic::traceFileMusic() const {
  Log::i("[MUSIC_PROBE] state file=",fileTheme.file," position=",fileSound.currentTime(),
         " gain=",fileSound.volume()," enabled=",enabled," tails=",fileTails.size(),
         " finished=",fileSound.isFinished()," legacy=",impl->isEnabled());
  }

void GameMusic::stopMusic() {
  setEnabled(false);
  }

void GameMusic::setupSettings() {
  const int   musicEnabled  = Gothic::settingsGetI("SOUND",    "musicEnabled");
  const float musicVolume   = Gothic::settingsGetF("SOUND",    "musicVolume");
  const int   providerIndex = Gothic::settingsGetI("INTERNAL", "soundProviderIndex");

  volume = std::clamp(musicVolume,0.f,1.f);

  if(providerIndex != provider || musicEnabled!=isEnabled()) {
    Log::i("Switching music provider to ", providerIndex == PROVIDER_OPENGOTHIC ? "'OpenGothic'" : "'GothicKit'");
    sound = SoundEffect();

    std::unique_ptr<MusicProvider> p;
    if(providerIndex == PROVIDER_OPENGOTHIC) {
      p = std::make_unique<OpenGothicMusicProvider>(SAMPLE_RATE, 2);
      } else {
      p = std::make_unique<GothicKitMusicProvider>(SAMPLE_RATE, 2);
      }

    provider = providerIndex;
    impl = p.get();
    impl->playTheme(currentMusic.theme, currentMusic.tags);

    sound = device.load(std::move(p));
    sound.play();
    }

  setEnabled(musicEnabled != 0);
  sound.setVolume(volume);
  }
