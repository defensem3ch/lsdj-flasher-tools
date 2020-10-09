/*
 GBxCart RW - Console Interface
 Version: 1.24
 Author: Alex from insideGadgets (www.insidegadgets.com)
 Created: 7/11/2016
 Last Modified: 8/08/2019

 GBxCart RW allows you to dump your Gameboy/Gameboy Colour/Gameboy Advance games
 ROM, save the RAM and write to the RAM.

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

  printf("GBxCart RW v1.24 by insideGadgets\n");
  printf("################################\n");

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

  // Get firmware version
  gbxcartFirmwareVersion = request_value(READ_FIRMWARE_VERSION);

  // Get PCB version
  gbxcartPcbVersion = request_value(READ_PCB_VERSION);

  // Prompt for GB or GBA mode
  if (gbxcartPcbVersion == PCB_1_3) {
    set_mode(VOLTAGE_5V);
  }

  read_gb_header();

  // Get cartridge mode - Gameboy or Gameboy Advance
  cartridgeMode = request_value(CART_MODE);

  // Dump ROM
  // else if (optionSelected == '1') {
  printf("\n--- Read ROM ---\n");

  char titleFilename[30];
  strncpy(titleFilename, gameTitle, 20);
  time_t rawtime;
  struct tm *timeinfo;
  char timebuffer[25];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(timebuffer, 80, "%Y%m%d%H%M%S", timeinfo);
  strncat(titleFilename, "-", 1);
  strncat(titleFilename, timebuffer, 25);
  if (cartridgeMode == GB_MODE) {
    strncat(titleFilename, ".gb", 3);
  } else {
    strncat(titleFilename, ".gba", 4);
  }
  printf("Reading ROM to %s\n", titleFilename);
  printf("[             25%%             50%%             75%%            "
         "100%%]\n[");

  // Create a new file
  FILE *romFile = fopen(titleFilename, "wb");

  uint32_t readBytes = 0;
  cartridgeMode == GB_MODE;
  if (cartridgeMode == GB_MODE) {
    // Set start and end address
    currAddr = 0x0000;
    endAddr = 0x7FFF;

    // Read ROM
    for (uint16_t bank = 1; bank < romBanks; bank++) {
      if (cartridgeType >= 5) { // MBC2 and above
        set_bank(0x2100, bank & 0xFF);
        if (bank >= 256) {
          set_bank(0x3000, 1); // High bit
        }
      } else { // MBC1
        if ((strncmp(gameTitle, "MOMOCOL", 7) == 0) ||
            (strncmp(gameTitle, "BOMCOL", 6) == 0)) { // MBC1 Hudson
          set_bank(0x4000, bank >> 4);
          if (bank < 10) {
            set_bank(0x2000, bank & 0x1F);
          } else {
            set_bank(0x2000, 0x10 | (bank & 0x1F));
          }
        } else {                       // Regular MBC1
          set_bank(0x6000, 0);         // Set ROM Mode
          set_bank(0x4000, bank >> 5); // Set bits 5 & 6 (01100000) of ROM bank
          set_bank(0x2000,
                   bank & 0x1F); // Set bits 0 & 4 (00011111) of ROM bank
        }
      }

      if (bank > 1) {
        currAddr = 0x4000;
      }

      // Set start address and rom reading mode
      set_number(currAddr, SET_START_ADDRESS);
      set_mode(READ_ROM_RAM);

      // Read data
      while (currAddr < endAddr) {
        uint8_t comReadBytes = com_read_bytes(romFile, 64);
        if (comReadBytes == 64) {
          currAddr += 64;
          readBytes += 64;

          // Request 64 bytes more
          if (currAddr < endAddr) {
            com_read_cont();
          }
        } else { // Didn't receive 64 bytes, usually this only happens for Apple
                 // MACs
          fflush(romFile);
          com_read_stop();
          delay_ms(500);
          printf("Retrying\n");

          // Flush buffer
          RS232_PollComport(cport_nr, readBuffer, 64);

          // Start off where we left off
          fseek(romFile, readBytes, SEEK_SET);
          set_number(currAddr, SET_START_ADDRESS);
          set_mode(READ_ROM_RAM);
        }

        // Print progress
        print_progress_percent(readBytes, (romBanks * 16384) / 64);
      }
      com_read_stop(); // Stop reading ROM (as we will bank switch)
    }
    printf("]");
  } else { // GBA mode
    // Set start and end address
    currAddr = 0x00000;
    endAddr = romEndAddr;
    set_number(currAddr, SET_START_ADDRESS);

    uint16_t readLength = 64;
#if defined(__APPLE__)
    set_mode(GBA_READ_ROM);
#else
    if (gbxcartPcbVersion != PCB_1_0) {
      set_mode(GBA_READ_ROM_256BYTE);
      readLength = 256;
    } else {
      set_mode(GBA_READ_ROM);
    }
#endif

    // Read data
    while (currAddr < endAddr) {
#if defined(__APPLE__) // Apple only seems to like reading 64 bytes and has
                       // occasional time outs
      uint8_t comReadBytes = com_read_bytes(romFile, readLength);
      if (comReadBytes == readLength) {
        currAddr += readLength;

        // Request 64 bytes more
        if (currAddr < endAddr) {
          com_read_cont();
        }
      } else { // Didn't receive 64 bytes, usually this only happens for Apple
               // MACs
        fflush(romFile);
        com_read_stop();
        delay_ms(500);

        // Flush buffer
        RS232_PollComport(cport_nr, readBuffer, readLength);

        // Start off where we left off
        fseek(romFile, currAddr, SEEK_SET);
        set_number(currAddr / 2, SET_START_ADDRESS);
        set_mode(GBA_READ_ROM);
      }
#else // For Windows / Linux
      int comReadBytes = com_read_bytes(romFile, readLength);
      if (comReadBytes == readLength) {
        currAddr += readLength;

        // Request 64 bytes more
        if (currAddr < endAddr) {
          com_read_cont();
        }
      } else { // Didn't receive 256 bytes
        fflush(romFile);
        com_read_stop();
        delay_ms(500);
        printf("Retrying\n");

        // Flush buffer
        RS232_PollComport(cport_nr, readBuffer, readLength);

        // Start off where we left off
        fseek(romFile, currAddr, SEEK_SET);
        set_number(currAddr / 2, SET_START_ADDRESS);
        if (gbxcartPcbVersion != PCB_1_0) {
          set_mode(GBA_READ_ROM_256BYTE);
        } else {
          set_mode(GBA_READ_ROM);
        }
      }
#endif

      // Print progress
      print_progress_percent(currAddr, endAddr / 64);
    }
    printf("]");
    com_read_stop();
  }

  fclose(romFile);
  printf("\nFinished\n");
  //}
  return 0;
}
