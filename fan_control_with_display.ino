/*
  Keep this file in win-1251 encoding!
*/

#include "./menu_item.h"
#include "./src/KeyMatrix/KeyMatrix.h"
#include "./src/TroykaOLED/TroykaOLED.h"
#include "./storage.h"
#include <WString.h>
#include <Wire.h>
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

#define GITHUB_URL "https://github.com/funvit/toilet-fan-control.atmega328pb"
#define VER_MAJOR 0
#define VER_MINOR 4
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

// Меню
#define MENU_ITEMS 4
byte menuIdx = 0;
#define MENU_ITEM_STATE_SELECTED 1
#define MENU_ITEM_STATE_EDIT 2
byte menuItemState = MENU_ITEM_STATE_SELECTED;
#define MENU_ITEM_SIGNAL_INC 1
#define MENU_ITEM_SIGNAL_DEC 2
#define MENU_ITEM_SIGNAL_SAVE 3

const char *Menu1[3] = {"Пауза до", "включения", ""};
const char *Menu2[3] = {"Длительно-", "сть работы", "вент."};
const char *Menu3[3] = {"Порог", "датчика", "света"};
const char *Menu4[3] = {"Яркость", "экрана", ""};

// Тексты
const char *TextSpace = " ";
const char *TextDelimiter = "/";
const char *TextMainScreenDelayMode = "ПАУЗА";
const char *TextMainScreenWaitMode = "ОЖИДАНИЕ";
const char *TextMinuteBig = "М.";
const char *TextMinuteSmall = "м.";
const char *TextSecondBig = "C.";
const char *TextSecondSmall = "c.";
const char *TextPercentSmall = "%";
const char *TextMenu = "МЕНЮ";
const char *TextMenuExitAfter = "выход через";
const char *TextVersion = "версия:";
const char *TextVersionDot = ".";
const char *TextVersionMinus = "-";
const char *TextVentilation = "Вентиляция";
const char *TextSceensaver = "РЕЖИМ СНА ЭКРАНА";
const char *TextLight = "СВЕТ:";

// screen saver
bool screensaverTimerInited = false;
bool screensaver = false;
uint32_t screensaverTimer;
uint8_t screensaverY = 0;

byte gLight = 0;
uint32_t exitMenuTimer;
uint32_t beforeFanOnTimer;
uint32_t fanWorkTimer;

bool isFanOnRepeat;

uint32_t menuSavedMarkTimer = 0;

//------------------------
// Настройки.
//------------------------

// Множитель для значения cfgDelayBeforeFanOn, что бы получить значение в
// секундах. Позволяет изменять значение на странице меню с установленным шагом.
#define delayBeforeFanOnSecondsMult 10
// Задержка перед включением реле
byte cfgDelayBeforeFanOn = 0; // значение по-умолчанию
// Длительность замкнутого состояния реле
byte cfgFanWorkDurationMinutes = 5; // значение по-умолчанию
// Пороговое значение сенсора освещенности,
// по превышению которого активируется реле.
// [0=темно, 99=светло]
byte cfgFanOnSensorLevel = 40; // значение по-умолчанию
// Яркость экрана
byte cfgDisplayBrigtness = 4;
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

// Кол-во секунд перед запуском screensaver-а.
// 0 = disabled.
// FIXME: enable screensaver
// #define SCREENSAVER 10 * 1000
#define SCREENSAVER 0

//------------------------
// debug, debugln
//------------------------
#define DEBUG 1 // set to 1 for serial print enable.

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x) ((void)0)
#define debugln(x) ((void)0)
#endif

MenuItem menu[MENU_ITEMS];

