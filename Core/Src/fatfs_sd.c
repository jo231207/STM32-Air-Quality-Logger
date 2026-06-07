#include "fatfs_sd.h"
#include "spi.h"

// SD Card Commands
#define CMD0 (0x40 + 0)           /* GO_IDLE_STATE */
#define CMD1 (0x40 + 1)           /* SEND_OP_COND */
#define CMD8 (0x40 + 8)           /* SEND_IF_COND */
#define CMD9 (0x40 + 9)           /* SEND_CSD */
#define CMD10 (0x40 + 10)         /* SEND_CID */
#define CMD12 (0x40 + 12)         /* STOP_TRANSMISSION */
#define CMD16 (0x40 + 16)         /* SET_BLOCKLEN */
#define CMD17 (0x40 + 17)         /* READ_SINGLE_BLOCK */
#define CMD18 (0x40 + 18)         /* READ_MULTIPLE_BLOCK */
#define CMD23 (0x40 + 23)         /* SET_BLOCK_COUNT */
#define CMD24 (0x40 + 24)         /* WRITE_BLOCK */
#define CMD25 (0x40 + 25)         /* WRITE_MULTIPLE_BLOCK */
#define CMD32 (0x40 + 32)         /* ERASE_WR_BLK_START_ADDR */
#define CMD33 (0x40 + 33)         /* ERASE_WR_BLK_END_ADDR */
#define CMD38 (0x40 + 38)         /* ERASE */
#define CMD41 (0x40 + 41)         /* SEND_OP_COND */
#define ACMD41 (0x80 + 0x40 + 41) /* SEND_OP_COND (ACMD) */
#define CMD55 (0x40 + 55)         /* APP_CMD */
#define CMD58 (0x40 + 58)         /* READ_OCR */

/*
 * FatFs disk status. Starts as STA_NOINIT, meaning the drive is not initialized.
 */
static volatile DSTATUS Stat = STA_NOINIT;

/*
 * SD card type flags detected during initialization.
 * CardType bit flags used in this driver:
 * bit 0, 0x01: MMC card
 * bit 1, 0x02: SD card
 * bit 2, 0x04: block-addressed card, SDHC/SDXC
 *
 * If block-addressed bit is not set, sector number must be converted
 * to byte address by sector * 512.
 */
static uint8_t CardType;

/*
 * SD debug values for OLED or debugger inspection when the card fails.
 * sd_disk_result: driver-local result code.
 * sd_last_cmd: last SD command sent.
 * sd_last_resp: last SD command response.
 * sd_last_token: last data token received.
 */
uint8_t sd_disk_result = 0;
uint8_t sd_last_cmd = 0;
uint8_t sd_last_resp = 0;
uint8_t sd_last_token = 0;

static void SPI_Timer_On(uint16_t ms);
static uint8_t SPI_Timer_Status(void);
static uint32_t Timer1;

static void SPI_Timer_On(uint16_t ms)
{
    Timer1 = HAL_GetTick() + ms;
}

static uint8_t SPI_Timer_Status(void)
{
    return ((int32_t)(HAL_GetTick() - Timer1) < 0) ? 1U : 0U;
}

static void SD_CS_HIGH(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
}

static void SD_CS_LOW(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
}

static uint8_t SPI_RxByte(void)
{
    uint8_t dummy = 0xFF;
    uint8_t data = 0;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &dummy, &data, 1, HAL_MAX_DELAY);
    return data;
}

static void SPI_TxByte(uint8_t data)
{
    uint8_t dummy;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &dummy, 1, HAL_MAX_DELAY);
}

static void SPI_RxBytePtr(uint8_t *buff)
{
    *buff = SPI_RxByte();
}

static uint8_t SD_ReadyWait(void)
{
    uint8_t res;
    SPI_Timer_On(500);
    SPI_RxByte();
    do
    {
        res = SPI_RxByte();
    } while ((res != 0xFF) && SPI_Timer_Status());
    return res;
}

static void SD_IdleClocks(uint8_t count)
{
    SD_CS_HIGH();
    for (uint8_t i = 0; i < count; i++)
    {
        SPI_TxByte(0xFF);
    }
}

/*
 * Reinitialize the STM32 SPI peripheral and send idle clocks with CS high.
 * This does not power-cycle the SD card.
 */
static void SD_ReinitSpiBus(void)
{
    SD_CS_HIGH();
    HAL_SPI_DeInit(&SD_SPI_HANDLE);
    HAL_Delay(2);
    MX_SPI1_Init();
    HAL_Delay(2);
    SD_IdleClocks(20);
}

static void SD_PowerOn(void)
{
    SD_ReinitSpiBus();
}

static void SD_PowerOff(void)
{
    SD_CS_HIGH();
    SD_IdleClocks(2);
}

