/*
  Keep this file in win-1251 encoding! In UTF-8 strlen is broken for russian
  strings...
*/

// Uncomment for serial debugging.
// #define DEBUG

#include "./menu_item.h"
#include "./src/KeyMatrix/KeyMatrix.h"
#include "./src/TroykaOLED/TroykaOLED.h"
#include "./storage.h"
#include <WString.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

// Иконка ветра
const unsigned char breeze[] PROGMEM = {
    24,   24,   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x86, 0x86, 0x83, 0x83, 0x83, 0xc7, 0xfe, 0x38, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0x99, 0x99, 0x19, 0x19, 0x19, 0x19, 0x19, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x30, 0xf0, 0xc0, 0x01, 0x01, 0x01, 0x01, 0x41,
    0x61, 0xc1, 0xc1, 0xc1, 0x63, 0x7f, 0x1c, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x18, 0x18, 0x18, 0x18, 0x0c, 0x0f, 0x03
    //
};

// Иконка повтора
const unsigned char repeat[] PROGMEM = {
    12,   12,   0xfc, 0x04, 0x04, 0x04, 0x84, 0x04, 0x04,
    0x1f, 0x0e, 0x04, 0x00, 0xfc, 0x03, 0x00, 0x02, 0x07,
    0x0f, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03
    //
};

#define byte uint8_t

// Размеры дисплея в точках
#define DISPLAY_W 128
#define DISPLAY_H 64

#define DISPLAY_MAX_BRIGTNESS 255

// Инициализация объекта дисплея с адресом и размерами
TroykaOLED display(0x3C, DISPLAY_W, DISPLAY_H);

#define GITHUB_URL F("https://github.com/funvit/toilet-fan-control.atmega328pb")
#define VER_MAJOR 0
#define VER_MINOR 5
#define VER_PATCH "menu"

// Ножка сенсора освещенности
#define LIGHT_SENSOR_PIN A0
// Ножка управления реле (используйте транзистор!)
#define RELAY_PIN 2
// Ножка сброса дисплея.
// Мой дисплей имеет пин RESET, его необходимо устанавливать в 1 после старта
// устройства. Иначе на экране будет мусор.
#define DISPLAY_RESET_PIN 4
// Built in led pin
#define STATUS_LED_PIN 13

//------------------------
// Матрица кнопок.
//------------------------

// Хотя кнопок всего 4 - проще использовать библиотеку работы с матрицой
// кнопок.
char keymap[2][2] = {{'a', 'b'}, {'c', 'd'}};
byte rowPins[2] = {5, 6};
byte colPins[2] = {7, 8};
KeyMatrix keypad((char *)keymap, (byte)2, (byte)2, rowPins, colPins);

//------------------------
// Глобальные переменные
//------------------------

// Переменные меню
#define MENU_ITEMS 7
#define MENU_ITEM_STATE_SELECTED 1
#define MENU_ITEM_STATE_EDIT 2
#define MENU_ITEM_SIGNAL_INC 1
#define MENU_ITEM_SIGNAL_DEC 2
#define MENU_ITEM_SIGNAL_SAVE 3

#define NO_MENU_ITEM_SELECTED 255
byte menuIdx = NO_MENU_ITEM_SELECTED;
byte menuItemState = MENU_ITEM_STATE_SELECTED;

// Элементы меню (DEFAULT VALUES)
// Пауза до включения
const uint8_t MENU1_VAL_MIN = 0;
const uint8_t MENU1_VAL_MAX = 30;

// Длительность работы вент
const uint8_t MENU2_VAL_MIN = 1;
const uint8_t MENU2_VAL_MAX = 15;

// Порог датчика света
const uint8_t MENU3_VAL_MIN = 1;
const uint8_t MENU3_VAL_MAX = 99;

// Яркость экрана
const uint8_t MENU4_VAL_MIN = 0;
const uint8_t MENU4_VAL_MAX = 255;

// Защита экрана через
const uint8_t MENU5_VAL_MIN = 0;
const uint8_t MENU5_VAL_MAX = 255;

// Тексты
#define TEXT_SCEENSAVER F("РЕЖИМ СНА ЭКРАНА")
#define SECONDS_SUFFIX "с."
#define MINUTES_SUFFIX "м."
#define HOURS_SUFFIX "ч."
#define DAYS_SUFFIX "д."
#define PERCENT_SYMBOL "%"

// screen saver
bool screensaverTimerInited = false;
bool screensaver = false;
uint32_t screensaverTimer;
uint8_t screensaverY = 0;

byte gLight = 0;
uint32_t exitMenuTimer;
uint32_t beforeFanOnTimer;
uint32_t fanWorkTimer; // в миллисекундах

bool isFanOnRepeat;

uint32_t menuSavedMarkTimer = 0;

