
// Simple utilities for reading and writing data from/to the flash 
// memory chip.
//
// jhrg 3/1/22

/**
 * This file holds functions that can be used to make, write and read simple
 * data files for the HAST leaf node. The files each hold one month's data.
 * Each file has a 6-byte header that holds the year (2 digits), the month
 * and the number of records. Each record contains a time stamp and various
 * data values. The size of each record must be the same.
 */

#include <Arduino.h>

#include <string.h>
#include <time.h>

#include <SPI.h>

#include <SerialFlash.h>

#define STATUS_LED 13
#define Serial SerialUSB // Needed for RS. jhrg 7/26/20
#define FLASH_CS 4

#define TWO_SEC 2000
#define HALF_SEC 500
#define TENTH_SEC 100

#define FILE_BASE_NAME "data"
#define EXTENTION "bin"
#define NAME_LEN 32

/**
 * @brief Halt
 */
static void stop()
{
    while (true)
    {
        yield();
    }
}

/**
 * @brief Run basic diagnostics and get the flash chip size in bytes
 * @param verbose true to print info using Serial.print(), quiet if false
 */
uint32_t space_on_flash(bool verbose = false)
{
    uint8_t buf[16];
    char msg[256];

    Serial.println(F("Read Chip Identification:"));

    SerialFlash.readID(buf);
    snprintf(msg, 255, "  JEDEC ID:     %02X %02X %0X", buf[0], buf[1], buf[2]);
    Serial.println(msg);

    uint32_t chipsize = SerialFlash.capacity(buf);
    snprintf(msg, 255, "  Memory Size:  %ld", chipsize);
    Serial.println(msg);

    if (chipsize == 0)
        return 0;

    uint32_t blocksize = SerialFlash.blockSize();
    snprintf(msg, 255, "  Block Size:   %ld", blocksize);
    Serial.println(msg);

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

/**
 * @brief Configure the SerialFlash library.
 * @param erase True if the chip should be erased. If false, the chip is not erased.
 * @param verbose If True, print information. False by default.
 * @return The capacity of the flash chip.
 */
uint32_t setup_spi_flash(bool erase, bool verbose = false)
{
    bool status = SerialFlash.begin(SPI, FLASH_CS);
    if (!status)
    {
        Serial.print("Flash memory initialization error, error code: ");
        Serial.println();
        stop();
    }

    // Without this configuration of the SPI bus, the SerialFlash library sees
    // the Winbond flash chip as only 1MB when it is, in fact, 2MB. With this
    // hack, the chip has the correct size and also the JEDEC ID (EF 40 15).
    // This maybe due to a bug introduced into the Arduino-SAMD core in version
    // 1.8.11. See https://github.com/PaulStoffregen/SerialFlash/issues/79.
    // SPI_CLOCK_DIV2 to SPI_CLOCK_DIV128 (which set the SPI freq to the CPU
    // frequency divided by 6 to 255, see SPI.h) seem to work. This has to follow
    // SerailFlash::begin(). jhrg 2/16/22

    SPI.setClockDivider(SPI_CLOCK_DIV64);

    if (erase)
        erase_flash();

    if (verbose)
        Serial.println("Space on the flash chip: ");

    return space_on_flash(verbose);
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
    if (SerialFlash.exists(filename))
    {
        Serial.print("The file already exists: ");
        Serial.println(filename);
        return false;
    }

    bool status = SerialFlash.create(filename, size_of_file);
    if (!status)
    {
        Serial.print("Failed to make the file: ");
        Serial.println(filename);
        return false;
    }

    flashFile = SerialFlash.open(filename);

    return flashFile ? true : false;
}

static bool write_uint16(SerialFlashFile &flashFile, const uint16_t value)
{
    uint32_t len = flashFile.write(&value, sizeof(uint16_t));
    if (len != sizeof(uint16_t))
    {
        return false;
    }

    return true;
}

/**
 * @brief write a tiny header to the file.
 * @param flashFile The file
 * @param year The year info
 * @param month
 * @param num_records The number of records that should be in this file if
 * it's full of data. Unused bytes should be null.
 * @return true if the header was written, false if an error was detected.
 */
bool write_header_to_file(SerialFlashFile &flashFile, const uint16_t year, const uint16_t month, const uint16_t num_records)
{
    if (!flashFile)
    {
        return false;
    }

    return write_uint16(flashFile, year) && write_uint16(flashFile, month) && write_uint16(flashFile, num_records);
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

static bool read_uint16(SerialFlashFile &flashFile, uint16_t &value)
{
    uint32_t len = flashFile.read(&value, sizeof(uint16_t));
    if (len != sizeof(uint16_t))
    {
        return false;
    }

    return true;
}

/**
 * @brief Read the data file header
 * @return True if successful, false otherwise
 */
bool read_header_from_file(SerialFlashFile &flashFile, uint16_t &year, uint16_t &month, uint16_t &num_records)
{
    if (!flashFile)
    {
        return false;
    }

    return read_uint16(flashFile, year) && read_uint16(flashFile, month) && read_uint16(flashFile, num_records);
}

/**
 * @brief Read a record from the file.
 *
 * Reads the next record from the file. Starts at the begining after the file
 * is opened. The header must be read first.
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
