#ifndef _GPIO_H_
#define _GPIO_H_


#define GPIO_PRT                        os_printf
#define WARN_PRT                        os_printf
#define FATAL_PRT                       os_printf

#ifdef CONFIG_TRACE32_ENABLE
#define JTAG_GPIO_FILTER
#endif


#define GPIO_INIT_FLAG                   ((UINT32)1)
#define GPIO_UNINIT_FLAG                 ((UINT32)-1)

#define GPIO_BASE_ADDR                       (0x0802800)

#define REG_GPIO_0_CONFIG                    (GPIO_BASE_ADDR + 0*4)
#define REG_GPIO_1_CONFIG                    (GPIO_BASE_ADDR + 1*4)
#define REG_GPIO_2_CONFIG                    (GPIO_BASE_ADDR + 2*4)
#define REG_GPIO_3_CONFIG                    (GPIO_BASE_ADDR + 3*4)
#define REG_GPIO_4_CONFIG                    (GPIO_BASE_ADDR + 4*4)
#define REG_GPIO_5_CONFIG                    (GPIO_BASE_ADDR + 5*4)
#define REG_GPIO_6_CONFIG                    (GPIO_BASE_ADDR + 6*4)
#define REG_GPIO_7_CONFIG                    (GPIO_BASE_ADDR + 7*4)
#define REG_GPIO_8_CONFIG                    (GPIO_BASE_ADDR + 8*4)
#define REG_GPIO_9_CONFIG                    (GPIO_BASE_ADDR + 9*4)
#define REG_GPIO_10_CONFIG                   (GPIO_BASE_ADDR + 10*4)
#define REG_GPIO_11_CONFIG                   (GPIO_BASE_ADDR + 11*4)
#define REG_GPIO_12_CONFIG                   (GPIO_BASE_ADDR + 12*4)
#define REG_GPIO_13_CONFIG                   (GPIO_BASE_ADDR + 13*4)
#define REG_GPIO_14_CONFIG                   (GPIO_BASE_ADDR + 14*4)
#define REG_GPIO_15_CONFIG                   (GPIO_BASE_ADDR + 15*4)
#define REG_GPIO_16_CONFIG                   (GPIO_BASE_ADDR + 16*4)
#define REG_GPIO_17_CONFIG                   (GPIO_BASE_ADDR + 17*4)
#define REG_GPIO_18_CONFIG                   (GPIO_BASE_ADDR + 18*4)
#define REG_GPIO_19_CONFIG                   (GPIO_BASE_ADDR + 19*4)
#define REG_GPIO_20_CONFIG                   (GPIO_BASE_ADDR + 20*4)
#define REG_GPIO_21_CONFIG                   (GPIO_BASE_ADDR + 21*4)
#define REG_GPIO_22_CONFIG                   (GPIO_BASE_ADDR + 22*4)
#define REG_GPIO_23_CONFIG                   (GPIO_BASE_ADDR + 23*4)
#define REG_GPIO_24_CONFIG                   (GPIO_BASE_ADDR + 24*4)
#define REG_GPIO_25_CONFIG                   (GPIO_BASE_ADDR + 25*4)
#define REG_GPIO_26_CONFIG                   (GPIO_BASE_ADDR + 26*4)
#define REG_GPIO_27_CONFIG                   (GPIO_BASE_ADDR + 27*4)
#define REG_GPIO_28_CONFIG                   (GPIO_BASE_ADDR + 28*4)
#define REG_GPIO_29_CONFIG                   (GPIO_BASE_ADDR + 29*4)
#define REG_GPIO_30_CONFIG                   (GPIO_BASE_ADDR + 30*4)
#define REG_GPIO_31_CONFIG                   (GPIO_BASE_ADDR + 31*4)

#define REG_GPIO_CFG_BASE_ADDR               (REG_GPIO_0_CONFIG)