//------------------------
// Настройки.
// Текущие значения хранятся в переменных.
// Указанные значения являются значениями по-умолчанию (пример: первый запуск,
// когда EEPROM еще пустое).
//------------------------

// Множитель для значения cfgDelayBeforeFanOn, что бы получить значение в
// секундах. Позволяет изменять значение на странице меню с установленным шагом.
#define delayBeforeFanOnSecondsMult 10
// Задержка перед включением реле
byte currentDelayBeforeFanOn = 0; // значение по-умолчанию
// Длительность замкнутого состояния реле
byte currentFanWorkDurationMinutes = 5; // значение по-умолчанию
// Пороговое значение сенсора освещенности,
// по превышению которого активируется реле.
// [0=темно, 99=светло]
byte currentFanOnSensorLevel = 40; // значение по-умолчанию
// Яркость экрана
byte currentDisplayBrigtness = 4;
// Кол-во секунд перед запуском screensaver-а.
// 0 = disabled.
byte currentScreensaverDelay = 10;
// /--------------------

// uptime (support more than 50 days)
unsigned long utDay = 0;
byte utHour = 0;
byte utMinute = 0;
byte utSecond = 0;
bool utHighMillis = false;
unsigned int utRollovers = 0;
// /----------------

uint32_t statusTimer = 1;
uint32_t introTimer = 10 * 1000;

#define LG_TIMER 100
uint32_t lgTimer = LG_TIMER;
static uint64_t lg = 0;

//------------------------
// debug, debugln
//------------------------
#ifdef DEBUG
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x);
#else
#define debug(x)
#define debugln(x)
#endif

MenuItem menu[MENU_ITEMS];

// Используется для антиспама в лог о текущей странице.
byte currentPageDbg = 255;
#ifdef DEBUG
#define debugView(name, pageId)                                                \
  {                                                                            \
    if (currentPageDbg != pageId) {                                            \
      debugln(name);                                                           \
      currentPageDbg = pageId;                                                 \
    }                                                                          \
  };
#else
#define debugView(name, pageId) ;
#endif

// ===========================================
// Стандартная функция перед вызовом loop().
// ===========================================
//
// Используется для инициализации дисплея
// и установки начальных значений глобальных переменных.
void setup() {
#ifdef DEBUG
  Serial.begin(9600);
  Serial.println(F("======================="));
  Serial.println(F("BOOT"));

  Serial.println(F("PROJECT: Toilet fan controller"));
  Serial.println(F("initializing..."));
#endif

  // Сторожевой таймер
  wdt_enable(WDTO_2S);

  // Установка режима ножек
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(DISPLAY_RESET_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);

  // послать на порт RESET дисплея сигнал.
  // иначе будет выводиться мусор!
  delay(10);
  digitalWrite(DISPLAY_RESET_PIN, 1);
  delay(10);

  // инициализируем дисплей
  display.begin();
  // выбираем кодировку символов: CP866, TXT_UTF8 или WIN1251
  display.setCoding(TXT_WIN1251);
  // не обновлять автоматически. только при вызове update().
  display.autoUpdate(false);
  display.setBrightness(currentDisplayBrigtness);

  //-------------------------
  // считывание настроек из eeprom
  debugln(F("SETUP loading params from eeprom"));

  uint8_t v;

  v = eepromGetDelayBeforeFanOnValue();
  if (MENU1_VAL_MIN <= v && v <= MENU1_VAL_MAX) {
    // debugln(F("EEPROM: delay before fan loaded"));
    currentDelayBeforeFanOn = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveDelayBeforeFanOnValue(currentDelayBeforeFanOn);
  }

  v = eepromGetFanWorkDurationValue();
  if (MENU2_VAL_MIN <= v && v <= MENU2_VAL_MAX) {
    // debugln(F("EEPROM: fan work duration loaded"));
    currentFanWorkDurationMinutes = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanWorkDurationValue(currentFanWorkDurationMinutes);
  }

  v = eepromGetFanOnSensorValue();
  if (MENU3_VAL_MIN <= v && v <= MENU3_VAL_MAX) {
    // debugln(F("EEPROM: sensor level loaded"));
    currentFanOnSensorLevel = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanOnSensorValue(currentFanOnSensorLevel);
  }

  v = eepromGetDisplayBrightnessValue();
  if (MENU4_VAL_MIN <= v && v <= MENU4_VAL_MAX) {
    currentDisplayBrigtness = v;
    display.setBrightness(v);
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveDisplayBrightnessValue(currentDisplayBrigtness);
  }

  v = eepromGetScreensaverOn();
  if (MENU5_VAL_MIN <= v && v <= MENU5_VAL_MAX) {
    currentScreensaverDelay = v;
    debug(F("eeprom: got screensaver delay: "));
    debugln(v);
    display.setBrightness(currentDisplayBrigtness);
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveScreensaverOn(currentScreensaverDelay);
  }
// -------------------------

//
// Инициализация объекта Меню
//
#pragma region Init menu object
  debug(F("MENU: initializing, items: "));
  debugln(MENU_ITEMS);
  delay(100);

  MenuItem m1 = MenuItem(SECONDS_SUFFIX, 0, 30, 1);
  m1.setValue(currentDelayBeforeFanOn);
  m1.setUpdateOnValueChange(&currentDelayBeforeFanOn);
  m1.setDisplayer(&getDelayBeforeFanForDisplay);
  menu[0] = m1;
  debugln(F("MENU: item 1 inited"));

  MenuItem m2 = MenuItem(MINUTES_SUFFIX, 1, 15, 1);
  m2.setValue(currentFanWorkDurationMinutes);
  m2.setUpdateOnValueChange(&currentFanWorkDurationMinutes);
  m2.setDisplayer(&getFanWorkTimeForDisplay);
  menu[1] = m2;
  debugln(F("MENU: item 2 inited"));

  MenuItem m3 = MenuItem(PERCENT_SYMBOL, 1, 99, 1);
  m3.setValue(currentFanOnSensorLevel);
  m3.setUpdateOnValueChange(&currentFanOnSensorLevel);
  m3.setDisplayer(&getFanOnSensorLevelForDisplay);
  menu[2] = m3;
  debugln(F("MENU: item 3 inited"));

  MenuItem m4 = MenuItem(0, 0, 255, 15);
  m4.setValue(currentDisplayBrigtness);
  m4.setUpdateOnValueChange(&currentDisplayBrigtness);
  m4.setDisplayer(&getDisplayBrigtnessForDisplay);
  m4.setOnValueChange(&setDisplayBrigtness);
  menu[3] = m4;
  debugln(F("MENU: item 4 inited"));

  MenuItem m5 = MenuItem(SECONDS_SUFFIX, 0, 255, 5);
  m5.setValue(currentScreensaverDelay);
  m5.setUpdateOnValueChange(&currentScreensaverDelay);
  m5.setDisplayer(&getScreensaverDelayForDisplay);
  menu[4] = m5;
  debugln(F("MENU: item 5 inited"));

#pragma endregion

  debugln(F("SETUP done"));
  debugln(F(""));
}

