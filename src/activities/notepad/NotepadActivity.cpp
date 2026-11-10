#include "NotepadActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr const char* kLatin[26] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                                    "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};

constexpr const char* kRussian[33] = {"а", "б", "в", "г", "д", "е", "ё", "ж", "з", "и", "й",
                                      "к", "л", "м", "н", "о", "п", "р", "с", "т", "у", "ф",
                                      "х", "ц", "ч", "ш", "щ", "ъ", "ы", "ь", "э", "ю", "я"};

// Цифры и знаки одинаковы для обеих раскладок
constexpr const char* kCommon[18] = {"0", "1", "2", "3", "4", "5", "6", "7", "8",
                                     "9", ".", ",", "!", "?", "-", ":", "(", ")"};

// Служебные клавиши (по порядку после символов)
constexpr const char* kSpecial[6] = {"SP", "DEL", "NL", "Aa", "RU/EN", "SAVE"};

enum SpecialKey { KEY_SPACE = 0, KEY_DELETE, KEY_NEWLINE, KEY_SHIFT, KEY_LANG, KEY_SAVE };

// Длина символа UTF-8 по первому байту
int utf8Len(unsigned char c) {
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  return 4;
}

// Заглавная версия клавиши (латиница и кириллица)
std::string toUpperKey(const char* s) {
  std::string r(s);
  if (r.size() == 1 && r[0] >= 'a' && r[0] <= 'z') {
    r[0] = static_cast<char>(r[0] - 'a' + 'A');
  } else if (r.size() == 2) {
    const auto b0 = static_cast<unsigned char>(r[0]);
    const auto b1 = static_cast<unsigned char>(r[1]);
    if (b0 == 0xD0 && b1 >= 0xB0 && b1 <= 0xBF) {  // а-п
      r[1] = static_cast<char>(b1 - 0x20);
    } else if (b0 == 0xD1 && b1 >= 0x80 && b1 <= 0x8F) {  // р-я
      r[0] = static_cast<char>(0xD0);
      r[1] = static_cast<char>(b1 + 0x20);
    } else if (b0 == 0xD1 && b1 == 0x91) {  // ё
      r[0] = static_cast<char>(0xD0);
      r[1] = static_cast<char>(0x81);
    }
  }
  return r;
}
}  // namespace

void NotepadActivity::buildKeys() {
  keyCount = 0;
  if (russian) {
    for (const char* k : kRussian) keys[keyCount++] = k;
  } else {
    for (const char* k : kLatin) keys[keyCount++] = k;
  }
  for (const char* k : kCommon) keys[keyCount++] = k;
  specialBase = keyCount;
  for (const char* k : kSpecial) keys[keyCount++] = k;
}

void NotepadActivity::onEnter() {
  Activity::onEnter();
  text.clear();
  text.reserve(kMaxLen);
  savedPath.clear();
  keyIndex = 0;
  shiftOn = false;
  russian = false;
  dirty = false;
  status[0] = '\0';
  buildKeys();
}

void NotepadActivity::onExit() {
  Activity::onExit();
}

bool NotepadActivity::saveNote() {
  Storage.mkdir("/notes");

  if (savedPath.empty()) {
    // Первое сохранение за сессию: ищем свободное имя
    char path[32];
    bool found = false;
    for (int n = 1; n < 1000; n++) {
      snprintf(path, sizeof(path), "/notes/note_%03d.txt", n);
      if (!Storage.exists(path)) {
        found = true;
        break;
      }
    }
    if (!found) {
      snprintf(status, sizeof(status), "Save error: no free name");
      return false;
    }
    savedPath = path;
  } else if (Storage.exists(savedPath.c_str())) {
    // Перезапись своего же файла: удаляем старый, чтобы не осталось хвоста
    Storage.remove(savedPath.c_str());
  }

  HalFile file;
  if (!Storage.openFileForWrite("NOTE", savedPath.c_str(), file)) {
    LOG_ERR("NOTE", "Cannot open %s", savedPath.c_str());
    snprintf(status, sizeof(status), "Save error");
    return false;
  }
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());

  dirty = false;
  snprintf(status, sizeof(status), "Saved: %s", savedPath.c_str() + 7);  // без "/notes/"
  return true;
}

