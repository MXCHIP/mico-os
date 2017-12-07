#include "lwip/opt.h"
#include "lwip/pbuf.h"

#include "netif/etharp.h"
#include "lwip/tcpip.h"
#include <string.h>

#include "stm32f4xx.h"
#include "cmsis_nvic.h"

#include "mico_eth.h"
#include "mico_lwip_ethif_logging.h"

#include "platform_peripheral.h"

#if PLATFORM_ETH_ENABLE

/******************************************************
 *                      Macros
 ******************************************************/


#define MULTICAST_IP_TO_MAC(ip)       { (uint8_t) 0x01,             \
                                        (uint8_t) 0x00,             \
                                        (uint8_t) 0x5e,             \
                                        (uint8_t) ((ip)[1] & 0x7F), \
                                        (uint8_t) (ip)[2],          \
                                        (uint8_t) (ip)[3]           \
                                      }


/******************************************************
 *                    Constants
 ******************************************************/

#define PHY_TASK_WAIT           (200)
#define ETH_ARCH_PHY_ADDRESS    (0x01)

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/* function */
static void _eth_arch_rx_task(mico_thread_arg_t arg);
static void _eth_arch_phy_task(mico_thread_arg_t arg);

#if LWIP_IPV4
static err_t _eth_arch_netif_output_ipv4(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr);
#endif
#if LWIP_IPV6
static err_t _eth_arch_netif_output_ipv6(struct netif *netif, struct pbuf *q, ip6_addr_t *ipaddr);
#endif

static err_t _eth_arch_low_level_output(struct netif *netif, struct pbuf *p);
static struct pbuf * _eth_arch_low_level_input(struct netif *netif);
static err_t _eth_arch_low_level_igmp_mac_filter(struct netif *netif, ip_addr_t *group, u8_t action);
static void _eth_arch_default_mac_address(char *mac);

static void ETH_AddMacFilter(ETH_HandleTypeDef *heth, uint8_t *Addr, uint8_t enable);
static OSStatus ETH_INIT( void );

MICO_WEAK uint8_t mbed_otp_mac_address(char *mac);

/******************************************************
 *               Variables Definitions
 ******************************************************/

ETH_HandleTypeDef EthHandle;

#if defined (__ICCARM__)   /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END; /* Ethernet Rx DMA Descriptor */

#if defined (__ICCARM__)   /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END; /* Ethernet Tx DMA Descriptor */

#if defined (__ICCARM__)   /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __ALIGN_END; /* Ethernet Receive Buffer */

#if defined (__ICCARM__)   /*!< IAR Compiler */
  #pragma data_alignment=4
#endif
__ALIGN_BEGIN uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __ALIGN_END; /* Ethernet Transmit Buffer */

static mico_semaphore_t rx_ready_sem;    /* receive ready semaphore */
static sys_mutex_t tx_lock_mutex;

/******************************************************
 *               Function Definitions
 ******************************************************/

/**
 * Ethernet Rx Transfer completed callback
 *
 * @param  heth: ETH handle
 * @retval None
 */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    mico_rtos_set_semaphore(&rx_ready_sem);
}


/**
 * Ethernet IRQ Handler
 *
 * @param  None
 * @retval None
 */
void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&EthHandle);
}

