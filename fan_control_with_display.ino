#include "./src/KeyMatrix/KeyMatrix.h"
#include "./src/TroykaOLED/TroykaOLED.h"
#include "./storage.h"
#include <WString.h>
#include <Wire.h>
#include <avr/wdt.h>

// Иконка вентилятора
const unsigned char fanOff[] PROGMEM = {
    24,   24,   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
    0x1c, 0x7e, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfc, 0xf8, 0xf0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xfc, 0xfc, 0xfe, 0xfe, 0xfe,
    0xfe, 0xc6, 0x30, 0x7c, 0x7d, 0xfd, 0x7d, 0x7d, 0x83, 0xc7, 0xc7,
    0x81, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x01, 0x07, 0x0f, 0x1f,
    0x1f, 0x1f, 0x07, 0x01, 0x00, 0x00, 0x0e, 0x1e, 0x3f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x3f, 0x1f, 0x0f, 0x03, 0x00
    //
};

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

// Хотя кнопок всего 4 - проще использовать библиотеку работы с матрицой кнопок.
char keymap[2][2] = {{'a', 'b'}, {'c', 'd'}};
byte rowPins[2] = {5, 6};
byte colPins[2] = {7, 8};
KeyMatrix keypad((char *)keymap, (byte)2, (byte)2, rowPins, colPins);

//------------------------
// Глобальные переменные
//------------------------

byte menuIdx = 0;
#define MENU_ITEM_STATE_SELECTED 1
#define MENU_ITEM_STATE_EDIT 2
byte menuItemState = MENU_ITEM_STATE_SELECTED;
#define MENU_ITEM_SIGNAL_INC 1
#define MENU_ITEM_SIGNAL_DEC 2
#define MENU_ITEM_SIGNAL_SAVE 3

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
byte cfgFanWorkTimeMinutes = 5; // значение по-умолчанию
// Пороговое значение сенсора освещенности,
// по превышению которого активируется реле.
// [0=темно, 99=светло]
byte cfgFanOnSensorLevel = 40; // значение по-умолчанию
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

