/*
 * pms7003.c
 *
 * PMS7003 UART active-mode frame receiver and parser.
 */

#include "pms7003.h"
#include <stddef.h>

extern UART_HandleTypeDef huart1;

#define PMS_BUFFER_SIZE 32U
#define PMS_FRAME_LENGTH 0x001CU

static uint8_t rx_data;
static uint8_t pms_buffer[PMS_BUFFER_SIZE];
static uint8_t pms_idx = 0;

static volatile PMS_Data_t pms_data;
static volatile bool new_data_flag = false;

static uint32_t pms_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    return primask;
}

static void pms_exit_critical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static uint16_t pms_read_u16(uint8_t offset)
{
    return ((uint16_t)pms_buffer[offset] << 8) | pms_buffer[offset + 1U];
}

void pms_init(void)
{
    pms_idx = 0;
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

void pms_uart_callback(void)
{
    pms_buffer[pms_idx] = rx_data;

    if (pms_idx == 0U)
    {
        if (pms_buffer[0] == 0x42U)
        {
            pms_idx++;
        }
    }
    else if (pms_idx == 1U)
    {
        if (pms_buffer[1] == 0x4DU)
        {
            pms_idx++;
        }
        else if (pms_buffer[1] == 0x42U)
        {
            pms_idx = 1U;
        }
        else
        {
            pms_idx = 0U;
        }
    }
    else
    {
        pms_idx++;
        if (pms_idx >= PMS_BUFFER_SIZE)
        {
            uint16_t checksum = 0;
            for (uint8_t i = 0; i < 30U; i++)
            {
                checksum += pms_buffer[i];
            }

            uint16_t frame_length = pms_read_u16(2U);
            uint16_t expected_checksum = pms_read_u16(30U);

            if ((frame_length == PMS_FRAME_LENGTH) && (checksum == expected_checksum))
            {
                PMS_Data_t parsed_data;

                /*
                 * PMS7003 active mode data map:
                 * Data 1-3  : CF=1 standard particle PM values.
                 * Data 4-6  : atmospheric PM values.
                 * Data 7-12 : particle counts per 0.1 L air.
                 * Data 13   : reserved.
                 */
                parsed_data.pm1_0_cf1 = pms_read_u16(4U);
                parsed_data.pm2_5_cf1 = pms_read_u16(6U);
                parsed_data.pm10_0_cf1 = pms_read_u16(8U);
                parsed_data.pm1_0_atm = pms_read_u16(10U);
                parsed_data.pm2_5_atm = pms_read_u16(12U);
                parsed_data.pm10_0_atm = pms_read_u16(14U);
                parsed_data.particles_0_3um = pms_read_u16(16U);
                parsed_data.particles_0_5um = pms_read_u16(18U);
                parsed_data.particles_1_0um = pms_read_u16(20U);
                parsed_data.particles_2_5um = pms_read_u16(22U);
                parsed_data.particles_5_0um = pms_read_u16(24U);
                parsed_data.particles_10_0um = pms_read_u16(26U);
                parsed_data.reserved = pms_read_u16(28U);

                pms_data = parsed_data;
                new_data_flag = true;
            }

            pms_idx = 0U;
        }
    }

    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

bool pms_is_data_ready(void)
{
    uint32_t primask = pms_enter_critical();
    bool ready = new_data_flag;

    if (new_data_flag)
    {
        new_data_flag = false;
    }

    pms_exit_critical(primask);

    return ready;
}

PMS_Data_t pms_get_data(void)
{
    PMS_Data_t data;
    uint32_t primask = pms_enter_critical();

    data = pms_data;

    pms_exit_critical(primask);

    return data;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        pms_uart_callback();
    }
}

bool PMS7003_ReadData(PMS_Data_t *data)
{
    uint32_t primask;

    if (data == NULL)
    {
        return false;
    }

    primask = pms_enter_critical();
    if (!new_data_flag)
    {
        pms_exit_critical(primask);
        return false;
    }

    *data = pms_data;
    new_data_flag = false;

    pms_exit_critical(primask);

    return true;
}
