#include "FactoryActivity.h"

#include <Arduino.h>
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

constexpr unsigned long TICK_MS = 2000;  // как часто игра делает шаг сама
constexpr int STEPS_PER_TICK = 2;        // сколько шагов за один раз (между перерисовками экрана)

// Прогресс хранится, пока читалка не перезагружена или не ушла в глубокий сон.
fgame::Game g_saved;
bool g_hasSaved = false;

// --- Значки, нарисованные линиями. Размеры рассчитаны на клетку 23 px и масштабируются. ---

// Винт (бур)
void drawScrew(GfxRenderer& r, int x, int y, int t) {
  auto S = [t](int v) { return v * t / 23; };
  const int cx = x + t / 2;
  r.drawLine(cx - S(5), y + S(3), cx + S(5), y + S(3), 3, true);   // шляпка
  r.drawLine(cx, y + S(3), cx, y + S(20), 2, true);                // стержень
  for (int i = 0; i < 3; i++) {                                    // резьба
    const int yy = y + S(8) + i * S(4);
    r.drawLine(cx - S(5), yy + S(3), cx + S(5), yy - S(1), 1, true);
  }
}

// Огонёк (печь)
void drawFlame(GfxRenderer& r, int x, int y, int t) {
  auto S = [t](int v) { return v * t / 23; };
  const int cx = x + t / 2;
  r.fillRoundedRect(x + S(6), y + S(10), S(11), S(10), S(5), Color::Black);  // основание
  for (int i = -4; i <= 4; i++) {                                           // острие
    r.drawLine(cx + S(i), y + S(12), cx, y + S(2), 1, true);
  }
  r.drawLine(cx, y + S(13), cx, y + S(18), 2, false);                       // светлая сердцевина
}

// Ящик (склад)
void drawCrate(GfxRenderer& r, int x, int y, int t) {
  auto S = [t](int v) { return v * t / 23; };
  const int cx = x + t / 2;
  r.drawRect(x + S(3), y + S(5), S(17), S(14), 2, true);
  r.drawLine(x + S(3), y + S(10), x + S(20), y + S(10), 1, true);  // крышка
  r.drawLine(cx, y + S(10), cx, y + S(19), 1, true);               // доска
  r.fillRoundedRect(cx - S(2), y + S(8), S(4), S(4), 1, Color::Black);  // замок
}

// Лента: стрелка по направлению, на ней предмет (чёрный квадрат — руда, рамка — слиток)
void drawBelt(GfxRenderer& r, int x, int y, int t, int dir, int item) {
  auto S = [t](int v) { return v * t / 23; };
  const int cx = x + t / 2;
  const int cy = y + t / 2;
  const int dx = fgame::DX[dir & 3];
  const int dy = fgame::DY[dir & 3];
  const int hx = cx + dx * S(9), hy = cy + dy * S(9);    // острие
  const int tx = cx - dx * S(9), ty = cy - dy * S(9);    // хвост
  r.drawLine(tx, ty, hx, hy, 1, true);
  const int bx = hx - dx * S(5), by = hy - dy * S(5);    // основание наконечника
  r.drawLine(hx, hy, bx - dy * S(4), by + dx * S(4), 1, true);
  r.drawLine(hx, hy, bx + dy * S(4), by - dx * S(4), 1, true);
  if (item == fgame::ITEM_ORE) {
    r.fillRoundedRect(cx - S(4), cy - S(4), S(8), S(8), S(2), Color::Black);
  } else if (item == fgame::ITEM_INGOT) {
    r.drawRect(cx - S(6), cy - S(3), S(12), S(6), 2, true);
  }
}
}  // namespace

void FactoryActivity::saveState() {
  g_saved = game;
  g_hasSaved = true;
}

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
  running = true;
  lastTick = millis();
  requestUpdate();
}

void FactoryActivity::onExit() {
  saveState();
  Activity::onExit();
}

