#include "include.h"
#include "sk_intf.h"
#include "main_none.h"
#include "eloop.h"

#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

#include "uart_pub.h"

SOCKET mgmt_get_socket_num(void)
{
	return SK_INTF_MGMT_SOCKET_NUM;
}

SOCKET ioctl_get_socket_num(void)
{
	return SK_INTF_IOCTL_SOCKET_NUM;
}

SOCKET data_get_socket_num(void)
{
	return SK_INTF_DATA_SOCKET_NUM;
}

int ke_mgmt_packet_tx(unsigned char *buf, int len, int flag)
{
	int ret;
	SOCKET sk = mgmt_get_socket_num();

	ret = ke_sk_send(sk, buf, len, flag);

#if CFG_WIFI_AP_MODE
	wpa_supplicant_poll(0);
#endif
	
	return ret;
}

int ke_mgmt_packet_rx(unsigned char *buf, int len, int flag)
{
	SOCKET sk = mgmt_get_socket_num();
	
	return ke_sk_recv(sk, buf, len, flag);
}

int ke_mgmt_peek_txed_next_payload_size(void)
{
	SOCKET sk = mgmt_get_socket_num();

	return ke_sk_send_peek_next_payload_size(sk);
}

int ke_mgmt_peek_rxed_next_payload_size(void)
{
	SOCKET sk = mgmt_get_socket_num();

	return ke_sk_recv_peek_next_payload_size(sk);
}

int ke_l2_packet_tx(unsigned char *buf, int len, int flag)
{
	int ret;
	SOCKET sk = data_get_socket_num();

	ret = ke_sk_send(sk, buf, len, flag);

#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
	{
		wpa_supplicant_poll(0);
	}
#endif

#if CFG_WIFI_STATION_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
#endif
	{
		wpa_supplicant_poll(0);
	}
#endif

	return ret;
}

int ke_l2_packet_rx(unsigned char *buf, int len, int flag)
{
	SOCKET sk = data_get_socket_num();
	
	return ke_sk_recv(sk, buf, len, flag);
}

int ke_data_peek_txed_next_payload_size(void)
{
	SOCKET sk = data_get_socket_num();

	return ke_sk_send_peek_next_payload_size(sk);
}

int ke_data_peek_rxed_next_payload_size(void)
{
	SOCKET sk = data_get_socket_num();

	return ke_sk_recv_peek_next_payload_size(sk);
}

int ws_mgmt_peek_rxed_next_payload_size(void)
{
	SOCKET sk = SK_INTF_MGMT_SOCKET_NUM;

	return socket_peek_recv_next_payload_size(sk);
}

int ws_get_mgmt_packet(unsigned char *buf, int len, int flag)
{
	SOCKET sk;

	sk = SK_INTF_MGMT_SOCKET_NUM;
	
	return recv(sk, buf, len, flag);
}

int ws_data_peek_rxed_next_payload_size(void)
{
	SOCKET sk = SK_INTF_DATA_SOCKET_NUM;

	return socket_peek_recv_next_payload_size(sk);
}

int ws_get_data_packet(unsigned char *buf, int len, int flag)
{
	SOCKET sk;

	sk = SK_INTF_DATA_SOCKET_NUM;
	
	return recv(sk, buf, len, flag);
}

// eof

