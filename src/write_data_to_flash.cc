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

#define STATUS_LED 13
#define LORA_CS 5
#define FLASH_CS 4
#define BAUD 115200
#define Serial SerialUSB // Needed for RS. jhrg 7/26/20
#define HALF_SEC 500
#define TENTH_SEC 100

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

// Erase the flash chip, flashing wile that happens.
// Once the erase is complete, flash 5 times quickly.
void erase_flash()
{
    // We start by formatting the flash...
    uint8_t id[5];
    SerialFlash.readID(id);
    SerialFlash.eraseAll();

    // Flash LED at 1Hz while formatting
    while (!SerialFlash.ready())
    {
        delay(HALF_SEC);
        digitalWrite(STATUS_LED, HIGH);
        delay(HALF_SEC);
        digitalWrite(STATUS_LED, LOW);
    }

    // Quickly flash LED a few times when completed, then leave the light on solid
    for (uint8_t i = 0; i < 5; i++)
    {
        delay(TENTH_SEC);
        digitalWrite(STATUS_LED, HIGH);
        delay(TENTH_SEC);
        digitalWrite(STATUS_LED, LOW);
    }
    digitalWrite(STATUS_LED, HIGH);
}

// make file object: SerialFlashFile flashFile;
// create file with size: SerialFlash.create(filename, fileSize)
// open file: flashFile = SerialFlash.open(filename);
// write data: flashFile.write(flashBuffer, flashBufferIndex);
// close file: flashFile.close();

SerialFlashFile flashFile;

#define FILE_BASE_NAME "data"
#define EXTENTION "bin"
#define NAME_LEN 32

// Use ones-indexing for the number of days
// TODO Leap years... jhrg 2/21/22
uint8_t days_per_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief Make the name for a new flash data file
 * @param month The month number
 * @param yy The last two digits of the year
 * @return A pointer to the filename. Uses local static storage.
 */
char *
make_data_file_name(const int month, const int yy)
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
 * file. The open file is referenced by the global 'flashFile.'
 * The function returns true to indicate success, false otherwise.
 *
 * @param month The month number
 * @param yy The last two digits of the year
 * @param record_size The size of each record
 * @param record_number The number of records this file will hold.
 * @return True if the file is created, otherwise false.
 */
bool make_new_data_file(const char *filename, const int size_of_file)
{
    bool status = SerialFlash.create(filename, size_of_file);
    if (!status)
    {
        Serial.println("Failed to initialize Serial Flash");
        return false;
    }

    flashFile = SerialFlash.open(filename);

    return flashFile ? true : false;
}

/**
 * @brief Write a record to the flash file.
 * @param recrod A pointer to the record.
 * @param record_size The number of bytes to write.
 * @return True if all the operations worked, false otherwise.
 * @todo Add a global status register for the flash file.
 */
bool write_record_to_file(const char *record, const uint32_t record_size)
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
 * @param record Value-result param that holds the data just read.
 * @param record_size
 * @return True if the read was successful, false otherwise
 */
bool read_record_from_file(char *record, const uint32_t record_size)
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

/**
 * @brief Smart erase - waits for erase to complete
 */
void flash_erase()
{
    SerialFlash.eraseAll();
    while (SerialFlash.ready() == false)
    {
        yield();
    }
}

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

    Serial.begin(BAUD);
    // Wait for serial port to be available
    // while (!Serial)
    //    ;

    Serial.println("Start Flash Write Tester");

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

    // Erase the chip
    flash_erase();

    Serial.println("Space on the flash chip: ");

    space_on_flash(true);

    int month = 2;
    int year = 22;

    char *file_name = make_data_file_name(month, year);
    Serial.print("The file name is: ");
    Serial.println(file_name);

    char record[11];
    const int samples_per_day = 24;
    const int size_of_file = days_per_month[month] * samples_per_day * sizeof(record);
    bool new_status = make_new_data_file(file_name, size_of_file);
    if (!new_status)
    {
        Serial.println("Could not make the new data file.");
    }

    // make some phony data...
    uint16_t message = 0;
    for (int i = 0; i < days_per_month[month]; ++i)
    {
        for (int j = 0; j < samples_per_day; ++j)
        {
            ++message;

            // first two bytes are the message num. filler after that.
            memcpy(record, &message, sizeof(message));
            // file the rest with 0xAA
            for (unsigned int k = 0; k < sizeof(record) - sizeof(message); ++k)
                record[k] = 0xAA;

            bool wr_status = write_record_to_file(record, sizeof(record));
            if (!wr_status)
            {
                Serial.print("Failed to write record number: ");
                Serial.println(message);
            }
        }
    }

    flashFile.close(); // this doesn't actually do anything

    // open the file and ...
    flashFile = SerialFlash.open(file_name);

    // read the data.
    message = 0;
    for (int i = 0; i < days_per_month[month]; ++i)
    {
        for (int j = 0; j < samples_per_day; ++j)
        {
            bool rd_status = read_record_from_file(record, sizeof(record));
            if (!rd_status)
            {
                Serial.print("Failed to read record number: ");
                Serial.println(message);
            }

            // first two bytes are the message num. filler after that.
            memcpy(&message, record, sizeof(message));
            Serial.print("Read record number: ");
            Serial.println(message);
        }
    }
}

void loop()
{
}