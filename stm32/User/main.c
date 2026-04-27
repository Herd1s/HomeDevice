#include "home_device_app.h"

int main(void) {
  HomeDevice_AppInit();
  while (1) {
    HomeDevice_AppLoop();
  }
}
