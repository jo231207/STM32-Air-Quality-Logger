/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 * @note Font policy: use only permissively licensed fonts with redistribution
 *       rights for production builds. Avoid vendor-branded fonts such as
 *       Adobe/DEC X11 fonts unless their license has been reviewed.
 *       For development and testing, any fonts can be used, but for production,
 *       ensure that the fonts included in the final build comply with licensing
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "u8g2.h" // u8g2 라이브러리 헤더 포함
#include "pms7003.h"
#include "bme280.h"
#include "scd41.h"
#include "ds1307.h"
#include "sd_logger.h"
#include "fatfs_sd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DISPLAY_UPDATE_MS 1000U
#define SCREEN_CHANGE_MS 5000U
#define SCREEN_COUNT 7U

#define DISPLAY_TITLE_FONT u8g2_font_7x13_tr
#define DISPLAY_BODY_FONT u8g2_font_6x10_tr
#define DISPLAY_SMALL_FONT u8g2_font_5x7_tr

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
u8g2_t u8g2; // U8g2 핸들(구조체) 변수 선언
/** @brief OLED 화면을 마지막으로 갱신한 시간(ms), HAL_GetTick() 기준 */
static uint32_t last_display_tick = 0;

/** @brief 센서 데이터를 마지막으로 읽은 시간(ms), HAL_GetTick() 기준 */
static uint32_t last_sensor_tick = 0;

/** @brief SD카드에 CSV 로그를 마지막으로 저장한 시간(ms), HAL_GetTick() 기준 */
static uint32_t last_log_tick = 0;

/** @brief 상태 LED를 마지막으로 토글한 시간(ms), HAL_GetTick() 기준 */
static uint32_t last_led_tick = 0;

static PMS_Data_t last_dust_data = {0};
static uint8_t dust_ready = 0;

static BME280_Data_t last_env_data = {0};
static uint8_t env_ready = 0;

static SCD41_Data_t last_scd_data = {0};
static uint8_t scd_ready = 0;

