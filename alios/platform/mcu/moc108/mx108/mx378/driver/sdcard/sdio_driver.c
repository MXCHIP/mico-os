#include "include.h"
#include "arm_arch.h"

#if CFG_USE_SDCARD_HOST

#include "sdio_driver.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "mem_pub.h"
#include "icu_pub.h"
#include "gpio_pub.h"

/******************************************************************************/
/**************************** platform function *******************************/
/******************************************************************************/
static void 
beken_sdcard_gpio_configuration(void)
{
    UINT32 param;
    
	param = GFUNC_MODE_SD_HOST;
	sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
}

static void 
beken_sdcard_clk_configuration(void)
{
    UINT32 param;
    
    param = PWD_SDIO_CLK_BIT;
	sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &param);
}

static void 
beken_sdcard_set_clk_div(UINT8 clkdiv)
{
    UINT32 reg;
    
    reg = REG_READ(REG_SDCARD_FIFO_THRESHOLD);
    reg &= ~(SDCARD_FIFO_SD_RATE_SELECT_MASK << SDCARD_FIFO_SD_RATE_SELECT_POSI);
    reg |= ((clkdiv & SDCARD_FIFO_SD_RATE_SELECT_MASK)
        << SDCARD_FIFO_SD_RATE_SELECT_POSI);
    REG_WRITE(REG_SDCARD_FIFO_THRESHOLD, reg);    
}

static void 
beken_sdcard_register_reset(void)
{
    UINT32 reg;

    /* Clear cmd rsp int bit */
    reg = REG_READ(REG_SDCARD_CMD_RSP_INT_SEL);
    REG_WRITE(REG_SDCARD_CMD_RSP_INT_SEL, reg);

    /* Clear tx/rx fifo */
    reg = SDCARD_FIFO_RX_FIFO_RST | SDCARD_FIFO_TX_FIFO_RST;
    REG_WRITE(REG_SDCARD_FIFO_THRESHOLD, reg);

    /* Disabe all sdio interrupt */
    reg = 0;
    REG_WRITE(REG_SDCARD_CMD_RSP_INT_MASK, reg);

    /* Config tx/rx fifo threshold */
    reg = ((SDCARD_RX_FIFO_THRD&SDCARD_FIFO_RX_FIFO_THRESHOLD_MASK) 
                << SDCARD_FIFO_RX_FIFO_THRESHOLD_POSI)
         |((SDCARD_TX_FIFO_THRD&SDCARD_FIFO_TX_FIFO_THRESHOLD_MASK)
                << SDCARD_FIFO_TX_FIFO_THRESHOLD_POSI);
    REG_WRITE(REG_SDCARD_FIFO_THRESHOLD, reg);
}


static void 
beken_sdcard_send_cmd( UINT8 cmd_index, UINT32 flag, 
            UINT32 timeout, VOID *arg )
{
    UINT32 reg;
    flag &= CMD_FLAG_MASK;

    reg = (UINT32)arg;
    REG_WRITE(REG_SDCARD_CMD_SEND_AGUMENT, reg);

    reg = timeout;
    REG_WRITE(REG_SDCARD_CMD_RSP_TIMER, reg);

    reg = ((((UINT32)cmd_index)&SDCARD_CMD_SEND_CTRL_CMD_INDEX_MASK)
                <<SDCARD_CMD_SEND_CTRL_CMD_INDEX_POSI)
        |((flag&SDCARD_CMD_SEND_CTRL_CMD_FLAGS_MASK)
                <<SDCARD_CMD_SEND_CTRL_CMD_FLAGS_POSI)
        | SDCARD_CMD_SEND_CTRL_CMD_START;
    REG_WRITE(REG_SDCARD_CMD_SEND_CTRL, reg);

}