#define GCFG_INPUT_POS                       0
#define GCFG_OUTPUT_POS                      1
#define GCFG_INPUT_ENABLE_POS                2
#define GCFG_OUTPUT_ENABLE_POS               3
#define GCFG_PULL_MODE_POS                   4
#define GCFG_PULL_ENABLE_POS                 5
#define GCFG_FUNCTION_ENABLE_POS             6
#define GCFG_INPUT_MONITOR_POS               7


#define GCFG_INPUT_BIT                       (1 << 0)
#define GCFG_OUTPUT_BIT                      (1 << 1)
#define GCFG_INPUT_ENABLE_BIT                (1 << 2)
#define GCFG_OUTPUT_ENABLE_BIT               (1 << 3)
#define GCFG_PULL_MODE_BIT                   (1 << 4)
#define GCFG_PULL_ENABLE_BIT                 (1 << 5)
#define GCFG_FUNCTION_ENABLE_BIT             (1 << 6)
#define GCFG_INPUT_MONITOR_BIT               (1 << 7)



#define REG_GPIO_FUNC_CFG                    (GPIO_BASE_ADDR + 32*4)
#define PERIAL_MODE_1                         (0)
#define PERIAL_MODE_2                         (1)

#define REG_GPIO_INTEN                       (GPIO_BASE_ADDR + 33*4)
#define REG_GPIO_INTLV0                      (GPIO_BASE_ADDR + 34*4)
#define REG_GPIO_INTLV1                      (GPIO_BASE_ADDR + 35*4)
#define REG_GPIO_INTSTA                      (GPIO_BASE_ADDR + 36*4)
#define REG_GPIO_DPLL_UNLOCK                 (GPIO_BASE_ADDR + 38*4)
#define REG_GPIO_DETECT                      (GPIO_BASE_ADDR + 39*4)
#define REG_GPIO_ENC_WORD                    (GPIO_BASE_ADDR + 40*4)
#define REG_GPIO_DBG_MSG                     (GPIO_BASE_ADDR + 41*4)
#define REG_GPIO_DBG_MUX                     (GPIO_BASE_ADDR + 42*4)
#define REG_GPIO_DBG_CFG                     (GPIO_BASE_ADDR + 43*4)
#define REG_GPIO_DBG_REPORT                  (GPIO_BASE_ADDR + 44*4)

#define REG_GPIO_MODULE_SELECT               (GPIO_BASE_ADDR + 45*4)
#define GPIO_MODUL_NONE                      0xff
#define GPIO_SD_DMA_MODULE                   (0 << 1)
#define GPIO_SD_HOST_MODULE                  (1 << 1)
#define GPIO_SPI_DMA_MODULE                  (0 << 0)
#define GPIO_SPI_MODULE                      (1 << 0)

#define REG_A0_CONFIG                        (GPIO_BASE_ADDR + 48*4)
#define REG_A1_CONFIG                        (GPIO_BASE_ADDR + 49*4)
#define REG_A2_CONFIG                        (GPIO_BASE_ADDR + 50*4)
#define REG_A3_CONFIG                        (GPIO_BASE_ADDR + 51*4)
#define REG_A4_CONFIG                        (GPIO_BASE_ADDR + 52*4)
#define REG_A5_CONFIG                        (GPIO_BASE_ADDR + 53*4)
#define REG_A6_CONFIG                        (GPIO_BASE_ADDR + 54*4)
#define REG_A7_CONFIG                        (GPIO_BASE_ADDR + 55*4)

UINT32 gpio_open(UINT32 op_flag);
UINT32 gpio_close(void);
UINT32 gpio_read(char *user_buf, UINT32 count, UINT32 op_flag);
UINT32 gpio_write(char *user_buf, UINT32 count, UINT32 op_flag);
UINT32 gpio_ctrl(UINT32 cmd, void *param);

void gpio_int_enable(UINT32 index, UINT32 mode, void (*p_Int_Handler)(unsigned char));
void gpio_int_disable(UINT32 index);
#endif // _GPIO_H_

// EOF

