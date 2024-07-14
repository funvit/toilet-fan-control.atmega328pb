
#include <WString.h>

class MenuItem {
private:
  const char *suffix;
  uint8_t value;
  uint8_t valueStep = 1;
  uint8_t valueMin = 0;
  uint8_t valueMax = 255;

  uint16_t (*displayer)();
  void (*onValueChange)(uint8_t v);

  uint8_t(*updateOnChange);

public:
  MenuItem();
  MenuItem(const char *suffix, uint8_t valueMin, uint8_t valueMax,
           uint8_t valueStep);

  const char *getSuffix() {
    return this->suffix;
  }
  uint8_t getValue() {
    return this->value;
  }

  // Увеличить значение на величину шага с учетом min и max границ.
  void incValue() {
    uint8_t v = this->value;

    if (v >= this->valueMax - this->valueStep) {
      v = this->valueMax;
    } else {
      v += this->valueStep;
      if (v > this->valueMax) {
        v = this->valueMax;
      } else if (v < this->valueMin) {
        v = this->valueMin;
      }
    }

    this->value = v;
    if (this->onValueChange) {
      this->onValueChange(value);
    }
    if (this->updateOnChange) {
      *this->updateOnChange = value;
    }
  }

  // Уменьшить значение на величину шага с учетом min и max границ.
  void decValue() {
    uint8_t v = this->value;

    if (v <= this->valueStep) {
      v = this->valueMin;
    } else {
      v -= this->valueStep;
      if (v > this->valueMax) {
        v = this->valueMax;
      } else if (v < this->valueMin) {
        v = this->valueMin;
      }
    }

    this->value = v;
    if (this->onValueChange) {
      this->onValueChange(this->value);
    }
    if (this->updateOnChange) {
      *this->updateOnChange = value;
    }
  }
  // Устанавливает внутреннее значение.
  // Callback-и не вызываются.
  void setValue(uint8_t v) {
    this->value = v;
    // if (this->updateOnChange) {
    //   *this->updateOnChange = value;
    // }
  }
  void setDisplayer(uint16_t (*fn)()) {
    this->displayer = fn;
  }
  // Указанная функция будет вызываться при изменении внутреннего значения.
  void setOnValueChange(void (*fn)(uint8_t v)) {
    this->onValueChange = fn;
  }
  // Обновляет значение по указателю при изменении внутреннего значения.
  void setUpdateOnValueChange(uint8_t *v) {
    this->updateOnChange = v;
  }
  // Возвращает значение для отображения на экране.
  uint16_t display() {
    return this->displayer();
  }
};