static uint8_t SD_RxDataBlock(BYTE *buff, UINT btr)
{
    uint8_t token;
    SPI_Timer_On(1000);
    do
    {
        token = SPI_RxByte();
    } while ((token == 0xFF || token == 0x00) && SPI_Timer_Status());

    sd_last_token = token;
    if (token != 0xFE)
        return 0;

    do
    {
        SPI_RxBytePtr(buff++);
        SPI_RxBytePtr(buff++);
    } while (btr -= 2);
    /*
     * TODO: Data block CRC16 is currently consumed but not verified. This
     * matches simple FatFs SPI SD examples, but CRC16 verification should be
     * added for long wiring or noisy environments.
     */
    SPI_RxByte();
    SPI_RxByte();
    return 1;
}

#if _USE_WRITE == 1
static uint8_t SD_TxDataBlock(const BYTE *buff, BYTE token)
{
    uint8_t resp;
    if (SD_ReadyWait() != 0xFF)
        return 0;

    SPI_TxByte(token);
    if (token != 0xFD)
    {
        uint16_t btr = 512;
        do
        {
            SPI_TxByte(*buff++);
            SPI_TxByte(*buff++);
        } while (btr -= 2);
        SPI_TxByte(0xFF); // CRC
        SPI_TxByte(0xFF);
        resp = SPI_RxByte();
        if ((resp & 0x1F) != 0x05)
            return 0;

        if (SD_ReadyWait() != 0xFF)
            return 0;
    }
    return 1;
}
#endif

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t n, res;
    uint8_t original_cmd = cmd;

    if (cmd & 0x80)
    {
        cmd &= 0x7F;
        res = SD_SendCmd(CMD55, 0);
        if (res > 1)
            return res;
    }

    if (cmd != CMD12)
    {
        SD_CS_HIGH();
        SPI_TxByte(0xFF);
        SD_CS_LOW();
        SPI_TxByte(0xFF);
    }

    SPI_TxByte(cmd);
    SPI_TxByte((uint8_t)(arg >> 24));
    SPI_TxByte((uint8_t)(arg >> 16));
    SPI_TxByte((uint8_t)(arg >> 8));
    SPI_TxByte((uint8_t)arg);

    n = 0x01;
    if (cmd == CMD0)
        n = 0x95;
    if (cmd == CMD8)
        n = 0x87;
    SPI_TxByte(n);

    if (cmd == CMD12)
        SPI_RxByte();

    n = 200;
    do
    {
        res = SPI_RxByte();
    } while ((res & 0x80) && --n);

    sd_last_cmd = original_cmd;
    sd_last_resp = res;
    return res;
}

DSTATUS SD_disk_initialize(BYTE pdrv)
{
    uint8_t n, cmd, ty, ocr[4];

    if (pdrv)
        return STA_NOINIT;

    HAL_Delay(100);

    SD_PowerOn();
    SD_CS_LOW();

    ty = 0;
    if (SD_SendCmd(CMD0, 0) == 1)
    {
        SPI_Timer_On(1000);
        if (SD_SendCmd(CMD8, 0x1AA) == 1)
        {
            for (n = 0; n < 4; n++)
                ocr[n] = SPI_RxByte();
            if (ocr[2] == 0x01 && ocr[3] == 0xAA)
            {
                while (SPI_Timer_Status() && SD_SendCmd(ACMD41, 1UL << 30))
                    ;
                if (SPI_Timer_Status() && SD_SendCmd(CMD58, 0) == 0)
                {
                    for (n = 0; n < 4; n++)
                        ocr[n] = SPI_RxByte();
                    /*
                     * OCR[0] bit 6 is CCS.
                     * CCS = 1: SDHC/SDXC, block addressing.
                     * CCS = 0: SDSC, byte addressing.
                     */
                    ty = (ocr[0] & 0x40) ? 6 : 2;
                }
            }
        }
        else
        {
            if (SD_SendCmd(CMD55, 0) <= 1 && SD_SendCmd(CMD41, 0) <= 1)
            {
                ty = 2;
                cmd = CMD41;
            }
            else
            {
                ty = 1;
                cmd = CMD1;
            }
            while (SPI_Timer_Status() && SD_SendCmd(cmd, 0))
                ;
            if (!SPI_Timer_Status() || SD_SendCmd(CMD16, 512) != 0)
            {
                ty = 0;
            }
        }
    }

    CardType = ty;
    SD_CS_HIGH();
    SPI_TxByte(0xFF);

    if (ty)
    {
        Stat &= ~STA_NOINIT;
    }
    else
    {
        SD_PowerOff();
        Stat |= STA_NOINIT;
    }
    return Stat;
}

void SD_disk_deinitialize(void)
{
    SD_PowerOff();
    CardType = 0;
    Stat = STA_NOINIT;
}

