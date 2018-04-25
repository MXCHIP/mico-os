#ifndef _POWER_SAVE_H_
#define _POWER_SAVE_H_

#define PS_DEBUG

#ifdef PS_DEBUG
#define PS_PRT                 os_printf
#define PS_WPRT                warning_prf
#define STATIC 
#else
#define PS_PRT                 os_null_printf
#define PS_WPRT                os_null_printf
#define STATIC                   static
#endif
extern void psave_wakeup_isr(void);

#endif // _POWER_SAVE_H_
// eof