// ===========================================
// Основной цикл программы.
// ===========================================
void loop() {
  unsigned long startAt = millis();

  updateUptime();

  // Получение значения сенсора освещенности
  gLight = round((1023 - analogRead(LIGHT_SENSOR_PIN)) / 10.3);

  // Выбор страницы для отображения на дисплее
  if (introTimer > 0) {
    display.setBrightness(currentScreensaverDelay);
    displayIntroPage();
  } else {
    if (menuIdx == NO_MENU_ITEM_SELECTED) {
      // Отобразить главный экран.

      if (screensaver) {
        screensaverTimerInited = false;
        display.setBrightness(0);
        displayScreenSaverView();
      } else {
        if (!screensaverTimerInited && currentScreensaverDelay != 0) {
          screensaverTimer = currentScreensaverDelay * 1000;
          screensaverTimerInited = true;
        }

        displayMainView();
      }
    } else {
      // Отобразить экран меню.
      resetScreensaver();
      displayMenuView();
    }
  }

  // подсчет дельты для таймеров
  unsigned long m = millis();
  unsigned long delta = 0;
  if (startAt > m) {
    // если значение аптайма превысило размерность переменной
    delta = 1 << 32 - startAt + m;
  } else {
    delta = m - startAt;
  }

  if (isTimerOut(&introTimer, delta)) {
    introTimer = 0;
  }

  // Статус (моргаем светодиодом)
  if (isTimerOut(&statusTimer, delta)) {
    if (digitalRead(STATUS_LED_PIN)) {
      digitalWrite(STATUS_LED_PIN, 0);
      statusTimer = 900;
    } else {
      digitalWrite(STATUS_LED_PIN, 1);
      statusTimer = 100;
    }
  }

  // таймер выхода из меню по бездействию
  if (isTimerOut(&exitMenuTimer, delta)) {
    // reset menu item value
    onMenuItemChangeAbort();

    menuIdx = NO_MENU_ITEM_SELECTED;
    menuItemState = MENU_ITEM_STATE_SELECTED;
  }

  if (currentScreensaverDelay > 0 && !screensaver && screensaverTimerInited &&
      isTimerOut(&screensaverTimer, delta)) {
    screensaver = true;
    debugln(F("activating screensaver"));
  }

  // реагирование на превышение порога сенсора
  if (beforeFanOnTimer == 0 && fanWorkTimer == 0 &&
      gLight > currentFanOnSensorLevel) {
    beforeFanOnTimer =
        currentDelayBeforeFanOn * (uint8_t)delayBeforeFanOnSecondsMult;
    beforeFanOnTimer *= 1000;

    // debug(F("TIMER setting delay timer value to "));
    // debugln(beforeFanOnTimer);
  }

  // таймер задержки перед включением реле
  if (fanWorkTimer == 0 && \ 
    (
          // задержка не установлена
          currentDelayBeforeFanOn == 0 ||
          // или таймер задержки вышел
          isTimerOut(&beforeFanOnTimer, delta)
          //
          )
      //
  ) {
    if (gLight > currentFanOnSensorLevel) {
      // установить значение таймера длительности работы вытяжки
      setFanWorkTimer();
      // включить реле
      digitalWrite(RELAY_PIN, true);
      debugln(F("RELAY on"));
    }
  }

  // таймер работы вытяжки
  if (isTimerOut(&fanWorkTimer, delta)) {
    if (gLight > currentFanOnSensorLevel) {
      // пере-установить значение таймера длительности работы вытяжки
      // ибо сенсор обнаружил свет
      setFanWorkTimer();
      debugln(F("RELAY on (continuing)"));
      isFanOnRepeat = true;
    } else {
      // выключить реле
      digitalWrite(RELAY_PIN, false);
      debugln(F("RELAY off"));
      isFanOnRepeat = false;
    }
  }

  if (isTimerOut(&menuSavedMarkTimer, delta)) {
    display.clearDisplay();
  }

  if (isTimerOut(&lgTimer, delta)) {
    lg = lg << 4;
    lg += round(gLight / 11.0 + 0.49);
    lgTimer = LG_TIMER;
  }

  // Сброс сторожевого таймера
  wdt_reset();
}

