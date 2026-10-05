#pragma once
// Ядро игры «фабрика»: только логика, без привязки к железу.
#include <stdint.h>

namespace fgame {

constexpr int GRID_W = 20;  // ширина поля в клетках
constexpr int GRID_H = 18;  // высота поля в клетках

enum Kind : uint8_t { KIND_EMPTY, KIND_DRILL, KIND_BELT, KIND_FURNACE, KIND_CHEST };
enum Item : uint8_t { ITEM_NONE, ITEM_ORE, ITEM_INGOT };

constexpr int DX[4] = {0, 1, 0, -1};  // 0 вверх, 1 вправо, 2 вниз, 3 влево
constexpr int DY[4] = {-1, 0, 1, 0};
constexpr int GOAL = 20;  // сколько слитков нужно доставить на склад

struct Cell {
  uint8_t kind;  // что построено
  uint8_t dir;   // куда смотрит (0..3)
  uint8_t item;  // предмет на ленте / готовый слиток в печи
  uint8_t ore;   // 1 = под клеткой залежь руды
  uint8_t a;     // печь: сколько руды внутри
  uint8_t b;     // таймер бура или печи
};

struct Game {
  Cell c[GRID_H][GRID_W];
  bool moved[GRID_H][GRID_W];
  uint16_t stored[3];
  uint32_t tick;
  bool changed;  // на последнем такте что-то сдвинулось (нужна перерисовка)

  void init() {
    *this = Game();  // всё в ноль
    for (int y = 3; y <= 6; y++)
      for (int x = 2; x <= 5; x++) c[y][x].ore = 1;  // стартовая залежь
  }

  static bool inside(int x, int y) { return x >= 0 && y >= 0 && x < GRID_W && y < GRID_H; }

  // Попытка передать предмет в клетку (x, y).
  bool insert(int x, int y, uint8_t it) {
    if (!inside(x, y)) return false;
    Cell& t = c[y][x];
    switch (t.kind) {
      case KIND_BELT:
        if (t.item) return false;
        t.item = it;
        moved[y][x] = true;  // чтобы лента не двинула его второй раз за такт
        changed = true;
        return true;
      case KIND_FURNACE:
        if (it != ITEM_ORE || t.a >= 5) return false;
        t.a++;
        changed = true;
        return true;
      case KIND_CHEST:
        stored[it]++;
        changed = true;
        return true;
      default:
        return false;
    }
  }

  // Один такт симуляции.
  void step() {
    tick++;
    changed = false;
    for (int y = 0; y < GRID_H; y++)
      for (int x = 0; x < GRID_W; x++) moved[y][x] = false;
    for (int y = 0; y < GRID_H; y++) {
      for (int x = 0; x < GRID_W; x++) {
        Cell& t = c[y][x];
        const int nx = x + DX[t.dir & 3], ny = y + DY[t.dir & 3];
        switch (t.kind) {
          case KIND_DRILL:
            if (t.b < 2) t.b++;
            if (t.b >= 2 && insert(nx, ny, ITEM_ORE)) t.b = 0;
            break;
          case KIND_BELT:
            if (t.item && !moved[y][x] && insert(nx, ny, t.item)) t.item = ITEM_NONE;
            break;
          case KIND_FURNACE:
            if (!t.item && t.a > 0 && ++t.b >= 3) {
              t.a--;
              t.item = ITEM_INGOT;
              t.b = 0;
            }
            if (t.item && insert(nx, ny, t.item)) t.item = ITEM_NONE;
            break;
          default:
            break;
        }
      }
    }
  }

  // Действия игрока.
  bool place(int x, int y, uint8_t kind, uint8_t dir) {
    if (!inside(x, y) || c[y][x].kind != KIND_EMPTY) return false;
    if (kind == KIND_DRILL && !c[y][x].ore) return false;  // бур только на руде
    c[y][x].kind = kind;
    c[y][x].dir = dir & 3;
    return true;
  }
  void rotate(int x, int y) {
    if (inside(x, y)) c[y][x].dir = (c[y][x].dir + 1) & 3;
  }
  void remove(int x, int y) {
    if (!inside(x, y)) return;
    Cell& t = c[y][x];
    t.kind = KIND_EMPTY;
    t.item = ITEM_NONE;
    t.a = 0;
    t.b = 0;
  }
  bool won() const { return stored[ITEM_INGOT] >= GOAL; }
};

}  // namespace fgame
