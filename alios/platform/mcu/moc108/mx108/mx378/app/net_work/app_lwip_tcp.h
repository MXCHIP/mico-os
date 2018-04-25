#ifndef _APP_LWIP_TCP_H_
#define _APP_LWIP_TCP_H_

void app_lwip_tcp_connect_to_servicer(void);
int app_lwip_tcp_send_packet(UINT8 *data, UINT32 len);

#endif // _APP_LWIP_TCP_H_

