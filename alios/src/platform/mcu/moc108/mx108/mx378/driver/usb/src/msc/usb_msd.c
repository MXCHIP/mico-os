/******************************************************************
*                                                                *
*        Copyright Mentor Graphics Corporation 2006              *
*                                                                *
*                All Rights Reserved.                            *
*                                                                *
*    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
*  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
*  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
*                                                                *
******************************************************************/
#include "include.h"

#include "mu_cdi.h"
#include "mu_mem.h"
#include "mu_impl.h"
#include "mu_stdio.h"
#include "mu_strng.h"
#include "mu_hfi.h"
#include "mu_spi.h"
#include "class/mu_msd.h"
#include "mu_mapi.h"
#include "mu_none.h"

#if CFG_SUPPORT_MSD
extern MUSB_FunctionClient MGC_MsdFunctionClient;
static void MGC_MsdNewOtgState(void *hClient, MUSB_BusHandle hBus,
                               MUSB_OtgState State);
static void MGC_MsdOtgError(void *hClient, MUSB_BusHandle hBus,
                            uint32_t dwStatus);

/**************************** GLOBALS *****************************/
STATIC MUSB_Port *MGC_pCdiPort = NULL;
STATIC MUSB_BusHandle MGC_hCdiBus = NULL;
STATIC uint8_t MGC_bDesireHostRole = TRUE;
STATIC uint8_t MGC_aMsdPeripheralList[256];
STATIC MUSB_DeviceDriver MGC_aDeviceDriver[2];

STATIC MUSB_HostClient MGC_MsdHostClient =
{
    MGC_aMsdPeripheralList,		/* peripheral list */
    0,			    /* filled in main */
    MGC_aDeviceDriver,
    0					/* filled in main */
};

STATIC MUSB_OtgClient MGC_MsdOtgClient =
{
    NULL,	/* no instance data; we are singleton */
    &MGC_bDesireHostRole,
    MGC_MsdNewOtgState,
    MGC_MsdOtgError
};

STATIC MUSB_HfiDevice *MGC_pHfiDevice = NULL;
STATIC uint8_t MediaIsOk = FALSE;
FUNCPTR trx_callback_ptr = NULL;

/*************************** FUNCTIONS ****************************/
/* OTG client */
static void MGC_MsdNewOtgState(void *hClient, MUSB_BusHandle hBus,
                               MUSB_OtgState State)
{
    char aAnswer[4];

    switch(State)
    {
	    case MUSB_AB_IDLE:
	        MUSB_PrintLine("S - Start Session");
	        MUSB_GetLine(aAnswer, 4);
	        if(('s' == aAnswer[0]) || ('S' == aAnswer[0]))
	        {
	            MUSB_RequestBus(MGC_hCdiBus);
	        }
	        break;
			
	    case MUSB_A_SUSPEND:
	        MUSB_PrintLine("R - Resume bus");
	        MUSB_GetLine(aAnswer, 4);
	        if(('r' == aAnswer[0]) || ('R' == aAnswer[0]))
	        {
	            MUSB_ResumeBus(MGC_hCdiBus);
	        }
	        break;
			
	    default:
	        break;
    }
}

static void MGC_MsdOtgError(void *hClient, MUSB_BusHandle hBus,
                            uint32_t dwStatus)
{
    case MUSB_STATUS_UNSUPPORTED_DEVICE:
        MUSB_PRT("Device not supported\r\n");
        break;
        
    case MUSB_STATUS_UNSUPPORTED_HUB:
        MUSB_PRT("Hubs not supported\r\n");
        break;
        
    case MUSB_STATUS_OTG_VBUS_INVALID:
        MUSB_PRT("Vbus error\r\n");
        break;
        
    case MUSB_STATUS_OTG_NO_RESPONSE:
        MUSB_PRT("Device not responding\r\n");
        break;
        
    case MUSB_STATUS_OTG_SRP_FAILED:
        MUSB_PRT("Device not responding (SRP failed)\r\n");
        break;
        
    default:
        break;
}

