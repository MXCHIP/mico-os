#include "include.h"
#include "arm_arch.h"

#include "flash.h"
#include "flash_pub.h"

#include "drv_model_pub.h"

#include <string.h>
#include "uart_pub.h"
#include "sys_ctrl_pub.h"

#define MODE_STD     0
#define MODE_DUAL    1
#define MODE_QUAD    2

static DD_OPERATIONS flash_op =
{
    NULL,
    NULL,
    flash_read,
    flash_write,
    flash_ctrl
};

static void flash_set_clk(UINT8 clk_conf)
{
    UINT32 value;

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(FLASH_CLK_CONF_MASK << FLASH_CLK_CONF_POSI);
    value |= (clk_conf << FLASH_CLK_CONF_POSI);
    REG_WRITE(REG_FLASH_CONF, value);
}

static void flash_write_enable(void)
{
    UINT32 value;

    value = (FLASH_OPCODE_WREN << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static void flash_write_disable(void)
{
    UINT32 value;

    value = (FLASH_OPCODE_WRDI << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static UINT16 flash_read_sr(void)
{
    UINT16 sr;
    UINT32 value;
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = (FLASH_OPCODE_RDSR2 << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_SR_DATA_CRC_CNT);
    sr = (value & 0xFF) << 8;

    value = (FLASH_OPCODE_RDSR << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_SR_DATA_CRC_CNT);
    sr |= (value & 0xFF);

    return sr;
}

static void flash_write_sr(UINT8 bytes,  UINT16 val)
{
    UINT32 value;
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(WRSR_DATA_MASK << WRSR_DATA_POSI);
    value |= (val << WRSR_DATA_POSI) | FWREN_FLASH_CPU;
    REG_WRITE(REG_FLASH_CONF, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    if(bytes == 1)
    {
        value = (FLASH_OPCODE_WRSR << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
        REG_WRITE(REG_FLASH_OPERATE_SW, value);
    }
    else if(bytes == 2)
    {
        value = (FLASH_OPCODE_WRSR2 << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
        REG_WRITE(REG_FLASH_OPERATE_SW, value);
    }

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static UINT8 flash_read_qe(void)
{
    UINT8 temp;
    UINT32 value;

    value = (FLASH_OPCODE_RDSR2 << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_SR_DATA_CRC_CNT);
    temp = (value & 0xFF);
    return temp;
}

static void flash_set_qe(void)
{
    UINT32 value;

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(WRSR_DATA_MASK << WRSR_DATA_POSI);
    value |= (0x200 << WRSR_DATA_POSI);
    REG_WRITE(REG_FLASH_CONF, value);

    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = (value & (ADDR_SW_REG_MASK << ADDR_SW_REG_POSI))
            | (FLASH_OPCODE_WRSR2 << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static void flash_set_qwfr(void)
{
    UINT32 value;

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(MODEL_SEL_MASK << MODEL_SEL_POSI);
    value |= (0x0A << MODEL_SEL_POSI);
    REG_WRITE(REG_FLASH_CONF, value);
}

static void flash_clr_qwfr(void)
{
    UINT32 value;

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(MODEL_SEL_MASK << MODEL_SEL_POSI);
    REG_WRITE(REG_FLASH_CONF, value);

    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = ((0 << ADDR_SW_REG_POSI)
             | (FLASH_OPCODE_CRMR << OP_TYPE_SW_POSI)
             | OP_SW
             | (value & WP_VALUE));
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static void flash_set_wsr(UINT16 data)
{
    UINT32 value;

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_CONF);
    value &= ~(WRSR_DATA_MASK << WRSR_DATA_POSI);
    value |= (data << WRSR_DATA_POSI);
    REG_WRITE(REG_FLASH_CONF, value);

    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = (value & (ADDR_SW_REG_MASK << ADDR_SW_REG_POSI))
            | (FLASH_OPCODE_WRSR2 << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static void flash_set_line_mode(UINT8 mode)
{
    if(1 == mode)
    {
        flash_clr_qwfr();
    }
    if(2 == mode)
    {
        UINT32 value;

        value = REG_READ(REG_FLASH_CONF);
        value &= ~(MODEL_SEL_MASK << MODEL_SEL_POSI);
        REG_WRITE(REG_FLASH_CONF, value);

        value |= ((MODE_DUAL & MODEL_SEL_MASK) << MODEL_SEL_POSI);
        REG_WRITE(REG_FLASH_CONF, value);
    }
    else if(4 == mode)
    {
        flash_set_qwfr();
    }
}

static UINT32 flash_get_id(void)
{
    UINT32 value;

    value = (FLASH_OPCODE_RDID << OP_TYPE_SW_POSI) | OP_SW | WP_VALUE;
    REG_WRITE(REG_FLASH_OPERATE_SW, value);

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    return (REG_READ(REG_FLASH_RDID_DATA_FLASH) >> 16);
}

static UINT32 flash_read_mid(void)
{
    UINT32 value;
    UINT32 flash_id;

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = ((value & (ADDR_SW_REG_MASK << ADDR_SW_REG_POSI))
             | (FLASH_OPCODE_RDID << OP_TYPE_SW_POSI)
             | OP_SW
             | (value & WP_VALUE));
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    flash_id = REG_READ(REG_FLASH_RDID_DATA_FLASH);

    return flash_id;
}

static void flash_erase_sector(UINT32 address)
{
    UINT32 value;
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    flash_set_line_mode(0);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = ((address << ADDR_SW_REG_POSI)
             | (FLASH_OPCODE_SE << OP_TYPE_SW_POSI)
             | OP_SW
             | (value & WP_VALUE));
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
    flash_set_line_mode(2);
}

static void flash_set_hpm(void)
{
    UINT32 value;

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
    value = REG_READ(REG_FLASH_OPERATE_SW);
    value = ((0x0 << ADDR_SW_REG_POSI)
             | (FLASH_OPCODE_HPM << OP_TYPE_SW_POSI)
             | (OP_SW)
             | (value & WP_VALUE));
    REG_WRITE(REG_FLASH_OPERATE_SW, value);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
}

static void flash_read_data(UINT8 *buffer, UINT32 address, UINT32 len)
{
    UINT32 i, reg_value;
    UINT32 addr = address & (~0x1F);
    UINT32 buf[8];
    UINT8 *pb = (UINT8 *)&buf[0];

    if(len == 0)
    {
        return;
    }

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
    while(len)
    {
        reg_value = REG_READ(REG_FLASH_OPERATE_SW);
        reg_value = ((addr << ADDR_SW_REG_POSI)
                     | (FLASH_OPCODE_READ << OP_TYPE_SW_POSI)
                     | OP_SW
                     | (reg_value & WP_VALUE));
        REG_WRITE(REG_FLASH_OPERATE_SW, reg_value);
        while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
        addr += 32;

        for(i = 0; i < 8; i++)
        {
            buf[i] = REG_READ(REG_FLASH_DATA_FLASH_SW);
        }

        for(i = address % 32; i < 32; i++)
        {
            *buffer++ = pb[i];
            address++;
            len--;
            if(len == 0)
            {
                break;
            }
        }
    }
}

static void flash_write_data(UINT8 *buffer, UINT32 address, UINT32 len)
{
    UINT32 i, reg_value;
    UINT32 addr = address & (~0x1F);
    UINT32 buf[8];
    UINT8 *pb = (UINT8 *)&buf[0];

    if(address % 32)
    {
        flash_read_data(pb, addr, 32);
    }
    else
    {
        memset(pb, 0xFF, 32);
    }

    //	flash_write_sr( 2, 0x002c );

    flash_set_line_mode(1);
    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
    while(len)
    {
        for (i = address % 32; i < 32; i++)
        {
            pb[i] = *buffer++;
            address++;
            len--;
            if (len == 0)
                break;
        }

        for (i = 0; i < 8; i++)
        {
            REG_WRITE(REG_FLASH_DATA_SW_FLASH, buf[i]);
        }

        reg_value = REG_READ(REG_FLASH_OPERATE_SW);
        reg_value = ((addr << ADDR_SW_REG_POSI)
                     | (FLASH_OPCODE_PP << OP_TYPE_SW_POSI)
                     | OP_SW
                     | (reg_value & WP_VALUE));
        REG_WRITE(REG_FLASH_OPERATE_SW, reg_value);
        while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);
        addr += 32;
        memset(pb, 0xFF, 32);
    }

    flash_set_line_mode(2);

    //	flash_write_sr( 2, 0x14 );


}

void flash_wp_256k(UINT32 flash_id )
{
    switch(flash_id)
    {
    case 0x51:               //MDxx
        FLASH_PRT("[Flash]Write Proection 256k of MDxx\r\n");
        flash_write_sr( 1, 0x18 );
        break;

    case 0xc8:              //QDxx ,
        FLASH_PRT("[Flash]Write Proection 256k of QDxx\r\n");
        flash_write_sr( 2, 0x002c );
        break;

    case 0xa1:              //FMXX
        FLASH_PRT("[Flash]Write Proection 256k of FMXX\r\n");
        flash_write_sr( 1, 0x18 );
        break;

    default:
        FLASH_PRT("[Flash]Write Proection 256k of xx\r\n");
        flash_write_sr( 2, 0x002c );
    }
}

void flash_wp_xxxk(UINT32 flash_id )
{
    switch(flash_id)
    {
    case 0x51:               //MDxx
        FLASH_PRT("[Flash]Write Proection 512k of MDxx\r\n");
        flash_write_sr( 1, 0x1c );
        break;
    case 0xc8:              //QDxx ,
        FLASH_PRT("[Flash]Write Proection 1024k of QDxx\r\n");
        flash_write_sr( 2, 0x14 ); // 768k 0x400C
        break;
    case 0xa1:              //FMXX
        FLASH_PRT("[Flash]Write Proection 512k of FMXX\r\n");
        flash_write_sr( 1, 0x1c );
        break;
    default:
        FLASH_PRT("[Flash]Write Proection 512k of xx\r\n");
        flash_write_sr( 2, 0x0030 );
    }
}

void flash_init(void)
{
    UINT32 id;

    while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW);

    id = flash_get_id();
    FLASH_PRT("[Flash]id:0x%x\r\n", id);

    //    flash_wp_xxxk(id);

    //#define ENABLE_4_LINE // if u enable 4 line mode, jtag can not halt cpu, when lost current at development board.
#define ENABLE_2_LINE

#ifdef ENABLE_4_LINE
    flash_set_qe();
    flash_set_line_mode(4);
    flash_set_clk(1);
#else
#ifdef ENABLE_2_LINE
    flash_set_line_mode(2);
    flash_set_clk(5);  // 60M
#endif
#endif

    ddev_register_dev(FLASH_DEV_NAME, &flash_op);
}

void flash_exit(void)
{
    ddev_unregister_dev(FLASH_DEV_NAME);
}

UINT32 flash_read(char *user_buf, UINT32 count, UINT32 address)
{
    flash_read_data((UINT8 *)user_buf, address, count);

    return FLASH_SUCCESS;
}

UINT32 flash_write(char *user_buf, UINT32 count, UINT32 address)
{
    flash_write_data((UINT8 *)user_buf, address, count);

    return FLASH_SUCCESS;
}

UINT32 flash_ctrl(UINT32 cmd, void *parm)
{
    UINT8 clk;
    UINT16 wsr;
    UINT32 address;
    UINT32 reg;
    UINT32 ret = FLASH_SUCCESS;

    switch(cmd)
    {
    case CMD_FLASH_SET_CLK:
        clk = (*(UINT8 *)parm);
        flash_set_clk(clk);
        break;

    case CMD_FLASH_SET_DPLL:
        ret = sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_SET_FLASH_DPLL, 0);
        ASSERT(DRV_FAILURE != ret);

        reg = REG_READ(REG_FLASH_CONF);
        reg &= ~(FLASH_CLK_CONF_MASK << FLASH_CLK_CONF_POSI);
        reg = reg | (5 << FLASH_CLK_CONF_POSI); /*dco--9*/
        REG_WRITE(REG_FLASH_CONF, reg);
        break;

    case CMD_FLASH_SET_DCO:
        ret = sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_SET_FLASH_DCO, 0);
        ASSERT(DRV_FAILURE != ret);

        reg = REG_READ(REG_FLASH_CONF);
        reg &= ~(FLASH_CLK_CONF_MASK << FLASH_CLK_CONF_POSI);
        reg = reg | (9 << FLASH_CLK_CONF_POSI);
        REG_WRITE(REG_FLASH_CONF, reg);

        REG_WRITE(REG_FLASH_DATA_FLASH_SW, *((volatile UINT32 *)0x20000));
        break;

    case CMD_FLASH_WRITE_ENABLE:
        flash_write_enable();
        break;

    case CMD_FLASH_WRITE_DISABLE:
        flash_write_disable();
        break;

    case CMD_FLASH_READ_SR:
        (*(UINT16 *)parm) = flash_read_sr();
        break;

    case CMD_FLASH_WRITE_SR:
        flash_write_sr(*(unsigned long *)parm & 0x00FF, ((*(unsigned long *)parm) >> 8) & 0x00FFFF);
        break;

    case CMD_FLASH_READ_QE:
        (*(UINT8 *)parm) = flash_read_qe();
        break;

    case CMD_FLASH_SET_QE:
        flash_set_qe();
        break;

    case CMD_FLASH_SET_QWFR:
        flash_set_qwfr();
        break;

    case CMD_FLASH_CLR_QWFR:
        flash_clr_qwfr();
        break;

    case CMD_FLASH_SET_WSR:
        wsr = (*(UINT16 *)parm);
        flash_set_wsr(wsr);
        break;

    case CMD_FLASH_GET_ID:
        (*(UINT32 *)parm) = flash_get_id();
        break;

    case CMD_FLASH_READ_MID:
        (*(UINT32 *)parm) = flash_read_mid();
        break;

    case CMD_FLASH_ERASE_SECTOR:
        address = (*(UINT32 *)parm);
        flash_erase_sector(address);
        break;

    case CMD_FLASH_SET_HPM:
        flash_set_hpm();
        break;

    default:
        ret = FLASH_FAILURE;
        break;
    }

    return ret;
}
// eof

