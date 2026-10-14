#include "tensorflow/lite/micro/system_setup.h"
#include <neorv32.h>

namespace tflite {
void InitializeTarget() {
  neorv32_uart0_setup(19200, 0);
}
}
