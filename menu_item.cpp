#include "menu_item.h"

MenuItem::MenuItem() {
}

MenuItem::MenuItem(const char *str[3], const char *suffix) {
  MenuItem(str, suffix, 0, 255, 1);
}

MenuItem::MenuItem(const char *str[3], const char *suffix, uint8_t valueMin,
                   uint8_t valueMax, uint8_t valueStep) {
  for (size_t i = 0; i < 3; i++) {
    titles[i] = str[i];
  }

  this->suffix = suffix;
  this->valueMin = valueMin;
  this->valueMax = valueMax;
  this->valueStep = valueStep;
}
