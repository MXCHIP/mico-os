#ifndef _USB_MSD_H_
#define _USB_MSD_H_

#define UMSD_DEBUG  

#ifdef UMSD_DEBUG
#define STATIC        
#else
#define STATIC             static
#endif // UMSD_DEBUG

/*******************************************************************************
* Function Declarations
*******************************************************************************/
extern int usb_sw_init(void);
extern int usb_sw_uninit(void);

#endif // _USB_MSD_H_

// EOF
