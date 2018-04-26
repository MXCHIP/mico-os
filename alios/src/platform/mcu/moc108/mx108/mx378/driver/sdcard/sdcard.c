#include "include.h"

#if CFG_USE_SDCARD_HOST
#include "sdio_driver.h"

#include "sdcard.h"
#include "sdcard_pub.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "mem_pub.h"

// for assertions
#include "arch.h"

/* Standard sd  commands (  )           type  argument     response */
#define GO_IDLE_STATE             0   /* bc                          */
#define ALL_SEND_CID              2
#define SEND_RELATIVE_ADDR        3   /* ac   [31:16] RCA        R6  */
#define IO_SEND_OP_COND  		  5   /* ac                      R4  */
#define SWITCH_FUNC               6
#define SELECT_CARD               7   /* ac   [31:16] RCA        R7  */
#define SEND_IF_COND              8   /* adtc                    R1  */
#define SEND_CSD                  9
#define SEND_STATUS               13
#define READ_SINGLE_BLOCK         17
#define WRITE_BLOCK               24
#define SD_APP_OP_COND            41
#define IO_RW_DIRECT              52  /* ac   [31:0] See below   R5  */
#define IO_RW_EXTENDED            53  /* adtc [31:0] See below   R5  */
#define APP_CMD                   55 

#define R5_COM_CRC_ERROR	      (1 << 15)	/* er, b */
#define R5_ILLEGAL_COMMAND	      (1 << 14)	/* er, b */
#define R5_ERROR		          (1 << 11)	/* erx, c */
#define R5_FUNCTION_NUMBER	      (1 << 9)	/* er, c */
#define R5_OUT_OF_RANGE		      (1 << 8)	/* er, c */
#define R5_STATUS(x)		      (x & 0xCB00)
#define R5_IO_CURRENT_STATE(x)	  ((x & 0x3000) >> 12) /* s, b */

/*STM32 register bit define*/
#define SDIO_ICR_MASK             0x5FF
#define SDIO_STATIC_FLAGS         ((UINT32)0x000005FF)
#define SDIO_FIFO_ADDRESS         ((UINT32)0x40018080)

#define OCR_MSK_BUSY             0x80000000 // Busy flag
#define OCR_MSK_HC               0x40000000 // High Capacity flag
#define OCR_MSK_VOLTAGE_ALL      0x00FF8000 // All Voltage flag

#define SD_DEFAULT_OCR           (OCR_MSK_VOLTAGE_ALL|OCR_MSK_HC)

#define SD_MAX_VOLT_TRIAL        ((INT32)0x0000FFFF)
 
typedef enum
{
    SD_CARD_IDLE                 = ((UINT32)0),
    SD_CARD_READY                = ((UINT32)1),
    SD_CARD_IDENTIFICATION       = ((UINT32)2),
    SD_CARD_STANDBY              = ((UINT32)3),
    SD_CARD_TRANSFER             = ((UINT32)4),
    SD_CARD_SENDING              = ((UINT32)5),
    SD_CARD_RECEIVING            = ((UINT32)6),
    SD_CARD_PROGRAMMING          = ((UINT32)7),
    SD_CARD_DISCONNECTED         = ((UINT32)8),
    SD_CARD_ERROR                = ((UINT32)0xff)
}SDCardState;

typedef struct sdio_command {
	UINT32	index;
	UINT32  arg;
	UINT32	flags;		    /* expected response type */
    UINT32  timeout;
    UINT32	resp[4];
	void    *data;		    /* data segment associated with cmd */
    SDIO_Error	err;		/* command error */
}SDIO_CMD_S, *SDIO_CMD_PTR;

typedef struct _sdcard_
{
    UINT32  total_block;
    UINT16  block_size;
    UINT16  card_rca;
    UINT16  init_flag;
	UINT16	Addr_shift_bit;
}SDCARD_S, *SDCARD_PTR;

static SDCARD_S sdcard;
static DD_OPERATIONS sdcard_op = {
            sdcard_open,
            sdcard_close,
            sdcard_read,
            sdcard_write,
            sdcard_ctrl
};

/******************************************************************************/
/***************************** public function ********************************/
/******************************************************************************/
static void sdio_hw_init(void)
{
    /* config sdcard gpio */
    sdio_gpio_config();

    /* set sdcard clk */
    sdio_clk_config();

    /* reset sdcard moudle register */
    sdio_register_reset();
}