// Валидатор длительности работы вытяжки.
bool isFanWorkDurationValueValid(uint8_t minutes) {
  bool ok = minutes >= 0 && minutes <= 15;
  return ok;
}

// Валидатор порогового значения датчика.
bool isFanOnSensorLevelValueValid(uint8_t percent) {
  bool ok = percent > 0 && percent < 99;
  return ok;
}

bool isDisplayBrigtnessValueValid(uint8_t n) {
  if (n >= 1 && n <= DISPLAY_MAX_BRIGTNESS) {
    return true;
  }
  return false;
}

//
// Главный экран дисплея
//
void displayMainView() {
  debugView(F("DISPLAY main view"), 1);

  display.clearDisplay();
  display.invertDisplay(false);
  display.invertText(false);

  byte countersAmount = 15;
  uint64_t _lg = lg;
  display.drawRect(0, 0, countersAmount * 4 + 2, 11, false, 1);
  for (byte i = 1; i <= countersAmount; i++) {
    byte v = _lg % 16;
    _lg = _lg >> 4;
    display.drawRect(2 + (countersAmount - i) * 4, 11 - v,
                     2 + (countersAmount - i) * 4 + 2, 11, true, 1);
  }

  for (byte i = 1; i < countersAmount; i++) {
    display.drawPixel(1 + (countersAmount - i) * 4,
                      11 - round(currentFanOnSensorLevel / 11.0 + 0.49), 1);
  }

  display.setFont(fontRus6x8);
  // информация от сенсора
  display.print(F("СВЕТ:"), 6 * 11 + 2, 3);

  display.setCursor(6 * 16 + 2, 3);
  if (gLight <= 9) {
    display.print(F(" "));
  }

  // вывод значений
  display.print(gLight);
  display.print(F("/"));
  if (currentFanOnSensorLevel <= 9) {
    display.print(F(" "));
  }
  display.print(currentFanOnSensorLevel);

  // вывод состояния на основную чать дисплея
  if (fanWorkTimer > 0) {
    display.setFont(fontRus12x10);
    display.print(F("ВКЛЮЧЁН"), 0, 24);

    // вытяжка включена - вывод таймера до отключения
    display.setFont(mediumNumbers);
    u16 v = fanWorkTimer / 1000; // ex: 5m * 60  = 300
    byte x = display.getWidth() - countDigits(v) * display.getFontWidth();
    display.print(fanWorkTimer / 1000, x, 24);

  } else {
    if (beforeFanOnTimer > 0) {
      // вывод таймера задержки перед включением
      display.setFont(fontRus12x10);
      display.print(F("ПАУЗА"), 0, 24);
      uint16_t v = beforeFanOnTimer / 1000;
      display.setFont(mediumNumbers);
      display.print(
          v, display.getWidth() - countDigits(v) * display.getFontWidth(), 24);
    } else {
      display.setFont(fontRus12x10);
      // вытяжка выключена - состояние ожидания
      const char *TextMainScreenWaitMode = "ОЖИДАНИЕ";
      display.print(TextMainScreenWaitMode,
                    getXForDisplayTextCentered(TextMainScreenWaitMode), 24);
    }
  }

  // Вывод uptime
  display.setFont(fontRus6x8);
  display.invertText(false);
  String uptime = uptimeForDisplay();
  display.setCursor(display.getWidth() - getStrWidthForDisplay(&uptime) -
                        (30 - utSecond / 2),
                    display.getHeigth() - display.getFontHeight());
  display.print(uptime);

  // Обновить экран
  display.update();

  // Кнопки
  if (keypad.pollEvent()) {
    // есть событие, обрабатываем
    if (keypad.event.type == KM_KEYDOWN) {
      resetScreensaver();

      switch (keypad.event.c) {
      case 'a':
        // нажата кнопка "меню"
        debugln(F("VIEW main: menu key pressed"));
        menuIdx = 0;
        display.clearDisplay();
        break;

      default:
        break;
      }
    }
  }
}

