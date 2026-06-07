#ifndef INC_SD_LOGGER_H_
#define INC_SD_LOGGER_H_

#include "main.h"
#include "pms7003.h"
#include "bme280.h"
#include "scd41.h"
#include "ds1307.h"
#include "fatfs.h"

#define SD_LOGGER_CSV_BUFFER_SIZE 256U /* CSV line and last-attempt string buffer size. */

typedef enum
{
    SD_STATUS_UNKNOWN = 0, /* SD logger not initialized yet. */
    SD_STATUS_OK = 1,      /* SD card mounted and ready. */
    SD_STATUS_RETRY = 2    /* SD error occurred, retry/reconnect scheduled. */
} SD_Status_t;

typedef enum
{
    SD_ERROR_STAGE_NONE = 0,      /* No SD logger error. */
    SD_ERROR_STAGE_MOUNT = 1,     /* FatFs mount or low-level init failed. */
    SD_ERROR_STAGE_OPEN = 2,      /* CSV file open failed. */
    SD_ERROR_STAGE_SEEK = 3,      /* CSV append seek failed. */
    SD_ERROR_STAGE_WRITE = 4,     /* CSV file write failed. */
    SD_ERROR_STAGE_CSV_BUILD = 5, /* CSV line build failed or buffer was too small. */
    SD_ERROR_STAGE_CLOSE = 6      /* CSV file close failed. */
} SD_ErrorStage_t;

/* Public status values for OLED/debug display.
 * These variables are owned by sd_logger.c.
 * Other modules should read them only.
 */
extern SD_Status_t sd_status;
extern FRESULT last_fres;
extern uint8_t sd_retry_seconds;
extern uint8_t sd_reconnecting;
extern SD_ErrorStage_t sd_error_stage;
extern char last_log_attempt[SD_LOGGER_CSV_BUFFER_SIZE];

/**
 * SD_Logger_Init - Initialize SD logger.
 *
 * Try SD low-level initialization and FatFs mount.
 * If mount fails, enter retry state instead of stopping the system.
 */
void SD_Logger_Init(void);

/**
 * SD_Logger_Service - Advance SD retry/reconnect state.
 *
 * Call this periodically from the main loop.
 * Retry waiting is scheduled with HAL_GetTick() instead of HAL_Delay().
 * A reconnect attempt can still block briefly inside the SD/FatFs driver.
 */
void SD_Logger_Service(void);

/**
 * SD_Logger_Write_CSV - Append one sensor sample to datalog.csv.
 * @time: DS1307 timestamp
 * @env: BME280 temperature, humidity, and pressure data
 * @scd: SCD41 CO2 data
 * @dust: PMS7003 dust data
 *
 * Builds one CSV line and appends it to datalog.csv.
 * If the SD card is not ready, this function returns without writing.
 *
 * last_log_attempt stores the last generated CSV line attempted for logging,
 * not necessarily the last successfully written line.
 */
void SD_Logger_Write_CSV(DS1307_Time_t *time, BME280_Data_t *env, SCD41_Data_t *scd,
                         PMS_Data_t *dust);

#endif /* INC_SD_LOGGER_H_ */
