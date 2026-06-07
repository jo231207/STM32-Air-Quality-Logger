#include "sd_logger.h"
#include "fatfs.h"
#include "fatfs_sd.h"
#include <stdio.h>
#include <string.h>

#define SD_RETRY_DELAY_MS 10000U
#define SD_RECONNECT_DELAY_MS 1000U
#define SD_MS_PER_SECOND 1000U

SD_Status_t sd_status = SD_STATUS_UNKNOWN;
FRESULT last_fres = FR_OK;
uint8_t sd_retry_seconds = 0;
uint8_t sd_reconnecting = 0;
SD_ErrorStage_t sd_error_stage = SD_ERROR_STAGE_NONE;
char last_log_attempt[SD_LOGGER_CSV_BUFFER_SIZE] = "No data yet";

static uint32_t sd_retry_tick = 0;

static uint8_t tick_reached(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static void SD_Logger_EnterOkState(void)
{
    sd_status = SD_STATUS_OK;
    last_fres = FR_OK;
    sd_retry_seconds = 0;
    sd_reconnecting = 0;
    sd_error_stage = SD_ERROR_STAGE_NONE;
}

/*
 * SD hot-swap is not actively recommended. However, temporary SD contact
 * failure should not stop the logger forever. When an SD error occurs, the
 * logger unmounts/deinitializes the card and waits before retrying. The delay
 * gives the user time to reseat the card and helps avoid repeated mount
 * attempts while the contact state is unstable.
 */
static void SD_Logger_EnterRetryState(FRESULT res)
{
    sd_status = SD_STATUS_RETRY;
    last_fres = res;
    sd_retry_seconds = (uint8_t)(SD_RETRY_DELAY_MS / SD_MS_PER_SECOND);
    sd_reconnecting = 0;
    sd_retry_tick = HAL_GetTick() + SD_RETRY_DELAY_MS;
    f_mount(NULL, USERPath, 0);
    SD_disk_deinitialize();
}

static FRESULT SD_Logger_MountNow(void)
{
    DSTATUS stat = SD_disk_initialize(0);

    /*
     * DSTATUS is a bitmask defined by FatFs diskio.h.
     * STA_NOINIT = 0x01: drive not initialized.
     * STA_NODISK = 0x02: no medium in the drive.
     * STA_PROTECT = 0x04: write protected.
     * Multiple flags can be set together, such as STA_NOINIT | STA_NODISK.
     * Use a bitmask check instead of equality comparison.
     */
    if (stat & STA_NOINIT)
    {
        return FR_NOT_READY;
    }

    return f_mount(&USERFatFS, USERPath, 1);
}

/**
 * @brief Advance SD retry/reconnect state and report whether SD is usable.
 * @return 1 if SD is mounted and usable.
 * @return 0 if SD is unavailable, waiting for retry, or reconnecting.
 */
static uint8_t SD_Logger_ServiceReconnect(void)
{
    uint32_t now = HAL_GetTick();

    if (sd_status == SD_STATUS_OK)
    {
        return 1;
    }

    /*
     * Reconnecting state starts after the 10-second retry wait.
     * Wait one more second before attempting mount so the card/bus can settle.
     */
    if (sd_reconnecting)
    {
        if (!tick_reached(now, sd_retry_tick))
        {
            return 0;
        }

        FRESULT m_res = SD_Logger_MountNow();
        if (m_res != FR_OK)
        {
            sd_error_stage = SD_ERROR_STAGE_MOUNT;
            SD_Logger_EnterRetryState(m_res);
            return 0;
        }

        SD_Logger_EnterOkState();
        return 1;
    }
    /*
     * Retry wait state. Do not keep trying mount immediately after an SD error;
     * give the user and the card contact state time to recover.
     */
    if (!tick_reached(now, sd_retry_tick))
    {
        uint32_t remaining_ms = sd_retry_tick - now;
        sd_retry_seconds = (uint8_t)((remaining_ms + (SD_MS_PER_SECOND - 1U)) / SD_MS_PER_SECOND);
        return 0;
    }

    sd_retry_seconds = 0;
    sd_reconnecting = 1;
    sd_retry_tick = now + SD_RECONNECT_DELAY_MS;
    return 0;
}

void SD_Logger_Service(void)
{
    (void)SD_Logger_ServiceReconnect();
}

void SD_Logger_Init(void)
{
    FRESULT res = SD_Logger_MountNow();
    if (res != FR_OK)
    {
        sd_error_stage = SD_ERROR_STAGE_MOUNT;
        SD_Logger_EnterRetryState(res);
    }
    else
    {
        SD_Logger_EnterOkState();
    }
}

void SD_Logger_Write_CSV(DS1307_Time_t *time, BME280_Data_t *env, SCD41_Data_t *scd,
                         PMS_Data_t *dust)
{
    const char *filename = "datalog.csv";
    char csv_data[SD_LOGGER_CSV_BUFFER_SIZE];
    UINT byteswritten;
    int csv_len;

    SD_Logger_Service();
    if (sd_status != SD_STATUS_OK)
    {
        return;
    }

    int temp_int = (int)(env->temperature);
    int temp_frac = (int)(env->temperature * 10) % 10;
    if (temp_frac < 0)
    {
        temp_frac = -temp_frac;
    }

    int humi_int = (int)(env->humidity);
    int humi_frac = (int)(env->humidity * 10) % 10;

    int pres_int = (int)(env->pressure);
    int pres_frac = (int)(env->pressure * 10) % 10;

    csv_len = snprintf(
        csv_data, sizeof(csv_data),
        "20%02d/%02d/%02d,%02d:%02d:%02d,%d.%d,%d.%d,%d.%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
        "%u,%u,%u,%u",
        time->year, time->month, time->dayofmonth, time->hour, time->minutes, time->seconds,
        temp_int, temp_frac, humi_int, humi_frac, pres_int, pres_frac, (unsigned int)scd->co2,
        (unsigned int)dust->pm1_0_cf1, (unsigned int)dust->pm2_5_cf1,
        (unsigned int)dust->pm10_0_cf1, (unsigned int)dust->pm1_0_atm,
        (unsigned int)dust->pm2_5_atm, (unsigned int)dust->pm10_0_atm,
        (unsigned int)dust->particles_0_3um, (unsigned int)dust->particles_0_5um,
        (unsigned int)dust->particles_1_0um, (unsigned int)dust->particles_2_5um,
        (unsigned int)dust->particles_5_0um, (unsigned int)dust->particles_10_0um);

    /* Keep room for the appended '\n' and the final '\0'. */
    if ((csv_len < 0) || (csv_len > (int)(sizeof(csv_data) - 2U)))
    {
        sd_error_stage = SD_ERROR_STAGE_CSV_BUILD;
        last_fres = FR_INVALID_PARAMETER;
        return;
    }

    strncpy(last_log_attempt, csv_data, sizeof(last_log_attempt) - 1);
    last_log_attempt[sizeof(last_log_attempt) - 1] = '\0';

    csv_data[csv_len++] = '\n';
    csv_data[csv_len] = '\0';

    /*
     * TODO: A future version can buffer CSV rows into 512-byte blocks to reduce
     * SD write frequency. This version opens, writes, and closes every log row
     * to keep the code simple and minimize data loss on sudden power loss.
     */
    FRESULT o_res = f_open(&USERFile, filename, FA_OPEN_ALWAYS | FA_WRITE);
    if (o_res == FR_OK)
    {
        FRESULT s_res = f_lseek(&USERFile, f_size(&USERFile));
        if (s_res == FR_OK)
        {
            FRESULT w_res = f_write(&USERFile, csv_data, (UINT)csv_len, &byteswritten);
            if (w_res == FR_OK && byteswritten == (UINT)csv_len)
            {
                FRESULT c_res = f_close(&USERFile);
                if (c_res == FR_OK)
                {
                    SD_Logger_EnterOkState();
                    return;
                }

                sd_error_stage = SD_ERROR_STAGE_CLOSE;
                last_fres = c_res;
                SD_Logger_EnterRetryState(last_fres);
                return;
            }
            else
            {
                sd_error_stage = SD_ERROR_STAGE_WRITE;
                last_fres = (w_res == FR_OK) ? FR_DISK_ERR : w_res;
            }
        }
        else
        {
            sd_error_stage = SD_ERROR_STAGE_SEEK;
            last_fres = s_res;
        }

        /*
         * f_open succeeded, so close the file even after a seek/write failure
         * before entering retry state. sd_error_stage and last_fres are already
         * set by the failing branch above.
         */
        (void)f_close(&USERFile);
        SD_Logger_EnterRetryState(last_fres);
    }
    else
    {
        sd_error_stage = SD_ERROR_STAGE_OPEN;
        SD_Logger_EnterRetryState(o_res);
    }
}