void FactoryActivity::loop() {
  using Button = MappedInputManager::Button;
  bool dirty = false;

  // Автоматический ход: бур сам добывает руду, ленты сами её несут.
  if (running) {
    const unsigned long now = millis();
    if (now - lastTick >= TICK_MS) {
      lastTick = now;
      bool moved = false;
      for (int i = 0; i < STEPS_PER_TICK; i++) {
        game.step();
        moved = moved || game.changed;
      }
      if (moved) dirty = true;  // экран перерисовываем только если что-то сдвинулось
    }
  }

  if (mappedInput.wasReleased(Button::Back)) {
    // На клетке с постройкой «Назад» убирает её, на пустой клетке и на панели выходит из игры.
    if (!onToolbar && game.c[curY][curX].kind != fgame::KIND_EMPTY) {
      game.remove(curX, curY);
      dirty = true;
    } else {
      saveState();
      finish();
      return;
    }
  } else if (mappedInput.wasReleased(Button::Up)) {
    if (onToolbar) {
      onToolbar = false;
      curY = fgame::GRID_H - 1;
    } else if (curY == 0) {
      onToolbar = true;
    } else {
      curY--;
    }
    placeDir = 0;
    dirty = true;
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
    dirty = true;
  } else if (mappedInput.wasReleased(Button::Left)) {
    if (onToolbar) {
      toolCol = (toolCol + 2) % 3;
    } else {
      curX = (curX + fgame::GRID_W - 1) % fgame::GRID_W;
    }
    placeDir = 3;
    dirty = true;
  } else if (mappedInput.wasReleased(Button::Right)) {
    if (onToolbar) {
      toolCol = (toolCol + 1) % 3;
    } else {
      curX = (curX + 1) % fgame::GRID_W;
    }
    placeDir = 1;
    dirty = true;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    if (onToolbar) {
      if (toolCol == 0) {
        tool = (tool + 1) % TOOL_COUNT;
      } else if (toolCol == 1) {
        running = !running;
        lastTick = millis();
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
    dirty = true;
  }

  if (dirty) {
    saveState();
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

  // Верхняя панель: [Строить: ...] [Пауза/Пуск] [Заново]
  const int toolbarY = metrics.topPadding + metrics.headerHeight + 15;
  const int btnH = 26;
  const int total = pageWidth - 40;
  const int w0 = total * 44 / 100;
  const int w1 = (total - w0 - 20) / 2;
  const int widths[3] = {w0, w1, w1};
  int bx = 20;

  char label[40];
  snprintf(label, sizeof(label), "Строить: %s", TOOL_NAMES[tool]);
  const char* labels[3] = {label, running ? "Пауза" : "Пуск", "Заново"};
  for (int i = 0; i < 3; i++) {
    const bool selected = onToolbar && toolCol == i;
    renderer.drawRoundedRect(bx, toolbarY, widths[i], btnH, 1, 6, true);
    if (selected) {
      renderer.fillRoundedRect(bx, toolbarY, widths[i], btnH, 6, Color::Black);
    }
    const int tw = renderer.getTextWidth(SMALL_FONT_ID, labels[i]);
    const int tx = bx + (widths[i] - tw) / 2;
    const int ty = toolbarY + (btnH - renderer.getLineHeight(SMALL_FONT_ID)) / 2;
    renderer.drawText(SMALL_FONT_ID, tx, ty, labels[i], !selected);
    bx += widths[i] + 10;
  }

  const int gridY = toolbarY + btnH + 15;

  // Клетки
  for (int y = 0; y < fgame::GRID_H; y++) {
    for (int x = 0; x < fgame::GRID_W; x++) {
      const fgame::Cell& cell = game.c[y][x];
      const int cellX = gridX + x * tile;
      const int cellY = gridY + y * tile;

      switch (cell.kind) {
        case fgame::KIND_DRILL:
          drawScrew(renderer, cellX, cellY, tile);
          break;
        case fgame::KIND_FURNACE:
          drawFlame(renderer, cellX, cellY, tile);
          break;
        case fgame::KIND_CHEST:
          drawCrate(renderer, cellX, cellY, tile);
          break;
        case fgame::KIND_BELT:
          drawBelt(renderer, cellX, cellY, tile, cell.dir, cell.item);
          break;
        default:
          if (cell.ore) {
            const int tw = renderer.getTextWidth(SMALL_FONT_ID, ":");
            const int th = renderer.getLineHeight(SMALL_FONT_ID);
            renderer.drawText(SMALL_FONT_ID, cellX + (tile - tw) / 2, cellY + (tile - th) / 2, ":", true);
          }
          break;
      }

      // Чёрточка у края клетки показывает, куда бур и печь отдают предметы.
      if (cell.kind == fgame::KIND_DRILL || cell.kind == fgame::KIND_FURNACE) {
        const int d = cell.dir & 3;
        const int cx = cellX + tile / 2;
        const int cy = cellY + tile / 2;
        const int outer = tile / 2 - 1;
        const int inner = tile / 2 - 5;
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
  snprintf(status, sizeof(status), "Слитки: %u/%d%s", static_cast<unsigned>(game.stored[fgame::ITEM_INGOT]),
           fgame::GOAL, running ? "" : "   ПАУЗА");
  int textY = gridY + gridH + 12;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, status, true, EpdFontFamily::BOLD);
  textY += renderer.getLineHeight(SMALL_FONT_ID) + 6;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, "Винт: бур, огонь: печь, ящик: склад", true,
                            EpdFontFamily::REGULAR);
  textY += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, "Чёрный квадрат: руда, рамка: слиток", true,
                            EpdFontFamily::REGULAR);
  textY += renderer.getLineHeight(SMALL_FONT_ID) + 4;
  renderer.drawCenteredText(SMALL_FONT_ID, textY, "Чёрточка у края: куда выходит", true, EpdFontFamily::REGULAR);

  if (game.won()) {
    textY += renderer.getLineHeight(SMALL_FONT_ID) + 10;
    renderer.drawCenteredText(UI_12_FONT_ID, textY, "ФАБРИКА РАБОТАЕТ!", true, EpdFontFamily::BOLD);
  }

  // Левая нижняя кнопка: на постройке она убирает её, иначе выходит.
  const bool canRemove = !onToolbar && game.c[curY][curX].kind != fgame::KIND_EMPTY;
  const char* backLabel = canRemove ? "Убрать" : tr(STR_BACK);
  const auto hints = mappedInput.mapLabels(backLabel, tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);

  renderer.displayBuffer();
}
