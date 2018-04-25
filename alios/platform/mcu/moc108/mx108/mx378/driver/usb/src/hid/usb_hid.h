#ifndef _USB_HID_H_
#define _USB_HID_H_

#define UHID_DEBUG  

#ifdef UHID_DEBUG
#define STATIC        
#else
#define STATIC             static
#endif // UHID_DEBUG

/*******************************************************************************
* Function Declarations
*******************************************************************************/
extern int usb_sw_init(void);
extern int usb_sw_uninit(void);

#endif // _USB_HID_H_

// EOF