//
// Экран меню
//
void displayMenuView() {
  debugView(F("DISPLAY: menu view"), 2);

  display.clearDisplay();

  byte menuItemSignal = handleKeyboardEvents();

  if (menuIdx == NO_MENU_ITEM_SELECTED) {
    // очистить экран при выходе со страницы
    display.clearDisplay();
    return;
  }

  //
  // Меню - вывод текстов
  //
  display.setFont(fontRus6x8);

  if (menuSavedMarkTimer > 0) {
    // вывод в 1 строке метки "сохранено"
    display.invertText(true);
    const String text = F("сохранено");
    byte x = display.getWidth() - 9 * display.getFontWidth() - 2 * 2;
    display.drawRect(x - 2, 0, x + 2 + display.getFontWidth() * 9, 10, true, 1);
    display.print(text, x, 1);
    display.invertText(false);
  }

  // ---
  display.setCursor(0, 0);
  display.invertText(false);
  display.print(F("МЕНЮ"));
  if (exitMenuTimer > 0 && exitMenuTimer < 5 * 1000) {
    const char *TextMenuExitAfter = "выход через";
    uint16_t v = exitMenuTimer / 1000;
    uint16_t x = display.getWidth();
    x -= getStrWidthForDisplay(TextMenuExitAfter);
    x -= (countDigits(v) + 1) * display.getFontWidth();
    display.setCursor(x, 0);
    display.print(TextMenuExitAfter);
    display.print(F(" "));
    display.print(v);
  }

  // --------------------
  // Меню - вывод значений настроек

  // Вывод элемента меню
  if (menuIdx != NO_MENU_ITEM_SELECTED && menuIdx <= MENU_ITEMS) {
    drawMenuItem(menuIdx, menuItemPassValueForSameIdx(menuItemState, menuIdx),
                 menuItemPassValueForSameIdx(menuItemSignal, menuIdx));
  }

  //
  // Вывод текущей позиции меню
  //
  byte menuPagerItemWidth = display.getWidth() / MENU_ITEMS;
  for (byte i = 0; i < display.getWidth(); i += 2) {
    // Точки
    display.drawPixel(i, 14, WHITE);
  }
  for (byte i = 0; i < 3 - 1; i++) {
    // Толстый прямоугольник
    display.drawLine(menuPagerItemWidth * menuIdx, 13 + i,
                     menuPagerItemWidth * (menuIdx + 1), 13 + i, WHITE);
  }

  display.update();
}