static SDIO_Error 
beken_wait_cmd_response(void)
{
    UINT32 reg;
    while(1){
        reg = REG_READ(REG_SDCARD_CMD_RSP_INT_SEL);
        if(reg & (SDCARD_CMDRSP_NORSP_END_INT
                   | SDCARD_CMDRSP_RSP_END_INT
                   | SDCARD_CMDRSP_TIMEOUT_INT)
                  ){
            break;
        }
    }

    REG_WRITE(REG_SDCARD_CMD_RSP_INT_SEL, SD_CMD_RSP);//clear the int flag
    if(reg&SDCARD_CMDRSP_TIMEOUT_INT){
        SDCARD_WARN("sdcard cmd timeout,cmdresp_int_reg:0x%x\r\n", reg);
        return SD_CMD_RSP_TIMEOUT;
    }
    if(reg&SDCARD_CMDRSP_CMD_CRC_FAIL){
        SDCARD_WARN("sdcard cmd crcfail,cmdresp_int_reg:0x%x\r\n", reg);
        return SD_CMD_CRC_FAIL;
    }
    return SD_OK;
}

static void 
beken_sdcard_set_host_buswidth_4line(void)
{

}

static void 
beken_sdcard_set_host_buswidth_1line(void)
{

}

static void 
beken_sdcard_set_host_datactrl_reg(UINT32 data_dir, 
        UINT32 byte_len, UINT32 timeout)
{
    UINT32 reg;

    reg = 0x3ffff; // clear fifo
    REG_WRITE(REG_SDCARD_FIFO_THRESHOLD, reg);

    reg = timeout;
    REG_WRITE(REG_SDCARD_DATA_REC_TIMER, reg);

    if(data_dir == SD_DATA_DIR_WR)
        reg = SDCARD_DATA_REC_CTRL_DATA_WR_DATA_EN;
    else
        reg = 0;

    reg |= SDCARD_DATA_REC_CTRL_DATA_BYTE_SEL
          |((byte_len&SDCARD_DATA_REC_CTRL_BLK_SIZE_MASK)
             <<SDCARD_DATA_REC_CTRL_BLK_SIZE_POSI)
#if CONFIG_SDCARD_BUSWIDTH_4LINE         
          | SDCARD_DATA_REC_CTRL_DATA_BUS
#endif
          | SDCARD_DATA_REC_CTRL_DATA_EN;
    
    REG_WRITE(REG_SDCARD_DATA_REC_CTRL, reg);
}

static SDIO_Error 
beken_sdcard_wait_receive_data(UINT32* receive_buf)
{
    UINT32 reg, i;
    while(1){
        reg = REG_READ(REG_SDCARD_CMD_RSP_INT_SEL);
        if(reg & (SDCARD_CMDRSP_DATA_REC_END_INT
                   | SDCARD_CMDRSP_DATA_CRC_FAIL
                   | SDCARD_CMDRSP_DATA_TIME_OUT_INT)){
            break;
        }
    }

    REG_WRITE(REG_SDCARD_CMD_RSP_INT_SEL, SD_DATA_RSP);//clear the int flag
    if(reg&SDCARD_CMDRSP_DATA_TIME_OUT_INT){
        return SD_DATA_TIMEOUT;
    }
    if(reg&SDCARD_CMDRSP_DATA_CRC_FAIL){
        SDCARD_WARN("sdcard data crcfail,cmdresp_int_reg:0x%x\r\n", reg);
        return SD_DATA_CRC_FAIL;
    }

    for (i = 0; i < (SD_DEFAULT_BLOCK_SIZE >> 2); i++){
        int tmp = 0;
        reg = REG_READ(REG_SDCARD_FIFO_THRESHOLD);
        /* wait fifo data valid */
        while(!(reg&SDCARD_FIFO_RXFIFO_RD_READY)){
            //avoid dead loop
            tmp++;
            if(tmp > 0x20)
                break;
        }
        *(receive_buf + i) = REG_READ(REG_SDCARD_RD_DATA_ADDR);
	}

    return SD_OK;
}

