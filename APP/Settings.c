#include "Settings.h"

const uint32_t BAUDRATE_LSIT[] = { 9600U, 19200U, 38400U, 57600U, 74880U,
                                   115200U, 230400U, 460800U, 921600U, 1000000U };
const uint8_t BAUDRATE_LSIT_SIZE = (uint8_t)(sizeof(BAUDRATE_LSIT) / sizeof(BAUDRATE_LSIT[0]));
uint8_t baudrate_index = 5;
