#pragma once

#include <string>

#include "activities/Activity.h"
#include "factory_core.h"

class FactoryActivity final : public Activity {
 private:
  fgame::Game game;

  int curX = fgame::GRID_W / 2;
  int curY = fgame::GRID_H / 2;
  bool onToolbar = false;  // курсор на верхней панели
  int toolCol = 0;         // 0 = выбор постройки, 1 = шаги, 2 = заново
  int tool = 0;            // индекс в списке построек
  uint8_t placeDir = 1;    // направление новых построек = последнее движение курсора

 public:
  explicit FactoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Factory", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
