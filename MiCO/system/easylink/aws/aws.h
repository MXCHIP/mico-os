
#ifndef _AWS_H_
#define _AWS_H_

typedef void (*notify_ap_up_callback)           (void);

OSStatus micoWlanStartAws(int seconds);
OSStatus micoWlanStopAws(void);
OSStatus mico_wlan_aws_uap_start(int inTimeout, char *ssid, char *key, int channel,notify_ap_up_callback fn);

#endif
