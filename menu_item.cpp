#include "menu_item.h"

MenuItem::MenuItem() {}

MenuItem::MenuItem(char *str[3], char *_suffix) {
  for (size_t i = 0; i < 3; i++) {
    titles[i] = str[i];
  }

  suffix = _suffix;
}
