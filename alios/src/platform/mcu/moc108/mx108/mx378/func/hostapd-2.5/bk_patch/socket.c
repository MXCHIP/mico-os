#include "include.h"
#include "socket.h"
#include "mem_pub.h"
#include "uart_pub.h"
#include "hostapd_intf_pub.h"
#include "ll.h"

static SOCKET_ENTITY socket_entity = {
	DL_LIST_HEAD_INIT(socket_entity.sk_head)
};

#if 1
int ke_sk_send(SOCKET sk, const unsigned char *buf, int len, int flag)
{
	int ret = 0;
	unsigned char *data_buf;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg;
	GLOBAL_INT_DECLARATION();

	SK_PRT("ke_tx:%d,buf:0x%x, len:%d\r\n", sk, buf, len);
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto tx_exit;
	}

	sk_msg = (SOCKET_MSG *)os_malloc(sizeof(SOCKET_MSG));
	if(0 == sk_msg)
	{
		goto tx_exit;
	}
	data_buf = (unsigned char *)os_malloc(len); 
	if(0 == data_buf)
	{
		goto malloc_buf_exit;
	}
	ret = len;
	sk_msg->len = len;
	sk_msg->msg = data_buf;
	
	os_memcpy(sk_msg->msg, buf, len);

	GLOBAL_INT_DISABLE();
	dl_list_add_tail(&element->sk_rx_msg, &sk_msg->data);
	GLOBAL_INT_RESTORE();
	
	return ret;
	
malloc_buf_exit:
	os_free(sk_msg);
	
tx_exit:	
	return ret;
}

int ke_sk_recv(SOCKET sk, const unsigned char *buf, int len, int flag)
{
	int count;
	int ret = 0;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();
	
	SK_PRT("ke_rx:%d,buf:0x%x, len:%d\r\n", sk, buf, len);
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto rx_exit;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_tx_msg, SOCKET_MSG, data)
	{
		count = MIN(sk_msg->len, len);
		
		ASSERT(count);
		ASSERT(sk_msg);
		SK_PRT("r1:%d,buf:0x%x, len:%d\r\n", sk, buf, count);
		os_memcpy((void *)buf, (void *)sk_msg->msg, count);

		ret = count;
		
		os_free(sk_msg->msg);
		sk_msg->msg = 0;
		sk_msg->len = 0;

		dl_list_del(&sk_msg->data);
		os_free(sk_msg);
		sk_msg = 0;
		
		break;
	}

	GLOBAL_INT_RESTORE();
	return ret;
	
rx_exit:
	return 0;
}

int ke_sk_recv_peek_next_payload_size(SOCKET sk)
{
	int ret = 0;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();
	
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto rx_exit;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_tx_msg, SOCKET_MSG, data)
	{
		ret = sk_msg->len;
		break;
	}

	GLOBAL_INT_RESTORE();
	
rx_exit:
	return ret;
}

int ke_sk_send_peek_next_payload_size(SOCKET sk)
{
	int ret = 0;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();
	
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto rx_exit;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_rx_msg, SOCKET_MSG, data)
	{
		ret = sk_msg->len;
		break;
	}

	GLOBAL_INT_RESTORE();
	
rx_exit:
	return ret;
}
#endif

#if 2
BK_SOCKET *sk_get_sk_element(SOCKET sk)
{
	BK_SOCKET *tmp;
	BK_SOCKET *bk_sk_ptr;
	
	dl_list_for_each_safe(bk_sk_ptr, tmp, &socket_entity.sk_head, BK_SOCKET, sk_element)
	{
		if(sk == bk_sk_ptr->sk)
		{
			return bk_sk_ptr;
		}
	}
	
	return 0;
}

SOCKET bk_socket(int af, int type, int protocol)
{
	SOCKET sk;
	BK_SOCKET *sk_ptr;
	GLOBAL_INT_DECLARATION();

	sk = af + type + protocol;
	ASSERT(0 ==  sk_get_sk_element(sk));
	
	sk_ptr = (BK_SOCKET *)os_malloc(sizeof(BK_SOCKET));
	if(0 == sk_ptr)
	{
		SK_PRT("create socket unexceptionally\r\n");
		return 0;
	}
	
	sk_ptr->sk = sk;
	
	GLOBAL_INT_DISABLE();
	dl_list_init(&sk_ptr->sk_rx_msg);
	dl_list_init(&sk_ptr->sk_tx_msg);
	
	dl_list_add(&socket_entity.sk_head, &sk_ptr->sk_element);
	GLOBAL_INT_RESTORE();
	
	SK_PRT("create socket:%d\r\n", sk);
	return sk;
}

