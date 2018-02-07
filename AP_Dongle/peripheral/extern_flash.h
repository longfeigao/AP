
#ifndef  EXTERN_FLASH_H
#define  EXTERN_FLASH_H

#include <ti/drivers/NVS.h>
#include <ti/drivers/nvs/NVSSPI25X.h>
#include "datatype.h"

// Return Message
typedef enum {
    FlashOperationSuccess,
    FlashWriteRegFailed,
    FlashTimeOut,
    FlashIsBusy,
    FlashQuadNotEnable,
    FlashAddressInvalid,
}ReturnMsg;

#define    FlashID          0xc22015
#define     FlashID_GD      0xc84015
#define    ElectronicID     0x14
#define    RESID0           0xc214
#define    RESID1           0x14c2

extern void init_nvs_spi_flash(void);
extern void extern_flash_open(void);
extern void extern_flash_close(void);
extern ReturnMsg CMD_SE(WORD seg_addr);
extern ReturnMsg CMD_PP(WORD addr, WORD data, WORD len);
extern ReturnMsg CMD_FASTREAD(WORD addr, WORD buf, WORD len);

#endif
