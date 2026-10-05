#include "FactoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int TOOL_COUNT = 5;
constexpr const char* TOOL_NAMES[TOOL_COUNT] = {"Drill", "Belt", "Furnace", "Chest", "Erase"};
constexpr uint8_t TOOL_KINDS[TOOL_COUNT] = {fgame::KIND_DRILL, fgame::KIND_BELT, fgame::KIND_FURNACE,
                                            fgame::KIND_CHEST, fgame::KIND_EMPTY};
constexpr int TOOL_ERASE = 4;
constexpr int STEPS_PER_PRESS = 5;
}  // namespace

void FactoryActivity::onEnter() {
  Activity::onEnter();
  game.init();
  curX = fgame::GRID_W / 2;
  curY = fgame::GRID_H / 2;
  onToolbar = false;
  toolCol = 0;
  tool = 0;
  placeDir = 1;
  requestUpdate();
}

void FactoryActivity::onExit() { Activity::onExit(); }

void FactoryActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(Button::Up)) {
    if (onToolbar) {
      onToolbar = false;
      curY = fgame::GRID_H - 1;
    } else if (curY == 0) {
      onToolbar = true;
    } else {
      curY--;
    }
    placeDir = 0;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Down)) {
    if (onToolbar) {
      onToolbar = false;
      curY = 0;
    } else if (curY == fgame::GRID_H - 1) {
      onToolbar = true;
    } else {
      curY++;
    }
    placeDir = 2;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Left)) {
    if (onToolbar) {
      toolCol = (toolCol + 2) % 3;
    } else {
      curX = (curX + fgame::GRID_W - 1) % fgame::GRID_W;
    }
    placeDir = 3;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Right)) {
    if (onToolbar) {
      toolCol = (toolCol + 1) % 3;
    } else {
      curX = (curX + 1) % fgame::GRID_W;
    }
    placeDir = 1;
    requestUpdate();
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    if (onToolbar) {
      if (toolCol == 0) {
        tool = (tool + 1) % TOOL_COUNT;
      } else if (toolCol == 1) {
        for (int i = 0; i < STEPS_PER_PRESS; i++) game.step();
      } else {
        game.init();
      }
    } else {
      fgame::Cell& cell = game.c[curY][curX];
      if (tool == TOOL_ERASE) {
        game.remove(curX, curY);
      } else if (cell.kind == fgame::KIND_EMPTY) {
        game.place(curX, curY, TOOL_KINDS[tool], placeDir);
      } else {
        game.rotate(curX, curY);
      }
    }
    requestUpdate();
  }
}

void FactoryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Factory");

  // Размер клетки подбирается под ширину экрана.
  int tile = (pageWidth - 20) / fgame::GRID_W;
  if (tile > 36) tile = 36;
  const int gridW = tile * fgame::GRID_W;
  const int gridH = tile * fgame::GRID_H;
  const int gridX = (pageWidth - gridW) / 2;

  // Верхняя панель: [Build: ...] [Step x5] [New]
  const int toolbarY = metrics.topPadding + metrics.headerHeight + 15;
  const int btnH = 26;
  int btnW = (pageWidth - 60) / 3;
  if (btnW > 130) btnW = 130;
  const int btnX0 = (pageWidth - (3 * btnW + 20)) / 2;

  char label[24];
  snprintf(label, sizeof(label), "Build: %s", TOOL_NAMES[tool]);
  const char* labels[3] = {label, "Step x5", "New"};
  for (int i = 0; i < 3; i++) {
    const int bx = btnX0 + i * (btnW + 10);
    const bool selected = onToolbar && toolCol == i;
    renderer.drawRoundedRect(bx, toolbarY, btnW, btnH, 1, 6, true);
    if (selected) {
      renderer.fillRoundedRect(bx, toolbarY, btnW, btnH, 6, Color::Black);
    }
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
    const int tx = bx + (btnW - tw) / 2;
    const int ty = toolbarY + (btnH - renderer.getLineHeight(SMALL_FONT_ID)) / 2;
    renderer.drawText(SMALL_FONT_ID, tx, ty, labels[i], !selected);
  }

  const int gridY = toolbarY + btnH + 15;

  // Клетки
  for (int y = 0; y < fgame::GRID_H; y++) {
    for (int x = 0; x < fgame::GRID_W; x++) {
      const char g = game.glyph(x, y);
      if (g == ' ') continue;
      const char buf[2] = {g, '\0'};
      const bool machine = (g == 'D' || g == 'F' || g == 'S');
      const EpdFontFamily::Style style = machine ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
      const int tw = renderer.getTextWidth(SMALL_FONT_ID, buf, style);
      const int th = renderer.getLineHeight(SMALL_FONT_ID);
      renderer.drawText(SMALL_FONT_ID, gridX + x * tile + (tile - tw) / 2, gridY + y * tile + (tile - th) / 2, buf, true,
                        style);
    }
  }

  // Курсор
  if (!onToolbar) {
    renderer.drawRect(gridX + curX * tile + 1, gridY + curY * tile + 1, tile - 2, tile - 2, 2, true);
  }

  // Линии сетки
  for (int i = 0; i <= fgame::GRID_W; i++) {
    const int x = gridX + i * tile;
    renderer.drawLine(x, gridY, x, gridY + gridH, 1, true);
  }
  for (int i = 0; i <= fgame::GRID_H; i++) {
    const int y = gridY + i * tile;
    renderer.drawLine(gridX, y, gridX + gridW, y, 1, true);
  }

  // Состояние и подсказка по значкам
  char status[48];
  snprintf(status, sizeof(status), "Ingots: %u/%d   Tick: %lu", static_cast<unsigned>(game.stored[fgame::ITEM_INGOT]),
           fgame::GOAL, static_cast<unsigned long>(game.tick));
  int curY2 = gridY + gridH + 12;
  renderer.drawCenteredText(SMALL_FONT_ID, curY2, status, true, EpdFontFamily::REGULAR);
  curY2 += renderer.getLineHeight(SMALL_FONT_ID) + 6;
  renderer.drawCenteredText(SMALL_FONT_ID, curY2, "D drill  F furnace  S chest  o ore  = ingot", true,
                            EpdFontFamily::REGULAR);

  if (game.won()) {
    curY2 += renderer.getLineHeight(SMALL_FONT_ID) + 12;
    renderer.drawCenteredText(UI_12_FONT_ID, curY2, "FACTORY WORKS!", true, EpdFontFamily::BOLD);
  }

  const auto hints = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);

  (void)pageHeight;
  renderer.displayBuffer();
}