MUSB_HfiStatus MUSB_HfiAddDevice(MUSB_HfiVolumeHandle *phVolume,
                  const MUSB_HfiDeviceInfo *pInfo,
                  MUSB_HfiDevice *pDevice)
{
    MGC_pHfiDevice = pDevice;
    MediaIsOk = TRUE;
    return MUSB_HFI_SUCCESS;

}

void MUSB_HfiMediumInserted(MUSB_HfiVolumeHandle 	 hVolume,
                       const MUSB_HfiMediumInfo *pMediumInfo)
{
}

void MUSB_HfiMediumRemoved(MUSB_HfiVolumeHandle hVolume)
{
}

void MUSB_HfiDeviceRemoved(void)
{
    MGC_pHfiDevice = NULL;
    MediaIsOk = FALSE;
}

uint8_t MGC_MsdGetMediumstatus(void)
{
    uint8_t Ret = 0;
    {
        Ret = MediaIsOk;
    }
    
    return Ret;
}

void MGC_StartupAppEvent(void)
{
    if(trx_callback_ptr)
    {
        (*trx_callback_ptr)();
    }
}

void MGC_RegisterCBTransferComplete(FUNCPTR func)
{
    trx_callback_ptr = func;
}

uint32_t MGC_MsdTransferComplete(MUSB_HfiVolumeHandle hVolume,
        uint16_t wActualBlocks)

{
    if(trx_callback_ptr)
    {
        (*trx_callback_ptr)();
    }
    
    return 2;
}
uint32_t MUSB_HfiRead( uint32_t first_block, uint32_t block_num, uint8_t *dest)
{
    uint32_t RetValue = 1;

    if(MGC_pHfiDevice)
    {
        RetValue = MGC_pHfiDevice->pfReadDevice(MGC_pHfiDevice->pPrivateData,
                                                first_block, 0, block_num, dest, 
                                                MGC_MsdTransferComplete, TRUE);
    }
    
    return RetValue;
}
uint32_t MUSB_HfiWrite( uint32_t first_block, uint32_t block_num, uint8_t *dest)
{
    uint32_t RetValue = 1;

    if(MGC_pHfiDevice)
    {
        RetValue = MGC_pHfiDevice->pfWriteDevice(MGC_pHfiDevice->pPrivateData,
                                                first_block, 0, block_num, dest, 
                                                0, MGC_MsdTransferComplete, FALSE);
    }
    
    return RetValue;
}

int usb_sw_init(void)
{
    uint8_t *pList;
    uint8_t bDriver = 0;
    uint16_t wCount = 0;
    uint16_t wSize = 0;
    uint16_t wRemain;
    MUSB_DeviceDriver *pDriver;
    
    wRemain = (uint16_t)sizeof(MGC_aMsdPeripheralList);
    pList = MGC_aMsdPeripheralList;
	
    wSize = MUSB_FillMsdPeripheralList(bDriver, pList, wRemain);
    if(wSize < wRemain)
    {
        pDriver = MUSB_GetStorageClassDriver();
        if(pDriver)
        {
            MUSB_MemCopy(&(MGC_MsdHostClient.aDeviceDriverList[bDriver]),
                         pDriver, 
                         sizeof(MUSB_DeviceDriver));
			
            pList += wSize;
            wCount += wSize;
            wRemain -= wSize;
            
            bDriver++;
        }
    }

    MGC_MsdHostClient.wPeripheralListLength = wCount;
    MGC_MsdHostClient.bDeviceDriverListLength = bDriver;

    if(!MUSB_InitSystem(5))
    {
        MUSB_PRT("MUSB_InitSystem failed\r\n");
        return -1;
    }

    /* find first CDI port */
    MGC_pCdiPort = MUSB_GetPort(0);
   
    /* start session */
    MGC_hCdiBus = MUSB_RegisterOtgClient(MGC_pCdiPort,
                                         &MGC_MsdFunctionClient, 
                                         &MGC_MsdHostClient, 
                                         &MGC_MsdOtgClient);
    
    MUSB_NoneRunBackground();
    
    return 0;
}

int usb_sw_uninit(void)
{
	return 0;
}
#endif // CFG_SUPPORT_MSD

// eof