//
// Обработка сигналов с клавиатуры
//
// returns menu item signal.
byte handleKeyboardEvents() {
  byte menuItemSignal = 0;

  // Кнопки
  if (keypad.pollEvent()) {
    // установка таймера по бездействию
    exitMenuTimer = 20 * 1000;

    if (keypad.event.type == KM_KEYDOWN) {
      switch (keypad.event.c) {
      case 'a': // Note: кнопка "menu" работает и как отмена изменения значения
        if (menuItemState == MENU_ITEM_STATE_EDIT) {
          // Отменить изменение текущих настроек (считать текущие сохраненные
          // значения из eeprom)
          debugln(F("MENU: canceling edit value mode"));
          onMenuItemChangeAbort();

          menuItemState = MENU_ITEM_STATE_SELECTED;
        } else {
          // Выйти со страницы меню
          debugln(F("MENU: closing"));

          menuIdx = NO_MENU_ITEM_SELECTED;
          menuItemState = MENU_ITEM_STATE_SELECTED;
          menuItemSignal = 0;
        }
        break;

      case 'b': // кнопка "-"
        if (menuIdx != NO_MENU_ITEM_SELECTED &&
            menuItemState == MENU_ITEM_STATE_EDIT) {
          // Уменьшить выбранное значение
          debugln(F("MENU: signal to dec item value"));
          menuItemSignal = MENU_ITEM_SIGNAL_DEC;
        } else {
          // Перемещение по меню "вверх"
          debugln(F("MENU: move up"));
          if (menuIdx == 0) {
            menuIdx = MENU_ITEMS - 1;
          } else {
            menuIdx -= 1;
          }
        }
        break;

      case 'c': // кнопка "+"
        if (menuIdx != NO_MENU_ITEM_SELECTED &&
            menuItemState == MENU_ITEM_STATE_EDIT) {
          // Увеличить выбранное значение
          debugln(F("MENU: signal to inc item value"));
          menuItemSignal = MENU_ITEM_SIGNAL_INC;
        } else {
          // Перемещение по меню "вниз"
          debugln(F("MENU: move down"));
          if (menuIdx >= MENU_ITEMS - 1) {
            menuIdx = 0;
          } else {
            menuIdx += 1;
          }
        }
        break;

      case 'd': // кнопка "ok"
        if (menuItemState == MENU_ITEM_STATE_EDIT) {
          // Сохранить измененное значение и вернутся в режим листания меню.
          debugln(F("MENU: saving param to eeprom..."));

          menuItemSignal = MENU_ITEM_SIGNAL_SAVE;

          // TODO: move to menu?
          switch (menuIdx) {
          case 0:
            debug(F("EEPROM: saving before fan on delay: "));
            debugln(currentDelayBeforeFanOn);
            eepromSaveDelayBeforeFanOnValue(currentDelayBeforeFanOn);
            break;
          case 1:
            debug(F("EEPROM: saving fan work duration: "));
            debugln(currentFanWorkDurationMinutes);
            eepromSaveFanWorkDurationValue(currentFanWorkDurationMinutes);
            break;
          case 2:
            debug(F("EEPROM: saving fan on sensor level: "));
            debugln(currentFanOnSensorLevel);
            eepromSaveFanOnSensorValue(currentFanOnSensorLevel);
            break;
          case 3:
            debug(F("EEPROM: saving display brightness: "));
            debugln(currentDisplayBrigtness);
            eepromSaveDisplayBrightnessValue(currentDisplayBrigtness);
            break;
          case 4:
            debug(F("EEPROM: saving screensaver delay: "));
            debugln(currentScreensaverDelay);
            eepromSaveScreensaverOn(currentScreensaverDelay);
            break;
          }

          menuSavedMarkTimer = 2 * 1000;
          menuItemState = MENU_ITEM_STATE_SELECTED;

        } else {
          // Переход в режим редактирования значения
          debugln(F("MENU: edit value mode"));

          menuItemState = MENU_ITEM_STATE_EDIT;
          menuItemSignal = 0;

          // Сбросить изменения в объекте menu (если ранее было изменение без
          // сохранения)
          onMenuItemChangeAbort();
        }
      }
    }
    display.clearDisplay();
  }

  // Обновить значение в объекте меню
  if (menuItemState == MENU_ITEM_STATE_EDIT) {
    if (menuItemSignal == MENU_ITEM_SIGNAL_INC) {
      debugln(F("MENU: inc menu item value"));
      menu[menuIdx].incValue();

      debug(F("!menu item "));
      debug(menuIdx);
      debug(F(" value is "));
      debugln(menu[menuIdx].getValue());

      debug(F("param value is "));
      debugln(currentDelayBeforeFanOn);
    }
    if (menuItemSignal == MENU_ITEM_SIGNAL_DEC) {
      debugln(F("MENU: dec menu item value"));
      menu[menuIdx].decValue();

      debug(F("!menu item "));
      debug(menuIdx);
      debug(F(" value is "));
      debugln(menu[menuIdx].getValue());

      debug(F("param value is "));
      debugln(currentDelayBeforeFanOn);
    }
  }

  return menuItemSignal;
}

void onMenuItemChangeAbort() {

  switch (menuIdx) {
  case 0:
    currentDelayBeforeFanOn = eepromGetDelayBeforeFanOnValue();
    menu[0].setValue(currentDelayBeforeFanOn);
    break;
  case 1:
    currentFanWorkDurationMinutes = eepromGetFanWorkDurationValue();
    menu[1].setValue(currentFanWorkDurationMinutes);
    break;
  case 2:
    currentFanOnSensorLevel = eepromGetFanOnSensorValue();
    menu[2].setValue(currentFanOnSensorLevel);
    break;
  case 3:
    currentDisplayBrigtness = eepromGetDisplayBrightnessValue();
    menu[3].setValue(currentDisplayBrigtness);
    break;
  case 4:
    currentScreensaverDelay = eepromGetScreensaverOn();
    menu[4].setValue(currentScreensaverDelay);
    break;
  }
}

byte menuItemPassValueForSameIdx(byte val, byte idx) {
  if (menuIdx != idx) {
    return 0;
  }
  return val;
}

