#ifndef FATFS_SD_H
#define FATFS_SD_H

#include "stm32f1xx_hal.h"
#include "diskio.h"

#define SD_SPI_HANDLE hspi1

// CS pin defined in gpio.c (PA4)
#define SD_CS_PORT GPIOA
#define SD_CS_PIN GPIO_PIN_4

extern SPI_HandleTypeDef SD_SPI_HANDLE;
extern uint8_t sd_disk_result;
extern uint8_t sd_last_cmd;
extern uint8_t sd_last_resp;
extern uint8_t sd_last_token;

// Function prototypes
DSTATUS SD_disk_initialize(BYTE pdrv);
void SD_disk_deinitialize(void);
DSTATUS SD_disk_status(BYTE pdrv);
DRESULT SD_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT SD_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT SD_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);

#endif