static SDIO_Error 
beken_sdcard_wait_write_data(UINT32* write_buf)
{
    UINT32 bytestransferred = 0, restwords, count;
    UINT32 reg;
    while(1){
        reg = REG_READ(REG_SDCARD_CMD_RSP_INT_SEL);
        if(reg & (SDCARD_CMDRSP_DATA_WR_END_INT
                   | SDCARD_CMDRSP_DATA_CRC_FAIL
                   | SDCARD_CMDRSP_DATA_TIME_OUT_INT)){
            break;
        }
        reg = REG_READ(REG_SDCARD_FIFO_THRESHOLD);
        if (reg&SDCARD_FIFO_TXFIFO_WR_READY){
            if ((SD_DEFAULT_BLOCK_SIZE - bytestransferred) < 32){
                restwords = ((SD_DEFAULT_BLOCK_SIZE - bytestransferred) % 4 == 0) ? 
                    ((SD_DEFAULT_BLOCK_SIZE - bytestransferred) / 4) 
                  : (( SD_DEFAULT_BLOCK_SIZE -  bytestransferred) / 4 + 1);
                for (count = 0; count < restwords; 
                     count++, write_buf++, bytestransferred += 4){
                        REG_WRITE(REG_SDCARD_WR_DATA_ADDR, *write_buf);
                }
            }else{
                for (count = 0; count < 8; count++){
                    REG_WRITE(REG_SDCARD_WR_DATA_ADDR, *(write_buf + count));
                }
                write_buf += 8;
                bytestransferred += 32;
            }
        }
    }

    REG_WRITE(REG_SDCARD_CMD_RSP_INT_SEL, SD_DATA_RSP);//clear the int flag
    if(reg&SDCARD_CMDRSP_DATA_TIME_OUT_INT){
        return SD_DATA_TIMEOUT;
    }
    if(reg&SDCARD_CMDRSP_DATA_CRC_FAIL){
        SDCARD_WARN("sdcard data crcfail,cmdresp_int_reg:0x%x\r\n", reg);
        return SD_DATA_CRC_FAIL;
    }

    return SD_OK;
}

/******************************************************************************/
/**************************** interface function ******************************/
/******************************************************************************/
void sdio_set_low_clk(void)
{
    beken_sdcard_set_clk_div(CLK_200K);
}

void sdio_set_high_clk(void)
{
    beken_sdcard_set_clk_div(CLK_13M);
}

void sdio_gpio_config(void)
{
    beken_sdcard_gpio_configuration();
}

void sdio_clk_config(void)
{
    beken_sdcard_clk_configuration();
}

void sdio_register_reset(void)
{
    beken_sdcard_register_reset();
}

void sdio_sendcmd_function( UINT8 cmd_index, UINT32 flag, 
        UINT32 timeout, VOID *arg )
{
    beken_sdcard_send_cmd(cmd_index, flag, timeout, arg);
}

SDIO_Error sdio_wait_cmd_response(void)
{
    return beken_wait_cmd_response();
}

void sdio_get_cmdresponse_argument(UINT8 num, UINT32* resp)
{
    switch(num)
    {
        case 0:
            *resp = REG_READ(REG_SDCARD_CMD_RSP_AGUMENT0); 
            break;
        case 1:
            *resp = REG_READ(REG_SDCARD_CMD_RSP_AGUMENT1);
            break;  
        case 2:
            *resp = REG_READ(REG_SDCARD_CMD_RSP_AGUMENT2);
            break;
        case 3:
            *resp = REG_READ(REG_SDCARD_CMD_RSP_AGUMENT3);
            break;  
        default:
            break;
    }  
}

void sdio_setup_data(UINT32 data_dir, UINT32 byte_len, UINT8 *buf)
{
    UINT32 timeout = DEF_DATA_TIME_OUT;
    buf = buf;
    beken_sdcard_set_host_datactrl_reg(data_dir, byte_len, timeout);
}

void sdcard_set_host_buswidth_4line(void)
{
    beken_sdcard_set_host_buswidth_4line();
}

void sdcard_set_host_buswidth_1line(void)
{
    beken_sdcard_set_host_buswidth_1line();
}

SDIO_Error sdcard_wait_receive_data(UINT32* receive_buf)
{
    return beken_sdcard_wait_receive_data(receive_buf);
}

SDIO_Error sdcard_wait_write_data(UINT32* write_buf)
{
    return beken_sdcard_wait_write_data(write_buf);
}

#endif // CFG_USE_SDCARD_HOST
// EOF

