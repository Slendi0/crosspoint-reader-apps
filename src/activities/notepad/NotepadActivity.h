#pragma once

#include <string>

#include "activities/Activity.h"

// Блокнот: текст вводится выбором символов на экранной сетке (латиница и кириллица).
// Файлы сохраняются на SD-карту в папку /notes как note_001.txt, note_002.txt и т.д.
class NotepadActivity final : public Activity {
 private:
  static constexpr int kCols = 8;
  static constexpr int kMaxKeys = 64;
  static constexpr int kMaxLen = 2000;  // байт (кириллица занимает по 2 байта)

  std::string text;
  std::string savedPath;  // путь файла, в который уже сохраняли в этой сессии
  const char* keys[kMaxKeys] = {nullptr};
  int keyCount = 0;
  int specialBase = 0;  // индекс первой служебной клавиши
  int keyIndex = 0;
  bool shiftOn = false;
  bool russian = false;
  bool dirty = false;  // есть несохранённые изменения
  char status[40] = {0};

  void buildKeys();
  void pressKey();
  bool saveNote();

 public:
  explicit NotepadActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Notepad", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
