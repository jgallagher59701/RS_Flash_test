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

#define BAUD 115200
#define Serial SerialUSB // Needed for RS. jhrg 7/26/20

#ifndef ERASE_FLASH
#define ERASE_FLASH 0
#endif

#ifndef JLINK
#define JLINK 0
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

SerialFlashFile flashFile;

/**
 * @brief build up phony data to test flash behavior.
 */
bool read_and_write_test_data(int month, int year, bool verbose = false)
{
    char *file_name = make_data_file_name(month, year);
    Serial.print("The file name is: ");
    Serial.println(file_name);

    const int header_size = 3 * sizeof(uint16_t);
    char record[11];
    const int samples_per_day = 24;
    const int dpm = days_per_month(month, year);
    const int size_of_file = (dpm * samples_per_day * sizeof(record)) + header_size;
    bool new_status = make_new_data_file(flashFile, file_name, size_of_file);
    if (!new_status)
    {
        Serial.println("Could not make the new data file.");
        return false;
    }

    bool header_status = write_header_to_file(flashFile, year, month, dpm * samples_per_day);
    if (!header_status)
    {
        Serial.println("Could not write the data file header.");
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
                Serial.print("Failed to write record number: ");
                Serial.println(message);
                return false;
            }
        }
    }

    flashFile.close(); // this doesn't actually do anything

    // open the file and ...
    flashFile = SerialFlash.open(file_name);

    // read the header
    uint16_t header_year, header_month, header_n_records;
    header_status = read_header_from_file(flashFile, header_year, header_month, header_n_records);
    if (!header_status) {
        Serial.println("Could not read the data file header.");
        return false;
    }
    char msg[256];
    snprintf(msg, sizeof(msg), "year: %d, Month %d, number of records: %d", header_year, header_month, header_n_records);
    Serial.println(msg);

    // read the data.
    message = 0;
    for (int i = 0; i < dpm; ++i)
    {
        for (int j = 0; j < samples_per_day; ++j)
        {
            bool rd_status = read_record_from_file(flashFile, record, sizeof(record));
            if (!rd_status)
            {
                Serial.print("Failed to read record number: ");
                Serial.println(message);
                return false;
            }

            // first two bytes are the message num. filler after that.
            memcpy(&message, record, sizeof(message));
            if (message != (j + 1) + (i * samples_per_day))
            {
                Serial.print("Invalid message number: ");
                Serial.print(message);
                Serial.print(", expected: ");
                Serial.println((j + 1) + (i * samples_per_day));
            }

            char filler[9] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
            if (memcmp(&record[2], filler, sizeof(filler)) != 0)
                Serial.println("Invalid record filler");

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

void setup()
{
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);

    Serial.begin(BAUD);

#if JLINK == 0
    // Wait for serial port to be available
    while (!SerialUSB)
        ;
#endif

    Serial.println("Start Flash Write Tester");

    setup_spi_flash(ERASE_FLASH, VERBOSE);
    
    for (int year = 22; year < 24; year++)
    {
        for (int month = 1; month < 13; month++) {
            if (!read_and_write_test_data(month, year, false /*verbose*/))
                continue;
        }

        for (int month = 1; month < 13; month++) {
            char *file_name = make_data_file_name(month, year);
            flashFile = SerialFlash.open(file_name);
            if (!flashFile) {
                Serial.print("Could not open: ");
                Serial.println(file_name);
                continue;
            }
            char msg[256];
            snprintf(msg, sizeof(msg), "File %s starts at 0x%08lx", file_name, flashFile.getFlashAddress());
            Serial.println(msg);
        }
    }
}

void loop()
{
}