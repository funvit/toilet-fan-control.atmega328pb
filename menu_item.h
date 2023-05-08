
#include <WString.h>

class MenuItem {
private:
  const char(*titles[3]);
  const char(*suffix);
  uint8_t value;
  uint8_t valueStep = 1;
  uint8_t valueMin = 0;
  uint8_t valueMax = 255;

  uint16_t (*displayer)();
  void (*onValueChange)(uint8_t v);

  uint8_t(*updateOnChange);

public:
  MenuItem();
  MenuItem(const char *str[3], const char *suffix);
  MenuItem(const char *str[3], const char *suffix, uint8_t valueMin,
           uint8_t valueMax, uint8_t valueStep);

  const char *getTitle(uint8_t idx) {
    return titles[idx];
  }
  const char *getSuffix() {
    return suffix;
  }
  uint8_t getValue() {
    return value;
  }

  // ��������� �������� �� �������� ����.
  void incValue() {
    uint8_t v = value;

    if (v >= valueMax - valueStep) {
      v = valueMax;
    } else {
      v += valueStep;
      if (v > valueMax) {
        v = valueMax;
      } else if (v < valueMin) {
        v = valueMin;
      }
    }

    value = v;
    if (onValueChange) {
      onValueChange(value);
    }
    if (updateOnChange) {
      *updateOnChange = value;
    }
  }
  // ��������� �������� �� �������� ����.
  void decValue() {
    uint8_t v = value;

    if (v <= valueStep) {
      v = valueMin;
    } else {
      v -= valueStep;
      if (v > valueMax) {
        v = valueMax;
      } else if (v < valueMin) {
        v = valueMin;
      }
    }

    value = v;
    if (onValueChange) {
      onValueChange(value);
    }
    if (updateOnChange) {
      *updateOnChange = value;
    }
  }
  // ������������� ���������� ��������.
  // Callback-� �� ����������.
  void setValue(uint8_t v) {
    value = v;
  }
  void setDisplayer(uint16_t (*fn)()) {
    displayer = fn;
  }
  // ��������� ������� ����� ���������� ��� ��������� ����������� ��������.
  void setOnValueChange(void (*fn)(uint8_t v)) {
    onValueChange = fn;
  }
  // ��������� �������� �� ��������� ��� ��������� ����������� ��������.
  void setUpdateOnValueChange(uint8_t *v) {
    updateOnChange = v;
  }
  // ���������� �������� ��� ����������� �� ������.
  uint16_t display() {
    return displayer();
  }
};
