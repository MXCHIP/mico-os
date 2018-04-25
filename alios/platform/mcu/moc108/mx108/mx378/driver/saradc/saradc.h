#ifndef _SARADC_H_
#define _SARADC_H_

#define SARADC_BASE                     (0x00802C00)
#define SARADC_ADC_CONFIG               (SARADC_BASE + 0 * 4)
#define SARADC_ADC_MODE_POSI            (0)
#define SARADC_ADC_MODE_MASK            (0x03)
#define SARADC_ADC_CHNL_EN              (0x01UL << 2)
#define SARADC_ADC_CHNL_POSI            (3)
#define SARADC_ADC_CHNL_MASK            (0x07)
#define SARADC_ADC_FIFO_FULL           (0x01UL << 29)
#define SARADC_ADC_FIFO_EMPTY           (0x01UL << 28)
#define SARADC_ADC_BUSY                 (0x01UL << 27)
#define SARADC_ADC_SAMPLE_RATE_POSI     (16)
#define SARADC_ADC_SAMPLE_RATE_MASK     (0x00FF)
#define SARADC_ADC_DELAY_CLK_POSI       (6)
#define SARADC_ADC_SETTING              (0x01UL << 6)
#define SARADC_ADC_INT_CLR              (0x01UL << 7)
#define SARADC_ADC_PRE_DIV_POSI        (8)
#define SARADC_ADC_PRE_DIV_MASK        (0x00FF)

#define SARADC_ADC_DATA                 (SARADC_BASE + 1 * 4)

/*******************************************************************************
* Function Declarations
*******************************************************************************/
static UINT32 saradc_open(UINT32 op_flag);
static UINT32 saradc_close(void);
static UINT32 saradc_ctrl(UINT32 cmd, void *param);
#endif //_SARADC_H_