//
// Отрисовка одного элемента меню
//
void drawMenuItem(byte menuIdx, byte state, byte signal) {
  // MenuItem *item = menu[menuIdx];

  display.invertText(false);
  display.setFont(fontRus12x10);

  switch (menuIdx) {
  case 0:
    display.print(F("Пауза до"), 0, 15);
    display.print(F("включения"), 0, 15 * 2);
    break;
  case 1: // Длительность работы вент.
    display.print(F("Длительно-"), 0, 15);
    display.print(F("сть работы"), 0, 15 * 2);
    display.print(F("вент."), 0, 15 * 3);
    break;
  case 2: // Порог датчика света
    display.print(F("Порог"), 0, 15);
    display.print(F("датчика"), 0, 15 * 2);
    display.print(F("света"), 0, 15 * 3);
    break;
  case 3: // Яркость экрана
    display.print(F("Яркость"), 0, 15);
    display.print(F("экрана"), 0, 15 * 2);
    break;
  case 4: // Защита экрана через
    display.print(F("Защита эк-"), 0, 15);
    display.print(F("рана через"), 0, 15 * 2);
    break;
  case 5: // github link
    display.setFont(font6x8);
    display.print(F("GitHub:"), 0, 18);
    display.printWrapping(GITHUB_URL, 0, 18 * 2, false);
    return;
    break;
  case 6: // uptime
    display.print(F("Uptime"), 0, 15);
    display.printWrapping(uptimeForDisplay(), 0, 15 * 2, false);
    return;
    break;
  }

  int16_t displayVal = menu[menuIdx].display();
  byte maxValueDigits = 1;
  int16_t v = displayVal;
  for (size_t i = 0; v >= 10; i++) {
    v /= 10;
    if (v > 0) {
      maxValueDigits++;
    }
  }

  byte totalDigits = maxValueDigits + strlen(menu[menuIdx].getSuffix());

  if (state == MENU_ITEM_STATE_EDIT) {
    // Значение выбрано для изменения - инвертировать и подсветить текст
    display.invertText(true);
    display.setCursor(
        display.getWidth() - display.getFontWidth() * totalDigits - 6, 48);
  }

  display.setCursor(
      display.getWidth() - (display.getFontWidth() * totalDigits) - 2, 48);
  display.print(displayVal);
  display.print(menu[menuIdx].getSuffix());
  // free(item);
}

//
// Экран интро (отображается один раз при запуске устройства).
//
// Выводится название устройства, версия прошивки, web-ссылка на проект.
void displayIntroPage() {
  debugView(F("DISPLAY: intro view"), 0);

  static bool isCleared = false;

  if (!isCleared) {
    display.clearDisplay();

    display.invertDisplay(true);
    delay(300);
    display.invertDisplay(false);

    // Вывод версии прошивки
    display.setFont(fontRus6x8);
    display.print(F("версия:"), 0, 0);
    display.print(F(" "));
    display.print(VER_MAJOR);
    display.print(F("."));
    display.print(VER_MINOR);
    if (VER_PATCH != "") {
      display.print(F("-"));
      display.print(VER_PATCH);
    }

    // Вывод названия продукта
    display.setFont(fontRus12x10);
    display.print(F("Вентиляция"), 0, 16);

    // Вывод ссылки на github
    display.setFont(font6x8);
    display.printWrapping(GITHUB_URL, 0, 36, false);

    isCleared = true;
  }

  display.setFont(fontRus6x8);
  display.setCursor(display.getWidth() - display.getFontWidth() * 2, 0);
  if (introTimer / 1000 < 10) {
    display.print(F(" "));
  }
  display.print(introTimer / 1000);

  display.update();

  if (keypad.pollEvent()) {
    if (keypad.event.type == KM_KEYDOWN) {
      switch (keypad.event.c) {
      case 'd': // ok
                // быстро уйти со страницы интро
        display.clearDisplay();
        introTimer = 0;
        return;
      }
    }
  }
}

//
// Экран screensaver-а
//
void displayScreenSaverView() {
  debugView(F("DISPLAY: screensaver view"), 3);

  if (keypad.pollEvent()) {
    resetScreensaver();
    return;
  }

  unsigned long secsUp = millis() / 1000;

  display.clearDisplay();

  uint8_t sec2 = secsUp % 60 % 10;

  if (sec2 != 5 && sec2 != 6) {
    display.update();
    return;
  }

  display.setFont(fontRus6x8);
  uint8_t y = screensaverY;
  if (y >= 100) {
    y = display.getHeigth() - 8 - (screensaverY - 100) - 16;
  }
  const char *t = (char *)pgm_read_ptr(TEXT_SCEENSAVER);
  display.print(t, getXForDisplayTextCentered(t), y + 16);

  display.update();

  screensaverY += 1;
  if (screensaverY < 100) {
    if (screensaverY + 8 > display.getHeigth() - 16) {
      screensaverY = 100;
    }
  }
  if (screensaverY > 100 + (display.getHeigth() - 16) - 8) {
    screensaverY = 0;
  }
}

