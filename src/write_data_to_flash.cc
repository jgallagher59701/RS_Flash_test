/**
 * Test writing data to the 2M bit flash memory.
 * https://github.com/PaulStoffregen/SerialFlash
 * jhrg 2/5/22
 */

#include <Arduino.h>

#include <string.h>
#include <time.h>

#include <SPI.h>

#include <SerialFlash.h>

#include "flash_utils.h"

#define STATUS_LED 13
#define LORA_CS 5
#define FLASH_CS 4
#define BAUD 115200
#define Serial SerialUSB // Needed for RS. jhrg 7/26/20

#ifndef ERASE_FLASH
#define ERASE_FLASH 0
#endif

#ifndef JLINK
#define JLINK 0
#define Serial_println(x) SerialUSB.println(x)
#define Serial_print(x) SerialUSB.print(x)
#else
#define Serial_println(x)
#define Serial_print(x)
#endif

SerialFlashFile flashFile;

/**
 * @brief build up phony data to test flash behavior.
 */
bool read_and_write_test_data(int month, int year, bool verbose = false)
{
    char *file_name = make_data_file_name(month, year);
    Serial_print("The file name is: ");
    Serial_println(file_name);

    char record[11];
    const int samples_per_day = 24;
    const int dpm = days_per_month(month, year);
    const int size_of_file = dpm * samples_per_day * sizeof(record);
    bool new_status = make_new_data_file(flashFile, file_name, size_of_file);
    if (!new_status)
    {
        Serial_println("Could not make the new data file.");
        return false;
    }

    // make some phony data...
    uint16_t message = 0;
    for (int i = 0; i < dpm; ++i)
    {
        for (int j = 0; j < samples_per_day; ++j)
        {
            ++message;

            // first two bytes are the message num. filler after that.
            memcpy(record, &message, sizeof(message));
            // file the rest with 0xAA
            for (unsigned int k = sizeof(message); k < sizeof(record); ++k)
                record[k] = 0xAA;

            bool wr_status = write_record_to_file(flashFile, record, sizeof(record));
            if (!wr_status)
            {
                Serial_print("Failed to write record number: ");
                Serial_println(message);
                return false;
            }
        }
    }

    flashFile.close(); // this doesn't actually do anything

    // open the file and ...
    flashFile = SerialFlash.open(file_name);

    // read the data.
    message = 0;
    for (int i = 0; i < dpm; ++i)
    {
        for (int j = 0; j < samples_per_day; ++j)
        {
            bool rd_status = read_record_from_file(flashFile, record, sizeof(record));
            if (!rd_status)
            {
                Serial_print("Failed to read record number: ");
                Serial_println(message);
                return false;
            }

            // first two bytes are the message num. filler after that.
            memcpy(&message, record, sizeof(message));
            if (message != (j + 1) + (i * samples_per_day))
            {
                Serial_print("Invalid message number: ");
                Serial_print(message);
                Serial_print(", expected: ");
                Serial_println((j + 1) + (i * samples_per_day));
            }

            char filler[9] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
            if (memcmp(&record[2], filler, sizeof(filler)) != 0)
                Serial_println("Invalid record filler");

            if (verbose)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "record number: %d, data: %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                         message, record[2], record[3], record[4], record[5], record[6], record[7], record[8], record[9], record[10]);
                SerialUSB.println(msg);
            }
        }
    }

    return true;
}

/**
 * @brief Halt
 */
void stop()
{
    while (true)
    {
        yield();
    }
}

void setup()
{
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);

#if !JLINK
    Serial.begin(BAUD);

    // Wait for serial port to be available
    while (!Serial)
        ;
#endif

    Serial_println("Start Flash Write Tester");

    bool status = SerialFlash.begin(SPI, FLASH_CS);
    if (!status)
    {
        Serial_print("Flash memory initialization error, error code: ");
        Serial_println();
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

#if ERASE_FLASH
    // Erase the chip
    erase_flash();
#endif

    Serial_println("Space on the flash chip: ");

    space_on_flash(true);

    for (int year = 22; year < 24; year++) {
        for (int month = 1; month < 13; month++) {
            if (!read_and_write_test_data(month, year, false /*verbose*/))
                break;
        }

        for (int month = 1; month < 13; month++) {
            char *file_name = make_data_file_name(month, year);
            flashFile = SerialFlash.open(file_name);
            if (!flashFile) {
                Serial_print("Could not open: ");
                Serial_println(file_name);
                break;
            }
            char msg[256];
            snprintf(msg, sizeof(msg), "File %s starts at 0x%08lx", file_name, flashFile.getFlashAddress());
            Serial_println(msg);
        }
    }
}

void loop()
{
}