static DS1307_Time_t current_time = {0};
static uint8_t rtc_ready = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// U8g2와 STM32 HAL I2C/GPIO를 연결해주는 콜백 함수들
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
// 함수의 반환 타입을 void -> uint8_t 로 변경
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();
    MX_SPI1_Init();
    MX_FATFS_Init();
    /* USER CODE BEGIN 2 */

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0, u8x8_byte_stm32_hw_i2c,
        u8x8_gpio_and_delay_stm32); // 뒤에 있는건 함수 콜백 (함수 원형 넘겨서 해당 함수가 나중에
                                    // 라이브러리에서 호출될 수 있게 함)
    u8g2_InitDisplay(&u8g2);        // 디스플레이 초기화
    u8g2_SetPowerSave(&u8g2, 0);    // 절전 모드 비활성화

    pms_init();          // 미세먼지 센서 UART 수신 초기화
    BME280_Init(&hi2c1); // BME280 센서 I2C 초기화
    SCD41_Init(&hi2c1);  // SCD41 센서 I2C 초기화
    DS1307_Init(&hi2c1); // DS1307 RTC I2C 초기화

    SD_Logger_Init(); // Initialize SD FatFs logger and retry state.
                      /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        // 온보드 LED (PC13) 토글
        uint32_t now = HAL_GetTick();

        /* 1초마다 LED 토글 */
        if ((uint32_t)(now - last_led_tick) >= 1000U)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            last_led_tick = now;
        }

        /* SD 카드 상태 관리 */
        SD_Logger_Service();

        /* 1초마다 센서 데이터 갱신 */
        if ((uint32_t)(now - last_sensor_tick) >= 1000U)
        {
            if (PMS7003_ReadData(&last_dust_data))
            {
                dust_ready = 1;
            }

            if (BME280_ReadData(&last_env_data))
            {
                env_ready = 1;
            }

            if (SCD41_ReadData(&last_scd_data))
            {
                scd_ready = 1;
            }

            if (DS1307_GetTime(&current_time))
            {
                rtc_ready = 1;
            }

            last_sensor_tick = now;
        }

        /* 1초마다 SD 로그 저장 */
        if (rtc_ready && (uint32_t)(now - last_log_tick) >= 1000U)
        {
            SD_Logger_Write_CSV(&current_time, &last_env_data, &last_scd_data, &last_dust_data);

            last_log_tick = now;
        }

        /* Update OLED at a fixed cadence. */
        if ((uint32_t)(now - last_display_tick) >= DISPLAY_UPDATE_MS)
        {
            last_display_tick = now;

            uint8_t screen_mode = (now / SCREEN_CHANGE_MS) % SCREEN_COUNT;
            char buf[32];

            u8g2_ClearBuffer(&u8g2);
            switch (screen_mode)
            {
            case 0:
            {
                u8g2_SetFont(&u8g2, DISPLAY_TITLE_FONT);
                u8g2_DrawStr(&u8g2, 0, 12, "TIME");

                u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                if (rtc_ready)
                {
                    sprintf(buf, "20%02d/%02d/%02d", current_time.year, current_time.month,
                            current_time.dayofmonth);
                    u8g2_DrawStr(&u8g2, 0, 34, buf);
                    sprintf(buf, "%02d:%02d:%02d", current_time.hour, current_time.minutes,
                            current_time.seconds);
                    u8g2_DrawStr(&u8g2, 0, 56, buf);
                }
                else
                {
                    u8g2_DrawStr(&u8g2, 0, 36, "RTC not ready");
                }
                break;
            }
            case 1:
            {
                u8g2_SetFont(&u8g2, DISPLAY_TITLE_FONT);
                u8g2_DrawStr(&u8g2, 0, 12, "DUST");

                u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                if (dust_ready)
                {
                    sprintf(buf, "PM1.0  %4d ug", last_dust_data.pm1_0_atm);
                    u8g2_DrawStr(&u8g2, 0, 30, buf);
                    sprintf(buf, "PM2.5  %4d ug", last_dust_data.pm2_5_atm);
                    u8g2_DrawStr(&u8g2, 0, 45, buf);
                    sprintf(buf, "PM10   %4d ug", last_dust_data.pm10_0_atm);
                    u8g2_DrawStr(&u8g2, 0, 60, buf);
                }
                else
                {
                    u8g2_DrawStr(&u8g2, 0, 36, "PMS waiting");
                }
                break;
            }
            case 2:
            {
                u8g2_SetFont(&u8g2, DISPLAY_SMALL_FONT);
                u8g2_DrawStr(&u8g2, 0, 8, "DUST MASS");

                if (dust_ready)
                {
                    sprintf(buf, "CF1 1.0 %5u", (unsigned int)last_dust_data.pm1_0_cf1);
                    u8g2_DrawStr(&u8g2, 0, 18, buf);
                    sprintf(buf, "CF1 2.5 %5u", (unsigned int)last_dust_data.pm2_5_cf1);
                    u8g2_DrawStr(&u8g2, 0, 27, buf);
                    sprintf(buf, "CF1 10  %5u", (unsigned int)last_dust_data.pm10_0_cf1);
                    u8g2_DrawStr(&u8g2, 0, 36, buf);
                    sprintf(buf, "ATM 1.0 %5u", (unsigned int)last_dust_data.pm1_0_atm);
                    u8g2_DrawStr(&u8g2, 0, 45, buf);
                    sprintf(buf, "ATM 2.5 %5u", (unsigned int)last_dust_data.pm2_5_atm);
                    u8g2_DrawStr(&u8g2, 0, 54, buf);
                    sprintf(buf, "ATM 10  %5u", (unsigned int)last_dust_data.pm10_0_atm);
                    u8g2_DrawStr(&u8g2, 0, 63, buf);
                }
                else
                {
                    u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                    u8g2_DrawStr(&u8g2, 0, 36, "PMS waiting");
                }
                break;
            }
            case 3:
            {
                u8g2_SetFont(&u8g2, DISPLAY_SMALL_FONT);
                u8g2_DrawStr(&u8g2, 0, 8, "DUST COUNT / 0.1L");

                if (dust_ready)
                {
                    sprintf(buf, ">0.3um %5u", (unsigned int)last_dust_data.particles_0_3um);
                    u8g2_DrawStr(&u8g2, 0, 18, buf);
                    sprintf(buf, ">0.5um %5u", (unsigned int)last_dust_data.particles_0_5um);
                    u8g2_DrawStr(&u8g2, 0, 27, buf);
                    sprintf(buf, ">1.0um %5u", (unsigned int)last_dust_data.particles_1_0um);
                    u8g2_DrawStr(&u8g2, 0, 36, buf);
                    sprintf(buf, ">2.5um %5u", (unsigned int)last_dust_data.particles_2_5um);
                    u8g2_DrawStr(&u8g2, 0, 45, buf);
                    sprintf(buf, ">5.0um %5u", (unsigned int)last_dust_data.particles_5_0um);
                    u8g2_DrawStr(&u8g2, 0, 54, buf);
                    sprintf(buf, ">10 um %5u", (unsigned int)last_dust_data.particles_10_0um);
                    u8g2_DrawStr(&u8g2, 0, 63, buf);
                }
                else
                {
                    u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                    u8g2_DrawStr(&u8g2, 0, 36, "PMS waiting");
                }
                break;
            }
            case 4:
            {
                u8g2_SetFont(&u8g2, DISPLAY_TITLE_FONT);
                u8g2_DrawStr(&u8g2, 0, 12, "BME280");

                u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                if (env_ready)
                {
                    int temp_int = (int)(last_env_data.temperature);
                    int temp_frac = (int)(last_env_data.temperature * 10) % 10;
                    int humi_int = (int)(last_env_data.humidity);
                    int humi_frac = (int)(last_env_data.humidity * 10) % 10;
                    int pres_int = (int)(last_env_data.pressure);
                    if (temp_frac < 0)
                        temp_frac = -temp_frac;
                    sprintf(buf, "Temp  %d.%d C", temp_int, temp_frac);
                    u8g2_DrawStr(&u8g2, 0, 30, buf);
                    sprintf(buf, "Humi  %d.%d %%", humi_int, humi_frac);
                    u8g2_DrawStr(&u8g2, 0, 45, buf);
                    sprintf(buf, "Pres  %d hPa", pres_int);
                    u8g2_DrawStr(&u8g2, 0, 60, buf);
                }
                else
                {
                    u8g2_DrawStr(&u8g2, 0, 36, "BME waiting");
                }
                break;
            }
            case 5:
            {
                u8g2_SetFont(&u8g2, DISPLAY_TITLE_FONT);
                u8g2_DrawStr(&u8g2, 0, 12, "SCD41");

                u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                if (scd_ready)
                {
                    int temp_int = (int)(last_scd_data.temperature);
                    int temp_frac = (int)(last_scd_data.temperature * 10) % 10;
                    int humi_int = (int)(last_scd_data.humidity);
                    int humi_frac = (int)(last_scd_data.humidity * 10) % 10;
                    if (temp_frac < 0)
                        temp_frac = -temp_frac;
                    sprintf(buf, "CO2   %d ppm", last_scd_data.co2);
                    u8g2_DrawStr(&u8g2, 0, 30, buf);
                    sprintf(buf, "Temp  %d.%d C", temp_int, temp_frac);
                    u8g2_DrawStr(&u8g2, 0, 45, buf);
                    sprintf(buf, "Humi  %d.%d %%", humi_int, humi_frac);
                    u8g2_DrawStr(&u8g2, 0, 60, buf);
                }
                else
                {
                    u8g2_DrawStr(&u8g2, 0, 36, "SCD waiting");
                }
                break;
            }
            case 6:
            {
                u8g2_SetFont(&u8g2, DISPLAY_TITLE_FONT);
                u8g2_DrawStr(&u8g2, 0, 12, "SD CARD");

                u8g2_SetFont(&u8g2, DISPLAY_BODY_FONT);
                if (sd_status == SD_STATUS_OK)
                {
                    u8g2_DrawStr(&u8g2, 0, 28, "Logging active");
                }
                else if (sd_error_stage == SD_ERROR_STAGE_MOUNT && last_fres == FR_NOT_READY)
                {
                    u8g2_DrawStr(&u8g2, 0, 28, "SD card removed");
                }
                else if (sd_status == SD_STATUS_RETRY)
                {
                    u8g2_DrawStr(&u8g2, 0, 28, "SD error/retry");
                }
                else
                {
                    u8g2_DrawStr(&u8g2, 0, 28, "Preparing SD");
                }
                if (sd_status != SD_STATUS_OK)
                {
                    u8g2_SetFont(&u8g2, DISPLAY_SMALL_FONT);
                    if (sd_status == SD_STATUS_RETRY)
                    {
                        if (sd_reconnecting)
                        {
                            u8g2_DrawStr(&u8g2, 0, 40, "Reconnecting...");
                        }
                        else
                        {
                            sprintf(buf, "Retry in %u sec", sd_retry_seconds);
                            u8g2_DrawStr(&u8g2, 0, 40, buf);
                        }
                        sprintf(buf, "F:%u S:%u D:%u", (unsigned int)last_fres,
                                (unsigned int)sd_error_stage, (unsigned int)sd_disk_result);
                        u8g2_DrawStr(&u8g2, 0, 50, buf);
                        sprintf(buf, "C:%02X R:%02X T:%02X", (unsigned int)sd_last_cmd,
                                (unsigned int)sd_last_resp, (unsigned int)sd_last_token);
                        u8g2_DrawStr(&u8g2, 0, 60, buf);
                    }
                    break;
                }

                // Display the latest CSV line attempted for SD write.
                u8g2_SetFont(&u8g2, DISPLAY_SMALL_FONT); // 작은 폰트 사용
                char line1[22] = {0};
                char line2[22] = {0};
                char line3[22] = {0};
                strncpy(line1, last_log_attempt, 21);
                if (strlen(last_log_attempt) > 21)
                {
                    strncpy(line2, last_log_attempt + 21, 21);
                }
                if (strlen(last_log_attempt) > 42)
                {
                    strncpy(line3, last_log_attempt + 42, 21);
                }
                u8g2_DrawStr(&u8g2, 0, 40, line1);
                u8g2_DrawStr(&u8g2, 0, 50, line2);
                u8g2_DrawStr(&u8g2, 0, 60, line3);
                break;
            }
            }

            u8g2_SendBuffer(&u8g2); // 내부 메모리의 내용을 실제 화면으로 전송합니다.
        }

        // Sensor samples are logged once per second by SD_Logger_Write_CSV().
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    /** Enables the Clock Security System
     */
    HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */
/**
 * u8x8_byte_stm32_hw_i2c - U8g2 byte callback for STM32 HAL I2C.
 * @msg: U8x8 byte operation command.
 * @arg_int: Number of bytes to send from @arg_ptr.
 * @arg_ptr: Pointer to byte data provided by U8g2.
 *
 * Return: 1 when the callback handled the request.
 */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    static uint8_t buffer[32];
    static uint8_t buf_idx;
    uint8_t *data;

    switch (msg)
    {
    case U8X8_MSG_BYTE_SEND:
        data = (uint8_t *)arg_ptr;
        while (arg_int > 0)
        {
            buffer[buf_idx++] = *data;
            data++;
            arg_int--;
        }
        break;
    case U8X8_MSG_BYTE_INIT:
        break;
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        buf_idx = 0;
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx,
                                    HAL_MAX_DELAY) != HAL_OK)
        {
            return 0;
        }
        break;
    default:
        return 0;
    }
    return 1;
}

// U8g2가 딜레이나 GPIO 제어를 요청할 때 호출되는 콜백 함수
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        break;
    default:
        u8x8_SetGPIOResult(u8x8, 1);
        break;
    }
    return 1;
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