static void ETH_AddMacFilter(ETH_HandleTypeDef *heth, uint8_t *Addr, uint8_t enable)
{
    uint32_t tmpreg1;
    uint32_t tmpreg2;
    uint8_t record_not_exist = 1;

    uint32_t MacAddr[3] = {ETH_MAC_ADDRESS1, ETH_MAC_ADDRESS2, ETH_MAC_ADDRESS3};

    /* Calculate the selected MAC address high register */
    tmpreg1 = (1<< 31U) | ((uint32_t)Addr[5U] << 8U) | (uint32_t)Addr[4U];
    /* Calculate the selected MAC address low register */
    tmpreg2 = ((uint32_t)Addr[3U] << 24U) | ((uint32_t)Addr[2U] << 16U) | ((uint32_t)Addr[1U] << 8U) | Addr[0U];

    for( int i = 0; i < 3; i++ ) {
        if( tmpreg1 == (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_HBASE + MacAddr[i]))) &&
            tmpreg2 == (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_LBASE + MacAddr[i]))) ) {
            /* Find a recored, remove mac filter */
            record_not_exist = 0;
            if( enable == 0) {
                eth_log("Remove MAC address filter record %d: %02x:%02x:%02x:%02x:%02x:%02x", i+1,
                        Addr[0], Addr[1], Addr[2], Addr[3], Addr[4], Addr[5]);
                (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_HBASE + MacAddr[i]))) = 0;
                (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_LBASE + MacAddr[i]))) = 0;
            }

        }
    }

    /* Recored not exist, and add mac filter */
    if( record_not_exist && enable == 1 ) {
        for( int i = 0; i < 3; i++ ) {
            if ( ((*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_HBASE + MacAddr[i]))) & (1<< 31U)) == 0 ) {
                eth_log("Add MAC address filter record %d: %02x:%02x:%02x:%02x:%02x:%02x", i+1,
                        Addr[0], Addr[1], Addr[2], Addr[3], Addr[4], Addr[5]);
                /* Load the selected MAC address high register */
                (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_HBASE + MacAddr[i]))) = tmpreg1;
                /* Load the selected MAC address low register */
                (*(__IO uint32_t *)((uint32_t)(ETH_MAC_ADDR_LBASE + MacAddr[i]))) = tmpreg2;

                break;
            }
        }
    }
}

/**
 * STM32 Ethernet peripheral initialize
 *
 * @param  None
 * @retval None
 */
static OSStatus ETH_INIT( void )
{
    OSStatus err = kNoErr;
    uint8_t MACAddr[6];
    HAL_StatusTypeDef hal_eth_init_status;

    platform_eth_mac_address((char *)MACAddr);

    EthHandle.Instance = ETH;
    EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    EthHandle.Init.Speed = ETH_SPEED_10M;
    EthHandle.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
    EthHandle.Init.PhyAddress = ETH_ARCH_PHY_ADDRESS;

    EthHandle.Init.MACAddr = &MACAddr[0];
    EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
    EthHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_SOFTWARE;
    EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
    hal_eth_init_status = HAL_ETH_Init( &EthHandle );

    require_action_quiet( hal_eth_init_status == HAL_OK, exit, err = kConnectionErr );

    eth_log("Auto negotiation result is %s, %s", (EthHandle.Init.Speed)? "100M":"10M",
                                                 (EthHandle.Init.DuplexMode)? "FULLDUPLEX":"HALFDUPLEX" );

    /* Initialize Tx Descriptors list: Chain Mode */
    HAL_ETH_DMATxDescListInit( &EthHandle, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB );

    /* Initialize Rx Descriptors list: Chain Mode  */
    HAL_ETH_DMARxDescListInit( &EthHandle, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB );

    /* Enable MAC and DMA transmission and reception */
    HAL_ETH_Start( &EthHandle );

exit:
     return err;
}


/**
 * In this function, the hardware should be initialized.
 * Called from eth_arch_enetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void _eth_arch_low_level_init(struct netif *netif)
{
    uint32_t regvalue = 0;

    /* Init ETH */
    uint8_t MACAddr[6];
    platform_eth_mac_address((char *)MACAddr);

#if LWIP_ARP || LWIP_ETHERNET
   /* set MAC hardware address length */
   netif->hwaddr_len = ETHARP_HWADDR_LEN;

   /* set MAC hardware address */
   netif->hwaddr[0] = MACAddr[0];
   netif->hwaddr[1] = MACAddr[1];
   netif->hwaddr[2] = MACAddr[2];
   netif->hwaddr[3] = MACAddr[3];
   netif->hwaddr[4] = MACAddr[4];
   netif->hwaddr[5] = MACAddr[5];

   /* maximum transfer unit */
   netif->mtu = 1500;

   /* device capabilities */
   /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
   netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;