// ===========================================
// Стандартная функция перед вызовом loop().
// ===========================================
//
// Используется для инициализации дисплея
// и установки начальных значений глобальных переменных.
void setup() {
  Serial.begin(9600);
  Serial.println(F("======================="));
  Serial.println(F("BOOT"));

  Serial.println(F("PROJECT Toilet fan controller"));
  Serial.println(F("SETUP start"));

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
  display.setBrigtness(1); // default

  //-------------------------
  // считывание настроек из eeprom
  debugln(F("SETUP loading params from eeprom"));

  uint8_t v = eepromGetDelayBeforeFanOnValue();
  // FIXME: fix validator somehow
  // if (isDelayBeforeFanOnValueValid(v)) {
  // debugln(F("EEPROM: delay before fan loaded"));
  cfgDelayBeforeFanOn = v;
  // } else {
  //   // невалидное значение в eeprom - переписать дефолтным
  //   eepromSaveDelayBeforeFanOnValue(cfgDelayBeforeFanOn);
  // }

  v = eepromGetFanWorkDurationValue();
  if (isFanWorkDurationValueValid(v)) {
    // debugln(F("EEPROM: fan work duration loaded"));
    cfgFanWorkDurationMinutes = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanWorkDurationValue(cfgFanWorkDurationMinutes);
  }

  v = eepromGetFanOnSensorValue();
  if (isFanOnSensorLevelValueValid(v)) {
    // debugln(F("EEPROM: sensor level loaded"));
    cfgFanOnSensorLevel = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanOnSensorValue(cfgFanOnSensorLevel);
  }

  v = eepromGetDisplayBrigtnessValue();
  if (isDisplayBrigtnessValueValid(v)) {
    delay(100);
    cfgDisplayBrigtness = v;
    display.setBrigtness(v);
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveDisplayBrigtnessValue(cfgDisplayBrigtness);
  }
  // -------------------------

  // =========================
  // МЕНЮ
  debugln(F("Init menu"));
  delay(100);

  MenuItem m1 = MenuItem(Menu1, TextSecondSmall, 0, 30, 1);
  m1.setValue(cfgDelayBeforeFanOn);
  m1.setUpdateOnValueChange(&cfgDelayBeforeFanOn);
  m1.setDisplayer(&getDelayBeforeFanForDisplay);
  menu[0] = m1;
  // debugln(F("Init menu: item 1 ok"));

  MenuItem m2 = MenuItem(Menu2, TextMinuteSmall, 1, 15, 1);
  m2.setValue(cfgFanWorkDurationMinutes);
  m2.setUpdateOnValueChange(&cfgFanWorkDurationMinutes);
  m2.setDisplayer(&getFanWorkTimeForDisplay);
  menu[1] = m2;
  // debugln(F("Init menu: item 2 ok"));

  MenuItem m3 = MenuItem(Menu3, TextPercentSmall, 1, 99, 1);
  m3.setValue(cfgFanOnSensorLevel);
  m3.setUpdateOnValueChange(&cfgFanOnSensorLevel);
  m3.setDisplayer(&getFanOnSensorLevelForDisplay);
  menu[2] = m3;
  // debugln(F("Init menu: item 3 ok"));

  MenuItem m4 = MenuItem(Menu4, 0, 0, 255, 15);
  m4.setValue(cfgDisplayBrigtness);
  m4.setUpdateOnValueChange(&cfgDisplayBrigtness);
  m4.setDisplayer(&getDisplayBrigtnessForDisplay);
  m4.setOnValueChange(&setDisplayBrigtness);
  menu[3] = m4;
  // debugln(F("Init menu: item 4 ok"));
  // /===================

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
    displayIntroPage();
  } else {
    if (menuIdx == 0) {
      // Отобразить главный экран.

      if (screensaver) {
        screensaverTimerInited = false;
        displayScreenSaverView();
      } else {
        if (!screensaverTimerInited && SCREENSAVER != 0) {
          screensaverTimer = SCREENSAVER;
          screensaverTimerInited = true;
        }

        displayMainView();
      }
    } else {
      // Отобразить экран меню.
      screensaver = false;
      screensaverTimerInited = false;
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
    menuIdx = 0;
    menuItemState = MENU_ITEM_STATE_SELECTED;
  }

  if (!screensaver && isTimerOut(&screensaverTimer, delta)) {
    screensaver = true;
  }

  // реагирование на превышение порога сенсора
  if (beforeFanOnTimer == 0 && fanWorkTimer == 0 &&
      gLight > cfgFanOnSensorLevel) {
    beforeFanOnTimer = getDelayBeforeFanForDisplay();
    beforeFanOnTimer *= 1000;

    // debug(F("TIMER setting delay timer value to "));
    // debugln(beforeFanOnTimer);
  }

  // таймер задержки перед включением реле
  if (fanWorkTimer == 0 && \ 
    (
          // задержка не установлена
          cfgDelayBeforeFanOn == 0 ||
          // или таймер задержки вышел
          isTimerOut(&beforeFanOnTimer, delta)
          //
          )
      //
  ) {
    if (gLight > cfgFanOnSensorLevel) {
      // установить значение таймера длительности работы вытяжки
      setFanWorkTimer();
      // включить реле
      digitalWrite(RELAY_PIN, true);
      debugln(F("RELAY on"));
    }
  }

  // таймер работы вытяжки
  if (isTimerOut(&fanWorkTimer, delta)) {
    if (gLight > cfgFanOnSensorLevel) {
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

uint16_t getStrWidthForDisplay(char *str) {
  return strlen(str) * display.getFontWidth();
}

uint16_t getStrWidthForDisplay(const char *str) {
  return strlen(str) * display.getFontWidth();
}

uint16_t getStrWidthForDisplay(String str) {
  return str.length() * display.getFontWidth();
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

uint16_t countDigits(uint16_t v) {
  uint16_t r = 1;
  for (size_t i = 0; v >= 10; i++) {
    r++;
    v /= 10;
  }
  return r;
}

// Установить значение таймера длительности работы вытяжки.
void setFanWorkTimer() {
  fanWorkTimer = cfgFanWorkDurationMinutes;
  // значение хранится в минутах. конвертировать в миллисекунды.
  fanWorkTimer = fanWorkTimer * 60 * 1000;

  // debug(F("TIMER setting fan work timer value to "));
  // debugln(fanWorkTimer);
}

/** Функция для проверки срабатывания таймера.
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

// Главный экран дисплея.
void displayMainView() {
  debugln(F("DISPLAY main view"));

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
                      11 - round(cfgFanOnSensorLevel / 11.0 + 0.49), 1);
  }

  display.setFont(fontRus6x8);
  // информация от сенсора
  display.print(TextLight, 6 * 11 + 2, 3);

  display.setCursor(6 * 16 + 2, 3);
  if (gLight <= 9) {
    display.print(TextSpace);
  }

  // вывод значений
  display.print(gLight);
  display.print(TextDelimiter);
  // byte cfgFanOnSensorLevel = eepromGetFanOnSensorValue();
  if (cfgFanOnSensorLevel <= 9) {
    display.print(TextSpace);
  }
  display.print(cfgFanOnSensorLevel);

  // вывод состояния на основную чать дисплея
  if (fanWorkTimer > 0) {
    // вытяжка включена - вывод таймера до отключения
    display.setFont(mediumNumbers);
    byte x = 80;
    if (fanWorkTimer / 1000 < 100) {
      x += display.getFontWidth();
    }
    if (fanWorkTimer / 1000 < 10) {
      x += display.getFontWidth();
    }
    display.print(fanWorkTimer / 1000, x, 21);
    byte fw = display.getFontWidth();
    display.setFont(fontRus6x8);
    display.print(TextSecondBig, 80 + 3 * fw, 30);
  } else {
    if (beforeFanOnTimer > 0) {
      // вывод таймера задержки перед включением
      display.setFont(fontRus12x10);
      display.print(TextMainScreenDelayMode, 0, 24);
      uint16_t v = beforeFanOnTimer / 1000;
      display.print(
          beforeFanOnTimer / 1000,
          display.getWidth() - countDigits(v) * display.getFontWidth(), 24);
    } else {
      display.setFont(fontRus12x10);
      // вытяжка выключена - состояние ожидания
      display.print(TextMainScreenWaitMode,
                    getXForDisplayTextCentered(TextMainScreenWaitMode), 24);
    }
  }

  // uptime
  display.setFont(fontRus6x8);
  String uptime = uptimeForDisplay();
  display.setCursor(display.getWidth() - getStrWidthForDisplay(uptime) -
                        (30 - utSecond / 2),
                    display.getHeigth() - display.getFontHeight());
  display.print(uptime);

  display.update();

  // Кнопки
  if (keypad.pollEvent()) {
    // есть событие, обрабатываем
    if (keypad.event.type == KM_KEYDOWN) {
      switch (keypad.event.c) {
      case 'a':
        // нажата кнопка "меню"
        debugln(F("VIEW main: menu key pressed"));
        menuIdx = 1;
        display.clearDisplay();
        break;

      default:
        break;
      }
    }
  }
}

// Экран меню.
void displayMenuView() {
  debugln(F("DISPLAY menu view"));

  display.clearDisplay();

  byte menuItemSignal = 0;

  // Кнопки
  if (keypad.pollEvent()) {
    // установка таймера по бездействию
    exitMenuTimer = 20 * 1000;

    if (keypad.event.type == KM_KEYDOWN) {
      switch (keypad.event.c) {
      case 'a': // кнопка "menu" работает как отмена
        if (menuItemState == MENU_ITEM_STATE_EDIT) {
          // отменить изменения (считать текущие сохраненные значения из eeprom)
          cfgDelayBeforeFanOn = eepromGetDelayBeforeFanOnValue();
          cfgFanWorkDurationMinutes = eepromGetFanWorkDurationValue();
          cfgFanOnSensorLevel = eepromGetFanOnSensorValue();

          menuItemState = MENU_ITEM_STATE_SELECTED;
        } else {
          // выйти со страницы
          menuIdx = 0;
          menuItemState = MENU_ITEM_STATE_SELECTED;
          menuItemSignal = 0;
        }
        break;

      case 'b': // кнопка "-"
        if (menuIdx > 0 && menuItemState == MENU_ITEM_STATE_EDIT) {
          // уменьшить выбранное значение
          menuItemSignal = MENU_ITEM_SIGNAL_DEC;
        } else {
          // перемещение по меню "вверх"
          if (menuIdx == 1) {
            menuIdx = MENU_ITEMS;
          } else {
            menuIdx -= 1;
          }
        }
        break;

      case 'c': // кнопка "+"
        if (menuIdx > 0 && menuItemState == MENU_ITEM_STATE_EDIT) {
          // увеличить выбранное значение
          menuItemSignal = MENU_ITEM_SIGNAL_INC;
        } else {
          // перемещение по меню "вниз"
          if (menuIdx >= MENU_ITEMS) {
            menuIdx = 1;
          } else {
            menuIdx += 1;
          }
        }
        break;

      case 'd': // кнопка "ok"
        if (menuItemState == MENU_ITEM_STATE_EDIT) {
          // сохранение выбранного значения
          debugln(F("CONFIG saving to eeprom..."));

          menuItemSignal = MENU_ITEM_SIGNAL_SAVE;

          switch (menuIdx) {
          case 1:
            eepromSaveDelayBeforeFanOnValue(cfgDelayBeforeFanOn);
            break;
          case 2:
            eepromSaveFanWorkDurationValue(cfgFanWorkDurationMinutes);
            break;
          case 3:
            eepromSaveFanOnSensorValue(cfgFanOnSensorLevel);
            break;
          case 4:
            eepromSaveDisplayBrigtnessValue(cfgDisplayBrigtness);
            break;
          }

          menuSavedMarkTimer = 2 * 1000;
          menuItemState = MENU_ITEM_STATE_SELECTED;
        } else {
          menuItemState = MENU_ITEM_STATE_EDIT;
          menuItemSignal = 0;
        }
      }
    }
    display.clearDisplay();
  }

  if (menuIdx == 0) {
    // очистить экран при выходе со страницы
    display.clearDisplay();
    return;
  }

  // --------------
  // Меню - вывод текстов
  display.setFont(fontRus6x8);

  if (menuSavedMarkTimer > 0) {
    // вывод в 1 строке метки "сохранено"
    display.invertText(true);
    char text[] = "сохранено";
    byte x = display.getWidth() - 9 * display.getFontWidth() - 2 * 2;
    display.drawRect(x - 2, 0, x + 2 + display.getFontWidth() * 9, 10, true, 1);
    display.print(text, x, 1);
    display.invertText(false);
  }

  // ---
  display.setCursor(0, 0);
  display.invertText(false);
  display.print(TextMenu);
  if (exitMenuTimer > 0 && exitMenuTimer < 5 * 1000) {
    display.setCursor(display.getWidth() - (strlen(TextMenuExitAfter) + 3) *
                                               display.getFontWidth(),
                      0);
    display.print(TextMenuExitAfter);
    display.print(exitMenuTimer / 1000);
  }

  // --------------------
  // Меню - вывод значений настроек
  byte valuesX = 98;

  // Вывод элементов меню
  if (0 < menuIdx && menuIdx <= MENU_ITEMS) {
    drawMenuItem(&menu[menuIdx - 1],
                 menuItemPassValueForSameIdx(menuItemState, menuIdx),
                 menuItemPassValueForSameIdx(menuItemSignal, menuIdx));
  }

  // Вывод текущей позиции меню
  byte menuItems = 4;
  byte menuPagerItemWidth = display.getWidth() / menuItems;
  for (byte i = 0; i < display.getWidth(); i += 2) {
    display.drawPixel(i, 14, WHITE);
  }

  for (byte i = 0; i < 3; i++) {
    display.drawLine(menuPagerItemWidth * (menuIdx - 1) + 2, 13 + i,
                     menuPagerItemWidth * (menuIdx)-2, 13 + i, WHITE);
  }

  display.update();
}

byte menuItemPassValueForSameIdx(byte val, byte idx) {
  if (menuIdx != idx) {
    return 0;
  }
  return val;
}

void drawMenuItem(MenuItem *item, byte state, byte signal) {
  display.invertText(false);
  display.setFont(fontRus12x10);

  for (byte i = 0; i < 3; i++) {
    String t = item->getTitle(i);
    display.print(t, 0, 15 * i + 15);
  }

  if (state == MENU_ITEM_STATE_EDIT) {
    if (signal == MENU_ITEM_SIGNAL_INC) {
      debugln(F("inc menu item value"));
      item->incValue();
    }
    if (signal == MENU_ITEM_SIGNAL_DEC) {
      debugln(F("dec menu item value"));
      item->decValue();
    }
  }

  int16_t displayVal = item->display();

  byte maxValueDigits = 1;
  int16_t v = displayVal;
  for (size_t i = 0; v >= 10; i++) {
    v /= 10;
    if (v > 0) {
      maxValueDigits++;
    }
  }

  byte totalDigits = maxValueDigits + strlen(item->getSuffix());

  if (state == MENU_ITEM_STATE_EDIT) {
    // Значение выбрано для изменения - инвертировать и подсветить текст
    display.invertText(true);
    display.setCursor(
        display.getWidth() - display.getFontWidth() * totalDigits - 6, 48);
  }

  display.setCursor(
      display.getWidth() - (display.getFontWidth() * totalDigits) - 2, 48);
  display.print(displayVal);
  display.print(item->getSuffix());
}

// Экран интро (отображается один раз при запуске устройства).
//
// Выводится название устройства, версия прошивки, web-ссылка на проект.
void displayIntroPage() {
  debugln(F("DISPLAY intro view"));

  static bool isCleared = false;

  if (!isCleared) {
    display.clearDisplay();

    display.invertDisplay(true);
    delay(300);
    display.invertDisplay(false);

    // debugln(F("A"));
    // // Вывод версии прошивки
    // display.setFont(fontRus6x8);
    // display.print(TextVersion, 0, 0);
    // display.print(TextSpace);
    // display.print(VER_MAJOR);
    // display.print(TextVersionDot);
    // display.print(VER_MINOR);
    // if (VER_PATCH != "") {
    //   display.print(TextVersionMinus);
    //   display.print(VER_PATCH);
    // }

    debugln(F("B"));
    delay(100);
    // Вывод названия продукта
    display.setFont(fontRus12x10);
    // display.print(TextVentilation, 0, 16);
    display.print("Ventilation", 0, 16);

    // Вывод ссылки на github
    // display.setFont(font6x8);
    // display.printWrapping(String(GITHUB_URL), 0, 36, false);

    isCleared = true;
  }

  debugln(F("C"));

  display.setFont(fontRus6x8);
  display.setCursor(display.getWidth() - display.getFontWidth() * 2, 0);
  if (introTimer / 1000 < 10) {
    display.print(TextSpace);
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

// Экран screensaver-а.
void displayScreenSaverView() {
  // debugln(F("DISPLAY screensaver view"));

  if (keypad.pollEvent()) {
    screensaver = false;
    screensaverTimerInited = false;
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
  display.print(TextSceensaver, display.getWidth() - strlen(TextSceensaver) / 2,
                y + 16);

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

// Значение задержки включения в секундах.
//
// Так как значение хранится в eeprom в виде byte (0-255), то используется
// коэффициент.
uint16_t getDelayBeforeFanForDisplay() {
  return cfgDelayBeforeFanOn * (uint8_t)delayBeforeFanOnSecondsMult;
}

uint16_t getFanWorkTimeForDisplay() { return cfgFanWorkDurationMinutes; }

uint16_t getFanOnSensorLevelForDisplay() { return cfgFanOnSensorLevel; }

uint16_t getDisplayBrigtnessForDisplay() { return cfgDisplayBrigtness; }

void setDisplayBrigtness(uint8_t v) { display.setBrigtness(v); }

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
  String r = "";

  r.concat(utDay);
  r.concat(F("д "));

  if (utHour < 10) {
    r.concat(F("0"));
  }
  r.concat(utHour);
  r.concat(F("ч "));

  if (utMinute < 10) {
    r.concat(F("0"));
  }
  r.concat(utMinute);
  r.concat(F("м "));

  if (utSecond < 10) {
    r.concat(F("0"));
  }
  r.concat(utSecond);
  r.concat(F("с "));

  return r;
}