// ===========================================
// Стандартная функция перед вызовом loop().
// ===========================================
//
// Используется для инициализации дисплея
// и установки начальных значений глобальных переменных.
void setup() {
  Serial.begin(9600);

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
  display.setCoding(TXT_UTF8);
  // не обновлять автоматически. только при вызове update().
  display.autoUpdate(false);
  // уменьшить яркость (т к экран - это впомогательное устройство, не основное)
  display.setBrigtness(10);

  //-------------------------
  // считывание настроек из eeprom
  debugln(F("SETUP loading params from eeprom"));

  uint8_t v = eepromGetDelayBeforeFanOnValue();
  if (isDelayBeforeFanOnValueValid(v)) {
    cfgDelayBeforeFanOn = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveDelayBeforeFanOnValue(cfgDelayBeforeFanOn);
  }

  v = eepromGetFanWorkTimeValue();
  if (isFanWorkTimeValueValid(v)) {
    cfgFanWorkTimeMinutes = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanWorkTimeValue(cfgFanWorkTimeMinutes);
  }

  v = eepromGetFanOnSensorValue();
  if (isFanOnSensorLevelValueValid(v)) {
    cfgFanOnSensorLevel = v;
  } else {
    // невалидное значение в eeprom - переписать дефолтным
    eepromSaveFanOnSensorValue(cfgFanOnSensorLevel);
  }
  // -------------------------

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
      gLight > eepromGetFanOnSensorValue()) {
    beforeFanOnTimer = getDelayBeforeFanForDisplay();
    beforeFanOnTimer *= 1000;

    debug(F("TIMER setting delay timer value to "));
    debugln(beforeFanOnTimer);
  }

  // таймер задержки перед включением реле
  if (fanWorkTimer == 0 && \ 
    (
          // задержка не установлена
          eepromGetDelayBeforeFanOnValue() == 0 ||
          // или таймер задержки вышел
          isTimerOut(&beforeFanOnTimer, delta)
          //
          )
      //
  ) {
    if (gLight > eepromGetFanOnSensorValue()) {
      // установить значение таймера длительности работы вытяжки
      setFanWorkTimer();
      // включить реле
      digitalWrite(RELAY_PIN, true);
      debugln(F("RELAY on"));
    }
  }

  // таймер работы вытяжки
  if (isTimerOut(&fanWorkTimer, delta)) {
    if (gLight > eepromGetFanOnSensorValue()) {
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

// Установить значение таймера длительности работы вытяжки.
void setFanWorkTimer() {
  fanWorkTimer = eepromGetFanWorkTimeValue();
  // значение хранится в минутах. конвертировать в миллисекунды.
  fanWorkTimer = fanWorkTimer * 60 * 1000;

  debug(F("TIMER setting fan work timer value to "));
  debugln(fanWorkTimer);
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

// Валидатор значения задержки перед запуском вытяжки.
bool isDelayBeforeFanOnValueValid(byte seconds) {
  // необходимо использовать коэффициент
  bool ok = seconds * delayBeforeFanOnSecondsMult >= 0 &&
            seconds * delayBeforeFanOnSecondsMult <= 60 * 5;
  return ok;
}

// Валидатор длительности работы вытяжки.
bool isFanWorkTimeValueValid(byte minutes) {
  bool ok = minutes >= 0 && minutes <= 15;
  return ok;
}

// Валидатор порогового значения датчика.
bool isFanOnSensorLevelValueValid(byte percent) {
  bool ok = percent > 0 && percent < 99;
  return ok;
}

// Главный экран дисплея.
void displayMainView() {
  // debugln(F("DISPLAY main view"));
  display.clearDisplay();

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
  display.print(F("СВЕТ:"), 6 * 11 + 2, 3);

  display.setCursor(6 * 16 + 2, 3);
  if (gLight <= 9) {
    display.print(F(" "));
  }

  // вывод значений
  display.print(gLight);
  display.print(F("/"));
  byte cfgFanOnSensorLevel = eepromGetFanOnSensorValue();
  if (cfgFanOnSensorLevel <= 9) {
    display.print(F(" "));
  }
  display.print(cfgFanOnSensorLevel);

  // вывод состояния на основную чать дисплея
  display.drawImage(fanOff, 0, 16, 1);

  if (fanWorkTimer > 0) {
    // вытяжка включена - вывод таймера до отключения
    display.drawImage(breeze, 26, 16, 1);

    if (isFanOnRepeat) {
      display.drawImage(repeat, 54, 22, 1);
    }

    // значение таймера
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
    display.print(F("С."), 80 + 3 * fw, 30);
  } else {
    if (beforeFanOnTimer > 0) {
      // вывод таймера задержки перед включением
      display.print(F("ЗАДЕРЖКА "), 30, 24);
      display.print(beforeFanOnTimer / 1000);
    } else {
      // вытяжка выключена - состояние ожидания
      display.print(F("ОЖИДАНИЕ"), 30, 24);
    }
  }

  // uptime
  display.setFont(fontRus6x8);
  String uptime = uptimeForDisplay();
  size_t len = display.getTextLength(uptime);
  display.setCursor(display.getWidth() - len * display.getFontWidth(),
                    DISPLAY_H - display.getFontHeight());
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
  // debugln(F("DISPLAY menu view"));

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
          cfgFanWorkTimeMinutes = eepromGetFanWorkTimeValue();
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
            menuIdx = 3;
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
          if (menuIdx >= 3) {
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
            eepromSaveFanWorkTimeValue(cfgFanWorkTimeMinutes);
            break;
          case 3:
            eepromSaveFanOnSensorValue(cfgFanOnSensorLevel);
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
    byte x = DISPLAY_W - 9 * display.getFontWidth() - 2 * 2;
    display.drawRect(x - 2, 0, x + 2 + display.getFontWidth() * 9, 10, true, 1);
    display.print(text, x, 1);
    display.invertText(false);
  }

  // ---
  display.setCursor(0, 0);
  display.print(F("МЕНЮ"));
  if (exitMenuTimer > 0 && exitMenuTimer < 5 * 1000) {
    display.print(F("    выход через "));
    display.print(exitMenuTimer / 1000);
  }

  // --------------------
  // Меню - вывод значений настроек
  byte valuesX = 98;

  // Вывод элементов меню
  menuItem1(menuItemPassValueForSameIdx(menuItemState, 1),
            menuItemPassValueForSameIdx(menuItemSignal, 1));
  menuItem2(menuItemPassValueForSameIdx(menuItemState, 2),
            menuItemPassValueForSameIdx(menuItemSignal, 2));
  menuItem3(menuItemPassValueForSameIdx(menuItemState, 3),
            menuItemPassValueForSameIdx(menuItemSignal, 3));

  display.update();
}

byte menuItemPassValueForSameIdx(byte val, byte idx) {
  if (menuIdx != idx) {
    return 0;
  }
  return val;
}

void menuItem1(byte state, byte signal) {
  byte valueXPos = 98;

  display.invertText(false);

  if (state == MENU_ITEM_STATE_SELECTED || state == MENU_ITEM_STATE_EDIT) {
    display.drawImage(arrowRight, 0, 20, 1);
  }

  display.print(F("Задержка перед"), 8, 16);
  display.print(F("вкл. вент."), 8, 24);

  if (state == MENU_ITEM_STATE_EDIT) {
    if (signal == MENU_ITEM_SIGNAL_INC) {
      if (isDelayBeforeFanOnValueValid(cfgDelayBeforeFanOn + 1)) {
        cfgDelayBeforeFanOn += 1;
      }
    }
    if (signal == MENU_ITEM_SIGNAL_DEC) {
      if (isDelayBeforeFanOnValueValid(cfgDelayBeforeFanOn - 1)) {
        cfgDelayBeforeFanOn -= 1;
      }
    }
  }

  if (state == MENU_ITEM_STATE_EDIT) {
    // Значение выбрано для изменения - инвертировать и подсветить текст
    display.invertText(true);
    drawMenuValueFocus(valueXPos, 20, 5);
  }

  int16_t displayVal = getDelayBeforeFanForDisplay();

  // Значение из 3 цифр
  display.setCursor(valueXPos, 20);
  if (displayVal <= 9) {
    display.print(F(" "));
  }
  if (displayVal <= 99) {
    display.print(F(" "));
  }
  display.print(displayVal);
  display.print(F("с."));
}

void menuItem2(byte state, byte signal) {
  byte valueXPos = 98;

  display.invertText(false);

  if (state == MENU_ITEM_STATE_SELECTED || state == MENU_ITEM_STATE_EDIT) {
    display.drawImage(arrowRight, 0, 40, 1);
  }

  display.print(F("Длительность"), 8, 36);
  display.print(F("работы вент."), 8, 44);

  if (state == MENU_ITEM_STATE_EDIT) {
    if (signal == MENU_ITEM_SIGNAL_INC) {
      if (isFanWorkTimeValueValid(cfgFanWorkTimeMinutes + 1)) {
        cfgFanWorkTimeMinutes += 1;
      }
    }
    if (signal == MENU_ITEM_SIGNAL_DEC) {
      if (isFanWorkTimeValueValid(cfgFanWorkTimeMinutes - 1)) {
        cfgFanWorkTimeMinutes -= 1;
      }
    }
  }

  if (state == MENU_ITEM_STATE_EDIT) {
    // Значение выбрано для изменения - инвертировать и подсветить текст
    display.invertText(true);
    drawMenuValueFocus(valueXPos, 40, 5);
  }

  int16_t displayVal = cfgFanWorkTimeMinutes;

  // Значение из 2 цифр
  display.setCursor(valueXPos + display.getFontWidth(), 40);
  if (displayVal <= 9) {
    display.print(F(" "));
  }
  display.print(displayVal);
  display.print(F("м."));
}

void menuItem3(byte state, byte signal) {
  byte valueXPos = 98;

  display.invertText(false);

  if (state == MENU_ITEM_STATE_SELECTED || state == MENU_ITEM_STATE_EDIT) {
    display.drawImage(arrowRight, 0, 56, 1);
  }

  display.print(F("Порог датчика"), 8, 56);

  if (state == MENU_ITEM_STATE_EDIT) {
    if (signal == MENU_ITEM_SIGNAL_INC) {
      if (isFanOnSensorLevelValueValid(cfgFanOnSensorLevel + 1)) {
        cfgFanOnSensorLevel += 1;
      }
    }
    if (signal == MENU_ITEM_SIGNAL_DEC) {
      if (isFanOnSensorLevelValueValid(cfgFanOnSensorLevel - 1)) {
        cfgFanOnSensorLevel -= 1;
      }
    }
  }

  if (state == MENU_ITEM_STATE_EDIT) {
    // Значение выбрано для изменения - инвертировать и подсветить текст
    display.invertText(true);
    drawMenuValueFocus(valueXPos, 56, 5);
  }

  int16_t displayVal = cfgFanOnSensorLevel;

  // Значение из 2 цифр
  display.setCursor(valueXPos + display.getFontWidth(), 56);
  if (displayVal <= 9) {
    display.print(F(" "));
  }
  display.print(displayVal);
  display.print(F("%"));
}

// Рисует залитый прямоугольник
void drawMenuValueFocus(byte x, byte y, byte digits) {
  display.drawRect(x - 2, y - 1, x + digits * display.getFontWidth() + 2,
                   y + display.getFontHeight() + 1, true, 1);
}

// Значение задержки включения в секундах.
//
// Так как значение хранится в eeprom в виде byte (0-255), то используется
// коэффициент.
uint16_t getDelayBeforeFanForDisplay() {
  return cfgDelayBeforeFanOn * (uint8_t)delayBeforeFanOnSecondsMult;
}

// Экран интро (отображается один раз при запуске устройства).
//
// Выводится название устройства, версия прошивки, web-ссылка на проект.
void displayIntroPage() {
  // debugln(F("DISPLAY intro view"));

  static bool isCleared = false;

  if (!isCleared) {
    display.clearDisplay();

    display.invertDisplay(true);
    delay(300);
    display.invertDisplay(false);

    display.setFont(fontRus6x8);
    display.print(F("версия: "), 0, 0);
    display.print(VER_MAJOR);
    display.print(F("."));
    display.print(VER_MINOR);
    if (VER_PATCH != "") {
      display.print(F("-"));
      display.print(VER_PATCH);
    }

    display.setFont(fontRus12x10);
    display.print(F("Вентиляция"), 0, 16);

    display.setFont(fontRus6x8);
    display.printWrapping(String(GITHUB_URL), 0, 36, false);

    isCleared = true;
  }

  display.setFont(fontRus6x8);
  display.setCursor(DISPLAY_W - display.getFontWidth() * 2, 0);
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
    y = DISPLAY_H - 8 - (screensaverY - 100) - 16;
  }
  display.print(F("РЕЖИМ СНА ЭКРАНА"), 17, y + 16);

  display.update();

  screensaverY += 1;
  if (screensaverY < 100) {
    if (screensaverY + 8 > DISPLAY_H - 16) {
      screensaverY = 100;
    }
  }
  if (screensaverY > 100 + (DISPLAY_H - 16) - 8) {
    screensaverY = 0;
  }
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
  String r = "";

  r += utDay;
  r += F("д ");

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