static void sdio_send_cmd(SDIO_CMD_PTR sdio_cmd_ptr)
{
    sdio_sendcmd_function(sdio_cmd_ptr->index, 
        sdio_cmd_ptr->flags, sdio_cmd_ptr->timeout, (void*)sdio_cmd_ptr->arg);
}

static void sdio_hw_uninit(void)
{
    /* change sdcard gpio to GPIO mode*/
    // sdcard_gpio_config();

    /* reset sdcard moudle register */
    // sdcard_register_reset();

    /* close sdcard clk */
    //sdcard_clk_config();
}

static void sdio_sw_init(void)
{
    os_memset((void*)&sdcard, 0, sizeof(SDCARD_S));
}

/******************************************************************************/
/***************************** sdcard function ********************************/
/******************************************************************************/
/* GO_IDLE_STATE */
static SDIO_Error sdcard_cmd0_process(void)
{
    SDIO_CMD_S cmd;
    
    cmd.index = GO_IDLE_STATE;
    cmd.arg = 0;
    cmd.flags = SD_CMD_NORESP;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    return cmd.err;
}

static SDIO_Error sdcard_cmd8_process(void)
{
    SDIO_CMD_S cmd;
    UINT8 voltage_accpet, check_pattern;
    
    cmd.index = SEND_IF_COND;
    cmd.arg = 0x1AA;
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    if(cmd.err == SD_CMD_RSP_TIMEOUT){
        SDCARD_WARN("cmd8 noresp, voltage mismatch or Ver1.X SD or not SD\r\n");
        return SD_CMD_RSP_TIMEOUT;
    }else if(cmd.err == SD_CMD_CRC_FAIL){
        SDCARD_WARN("cmd8 cmdcrc err\r\n");
        return SD_CMD_CRC_FAIL;
    }
    
    SDCARD_PRT("found a Ver2.00 or later SDCard\r\n");

    // check Valid Response, 
    // R7-[11:8]:voltage accepted, [7:0] echo-back of check pattern
    sdio_get_cmdresponse_argument(0, &cmd.resp[0]);

    check_pattern = cmd.resp[0]&0xff;
    voltage_accpet = cmd.resp[0]>>8&0xf;
    
    if(voltage_accpet==0x1 && check_pattern==0xaa){
        SDCARD_PRT("support 2.7~3.6V\r\n");
        return SD_OK;
    }else{
        SDCARD_WARN("unsupport voltage\r\n");
        return SD_INVALID_VOLTRANGE;
    }
}

/*Send host capacity support information(HCS) and  asks
  the card to send its OCR in the response on CMD line*/
static SDIO_Error sdcard_acmd41_process(UINT32 ocr)
{
    SDIO_CMD_S cmd;

    cmd.index = APP_CMD;
    cmd.arg = 0;
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    if(cmd.err!=SD_OK){
        SDCARD_WARN("send cmd55 err:%d\r\n", cmd.err);
        return cmd.err;
    }

    cmd.index = SD_APP_OP_COND;
    cmd.arg = ocr;
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    // why cmd41 always return crc fail?
    if(cmd.err!=SD_OK && cmd.err!=SD_CMD_CRC_FAIL){
        SDCARD_WARN("send cmd41 err:%d\r\n", cmd.err);
        return cmd.err;
    }

    return SD_OK;
}

/*ask the CID number on the CMD line*/
// Manufacturer ID	        MID	    8	[127:120]
// OEM/Application          ID	OID	16	[119:104]
// Product name	            PNM	    40	[103:64]
// Product revision	        PRV	    8	[63:56]
// Product serial number	PSN	    32	[55:24]
// reserved	                --	    4	[23:20]
// Manufacturing date	    MDT	    12	[19:8]
static SDIO_Error sdcard_cmd2_process(void)
{
    SDIO_CMD_S cmd;
    
    cmd.index = ALL_SEND_CID;
    cmd.arg = 0;
    cmd.flags = SD_CMD_LONG;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    // dismiss the CID info
    
    return cmd.err;
}

/*ask the card to publish a new RCA*/
static SDIO_Error sdcard_cmd3_process(void)
{
    SDIO_CMD_S cmd;

    cmd.index = SEND_RELATIVE_ADDR;
    cmd.arg = 0;
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    if(cmd.err == SD_CMD_RSP_TIMEOUT){
        SDCARD_WARN("cmd3 noresp \r\n");
        return SD_CMD_RSP_TIMEOUT;
    }else if(cmd.err == SD_CMD_CRC_FAIL){
        SDCARD_WARN("cmd3 cmdcrc err\r\n");
        return SD_CMD_CRC_FAIL;
    }

    sdio_get_cmdresponse_argument(0, &cmd.resp[0]);
    sdcard.card_rca = (UINT16) (cmd.resp[0]>>16);
    SDCARD_PRT("cmd3 is ok, card rca:0x%x\r\n", sdcard.card_rca);
    return SD_OK;
}

