/**
 * Test writing data to the 2M bit flash memory.
 * https://github.com/PaulStoffregen/SerialFlash
 * jhrg 2/5/22
 */

#include <Arduino.h>

#include <string.h>
#include <time.h>
#include <errno.h>

#include <SPI.h>

#include <SerialFlash.h>

#include "flash_utils.h"

#define STATUS_LED 13
#define LORA_CS 5

#define BAUD 115200
#define Serial SerialUSB // Needed for RS. jhrg 7/26/20

#ifndef JLINK
#define JLINK 0
#endif

#ifndef VERBOSE
#define VERBOSE 0
#endif

SerialFlashFile flashFile;
/**
 * @brief extract info from the filename.
 *
 * data-mm-yy.bin
 *
 * @param filename The filename
 * @param month Value-result param for the month
 * @param year Value-result param for the year - two-digits
 * @return true if the parse worked, false otherwise
 */
bool parse_filename(const char *filename, int &month, int &year)
{
    const int month_offset = 5;
    const int year_offset = 8;
    char num[3] = {0};
    char *end_ptr;

    memcpy(&num[0], (filename + month_offset), sizeof(num) - 1);
    month = strtoul(num, &end_ptr, 10);
    if (month == 0 || errno == ERANGE)
        return false;

    num[2] = '\0';
    memcpy(&num[0], (filename + year_offset), sizeof(num) - 1);
    year = strtoul(num, &end_ptr, 10);
    if (year == 0 || errno == ERANGE)
        return false;

    return true;
}

bool read_file_data(const char *filename, bool verbose = false)
{
    if (!SerialFlash.exists(filename)) {
        Serial.print("The file does not exist: ");
        Serial.println(filename);
        return false;
    }

    flashFile = SerialFlash.open(filename);
    if (!flashFile) {
        Serial.print("Could not open: ");
        Serial.println(filename);
        return false;
    }

    Serial.print("File open: ");
    Serial.println(filename);

    // read the header
    uint16_t header_year, header_month, header_n_records;
    bool header_status = read_header_from_file(flashFile, header_year, header_month, header_n_records);
    if (!header_status)
    {
        Serial.println("Could not read the data file header.");
        return false;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Header: year: %d, month %d, number of records: %d", header_year, header_month, header_n_records);
    Serial.println(msg);

    char record[11];
    const int samples_per_day = 24;
    const int dpm = days_per_month(header_month, header_year);
    if (dpm * samples_per_day != header_n_records) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Expected records (%d) and number if header (%d) do not match", 
                dpm * samples_per_day, header_n_records);
        Serial.println(msg);
    }
    // unused const int size_of_file = header_n_records * sizeof(record);

    // read the data.
    uint16_t message = 0;
    for (int i = 0; i < header_n_records; ++i)
    {
        bool rd_status = read_record_from_file(flashFile, record, sizeof(record));
        if (!rd_status)
        {
            Serial.print("Failed to read record number: ");
            Serial.println(message);
            continue;
        }

        // first two bytes are the message num. filler after that.
        memcpy(&message, record, sizeof(message));
        if (message != i + 1)
        {
            Serial.print("Invalid message number: ");
            Serial.print(message);
            Serial.print(", expected: ");
            Serial.println(i + 1);
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

    Serial.println("Start Flash Read Tester");

    setup_spi_flash(false, VERBOSE);

    Serial.println("Files on the SPI flash chip:");

    SerialFlash.opendir();
    while (1)
    {
        char filename[64];
        uint32_t filesize;

        if (SerialFlash.readdir(filename, sizeof(filename), filesize))
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "%20s: %ld bytes", filename, filesize);
            Serial.println(msg);

            read_file_data(filename);
#if 0
            int month, year;
            if (!parse_filename(filename, month, year)) {
                Serial.print("Could not parse filename: ");
                Serial.println(filename);
            }

            snprintf(msg, sizeof(msg), "month: %ld, year: %ld", month, year);
            Serial.println(msg);
#endif
        }
        else
        {
            Serial.println("No more files");
            break; // no more files
        }
    }

#if 0


#endif
}

void loop()
{
}