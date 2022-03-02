
// Simple utilities for reading and writing data from/to the flash 
// memory chip.
//
// jhrg 3/1/22

#include <Arduino.h>

#include <string.h>
#include <time.h>

#include <SPI.h>

#include <SerialFlash.h>

#define STATUS_LED 13

#define TWO_SEC 2000
#define HALF_SEC 500
#define TENTH_SEC 100

#define FILE_BASE_NAME "data"
#define EXTENTION "bin"
#define NAME_LEN 32

#ifndef JLINK
#define JLINK 0
#define Serial_println(x) SerialUSB.println(x)
#define Serial_print(x) SerialUSB.print(x)
#else
#define Serial_println(x)
#define Serial_print(x)
#endif

/**
 * @brief Run basic diagnostics and get the flash chip size in bytes
 * @param verbose true to print info using Serial_print(), quiet if false
 */
uint32_t space_on_flash(bool verbose = false)
{
    uint8_t buf[16];
    char msg[256];

    Serial_println(F("Read Chip Identification:"));

    SerialFlash.readID(buf);
    snprintf(msg, 255, "  JEDEC ID:     %02X %02X %0X", buf[0], buf[1], buf[2]);
    Serial_println(msg);

    uint32_t chipsize = SerialFlash.capacity(buf);
    snprintf(msg, 255, "  Memory Size:  %ld", chipsize);
    Serial_println(msg);

    if (chipsize == 0)
        return 0;

    uint32_t blocksize = SerialFlash.blockSize();
    snprintf(msg, 255, "  Block Size:   %ld", blocksize);
    Serial_println(msg);

    return chipsize;
}

/**
 * @brief Smart erase - waits for erase to complete
 * While the flash chip is erasing, call yield and flash the status LED.
 * When the erase operation is complete, flash five times quickly.
 */
void erase_flash()
{
    // We start by formatting the flash...
    uint8_t id[5];
    SerialFlash.readID(id);
    SerialFlash.eraseAll();

    bool status_value = digitalRead(STATUS_LED); // record state

    uint32_t t = millis();
    while (SerialFlash.ready() == false)
    {
        yield();
        if (0 == (millis() - t) % HALF_SEC)
        {
            digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
        }
    }

#if !JLINK
    // Quickly flash LED a few times when completed, then leave the light on solid
    t = millis();
    uint32_t end_time = t + TWO_SEC;
    while (millis() < end_time)
    {
        yield();
        if (0 == (millis() - t) % TENTH_SEC)
        {
            digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
        }
    }
#endif

    digitalWrite(STATUS_LED, status_value); // exit with entry state
}

// Use ones-indexing for the number of days
// TODO Leap years... jhrg 2/21/22
static bool is_leap(uint16_t year)
{
    return ((year % 4) == 0 && (year % 100) != 0) || (year % 400) == 0;
}

uint8_t days_per_month(uint8_t month, uint16_t year)
{
    uint8_t dpm[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap(year + 2000))
    {
        return 29;
    }
    else
    {
        return dpm[month];
    }
}

/**
 * @brief Make the name for a new flash data file
 * @param month The month number
 * @param yy The last two digits of the year
 * @return A pointer to the filename. Uses local static storage.
 */
char *make_data_file_name(const int month, const int yy)
{
    static char filename[NAME_LEN];
    int size = snprintf(filename, sizeof(filename), "%s-%02d-%02d.%s", FILE_BASE_NAME, month, yy, EXTENTION);
    if (size > NAME_LEN - 1)
        return nullptr;
    else
        return filename;
}

/**
 * @brief Make a new file to hold one month's worth of data.
 *
 * This function makes the file name and creates and opens the
 * file. The open file is referenced by the 'flashFile' parameter.
 * The function returns true to indicate success, false otherwise.
 *
 * @param flashFile Value-result parameter for the open file
 * @param filename The name of the new file
 * @param size_of_file The size in bytes of the new file
 * @return True if the file is created, otherwise false.
 */
bool make_new_data_file(SerialFlashFile &flashFile, const char *filename, const int size_of_file)
{
    bool status = SerialFlash.create(filename, size_of_file);
    if (!status)
    {
        Serial_println("Failed to initialize Serial Flash");
        return false;
    }

    flashFile = SerialFlash.open(filename);

    return flashFile ? true : false;
}

/**
 * @brief Write a record to the flash file.
 *
 * @param flashFile The open file.
 * @param recrod A pointer to the record.
 * @param record_size The number of bytes to write.
 * @return True if all the operations worked, false otherwise.
 * @todo Add a global status register for the flash file.
 */
bool write_record_to_file(SerialFlashFile &flashFile, const char *record, const uint32_t record_size)
{
    if (!flashFile)
    {
        return false;
    }

    uint32_t len = flashFile.write(record, record_size);
    if (len != record_size)
    {
        return false;
    }

    return true;
}

/**
 * @brief Read a record from the file.
 *
 * @param flashFile The open file.
 * @param record Value-result param that holds the data just read.
 * @param record_size
 * @return True if the read was successful, false otherwise
 */
bool read_record_from_file(SerialFlashFile &flashFile, char *record, const uint32_t record_size)
{
    if (!flashFile)
    {
        return false;
    }

    uint32_t len = flashFile.read(record, record_size);
    if (len != record_size)
    {
        return false;
    }

    return true;
}
