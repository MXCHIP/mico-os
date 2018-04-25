#ifndef _USB_H_
#define _USB_H_

#define USB_DEBUG

#ifdef USB_DEBUG
    #define USB_PRT      os_printf
	#define USB_WARN     warning_prf
	#define USB_FATAL    fatal_prf
#else
    #define USB_PRT      null_prf
	#define USB_WARN     null_prf
	#define USB_FATAL    null_prf
#endif

#define USB_DPLL_DIVISION                 (2)


typedef volatile unsigned char *VUINT8_PTR;
typedef volatile unsigned long *VUINT32_PTR;

#define USB_BASE_ADDR           (0x00804000)


#define VREG_USB_FADDR          (*((VUINT8_PTR) (USB_BASE_ADDR + 0x00))) 
#define VREG_USB_POWER          (*((VUINT8_PTR) (USB_BASE_ADDR + 0x01))) 
#define VREG_USB_INTRTX1        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x02))) 
#define VREG_USB_INTRTX2        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x03))) 
#define VREG_USB_INTRRX1        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x04))) 
#define VREG_USB_INTRRX2        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x05))) 
#define VREG_USB_INTRTX1E       (*((VUINT8_PTR) (USB_BASE_ADDR + 0x06))) 
#define VREG_USB_INTRTX2E       (*((VUINT8_PTR) (USB_BASE_ADDR + 0x07))) 
#define VREG_USB_INTRRX1E       (*((VUINT8_PTR) (USB_BASE_ADDR + 0x08))) 
#define VREG_USB_INTRRX2E       (*((VUINT8_PTR) (USB_BASE_ADDR + 0x09))) 
#define VREG_USB_INTRUSB        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0A))) 
#define VREG_USB_INTRUSBE       (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0B))) 
#define VREG_USB_FRAME1         (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0C))) 
#define VREG_USB_FRAME2         (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0D))) 
#define VREG_USB_INDEX          (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0E))) 
#define REG_USB_TESTMODE        (*((VUINT8_PTR) (USB_BASE_ADDR + 0x0F))) 

#define VREG_USB_DEVCTL         (*((VUINT8_PTR) (USB_BASE_ADDR + 0x60))) 

#define VREG_USB_RAM_PROTECT   (*((VUINT32_PTR) (USB_BASE_ADDR + 0x300)))
#define USB_RAM_PROTECT_BIT                    (1 << 0)

#define VREG_USB_DIG_MODE      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x304)))
#define USB_DIGITAL_MODE_BIT                   (1 << 0)

#define VREG_USB_BYPASS        (*((VUINT32_PTR) (USB_BASE_ADDR + 0x308)))
#define BYPASS_OTG_DISABLE0_BIT                     (1 << 0)
#define BYPASS_DM_DATA0_BIT                         (1 << 1)
#define BYPASS_DM_EN0_BIT                           (1 << 2)
#define BYPASS_SEL0_BIT                             (1 << 3)

#define VREG_USB_TEST_MODE     (*((VUINT32_PTR) (USB_BASE_ADDR + 0x30C)))
#define USB_TEST_MODE_BIT                   (1 << 1)
#define USB_COMMON_ONN_BIT                  (1 << 0)

#define VREG_USB_PHY_REG0      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x310)))
#define USB_PHY_REG0_DEFAULT_VALUE          (0xE0078518)

#define VREG_USB_PHY_REG1      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x314)))
#define USB_PHY_REG1_DEFAULT_VALUE          (0x020002a7)

#define VREG_USB_PHY_REG2      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x318)))
#define USB_PHY_REG2_DEFAULT_VALUE          (0x45755556)

#define VREG_USB_PHY_REG3      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x31C)))
#define USB_PHY_REG3_DEFAULT_VALUE          (0x68c00005)

#define VREG_USB_PHY_REG4      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x320)))
#define USB_PHY_REG4_DEFAULT_VALUE          (0x420)

#define VREG_USB_PHY_REG5      (*((VUINT32_PTR) (USB_BASE_ADDR + 0x324)))
#define USB_PHY_REG5_DEFAULT_VALUE          (0x0)

#define VREG_USB_TEST_CTRL     (*((VUINT32_PTR) (USB_BASE_ADDR + 0x328))


/*******************************************************************************
* Function Declarations
*******************************************************************************/
extern UINT32 usb_open (UINT32 op_flag);
extern UINT32 usb_close (void);
extern UINT32 usb_read (char *user_buf, UINT32 count, UINT32 op_flag);
extern UINT32 usb_write (char *user_buf, UINT32 count, UINT32 op_flag);
extern UINT32 usb_ctrl(UINT32 cmd, void *param);
extern void usb_isr(void);
extern void usb_event_post(void);

#endif //_USB_H_ 
// eof 