int bk_send(SOCKET sk, const unsigned char *buf, int len, int flag)
{
	int ret = 0;
	unsigned char *data_buf;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg;
	GLOBAL_INT_DECLARATION();

	SK_PRT("hapd_tx:%d,buf:0x%x, len:%d\r\n", sk, buf, len);
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto tx_exit;
	}

	sk_msg = (SOCKET_MSG *)os_malloc(sizeof(SOCKET_MSG));
	if(0 == sk_msg)
	{
		goto tx_exit;
	}
	
	data_buf = (unsigned char *)os_malloc(len); 
	if(0 == data_buf)
	{
		goto malloc_buf_exit;
	}
	ret = len;
	sk_msg->len = len;
	sk_msg->msg = data_buf;
	
	os_memcpy(sk_msg->msg, buf, len);

	GLOBAL_INT_DISABLE();
	dl_list_add_tail(&element->sk_tx_msg, &sk_msg->data);
	GLOBAL_INT_RESTORE();

	bmsg_skt_tx_sender(flag);
	
	return ret;
	
malloc_buf_exit:
	os_free(sk_msg);
	
tx_exit:	
	return ret;
}

int bk_recv(SOCKET sk, const unsigned char *buf, int len, int flag)
{
	int count;
	int ret = 0;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();
	
	SK_PRT("hapd_rx:%d,buf:0x%x, len:%d\r\n", sk, buf, len);
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto rx_exit;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_rx_msg, SOCKET_MSG, data)
	{
		if(sk_msg->len > len)
		{
			SK_WPRT("recv_buf_small\r\n");
		}
		ASSERT(len > sk_msg->len);
		
		count = MIN(sk_msg->len, len);
		
		ASSERT(count);
		ASSERT(sk_msg);
		
		os_memcpy((void *)buf, (void *)sk_msg->msg, count);
		ret = count;
		
		os_free(sk_msg->msg);
		sk_msg->msg = 0;
		sk_msg->len = 0;

		dl_list_del(&sk_msg->data);
		os_free(sk_msg);
		sk_msg = 0;
		
		break;
	}
	GLOBAL_INT_RESTORE();
	
	return ret;
	
rx_exit:
	return 0;
}

void bk_close(SOCKET sk)
{
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();

	SK_PRT("close_sk:%d\r\n", sk);
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		return;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_tx_msg, SOCKET_MSG, data)
	{
		dl_list_del(&sk_msg->data);
		
		if(sk_msg->msg)
		{
			os_free(sk_msg->msg);
			sk_msg->msg = 0;
			sk_msg->len = 0;
		}

		os_free(sk_msg);
	}
	
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_rx_msg, SOCKET_MSG, data)
	{
		dl_list_del(&sk_msg->data);
		
		if(sk_msg->msg)
		{
			os_free(sk_msg->msg);
			sk_msg->msg = 0;
			sk_msg->len = 0;
		}

		os_free(sk_msg);
	}

	dl_list_del(&element->sk_element);

	os_free(element);
	element = 0;

	GLOBAL_INT_RESTORE();
}

int socket_peek_recv_next_payload_size(SOCKET sk)
{
	int ret = 0;
	BK_SOCKET *element;
	SOCKET_MSG *sk_msg, *tmp;
	GLOBAL_INT_DECLARATION();
	
	element = sk_get_sk_element(sk);
	if(0 == element)
	{
		goto rx_exit;
	}

	GLOBAL_INT_DISABLE();
	dl_list_for_each_safe(sk_msg, tmp, &element->sk_rx_msg, SOCKET_MSG, data)
	{
		ret = sk_msg->len;
		break;
	}

	GLOBAL_INT_RESTORE();
	return ret;
	
rx_exit:
	return 0;
}
#endif
// eof