//
// Значение задержки включения в секундах.
//
// Так как значение хранится в eeprom в виде byte (0-255), то используется
// коэффициент.
uint16_t getDelayBeforeFanForDisplay() {
  return currentDelayBeforeFanOn * (uint8_t)delayBeforeFanOnSecondsMult;
}

uint16_t getFanWorkTimeForDisplay() {
  return currentFanWorkDurationMinutes;
}

uint16_t getFanOnSensorLevelForDisplay() {
  return currentFanOnSensorLevel;
}

uint16_t getDisplayBrigtnessForDisplay() {
  return currentDisplayBrigtness;
}

uint16_t getScreensaverDelayForDisplay() {
  return currentScreensaverDelay;
}

void setDisplayBrigtness(uint8_t v) {
  display.setBrightness(v);
}

void resetScreensaver() {
  screensaver = false;
  screensaverTimerInited = false;
  display.setBrightness(currentDisplayBrigtness);
}

void updateUptime() {
  if (millis() >= 3000000000) {
    utHighMillis = true;
  }

  if (millis() <= 100000 && utHighMillis) {
    utRollovers++;
    utHighMillis = false;

    debug(F("UPTIME rollovers: "));
    debugln(utRollovers);
  }

  unsigned long secsUp = millis() / 1000;
  utSecond = secsUp % 60;
  utMinute = (secsUp / 60) % 60;
  utHour = (secsUp / 60 / 60) % 24;
  // First portion takes care of a rollover [around 50 days]
  utDay = (utRollovers * 50) + (secsUp / 60 / 60 / 24);
}

String uptimeForDisplay() {
  // TODO: переделать на char[], если памяти станет не хватать.

  String r;

  r += utDay;
  r += "д ";

  if (utHour < 10) {
    r += F("0");
  }
  r += utHour;
  r += F("ч ");

  if (utMinute < 10) {
    r += F("0");
  }
  r += utMinute;
  r += F("м ");

  if (utSecond < 10) {
    r += F("0");
  }
  r += utSecond;
  r += F("с");

  return r;
}

uint16_t getStrWidthForDisplay(char *str) {
  return strlen(str) * display.getFontWidth();
}

uint16_t getStrWidthForDisplay(const char *str) {
  return strlen(str) * display.getFontWidth();
}

uint16_t getStrWidthForDisplay(String *str) {
  return str->length() * display.getFontWidth();
}

uint16_t getStrWidthForDisplay(const String *str) {
  return str->length() * display.getFontWidth();
}

uint16_t getXForDisplayTextCentered(char *str) {
  uint16_t c = getStrWidthForDisplay(str);
  if (c > display.getWidth()) {
    return 0;
  }
  return (display.getWidth() - c) / 2;
}

uint16_t getXForDisplayTextCentered(const char *str) {
  uint16_t c = getStrWidthForDisplay(str);
  if (c > display.getWidth()) {
    return 0;
  }
  return (display.getWidth() - c) / 2;
}

uint16_t getXForDisplayTextCentered(String *str) {
  uint16_t c = getStrWidthForDisplay(str);
  if (c > display.getWidth()) {
    return 0;
  }
  return (display.getWidth() - c) / 2;
}

uint16_t getXForDisplayTextCentered(const String *str) {
  uint16_t c = getStrWidthForDisplay(str);
  if (c > display.getWidth()) {
    return 0;
  }
  return (display.getWidth() - c) / 2;
}

uint16_t countDigits(uint16_t v) {
  uint16_t r = 1;
  for (size_t i = 0; v >= 10; i++) {
    r++;
    v /= 10;
  }
  return r;
}

//
// Установить значение таймера длительности работы вытяжки.
//
void setFanWorkTimer() {
  fanWorkTimer = currentFanWorkDurationMinutes;
  // значение хранится в минутах. конвертировать в миллисекунды.
  fanWorkTimer = fanWorkTimer * 60 * 1000;

  // debug(F("TIMER setting fan work timer value to "));
  // debugln(fanWorkTimer);
}

/*
 * Функция для проверки срабатывания таймера.
 *
 * Уменьшает значение таймера на дельту. При достижении нуля - возвращяет true.
 *
 * Если значение уже равно 0 - то вернет false (для избежания поврорных ложных
 * срабатываний).
 *
 * @param t таймер (в миллисекундах)
 * @param delta время в миллисекундах, прошедшее с предыдущей проверки. Обычно
 * это дельта из loop().
 */
bool isTimerOut(uint32_t *t, uint32_t delta) {
  if (*t > 0) {
    if (*t >= delta) {
      *t -= delta;
    } else {
      *t = 0;
    }

    if (*t == 0) {
      return true;
    }
  }
  return false;
}