void NotepadActivity::pressKey() {
  status[0] = '\0';

  if (keyIndex < specialBase) {
    std::string s = shiftOn ? toUpperKey(keys[keyIndex]) : std::string(keys[keyIndex]);
    if (static_cast<int>(text.size() + s.size()) <= kMaxLen) {
      text += s;
      dirty = true;
    }
    return;
  }

  switch (keyIndex - specialBase) {
    case KEY_SPACE:
      if (static_cast<int>(text.size()) < kMaxLen) {
        text.push_back(' ');
        dirty = true;
      }
      break;
    case KEY_DELETE:
      if (!text.empty()) {
        // Удаляем целый символ UTF-8
        size_t n = text.size();
        do {
          n--;
        } while (n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80);
        text.resize(n);
        dirty = true;
      }
      break;
    case KEY_NEWLINE:
      if (static_cast<int>(text.size()) < kMaxLen) {
        text.push_back('\n');
        dirty = true;
      }
      break;
    case KEY_SHIFT:
      shiftOn = !shiftOn;
      break;
    case KEY_LANG:
      russian = !russian;
      buildKeys();
      keyIndex = 0;
      break;
    case KEY_SAVE:
      saveNote();
      break;
    default:
      break;
  }
}

void NotepadActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // При выходе сохраняем несохранённый текст, чтобы он не пропал
    if (dirty && !text.empty()) saveNote();
    finish();
    return;
  }

  const int rows = (keyCount + kCols - 1) / kCols;

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    keyIndex = (keyIndex - 1 + keyCount) % keyCount;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    keyIndex = (keyIndex + 1) % keyCount;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    keyIndex -= kCols;
    if (keyIndex < 0) {
      keyIndex += rows * kCols;
      if (keyIndex >= keyCount) keyIndex -= kCols;
    }
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    keyIndex += kCols;
    if (keyIndex >= keyCount) keyIndex %= kCols;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    pressKey();
    requestUpdate();
  }
}

void NotepadActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Notepad");

  // Геометрия экранной клавиатуры
  const int rows = (keyCount + kCols - 1) / kCols;
  const int cellW = (pageWidth - 20) / kCols;
  const int cellH = 34;
  const int keysX = (pageWidth - cellW * kCols) / 2;
  const int keysY = pageHeight - metrics.buttonHintsHeight - cellH * rows - 10;

  // Область текста
  const int areaX = 10;
  const int areaY = metrics.topPadding + metrics.headerHeight + 10;
  const int areaW = pageWidth - 20;
  const int areaH = keysY - 10 - areaY;
  renderer.drawRect(areaX, areaY, areaW, areaH, 1, true);

  const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(SMALL_FONT_ID);
  int maxLines = (areaH - 10 - smallH) / lineH;
  if (maxLines < 1) maxLines = 1;

  // Перенос строк по ширине области (по символам UTF-8)
  std::string shown = text;
  shown.push_back('_');  // метка конца текста
  std::vector<std::string> lines;
  lines.reserve(32);
  std::string cur;
  size_t i = 0;
  while (i < shown.size()) {
    const char ch = shown[i];
    if (ch == '\n') {
      lines.push_back(cur);
      cur.clear();
      i++;
      continue;
    }
    size_t len = static_cast<size_t>(utf8Len(static_cast<unsigned char>(ch)));
    if (i + len > shown.size()) len = shown.size() - i;
    std::string t = cur;
    t.append(shown, i, len);
    if (!cur.empty() && renderer.getTextWidth(UI_12_FONT_ID, t.c_str()) > areaW - 12) {
      lines.push_back(cur);
      cur.assign(shown, i, len);
    } else {
      cur = t;
    }
    i += len;
  }
  lines.push_back(cur);

  // Показываем последние строки
  int first = static_cast<int>(lines.size()) - maxLines;
  if (first < 0) first = 0;
  int y = areaY + 5;
  for (int k = first; k < static_cast<int>(lines.size()); k++) {
    renderer.drawText(UI_12_FONT_ID, areaX + 6, y, lines[k].c_str(), true);
    y += lineH;
  }

  // Строка состояния внизу области текста
  if (status[0] != '\0') {
    renderer.drawText(SMALL_FONT_ID, areaX + 6, areaY + areaH - smallH - 3, status, true);
  }

  // Клавиатура
  for (int k = 0; k < keyCount; k++) {
    const int row = k / kCols;
    const int col = k % kCols;
    const int x = keysX + col * cellW + 2;
    const int yy = keysY + row * cellH + 2;
    const int w = cellW - 4;
    const int h = cellH - 4;
    const bool selected = (k == keyIndex);

    renderer.drawRoundedRect(x, yy, w, h, 1, 6, true);
    if (selected) {
      renderer.fillRoundedRect(x, yy, w, h, 6, Color::Black);
    }

    std::string label;
    int fontId = UI_12_FONT_ID;
    if (k < specialBase) {
      label = shiftOn ? toUpperKey(keys[k]) : std::string(keys[k]);
    } else {
      fontId = SMALL_FONT_ID;
      label = keys[k];
    }

    const int textW = renderer.getTextWidth(fontId, label.c_str());
    const int textH = renderer.getLineHeight(fontId);
    renderer.drawText(fontId, x + (w - textW) / 2, yy + (h - textH) / 2, label.c_str(), !selected);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
