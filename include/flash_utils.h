
#ifndef flash_utils_h
#define flash_utils_h

#include <Arduino.h>

#include <SerialFlash.h>

#define FLASH_FILE_HEADER_SIZE 5 * sizeof(uint16_t) // bytes
#define RECORD_TYPE_01 01

uint32_t space_on_flash(bool verbose = false);
void erase_flash();
uint32_t setup_spi_flash(bool erase, bool verbose = false);

uint8_t days_per_month(uint8_t month, uint16_t year);
char *make_data_file_name(const int month, const int yy);

bool make_new_data_file(SerialFlashFile &flashFile, const char *filename, const int size_of_file);
bool make_new_data_file(SerialFlashFile &flashFile, const char *filename, const int num_records, const int record_size);

bool write_header_to_file(SerialFlashFile &flashFile, const uint16_t year, const uint16_t month,
                          const uint16_t num_records, const uint16_t record_size, const uint16_t record_type);
bool write_record_to_file(SerialFlashFile &flashFile, const char *record, const uint32_t record_size);

bool read_header_from_file(SerialFlashFile &flashFile, uint16_t &year, uint16_t &month, uint16_t &num_records,
                           uint16_t &record_size, uint16_t &record_type);
bool read_record_from_file(SerialFlashFile &flashFile, char *record, const uint32_t record_size);

#endif