/*get CSD Register content*/
static SDIO_Error sdcard_cmd9_process(void)
{
    SDIO_CMD_S cmd;
    int mult, csize;
    
    cmd.index = SEND_CSD;
    cmd.arg = (UINT32)(sdcard.card_rca << 16);
    cmd.flags = SD_CMD_LONG;
    cmd.timeout = DEF_CMD_TIME_OUT;
    
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    if(cmd.err!=SD_OK){
        return cmd.err;
    }

    sdio_get_cmdresponse_argument(0, &cmd.resp[0]);
    sdio_get_cmdresponse_argument(1, &cmd.resp[1]);
    sdio_get_cmdresponse_argument(2, &cmd.resp[2]);

    sdcard.block_size = 1<<((cmd.resp[1]>>16)&0xf);

    if((cmd.resp[0]>>30)&0x3 == 0){
        csize = (((cmd.resp[1] & 0x3FF ) << 2) | ((cmd.resp[2] >> 30 ) & 0x3));
        mult  = ( cmd.resp[2] >> 15 ) & 0x7;

        sdcard.total_block = (csize + 1 )*( 1<< (mult + 2 ) );
        sdcard.total_block *= (sdcard.block_size >> 9);
    }else{
        csize = (((cmd.resp[1] & 0x3F ) << 16) | ((cmd.resp[2] >> 16 ) & 0xFFFF));
        sdcard.total_block = (csize + 1)*1024;
    }
    //sdcard.block_size = SD_DEFAULT_BLOCK_SIZE;
    SDCARD_PRT("Bsize:%x;Total_block:%x\r\n", sdcard.block_size, sdcard.total_block);
    ASSERT_ERR(sdcard.block_size == SD_DEFAULT_BLOCK_SIZE);

    return SD_OK;
}


/*select/deselect card*/
static SDIO_Error sdcard_cmd7_process(void)
{
    SDIO_CMD_S cmd;
    
    cmd.index = SELECT_CARD;
    cmd.arg = (UINT32)(sdcard.card_rca << 16);
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;

    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    return cmd.err;
}

/*set bus width*/
static SDIO_Error sdcard_acmd6_process(void)
{
    SDIO_CMD_S cmd;  
    cmd.index = APP_CMD;
    cmd.arg = (UINT32)(sdcard.card_rca << 16);
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    if(cmd.err!=SD_OK)
        return cmd.err;
        
    cmd.index = SWITCH_FUNC;
    #ifdef CONFIG_SDCARD_BUSWIDTH_4LINE    
    cmd.arg = 2;
    #else
    cmd.arg = 0;    
    #endif
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;    
    
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    return cmd.err;
}

static SDCardState sdcard_get_data_transfer_status(void)
{
    SDIO_CMD_S cmd;  
    cmd.index = SEND_STATUS;
    cmd.arg = (UINT32)(sdcard.card_rca << 16);
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();
    if(cmd.err!=SD_OK){
        SDCARD_WARN("cmd13 err:%d, get data transfer status fail\r\n", cmd.err);
        return SD_CARD_ERROR;
    }

    sdio_get_cmdresponse_argument(0, &cmd.resp[0]);
    cmd.resp[0] = (cmd.resp[0] >> 9)&0x0F;

    return (SDCardState)cmd.resp[0];
}

