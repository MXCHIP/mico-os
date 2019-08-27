#include <stdio.h>
#include <string.h>

#include "mico.h"
#include "bluenrg1_aci.h"
#include "bluenrg1_hci_le.h"
#include "bluenrg1_events.h"
#include "gp_timer.h"

int8_t qc_status = 0;

static bool scan_stu = 0;
#define BD_ADDR_FMT "  ADDR:%02x:%02x:%02x:%02x:%02x:%02x, RSSI:%d\r\n"
#define BD_ADDR_ARG(x) (x)[5],(x)[4],(x)[3],(x)[2],(x)[1],(x)[0]

void hci_le_advertising_report_qc_event(uint8_t Num_Reports,
                                     Advertising_Report_t Advertising_Report[])
{
  if(scan_stu == 0)
  printf("BLE Scan Success:\r\n");
    scan_stu = 1;

  for(int i = 0;i<Num_Reports;i++)
  {
    printf(BD_ADDR_FMT,BD_ADDR_ARG(Advertising_Report[i].Address),Advertising_Report[i].RSSI);
  }
}

void connection_StateMachine(void)
{
  uint8_t ret;
  ret = aci_gap_start_general_discovery_proc(0x4000, 0x640, PUBLIC_ADDR, 0x01);//scan 1s
   
}

#define BD_ADDR_FMT "Local Bluetooth Address:%02x-%02x-%02x-%02x-%02x-%02x\r\n"
#define BD_ADDR_ARG(x) (x)[0],(x)[1],(x)[2],(x)[3],(x)[4],(x)[5]

void QC_read_DeviceAddress(void)
{
    uint8_t bdaddr[] = {0x00, 0x00, 0x00, 0xE1, 0x80, 0x02};
    uint8_t Data_Length;

    aci_hal_read_config_data(0x00,&Data_Length,bdaddr);
    printf(BD_ADDR_FMT,BD_ADDR_ARG(bdaddr));
}

uint8_t DeviceInit(void)
{
  uint8_t ret;
  uint16_t service_handle, dev_name_char_handle, appearance_char_handle;

  /* Setup the device address */
  QC_read_DeviceAddress();

  /* Set the TX power to -2 dBm */
  aci_hal_set_tx_power_level(1, 4);

  /* GATT Init */
  ret = aci_gatt_init();

  /* GAP Init */
  ret = aci_gap_init(GAP_CENTRAL_ROLE | GAP_PERIPHERAL_ROLE, 0x0, 0x07, &service_handle,
                     &dev_name_char_handle, &appearance_char_handle);

  return BLE_STATUS_SUCCESS;
}

void qc_test_blenrg()
{
    qc_status = 1;

    BlueNRG_Stack_Initialization();

    DeviceInit();

    /* Application tick */
    connection_StateMachine();

}