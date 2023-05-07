#include <WString.h>

class MenuItem {
private:
  char(*titles[3]);
  char(*suffix);
  uint8_t(*value);

  bool (*validator)(uint8_t v);
  uint16_t (*displayer)();

public:
  MenuItem(char *str[3], char *suffix);
  MenuItem();

  char *getTitle(uint8_t idx) { return titles[idx]; }
  char *getSuffix() { return suffix; }
  uint8_t getValue() { return *value; }

  void incValue() { (*value)++; }
  void decValue() { (*value)--; }
  void setValue(uint8_t *v) { value = v; }
  void setValidator(bool (*fn)(uint8_t)) { validator = fn; }
  void setDisplayer(uint16_t (*fn)()) { displayer = fn; }

  bool validate(uint8_t v) { return validator(v); }
  // Возвращает значение для отображения на экране.
  uint16_t display() { return displayer(); }
};