SDIO_Error sdcard_initialize(void)
{
    int i;
    SDIO_Error err = SD_OK;

    sdio_sw_init();
    sdio_hw_init();

    sdio_set_low_clk();
    for(i=0;i<74;i++);  // not sure 

    // rest card
    err = sdcard_cmd0_process();
    if(err != SD_OK){
        SDCARD_FATAL("send cmd0 err\r\n");
        return err;
    }

    // check support voltage
    err = sdcard_cmd8_process();
    if(err!=SD_OK && err!= SD_CMD_RSP_TIMEOUT ){
        SDCARD_FATAL("send cmd8 err\r\n");
        return err;
    }
    
    if(err==SD_OK){
        int retry_time = SD_MAX_VOLT_TRIAL;
        UINT32 resp0;
        while(retry_time){
            err = sdcard_acmd41_process(SD_DEFAULT_OCR);
            if(err!=SD_OK){
                SDCARD_FATAL("send cmd55&cmd41 err:%d, quite loop\r\n", err);
                return err;
            }
            sdio_get_cmdresponse_argument(0, &resp0);
            if(resp0&OCR_MSK_BUSY)
                break;
            retry_time--;
        }
        if(!retry_time){
            SDCARD_FATAL("send cmd55&cmd41 retry time out\r\n");
            return SD_INVALID_VOLTRANGE;
        }

        SDCARD_PRT("send cmd55&cmd41 complete, card is ready\r\n");
        if(resp0&OCR_MSK_HC){
            SDCARD_PRT("High Capacity SD Memory Card\r\n");
        }else{
            SDCARD_PRT("Standard Capacity SD Memory Card\r\n");
        } 
    }else if(err==SD_CMD_RSP_TIMEOUT){
        int retry_time = SD_MAX_VOLT_TRIAL;
        UINT32 resp0;
        while(retry_time){
            err = sdcard_acmd41_process(OCR_MSK_VOLTAGE_ALL);
            if(err!=SD_OK){
                SDCARD_FATAL("send cmd55&cmd41 err, quite loop\r\n");
                return err;
            }
            sdio_get_cmdresponse_argument(0, &resp0); 
            if(resp0&OCR_MSK_BUSY)
                break;
            retry_time--;
        }
        if(!retry_time){
            SDCARD_FATAL("send cmd55&cmd41 retry time out, maybe a MMC card\r\n");
            return SD_ERROR;
        }   
        SDCARD_PRT("send cmd55&cmd41 complete, SD V1.X card is ready\r\n");
    }

    // get CID, return R2
    err = sdcard_cmd2_process();
    if(err!=SD_OK){
        SDCARD_FATAL("send cmd2 err:%d\r\n", err);
        return err;
    }    

    // get RCA, 
    err = sdcard_cmd3_process();
    if(err!=SD_OK){
        SDCARD_FATAL("send cmd3 err:%d\r\n", err);
        return err;
    }  

    // change to high speed clk
    sdio_set_high_clk();

    // get CSD
    err = sdcard_cmd9_process();
    if(err!=SD_OK){
        SDCARD_FATAL("send cmd9 err:%d\r\n", err);
        return err;
    }      

    // select card
    err = sdcard_cmd7_process();
    if(err!=SD_OK){
        SDCARD_FATAL("send cmd7 err:%d\r\n", err);
        return err;
    }  

    // change bus width, for high speed
    #if CONFIG_SDCARD_BUSWIDTH_4LINE
    //sdio_set_host_buswidth_4line();
    #else
    //sdio_set_host_buswidth_1line();
    #endif
    err = sdcard_acmd6_process();
    if(err!=SD_OK){
        SDCARD_FATAL("send acmd6 err:%d\r\n", err);
        return err;
    } 

    SDCARD_PRT("sdcard initialize is done\r\n");

    return SD_OK;
}

void sdcard_uninitialize(void)
{
    sdio_hw_uninit();
    sdio_sw_init();
}

static SDIO_Error 
sdcard_read_single_block(UINT8 *readbuff, UINT32 readaddr, UINT32 blocksize)
{
    SDIO_CMD_S cmd;  

    ASSERT_ERR(sdcard.block_size == blocksize);

    // setup data reg first
    sdio_setup_data(SDIO_RD_DATA, blocksize, readbuff);

    cmd.index = READ_SINGLE_BLOCK;
    cmd.arg = (UINT32)(readaddr<<sdcard.Addr_shift_bit);
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    if(cmd.err == SD_CMD_RSP_TIMEOUT){
        SDCARD_FATAL("cmd17 noresp, readsingle block err\r\n");
        return cmd.err;
    }else if(cmd.err == SD_CMD_CRC_FAIL){
        SDCARD_FATAL("cmd17 cmdcrc err, readsingle block err\r\n");
        return cmd.err;
    }

    cmd.err = sdcard_wait_receive_data((UINT32*)readbuff);
    if(cmd.err != SD_OK){
        SDCARD_FATAL("read single block wait data receive err:%d\r\n",cmd.err);
        return cmd.err;
    }
	
	#if 0
    sdio_wait_data_useDMA(SDIO_RD_DATA, (UINT32*)readbuff, blocksize);
    cmd.err = sdio_wait_data_response();	
	#endif
	    
    return SD_OK;
}

