#pragma once

#include <string_view>

#include "abstracttrigger.h"

class ZoneTrigger : public AbstractTrigger {
  public:
    ZoneTrigger(Vob* parent, World& world, const zenkit::VTriggerChangeLevel& data, Flags flags);

    void onIntersect(Npc& n) override;
    bool changesTo(std::string_view world) const { return levelName==world; }
    std::string_view startVob() const { return startVobName; }

  private:
    std::string levelName;
    std::string startVobName;
  };
