#include "Settings.h"

uint32_t BAUDRATE_LSIT[] = { 9600, 19200, 38400, 57600, 74880, 115200, 230400, 460800, 921600, 1000000 };
uint8_t BAUDRATE_LSIT_SIZE = sizeof(BAUDRATE_LSIT) / sizeof(uint32_t);
uint8_t baudrate_index = 5;
uint8_t data_pack_enbale;
