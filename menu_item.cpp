#include "menu_item.h"

MenuItem::MenuItem() {
}

MenuItem::MenuItem(const char *suffix, uint8_t valueMin, uint8_t valueMax,
                   uint8_t valueStep) {

  this->suffix = suffix;
  this->valueMin = valueMin;
  this->valueMax = valueMax;
  this->valueStep = valueStep;
}