DSTATUS SD_disk_status(BYTE pdrv)
{
    if (pdrv)
        return STA_NOINIT;
    return Stat;
}

DRESULT SD_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv || !count)
    {
        sd_disk_result = 10;
        return RES_PARERR;
    }
    if (Stat & STA_NOINIT)
    {
        sd_disk_result = 11;
        return RES_NOTRDY;
    }

    /*
     * Non-SDHC/SDXC cards use byte addressing in SPI mode.
     * FatFs passes sector number, so convert sector to byte address.
     */
    if (!(CardType & 4))
        sector *= 512;

    if (count == 1)
    {
        if ((SD_SendCmd(CMD17, sector) == 0) && SD_RxDataBlock(buff, 512))
            count = 0;
    }
    else
    {
        if (SD_SendCmd(CMD18, sector) == 0)
        {
            do
            {
                if (!SD_RxDataBlock(buff, 512))
                    break;
                buff += 512;
            } while (--count);
            SD_SendCmd(CMD12, 0);
        }
    }
    SD_CS_HIGH();
    SPI_TxByte(0xFF);
    if (count)
    {
        sd_disk_result = 12;
        return RES_ERROR;
    }
    sd_disk_result = 0;
    return RES_OK;
}

#if _USE_WRITE == 1
DRESULT SD_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv || !count)
    {
        sd_disk_result = 20;
        return RES_PARERR;
    }
    if (Stat & STA_NOINIT)
    {
        sd_disk_result = 21;
        return RES_NOTRDY;
    }
    if (Stat & STA_PROTECT)
    {
        sd_disk_result = 22;
        return RES_WRPRT;
    }

    /*
     * Non-SDHC/SDXC cards use byte addressing in SPI mode.
     * FatFs passes sector number, so convert sector to byte address.
     */
    if (!(CardType & 4))
        sector *= 512;

    if (count == 1)
    {
        if ((SD_SendCmd(CMD24, sector) == 0) && SD_TxDataBlock(buff, 0xFE))
            count = 0;
    }
    else
    {
        if (CardType & 2)
        {
            SD_SendCmd(CMD55, 0);
            SD_SendCmd(CMD23, count);
        }
        if (SD_SendCmd(CMD25, sector) == 0)
        {
            do
            {
                if (!SD_TxDataBlock(buff, 0xFC))
                    break;
                buff += 512;
            } while (--count);
            if (!SD_TxDataBlock(0, 0xFD))
                count = 1;
        }
    }
    SD_CS_HIGH();
    SPI_TxByte(0xFF);
    if (count)
    {
        sd_disk_result = 23;
        return RES_ERROR;
    }
    sd_disk_result = 0;
    return RES_OK;
}
#endif

#if _USE_IOCTL == 1
DRESULT SD_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res;
    uint8_t n, csd[16];
    DWORD *dp, st, ed, csize;

    if (pdrv)
        return RES_PARERR;
    if (Stat & STA_NOINIT)
        return RES_NOTRDY;

    res = RES_ERROR;
    switch (cmd)
    {
    case CTRL_SYNC:
        if (SD_ReadyWait() == 0xFF)
            res = RES_OK;
        break;
    case GET_SECTOR_COUNT:
        if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
        {
            if ((csd[0] >> 6) == 1)
            {
                csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
                *(DWORD *)buff = csize << 10;
            }
            else
            {
                n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
                csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
                *(DWORD *)buff = csize << (n - 9);
            }
            res = RES_OK;
        }
        break;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        res = RES_OK;
        break;
    case GET_BLOCK_SIZE:
        if (CardType & 4)
        {
            if (SD_SendCmd(CMD9, 0) == 0 && SD_RxDataBlock(csd, 16))
            {
                *(DWORD *)buff = csd[10] / 2;
                res = RES_OK;
            }
        }
        else
        {
            if ((SD_SendCmd(CMD9, 0) == 0) && SD_RxDataBlock(csd, 16))
            {
                *(DWORD *)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7)) + 1;
                res = RES_OK;
            }
        }
        break;
    case CTRL_TRIM:
        dp = buff;
        st = dp[0];
        ed = dp[1];
        /*
         * Non-SDHC/SDXC cards use byte addressing in SPI mode.
         * FatFs passes sector number, so convert sector to byte address.
         */
        if (!(CardType & 4))
        {
            st *= 512;
            ed *= 512;
        }
        if (SD_SendCmd(CMD32, st) == 0 && SD_SendCmd(CMD33, ed) == 0 && SD_SendCmd(CMD38, 0) == 0)
        {
            SD_ReadyWait();
            res = RES_OK;
        }
        break;
    default:
        res = RES_PARERR;
    }
    SD_CS_HIGH();
    SPI_TxByte(0xFF);
    return res;
}
#endif
