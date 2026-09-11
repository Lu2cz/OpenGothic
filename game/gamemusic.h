#pragma once

#include <zenkit/addon/daedalus.hh>

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <optional>
#include <vector>
#include <future>

class GameMusic final {
  public:
    GameMusic();
    GameMusic(const GameMusic&)=delete;
    ~GameMusic();

    static GameMusic& inst();

    enum Music : uint8_t {
      SysLoading
      };

    enum Tags : uint8_t {
      Day = 0,
      Ngt = 1<<0,

      Std = 0,
      Fgt = 1<<1,
      Thr = 1<<2
      };

    static Tags mkTags(Tags daytime,Tags mode);

    void      setEnabled(bool e);
    bool      isEnabled() const;
    void      setMusic(Music m);
    void      setMusic(const zenkit::IMusicTheme &theme, Tags t);
    void      stopMusic();

    struct FileTheme {
      std::string file;
      uint64_t loopOverlap = 0, fadeIn = 0, fadeOut = 0;
      };
    void      setMusic(const FileTheme& theme);
    void      tick();
    void      traceFileMusic() const;

  private:
    struct MusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;

    void      setupSettings();
    void      startFileMusic(bool loop);
    void      loadFileMusic();

    static GameMusic* instance;

    struct {
      zenkit::IMusicTheme theme = {};
      Tags                tags  = Tags::Std;
      } currentMusic;

    int                  provider = -1;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;

    // ponytail: buffer the current track and fading tails; stream if long tracks
    // cause excessive memory use. Decode off-thread, with only one load in flight.
    FileTheme            fileTheme;
    std::string          requestedFile;
    FileTheme            nextFileTheme, loadingFileTheme;
    bool                 pendingFile = false;
    std::future<Tempest::Sound> fileLoad;
    std::optional<Tempest::Sound> fileBuffer;
    Tempest::SoundEffect fileSound;
    struct Tail {
      Tempest::SoundEffect sound;
      uint64_t start = 0, duration = 0;
      float volume = 1;
      bool fade = false;
      };
    std::vector<Tail>     fileTails;
    uint64_t             fileStarted = 0;
    uint64_t             fileFadeIn = 0;
    float                volume = 0.5f;
    bool                 enabled = true;
  };
