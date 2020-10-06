/*
 GBxCart RW - Console Interface Flasher
 Version: 1.22
 Author: Alex from insideGadgets (www.insidegadgets.com)
 Created: 26/08/2017
 Last Modified: 9/08/2019
 License: GPL

 This program allows you to write ROMs to Flash Carts that are supported.

 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "setup.h" // See defines, variables, constants, functions here

int main(int argc, char **argv) {

  printf("ROM / SAV flasher by DEFENSE MECHANISM\n");
  printf("based on GBxCart RW Flasher by insideGadgets\n");
  printf("#########################################\n");

  // Check arguments
  if (argc >= 2) {
    read_config();

    // Open COM port
    if (com_test_port() == 0) {
      printf("Device not connected and couldn't be auto detected\n");
      read_one_letter();
      return 1;
    }
    printf("Connected on COM port: %i\n", cport_nr + 1);

    // Break out of any existing functions on ATmega
    set_mode('0');

    // Get cartridge mode - Gameboy or Gameboy Advance
    cartridgeMode = request_value(CART_MODE);

    // Read header
    read_gb_header();

    // Get firmware version
    gbxcartFirmwareVersion = request_value(READ_FIRMWARE_VERSION);

    // Get PCB version
    gbxcartPcbVersion = request_value(READ_PCB_VERSION);

    if (gbxcartPcbVersion == PCB_1_0) {
      printf("\nPCB v1.0 is not supported for this function.");
      read_one_letter();
      return 1;
    }

    if (gbxcartFirmwareVersion <= 8) {
      printf("Firmware R9 or higher is required for this functionality.\n");
      read_one_letter();
      return 1;
    }

    // Separate the file name
    char filenameOnly[100];
    char filename[254];
    strncpy(filename, argv[1], 253);
    char filetype[4];

#ifdef _WIN32
    char *token = strtok(filename, "\\");
    while (token != NULL) {
      strncpy(filenameOnly, token, 99);
      token = strtok(NULL, "\\");
    }
#else
    char *token = strtok(filename, "/");
    while (token != NULL) {
      strncpy(filenameOnly, token, 99);
      token = strtok(NULL, "/");
    }
#endif
    // check if filename ends with '.gb'
    token = strtok(filenameOnly, ".");
    while (token != NULL) {
      strncpy(filetype, token, 4);
      token = strtok(NULL, ".");
    }
    if (strncmp(filetype, "gb", 2) == 0) {

      printf("\n--- Write ROM to Flash Cart ---\n");
      //		printf("Cart selected: ");

      uint32_t readBytes = 0;
      uint16_t sector = 0;
      long fileSize = 0;

      // Grab file size
      FILE *romFile = fopen(argv[1], "rb");
      if (romFile != NULL) {
        fseek(romFile, 0, SEEK_END);
        fileSize = ftell(romFile);
        fseek(romFile, 0, SEEK_SET);
      } else {
        printf("\n%s \nFile not found\n", argv[1]);
        read_one_letter();
        return 1;
      }

      //		printf("insideGadgets 4 MByte 128KB SRAM Gameboy Flash
      // Cart\n");
      printf("\nGoing to write to ROM (Flash cart) from %s\n", filenameOnly);

      // PCB v1.3 - Set 5V
      if (gbxcartPcbVersion == PCB_1_3) {
        set_mode(VOLTAGE_5V);
        delay_ms(500);
      }
      currAddr = 0x0000;
      endAddr = 0x7FFF;

      // Check file size
      if (fileSize > 0x400000) {
        fclose(romFile);
        printf("\n%s \nFile size is larger than the available Flash cart space "
               "of 4 MByte\n",
               argv[1]);
        read_one_letter();
        return 1;
      }

      // Calculate banks needed from ROM file size
      romBanks = fileSize / 16384;

      // Flash Setup
      set_mode(GB_CART_MODE); // Gameboy mode
      delay_ms(5);
      gb_flash_pin_setup(WE_AS_WR_PIN);             // WR pin
      gb_flash_program_setup(GB_FLASH_PROGRAM_AAA); // Flash program byte method
      gb_check_change_flash_id(GB_FLASH_PROGRAM_AAA);

      uint16_t sectorEraseAddress = 0x2000;

      // Check if Flash ID matches
      if (flashID[0] == 0x01 && flashID[1] == 0x01 && flashID[2] == 0x7E &&
          flashID[3] == 0x7E) {
      } else if (flashID[0] == 0xC2) {
      } else {
        printf("*** Flash ID doesn't match expected value of 0x01, 0x01, 0x7E, "
               "0x7E or 0xC2 ***\n");
        read_one_letter();
      }

      set_number(0, SET_START_ADDRESS);
      delay_ms(5);

      printf("\n\nWriting to ROM (Flash cart) from %s\n", filenameOnly);
      printf("[             25%%             50%%             75%%            "
             "100%%]\n[");

      // Write ROM
      currAddr = 0x0000;
      for (uint16_t bank = 1; bank < romBanks; bank++) {
        if (bank > 1) {
          currAddr = 0x4000;
        }

        // Set start address
        set_number(currAddr, SET_START_ADDRESS);
        delay_ms(5);

        // Read data
        while (currAddr < endAddr) {
          if (currAddr == 0x4000) { // Switch banks here just before the next
                                    // bank, not any time sooner
            set_bank(0x2100, bank);
            delay_ms(5);
          }
          if (readBytes % sectorEraseAddress == 0) { // Erase sectors
            gb_flash_write_address_byte(0xAAA, 0xAA);
            gb_flash_write_address_byte(0x555, 0x55);
            gb_flash_write_address_byte(0xAAA, 0x80);
            gb_flash_write_address_byte(0xAAA, 0xAA);
            gb_flash_write_address_byte(0x555, 0x55);
            if (readBytes < 0x8000) {
              gb_flash_write_address_byte(sector << 13, 0x30);
            } else if (readBytes == 0x8000) {
              gb_flash_write_address_byte(0x4000, 0x30); // Sector 5
            } else if (readBytes == 0xA000) {
              gb_flash_write_address_byte(0x6000, 0x30); // Sector 5
            } else if (readBytes == 0xC000) {
              gb_flash_write_address_byte(0x4000, 0x30); // Sector 5
            } else if (readBytes == 0xE000) {
              gb_flash_write_address_byte(0x6000, 0x30); // Sector 5
            } else if (readBytes >= 0xFFFF) {
              gb_flash_write_address_byte(0x4000, 0x30); // Sector 5
            }

            //					printf("Erasing.... %X, %i, bank
            //%i\n\n", readBytes, sector, bank);

            wait_for_flash_sector_ff(currAddr);
            sector++;

            set_number(currAddr, SET_START_ADDRESS);
            delay_ms(5);

            if (readBytes >= 0xFFFF) {
              sectorEraseAddress = 0x8000;
            }
          }

          //				printf("read 0x%X, bank %i\n",
          // readBytes, bank);

          com_write_bytes_from_file(GB_FLASH_WRITE_64BYTE, romFile, 64);
          com_wait_for_ack();
          currAddr += 64;
          readBytes += 64;

          // Print progress
          print_progress_percent(readBytes, (romBanks * 16384) / 64);
        }
      }

      printf("]");
      fclose(romFile);
    } // else, check if filename ends with '.sav'
    else if (strncmp(filetype, "sav", 3) == 0) {
    //  ramEndAddress = 0x10000;
    //  ramBanks = 2;
      FILE *ramFile = fopen(argv[1], "rb");
      if (ramFile != NULL) {
        if (ramEndAddress > 0) {
          printf("Going to flash save from %s...", argv[1]);
          printf("\nFlashing save from %s\n", argv[1]);
          printf("[             25%%             50%%             75%%         "
                 "   100%%]\n[");

          mbc2_fix();
          if (cartridgeType <= 4) { // MBC1
            set_bank(0x6000, 1);    // Set RAM Mode
          }
          set_bank(0x0000, 0x0A); // Initialise MBC

          // Write RAM
          uint32_t readBytes = 0;
          for (uint8_t bank = 0; bank < ramBanks; bank++) {
            uint16_t ramAddress = 0xA000;
            set_bank(0x4000, bank);
            set_number(0xA000, SET_START_ADDRESS); // Set start address again

            while (ramAddress < ramEndAddress) {
              com_write_bytes_from_file(WRITE_RAM, ramFile, 64);
              ramAddress += 64;
              readBytes += 64;

              // Print progress
              if (ramEndAddress == 0xA1FF) {
                print_progress_percent(readBytes, 64);
              } else if (ramEndAddress == 0xA7FF) {
                print_progress_percent(readBytes / 4, 64);
              } else {
                print_progress_percent(
                    readBytes, (ramBanks * (ramEndAddress - 0xA000 + 1)) / 64);
              }

              com_wait_for_ack();
            }
          }
          printf("]");
          set_bank(0x0000, 0x00); // Disable RAM

          fclose(ramFile);
          printf("\nFinished\n");
        } else {
          printf("No RAM\n");
        }
      } else {
        printf("%s not found\n", argv[1]);
      }
    } // else, could not determine filetype
    else {
      printf("Sorry, could not determine whether this is a ROM or SAV file.\n");
      printf("Please try again with a file ending in .gb(c) or .sav\n");
    }
  }
  printf("\n");
  return 0;
}