#endif

   if ( kNoErr == ETH_INIT()) {
       netif_set_link_up(netif);
   }
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t _eth_arch_low_level_output(struct netif *netif, struct pbuf *p)
{
    err_t errval;
    struct pbuf *q;
    uint8_t *buffer = (uint8_t*)(EthHandle.TxDesc->Buffer1Addr);
    __IO ETH_DMADescTypeDef *DmaTxDesc;
    uint32_t framelength = 0;
    uint32_t bufferoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t payloadoffset = 0;
    DmaTxDesc = EthHandle.TxDesc;
    bufferoffset = 0;

    sys_mutex_lock(&tx_lock_mutex);

    /* copy frame from pbufs to driver buffers */
    for (q = p; q != NULL; q = q->next) {
        /* Is this buffer available? If not, goto error */
        if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
            errval = ERR_USE;
            goto error;
        }

        /* Get bytes in current lwIP buffer */
        byteslefttocopy = q->len;
        payloadoffset = 0;

        /* Check if the length of data to copy is bigger than Tx buffer size*/
        while ((byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE) {
            /* Copy data to Tx buffer*/
            memcpy((uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset));

            /* Point to next descriptor */
            DmaTxDesc = (ETH_DMADescTypeDef*)(DmaTxDesc->Buffer2NextDescAddr);

            /* Check if the buffer is available */
            if ((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET) {
                errval = ERR_USE;
                goto error;
            }

            buffer = (uint8_t*)(DmaTxDesc->Buffer1Addr);

            byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
            payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
            framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
            bufferoffset = 0;
        }

        /* Copy the remaining bytes */
        memcpy((uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy);
        bufferoffset = bufferoffset + byteslefttocopy;
        framelength = framelength + byteslefttocopy;
    }

    /* Prepare transmit descriptors to give to DMA */
    HAL_ETH_TransmitFrame(&EthHandle, framelength);

    errval = ERR_OK;

error:

    /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
    if ((EthHandle.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET) {
        /* Clear TUS ETHERNET DMA flag */
        EthHandle.Instance->DMASR = ETH_DMASR_TUS;

        /* Resume DMA transmission*/
        EthHandle.Instance->DMATPDR = 0;
    }

    sys_mutex_unlock(&tx_lock_mutex);

    return errval;
}


/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf * _eth_arch_low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    struct pbuf *q;
    uint16_t len = 0;
    uint8_t *buffer;
    __IO ETH_DMADescTypeDef *dmarxdesc;
    uint32_t bufferoffset = 0;
    uint32_t payloadoffset = 0;
    uint32_t byteslefttocopy = 0;
    uint32_t i = 0;


    /* get received frame */
    if (HAL_ETH_GetReceivedFrame(&EthHandle) != HAL_OK)
        return NULL;

    /* Obtain the size of the packet and put it into the "len" variable. */
    len = EthHandle.RxFrameInfos.length;
    buffer = (uint8_t*)EthHandle.RxFrameInfos.buffer;

    if (len > 0) {
        /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL_RX);
    }

    if (p != NULL) {
        dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
        bufferoffset = 0;
        for (q = p; q != NULL; q = q->next) {
            byteslefttocopy = q->len;
            payloadoffset = 0;

            /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size*/
            while ((byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE) {
                /* Copy data to pbuf */
                memcpy((uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));

                /* Point to next descriptor */
                dmarxdesc = (ETH_DMADescTypeDef*)(dmarxdesc->Buffer2NextDescAddr);
                buffer = (uint8_t*)(dmarxdesc->Buffer1Addr);

                byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
                payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
                bufferoffset = 0;
            }
            /* Copy remaining data in pbuf */
            memcpy((uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
            bufferoffset = bufferoffset + byteslefttocopy;
        }

        /* Release descriptors to DMA */
        /* Point to first descriptor */
        dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
        /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
        for (i = 0; i < EthHandle.RxFrameInfos.SegCount; i++) {
            dmarxdesc->Status |= ETH_DMARXDESC_OWN;
            dmarxdesc = (ETH_DMADescTypeDef*)(dmarxdesc->Buffer2NextDescAddr);
        }

        /* Clear Segment_Count */
        EthHandle.RxFrameInfos.SegCount = 0;
    }

    /* When Rx Buffer unavailable flag is set: clear it and resume reception */
    if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET) {
        /* Clear RBUS ETHERNET DMA flag */
        EthHandle.Instance->DMASR = ETH_DMASR_RBUS;
        /* Resume DMA reception */
        EthHandle.Instance->DMARPDR = 0;
    }
    return p;
}

/**
 * This task receives input data
 *
 * \param[in] netif the lwip network interface structure
 */
static void _eth_arch_rx_task(mico_thread_arg_t arg)
{
    struct netif   *netif = (struct netif*)arg;
    struct pbuf    *p;

    while (1) {
        mico_rtos_get_semaphore(&rx_ready_sem, 100);
        do {
            p = _eth_arch_low_level_input(netif);
            if (p != NULL) {
                if (netif->input(p, netif) != ERR_OK) {
                    pbuf_free(p);
                }
            }
        } while(p != NULL);
    }
}

/**
 * This task checks phy link status and updates net status
 *
 * \param[in] netif the lwip network interface structure
 */
static void _eth_arch_phy_task(mico_thread_arg_t arg)
{
    struct netif   *netif = (struct netif*)arg;
    uint32_t phy_status = 0;

    HAL_ETH_ReadPHYRegister(&EthHandle, PHY_BSR, &phy_status);

    while (1) {
        uint32_t status;
        if (HAL_ETH_ReadPHYRegister(&EthHandle, PHY_BSR, &status) == HAL_OK) {
            if ((status & PHY_LINKED_STATUS) && !(phy_status & PHY_LINKED_STATUS)) {
                ETH_INIT();
                tcpip_callback_with_block((tcpip_callback_fn)netif_set_link_up, (void*) netif, 1);
            } else if (!(status & PHY_LINKED_STATUS) && (phy_status & PHY_LINKED_STATUS)) {
                HAL_ETH_Stop( &EthHandle );
                tcpip_callback_with_block((tcpip_callback_fn)netif_set_link_down, (void*) netif, 1);
            }
            phy_status = status;
        }
        mico_rtos_delay_milliseconds(PHY_TASK_WAIT);
    }
}


static err_t _eth_arch_low_level_igmp_mac_filter(struct netif *netif, ip_addr_t *group, u8_t action)
{
    uint8_t filter_mac_addr[] = MULTICAST_IP_TO_MAC((uint8_t*)group);

    ETH_AddMacFilter(&EthHandle, filter_mac_addr, action);
    return ERR_OK;
}

/**
 * This function is the ethernet IPv4 packet send function. It calls
 * etharp_output after checking link status.
 *
 * \param[in] netif the lwip network interface structure for this lpc_enetif
 * \param[in] q Pointer to pbug to send
 * \param[in] ipaddr IP address
 * \return ERR_OK or error code
 */
#if LWIP_IPV4
static err_t _eth_arch_netif_output_ipv4(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{
    /* Only send packet is link is up */
    if (netif->flags & NETIF_FLAG_LINK_UP) {
        return etharp_output(netif, q, ipaddr);
    }
    return ERR_CONN;
}
#endif

/**
 * This function is the ethernet packet send function. It calls
 * etharp_output after checking link status.
 *
 * \param[in] netif the lwip network IPv6 interface structure for this lpc_enetif
 * \param[in] q Pointer to pbug to send
 * \param[in] ipaddr IP address
 * \return ERR_OK or error code
 */
#if LWIP_IPV6
static err_t _eth_arch_netif_output_ipv6(struct netif *netif, struct pbuf *q, const ip6_addr_t *ipaddr)
{
    /* Only send packet is link is up */
    if (netif->flags & NETIF_FLAG_LINK_UP) {
        return ethip6_output(netif, q, ipaddr);
    }
    return ERR_CONN;
}
#endif

/**
 * Should be called at the beginning of the program to set up the
 * network interface.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param[in] netif the lwip network interface structure for this lpc_enetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t eth_arch_enetif_init(struct netif *netif)
{
    /* set MAC hardware address */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwipstm32";
#endif /* LWIP_NETIF_HOSTNAME */

    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->state = (void *)INTERFACE_ETH;

#if LWIP_IPV4
    netif->output = _eth_arch_netif_output_ipv4;
    netif->flags |= NETIF_FLAG_IGMP;
#endif
#if LWIP_IPV6
    netif->output_ip6 = _eth_arch_netif_output_ipv6;
    netif->flags |= NETIF_FLAG_MLD6;
#endif

    netif->igmp_mac_filter = _eth_arch_low_level_igmp_mac_filter;

    netif->linkoutput = _eth_arch_low_level_output;

    /* semaphore */
    mico_rtos_init_semaphore(&rx_ready_sem, 16);

    sys_mutex_new(&tx_lock_mutex);

    /* initialize the hardware */
    _eth_arch_low_level_init(netif);

    /* Check link status */
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "_eth_arch_phy_task", _eth_arch_phy_task, 0x500, (mico_thread_arg_t)netif);

    eth_log( "eth_arch_enetif_init done");
    
    /* Ethernet rx task */
    mico_rtos_create_thread(NULL, MICO_RTOS_HIGEST_PRIORITY, "_eth_arch_rx_task", _eth_arch_rx_task, 0x1000, (mico_thread_arg_t)netif);
    return ERR_OK;
}

void eth_arch_enable_interrupts(void)
{
    NVIC_SetVector(ETH_IRQn, (uint32_t)ETH_IRQHandler);
    HAL_NVIC_SetPriority(ETH_IRQn, 0x7, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}

void eth_arch_disable_interrupts(void)
{
    NVIC_DisableIRQ(ETH_IRQn);
}

/** This returns a unique 6-byte MAC address, based on the device UID
*  This function overrides mico_platform_common.c function
*  @param mac A 6-byte array to write the MAC address
*/

void platform_eth_mac_address(char *mac) {
    if (mbed_otp_mac_address(mac)) {
        return;
    } else {
        _eth_arch_default_mac_address(mac);
    }
    return;
}

__weak uint8_t mbed_otp_mac_address(char *mac) {
    return 0;
}

void _eth_arch_default_mac_address(char *mac) {
    unsigned char MXCHIP_mac_addr[3] = {0xc8, 0x93, 0x46}; // default STMicro mac address

    // Read unic id
#if defined (TARGET_STM32F2)
    uint32_t word0 = *(uint32_t *)0x1FFF7A10;
#elif defined (TARGET_STM32F4)
    uint32_t word0 = *(uint32_t *)0x1FFF7A10;
#elif defined (TARGET_STM32F7)
    uint32_t word0 = *(uint32_t *)0x1FF0F420;
#else
    #error MAC address can not be derived from target unique Id
#endif

    mac[0] = MXCHIP_mac_addr[0];
    mac[1] = MXCHIP_mac_addr[1];
    mac[2] = MXCHIP_mac_addr[2];
    mac[3] = (word0 & 0x00ff0000) >> 16;
    mac[4] = (word0 & 0x0000ff00) >> 8;
    mac[5] = (word0 & 0x000000ff);

    return;
}

#endif