static SDIO_Error 
sdcard_write_single_block(UINT8 *writebuff, UINT32 writeaddr, UINT32 blocksize)
{
    SDIO_CMD_S cmd;  
    UINT32 check_status_times = 0xffff;

    ASSERT_ERR(sdcard.block_size == blocksize);

    // check card is not busy
    while((sdcard_get_data_transfer_status()!=SD_CARD_TRANSFER)
        && (check_status_times--));     

    // setup data reg first
    sdio_setup_data(SDIO_WR_DATA, blocksize, writebuff);

    cmd.index = WRITE_BLOCK;
    cmd.arg = (UINT32)(writeaddr<<sdcard.Addr_shift_bit);
    cmd.flags = SD_CMD_SHORT;
    cmd.timeout = DEF_CMD_TIME_OUT;
    sdio_send_cmd(&cmd);
    cmd.err = sdio_wait_cmd_response();

    if(cmd.err == SD_CMD_RSP_TIMEOUT){
        SDCARD_FATAL("cmd17 noresp, readsingle block err\r\n");
        return cmd.err;
    }else if(cmd.err == SD_CMD_CRC_FAIL){
        SDCARD_FATAL("cmd17 cmdcrc err, readsingle block err\r\n");
        return cmd.err;
    }

    cmd.err = sdcard_wait_write_data((UINT32*)writebuff);
    if(cmd.err != SD_OK){
        SDCARD_FATAL("write single block wait data receive err:%d\r\n",cmd.err);
        return cmd.err;
    }
	
	#if 0
    sdio_wait_data_useDMA(SDIO_WR_DATA, (UINT32*)writeaddr, blocksize);
    cmd.err = sdio_wait_data_response();
	#endif
    
    return SD_OK;
}

void sdcard_init(void)
{
	ddev_register_dev(SDCARD_DEV_NAME, &sdcard_op);
}

void sdcard_exit(void)
{
    ddev_unregister_dev(SDCARD_DEV_NAME);
}


/******************************************************************************/
/***************************** sdcard API function ****************************/
/******************************************************************************/

UINT32 sdcard_open(UINT32 op_flag)
{
    op_flag = op_flag;
    if(sdcard_initialize())
    {
        SDCARD_FATAL("sdcard_open err\r\n");
        return SDCARD_FAILURE;
    }

    return SDCARD_SUCCESS;
}

UINT32 sdcard_close(void)
{
    sdcard_uninitialize();
    return SDCARD_SUCCESS;
}

UINT32 sdcard_read(char *user_buf, UINT32 count, UINT32 op_flag)
{
    SDIO_Error err = SD_OK;
    UINT32 start_blk_addr;
    UINT8  read_blk_numb, numb;
    UINT8* read_data_buf;

    // check operate parameter
    start_blk_addr = op_flag;
    read_blk_numb = (UINT8)count;
    read_data_buf = (UINT8*)user_buf;

    for(numb=0; numb<read_blk_numb; numb++){
        err = sdcard_read_single_block(read_data_buf, start_blk_addr, 
            SD_DEFAULT_BLOCK_SIZE);
        if(err!=SD_OK){
            SDCARD_FATAL("sdcard_read err:%d, curblk:0x%x\r\n",err, start_blk_addr);
            return (UINT32)err;
        }
            
        start_blk_addr++;
        read_data_buf += SD_DEFAULT_BLOCK_SIZE;
    }
        
    return (UINT32)SD_OK;    
}

UINT32 sdcard_write(char *user_buf, UINT32 count, UINT32 op_flag)
{
    SDIO_Error err = SD_OK;
    UINT32 start_blk_addr;
    UINT8  write_blk_numb, numb;
    UINT8* write_data_buf;

    // check operate parameter
    start_blk_addr = op_flag;
    write_blk_numb = (UINT8)count;
    write_data_buf = (UINT8*)user_buf;

    for(numb=0; numb<write_blk_numb; numb++){
        err = sdcard_write_single_block(write_data_buf, start_blk_addr, 
            SD_DEFAULT_BLOCK_SIZE);
        if(err!=SD_OK){
            SDCARD_FATAL("sdcard_write err:%d, curblk:0x%x\r\n",err, start_blk_addr);
            return (UINT32)err;
        }
            
        start_blk_addr++;
        write_data_buf += SD_DEFAULT_BLOCK_SIZE;
    }
        
    return (UINT32)SD_OK;  
}

UINT32 sdcard_ctrl(UINT32 cmd, void *parm)
{
	switch(cmd)
	{
		default:
			break;
	}
	
    return 0;
}

#endif  // CFG_USE_SDCARD_HOST
// EOF
