
#ifndef flash_utils_h
#define flash_utils_h

#include <Arduino.h>

#include <SerialFlash.h>

uint32_t space_on_flash(bool verbose = false);
void erase_flash();
char *make_data_file_name(const int month, const int yy);
bool make_new_data_file(SerialFlashFile &flashFile, const char *filename, const int size_of_file);
bool write_record_to_file(SerialFlashFile &flashFile, const char *record, const uint32_t record_size);
bool read_record_from_file(SerialFlashFile &flashFile, char *record, const uint32_t record_size);

uint8_t days_per_month(uint8_t month, uint16_t year);

#endif