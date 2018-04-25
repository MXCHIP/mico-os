#include "include.h"
#include "ddrv.h"

#include "sk_intf.h"
#include "ieee802_11_defs.h"
#include "driver_beken.h"
#include "hostapd_intf_pub.h"

int ioctl_host_ap(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	switch(cmd)
	{
		case PRISM2_IOCTL_HOSTAPD:
			ret = hapd_intf_ioctl(arg);
			break;
			
		case PRISM2_IOCTL_PRISM2_PARAM:
			break;
			
		case SIOCSIWESSID:
			break;
			
		case SIOCGIWRANGE:
			break;
			
		case SIOCSIWFREQ:
			break;

		default:
			break;
	}
	
	return ret;
}

int ioctl(int dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	
	if(ioctl_get_socket_num() == dev)
	{
		ret = ioctl_host_ap(cmd, arg);
	}
	
	return ret;
}

// eof

