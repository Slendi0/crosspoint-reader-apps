#include "FactoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int TOOL_COUNT = 5;
constexpr const char* TOOL_NAMES[TOOL_COUNT] = {"Бур", "Лента", "Печь", "Склад", "Стереть"};
constexpr uint8_t TOOL_KINDS[TOOL_COUNT] = {fgame::KIND_DRILL, fgame::KIND_BELT, fgame::KIND_FURNACE,
                                            fgame::KIND_CHEST, fgame::KIND_EMPTY};
constexpr int TOOL_ERASE = 4;
constexpr int STEPS_PER_PRESS = 5;

// Прогресс сохраняется, пока читалка не перезагружена: можно выйти и вернуться.
fgame::Game g_saved;
bool g_hasSaved = false;
}  // namespace

void FactoryActivity::onEnter() {
  Activity::onEnter();
  if (g_hasSaved) {
    game = g_saved;
  } else {
    game.init();
  }
  curX = fgame::GRID_W / 2;
  curY = fgame::GRID_H / 2;
  onToolbar = false;
  toolCol = 0;
  tool = 0;
  placeDir = 1;
  requestUpdate();
}

void FactoryActivity::onExit() {
  g_saved = game;
  g_hasSaved = true;
  Activity::onExit();
}

void FactoryActivity::loop() {
  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Back)) {
    // На клетке с постройкой «Назад» убирает её, на пустой клетке и на панели выходит из игры.
    if (!onToolbar && game.c[curY][curX].kind != fgame::KIND_EMPTY) {
      game.remove(curX, curY);
      requestUpdate();
    } else {
      finish();
    }
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
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Фабрика");

  // Размер клетки подбирается под ширину экрана.
  int tile = (pageWidth - 20) / fgame::GRID_W;
  if (tile > 36) tile = 36;
  const int gridW = tile * fgame::GRID_W;
  const int gridH = tile * fgame::GRID_H;
  const int gridX = (pageWidth - gridW) / 2;

  // Верхняя панель: [Строить: ...] [Шаг x5] [Заново]
  const int toolbarY = metrics.topPadding + metrics.headerHeight + 15;
  const int btnH = 26;
  int btnW = (pageWidth - 60) / 3;
  if (btnW > 130) btnW = 130;
  const int btnX0 = (pageWidth - (3 * btnW + 20)) / 2;

  char label[40];
  snprintf(label, sizeof(label), "Строить: %s", TOOL_NAMES[tool]);
  const char* labels[3] = {label, "Шаг x5", "Заново"};
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
      const fgame::Cell& cell = game.c[y][x];
      const char* g = game.glyph(x, y);
      const int cellX = gridX + x * tile;
      const int cellY = gridY + y * tile;

      if (g[0] != ' ') {
        const bool machine = (cell.kind == fgame::KIND_DRILL || cell.kind == fgame::KIND_FURNACE ||
                              cell.kind == fgame::KIND_CHEST);
        const EpdFontFamily::Style style = machine ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
        const int tw = renderer.getTextWidth(SMALL_FONT_ID, g, style);
        const int th = renderer.getLineHeight(SMALL_FONT_ID);
        renderer.drawText(SMALL_FONT_ID, cellX + (tile - tw) / 2, cellY + (tile - th) / 2, g, true, style);
      }

      // Чёрточка у края клетки показывает, куда бур и печь отдают предметы.
      if (cell.kind == fgame::KIND_DRILL || cell.kind == fgame::KIND_FURNACE) {
        const int d = cell.dir & 3;
        const int cx = cellX + tile / 2;
        const int cy = cellY + tile / 2;
        const int outer = tile / 2 - 2;
        const int inner = tile / 2 - 8;
        renderer.drawLine(cx + fgame::DX[d] * outer, cy + fgame::DY[d] * outer, cx + fgame::DX[d] * inner,
                          cy + fgame::DY[d] * inner, 3, true);
      }
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

  // Состояние и подсказки
  char status[64];
  snprintf(status, sizeof(status), "Слитки: %u/%d   Такт: %lu", static_cast<unsigned>(game.stored[fgame::ITEM_INGOT]),
           fgame::GOAL, static_cast<unsigned long>(game.tick));
  int textY = gridY + gridH + 12;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, status, true, EpdFontFamily::REGULAR);
  textY += renderer.getLineHeight(SMALL_FONT_ID) + 6;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, "Б бур   П печь   С склад   = слиток", true,
                            EpdFontFamily::REGULAR);
  textY += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, "Чёрточка у края: куда выходит", true, EpdFontFamily::REGULAR);

  if (game.won()) {
    textY += renderer.getLineHeight(SMALL_FONT_ID) + 12;
    renderer.drawCenteredText(UI_12_FONT_ID, textY, "ФАБРИКА РАБОТАЕТ!", true, EpdFontFamily::BOLD);
  }

  // Левая нижняя кнопка: на постройке она убирает её, иначе выходит.
  const bool canRemove = !onToolbar && game.c[curY][curX].kind != fgame::KIND_EMPTY;
  const char* backLabel = canRemove ? "Убрать" : tr(STR_BACK);
  const auto hints = mappedInput.mapLabels(backLabel, tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);

  renderer.displayBuffer();
}
