#include <stdio.h>
#include <string.h>

#include "mico.h"
#include "bluenrg1_aci.h"
#include "bluenrg1_hci_le.h"
#include "bluenrg1_events.h"
#include "gp_timer.h"

void connection_StateMachine(void)
{
  uint8_t ret;
  ret = aci_gap_start_general_discovery_proc(0x4000, 0x640, PUBLIC_ADDR, 0x01);//scan 1s
   
}

void qc_test_blenrg()
{
    BlueNRG_Stack_Initialization();
    
    CHAT_DeviceInit();

    /* Application tick */
    connection_StateMachine();

}