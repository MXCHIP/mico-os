AOS_SDK_MAKEFILES           		+= ./security/alicrypto/alicrypto.mk ./out/helloworld@mk3080/auto_component/auto_component.mk ./tools/cli/cli.mk ./utility/digest_algorithm/digest_algorithm.mk ././framework/common/common.mk ./kernel/hal/hal.mk ./example/helloworld/helloworld.mk ./kernel/init/init.mk ././kernel/vfs/device/device.mk ./utility/libc/libc.mk ./utility/log/log.mk ./board/mk3080/mk3080.mk ./kernel/modules/fs/kv/kv.mk ./framework/netmgr/netmgr.mk ././platform/arch/arm/armv7m/armv7m.mk ././platform/mcu/rtl8710bn/rtl8710bn.mk ././platform/mcu/rtl8710bn/peripherals/peripherals.mk ././platform/mcu/rtl8710bn/sdk/sdk.mk ./kernel/protocols/net/net.mk ./kernel/rhino/rhino.mk ./kernel/vcall/vcall.mk ./kernel/vfs/vfs.mk ./kernel/yloop/yloop.mk
TOOLCHAIN_NAME            		:= GCC
AOS_SDK_LDFLAGS             		+= -Wl,--gc-sections -Wl,--cref -L .//platform/mcu/rtl8710bn -T .//platform/mcu/rtl8710bn/script/rlx8711B-symbol-v02-img2_xip1.ld -L.//platform/mcu/rtl8710bn/lib/ -l_platform -l_wlan -l_wps -l_p2p -l_rtlstd -mcpu=cortex-m4 -mthumb -mthumb-interwork -mlittle-endian -nostartfiles --specs=nosys.specs -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning --specs=nano.specs -u _printf_float
AOS_SDK_LDS_FILES                     += 
AOS_SDK_LDS_INCLUDES                  += 
RESOURCE_CFLAGS					+= -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\" -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080 -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums -w -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"
AOS_SDK_LINK_SCRIPT         		+= 
AOS_SDK_LINK_SCRIPT_CMD    	 	+= 
AOS_SDK_PREBUILT_LIBRARIES 	 	+= 
AOS_SDK_CERTIFICATES       	 	+= 
AOS_SDK_PRE_APP_BUILDS      		+= 
AOS_SDK_LINK_FILES          		+=                                              
AOS_SDK_INCLUDES           	 	+=                           -I./security/alicrypto/./libalicrypto/inc -I./tools/cli/include -I./utility/digest_algorithm/. -I./board/mk3080/. -I./kernel/modules/fs/kv/include -I./framework/netmgr/include -I./framework/netmgr/../protocol/alink/os/platform/ -I././platform/arch/arm/armv7m/gcc/m4/ -I././platform/mcu/rtl8710bn/../../arch/arm/armv7m/gcc/m4 -I././platform/mcu/rtl8710bn/../../../board/amebaz_dev -I././platform/mcu/rtl8710bn/. -I././platform/mcu/rtl8710bn/arch -I././platform/mcu/rtl8710bn/sdk -I././platform/mcu/rtl8710bn/aos -I././platform/mcu/rtl8710bn/peripherals -I././platform/mcu/rtl8710bn/../include -I././platform/mcu/rtl8710bn/../../../include/hal/soc -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include/lwip -I./kernel/protocols/net/include -I./kernel/protocols/net/port/include -I./kernel/rhino/core/include -I./kernel/vcall/./mico/include -I./kernel/vfs/include -I./include -I./example/helloworld
AOS_SDK_DEFINES             		+=                      -DBUILD_BIN -DCONFIG_ALICRYPTO -DHAVE_NOT_ADVANCED_FORMATE -DCONFIG_AOS_CLI -DAOS_FRAMEWORK_COMMON -DAOS_HAL -DAOS_NO_WIFI -DSTDIO_UART=0 -DAOS_KV -DAOS_NETMGR -DCONFIG_AOS_KV_MULTIPTN_MODE -DCONFIG_AOS_KV_PTN=6 -DCONFIG_AOS_KV_SECOND_PTN=7 -DCONFIG_AOS_KV_PTN_SIZE=4096 -DCONFIG_AOS_KV_BUFFER_SIZE=8192 -DCONFIG_AOS_CLI_STACK_SIZE=4096 -DWITH_LWIP -DCONFIG_NET_LWIP -DVCALL_RHINO -DAOS_VFS -DAOS_LOOP
COMPONENTS                		:= alicrypto auto_component cli digest_algorithm framework hal helloworld kernel_init vfs_device newlib_stub log board_mk3080 kv netmgr armv7m rtl8710bn rtl8710bn_Peripheral_Drivers rtl8710bn_SDK net rhino vcall vfs yloop
PLATFORM_DIRECTORY        		:= mk3080
APP_FULL                  		:= helloworld
PLATFORM                  		:= mk3080
HOST_MCU_FAMILY                  	:= rtl8710bn
SUPPORT_BINS                          := no
APP                       		:= helloworld
HOST_OPENOCD              		:= rtl8710bn
JTAG              		        := jlink_swd
HOST_ARCH                 		:= Cortex-M4
NO_BUILD_BOOTLOADER           	:= 
NO_BOOTLOADER_REQUIRED         	:= 
alicrypto_LOCATION         := ./security/alicrypto/
auto_component_LOCATION         := ./out/helloworld@mk3080/auto_component/
cli_LOCATION         := ./tools/cli/
digest_algorithm_LOCATION         := ./utility/digest_algorithm/
framework_LOCATION         := ././framework/common/
hal_LOCATION         := ./kernel/hal/
helloworld_LOCATION         := ./example/helloworld/
kernel_init_LOCATION         := ./kernel/init/
vfs_device_LOCATION         := ././kernel/vfs/device/
newlib_stub_LOCATION         := ./utility/libc/
log_LOCATION         := ./utility/log/
board_mk3080_LOCATION         := ./board/mk3080/
kv_LOCATION         := ./kernel/modules/fs/kv/
netmgr_LOCATION         := ./framework/netmgr/
armv7m_LOCATION         := ././platform/arch/arm/armv7m/
rtl8710bn_LOCATION         := ././platform/mcu/rtl8710bn/
rtl8710bn_Peripheral_Drivers_LOCATION         := ././platform/mcu/rtl8710bn/peripherals/
rtl8710bn_SDK_LOCATION         := ././platform/mcu/rtl8710bn/sdk/
net_LOCATION         := ./kernel/protocols/net/
rhino_LOCATION         := ./kernel/rhino/
vcall_LOCATION         := ./kernel/vcall/
vfs_LOCATION         := ./kernel/vfs/
yloop_LOCATION         := ./kernel/yloop/
alicrypto_SOURCES          += ./libalicrypto/ali_crypto.c ./libalicrypto/mbed/asym/rsa.c ./libalicrypto/mbed/cipher/aes.c ./libalicrypto/mbed/hash/hash.c ./libalicrypto/mbed/mac/hmac.c ./libalicrypto/sw/ali_crypto_rand.c ./libalicrypto/test/ali_crypto_test.c ./libalicrypto/test/ali_crypto_test_aes.c ./libalicrypto/test/ali_crypto_test_comm.c ./libalicrypto/test/ali_crypto_test_hash.c ./libalicrypto/test/ali_crypto_test_hmac.c ./libalicrypto/test/ali_crypto_test_rand.c ./libalicrypto/test/ali_crypto_test_rsa.c ./mbedtls/library/aes.c ./mbedtls/library/asn1parse.c ./mbedtls/library/bignum.c ./mbedtls/library/hash_wrap.c ./mbedtls/library/hmac.c ./mbedtls/library/mbedtls_alt.c ./mbedtls/library/md5.c ./mbedtls/library/oid.c ./mbedtls/library/rsa.c ./mbedtls/library/sha1.c ./mbedtls/library/sha256.c ./mbedtls/library/threading.c 
auto_component_SOURCES          +=  component_init.c testcase_register.c
cli_SOURCES          += cli.c dumpsys.c 
digest_algorithm_SOURCES          += CheckSumUtils.c crc.c digest_algorithm.c md5.c 
framework_SOURCES          += main.c version.c 
hal_SOURCES          += ota.c wifi.c 
helloworld_SOURCES          += helloworld.c 
kernel_init_SOURCES          += aos_init.c 
vfs_device_SOURCES          += vfs_adc.c vfs_gpio.c vfs_i2c.c vfs_pwm.c vfs_rtc.c vfs_spi.c vfs_uart.c vfs_wdg.c 
newlib_stub_SOURCES          += newlib_stub.c 
log_SOURCES          += log.c 
board_mk3080_SOURCES          += board.c 
kv_SOURCES          += kvmgr.c 
netmgr_SOURCES          += netmgr.c 
armv7m_SOURCES          += gcc/m4/port_c.c gcc/m4/port_s.S 
rtl8710bn_SOURCES          += aos/aos.c aos/aos_osdep.c aos/ethernetif.c aos/soc_impl.c aos/trace_impl.c hal/flash.c hal/gpio.c hal/hw.c hal/ota_port.c hal/uart.c hal/wifi_port.c 
rtl8710bn_Peripheral_Drivers_SOURCES          += RingBufferUtils.c platform_8711.c platform_adc.c platform_gpio.c platform_i2c.c platform_mcu_powersave.c platform_pwm.c platform_rng.c platform_rtc.c platform_spi.c platform_uart.c platform_watchdog.c 
rtl8710bn_SDK_SOURCES          += /component/common/api/at_cmd/atcmd_sys.c /component/common/api/at_cmd/atcmd_wifi.c /component/common/api/at_cmd/log_service.c /component/common/api/lwip_netconf.c /component/common/api/network/src/ping_test.c /component/common/api/network/src/wlan_network.c /component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant/wifi_eap_config.c /component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant/wifi_p2p_config.c /component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant/wifi_wps_config.c /component/common/api/wifi/wifi_conf.c /component/common/api/wifi/wifi_ind.c /component/common/api/wifi/wifi_promisc.c /component/common/api/wifi/wifi_simple_config.c /component/common/api/wifi/wifi_util.c /component/common/drivers/wlan/realtek/src/osdep/lwip_intf.c /component/common/mbed/targets/hal/rtl8711b/analogin_api.c /component/common/mbed/targets/hal/rtl8711b/efuse_api.c /component/common/mbed/targets/hal/rtl8711b/flash_api.c /component/common/mbed/targets/hal/rtl8711b/gpio_api.c /component/common/mbed/targets/hal/rtl8711b/gpio_irq_api.c /component/common/mbed/targets/hal/rtl8711b/i2c_api.c /component/common/mbed/targets/hal/rtl8711b/i2s_api.c /component/common/mbed/targets/hal/rtl8711b/nfc_api.c /component/common/mbed/targets/hal/rtl8711b/pinmap.c /component/common/mbed/targets/hal/rtl8711b/pinmap_common.c /component/common/mbed/targets/hal/rtl8711b/port_api.c /component/common/mbed/targets/hal/rtl8711b/pwmout_api.c /component/common/mbed/targets/hal/rtl8711b/rtc_api.c /component/common/mbed/targets/hal/rtl8711b/serial_api.c /component/common/mbed/targets/hal/rtl8711b/sleep.c /component/common/mbed/targets/hal/rtl8711b/spi_api.c /component/common/mbed/targets/hal/rtl8711b/sys_api.c /component/common/mbed/targets/hal/rtl8711b/timer_api.c /component/common/mbed/targets/hal/rtl8711b/us_ticker.c /component/common/mbed/targets/hal/rtl8711b/us_ticker_api.c /component/common/mbed/targets/hal/rtl8711b/wait_api.c /component/common/mbed/targets/hal/rtl8711b/wdt_api.c /component/common/network/dhcp/dhcps.c /component/common/utilities/tcptest.c /component/os/os_dep/device_lock.c /component/os/os_dep/osdep_service.c /component/soc/realtek/8711b/app/monitor/ram/low_level_io.c /component/soc/realtek/8711b/app/monitor/ram/monitor.c /component/soc/realtek/8711b/app/monitor/ram/rtl_consol.c /component/soc/realtek/8711b/app/monitor/ram/rtl_trace.c /component/soc/realtek/8711b/cmsis/device/app_start.c /component/soc/realtek/8711b/cmsis/device/system_8195a.c /component/soc/realtek/8711b/fwlib/ram_lib/rtl8710b_dsleepcfg.c /component/soc/realtek/8711b/fwlib/ram_lib/rtl8710b_dstandbycfg.c /component/soc/realtek/8711b/fwlib/ram_lib/rtl8710b_intfcfg.c /component/soc/realtek/8711b/fwlib/ram_lib/rtl8710b_pinmapcfg.c /component/soc/realtek/8711b/fwlib/ram_lib/rtl8710b_sleepcfg.c /component/soc/realtek/8711b/fwlib/ram_lib/startup.c /component/soc/realtek/8711b/misc/rtl8710b_ota.c 
net_SOURCES          += api/api_lib.c api/api_msg.c api/err.c api/netbuf.c api/netdb.c api/netifapi.c api/sockets.c api/tcpip.c apps/tftp/tftp_client.c apps/tftp/tftp_common.c apps/tftp/tftp_ota.c apps/tftp/tftp_server.c core/def.c core/dns.c core/inet_chksum.c core/init.c core/ip.c core/ipv4/autoip.c core/ipv4/dhcp.c core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/igmp.c core/ipv4/ip4.c core/ipv4/ip4_addr.c core/ipv4/ip4_frag.c core/ipv6/dhcp6.c core/ipv6/ethip6.c core/ipv6/icmp6.c core/ipv6/inet6.c core/ipv6/ip6.c core/ipv6/ip6_addr.c core/ipv6/ip6_frag.c core/ipv6/mld6.c core/ipv6/nd6.c core/mem.c core/memp.c core/netif.c core/pbuf.c core/raw.c core/stats.c core/sys.c core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c core/udp.c netif/ethernet.c netif/slipif.c port/sys_arch.c 
rhino_SOURCES          += common/k_fifo.c common/k_trace.c core/k_buf_queue.c core/k_dyn_mem_proc.c core/k_err.c core/k_event.c core/k_idle.c core/k_mm.c core/k_mm_blk.c core/k_mm_debug.c core/k_mutex.c core/k_obj.c core/k_pend.c core/k_queue.c core/k_ringbuf.c core/k_sched.c core/k_sem.c core/k_stats.c core/k_sys.c core/k_task.c core/k_task_sem.c core/k_tick.c core/k_time.c core/k_timer.c core/k_workqueue.c 
vcall_SOURCES          += aos/aos_rhino.c 
vfs_SOURCES          += device.c select.c vfs.c vfs_file.c vfs_inode.c vfs_register.c 
yloop_SOURCES          += local_event.c yloop.c 
alicrypto_CHECK_HEADERS    += 
auto_component_CHECK_HEADERS    += 
cli_CHECK_HEADERS    += 
digest_algorithm_CHECK_HEADERS    += 
framework_CHECK_HEADERS    += 
hal_CHECK_HEADERS    += 
helloworld_CHECK_HEADERS    += 
kernel_init_CHECK_HEADERS    += 
vfs_device_CHECK_HEADERS    += 
newlib_stub_CHECK_HEADERS    += 
log_CHECK_HEADERS    += 
board_mk3080_CHECK_HEADERS    += 
kv_CHECK_HEADERS    += 
netmgr_CHECK_HEADERS    += 
armv7m_CHECK_HEADERS    += 
rtl8710bn_CHECK_HEADERS    += 
rtl8710bn_Peripheral_Drivers_CHECK_HEADERS    += 
rtl8710bn_SDK_CHECK_HEADERS    += 
net_CHECK_HEADERS    += 
rhino_CHECK_HEADERS    += 
vcall_CHECK_HEADERS    += 
vfs_CHECK_HEADERS    += 
yloop_CHECK_HEADERS    += 
alicrypto_INCLUDES         := -I./security/alicrypto/./mbedtls/include/mbedtls -I./security/alicrypto/./libalicrypto/mbed/inc -I./security/alicrypto/./libalicrypto/sw -I./security/alicrypto/./mbedtls/include -I./security/alicrypto/./libalicrypto/test/inc -I./security/alicrypto/./mbedtls/include/mbedtls -I./security/alicrypto/./libalicrypto/mbed/inc -I./security/alicrypto/./libalicrypto/sw -I./security/alicrypto/./mbedtls/include -I./security/alicrypto/./libalicrypto/test/inc
auto_component_INCLUDES         := 
cli_INCLUDES         := 
digest_algorithm_INCLUDES         := 
framework_INCLUDES         := 
hal_INCLUDES         := 
helloworld_INCLUDES         := 
kernel_init_INCLUDES         := 
vfs_device_INCLUDES         := -I././kernel/vfs/device/../include/device/ -I././kernel/vfs/device/../include/ -I././kernel/vfs/device/../../hal/soc/ -I././kernel/vfs/device/../include/device/ -I././kernel/vfs/device/../include/ -I././kernel/vfs/device/../../hal/soc/
newlib_stub_INCLUDES         := 
log_INCLUDES         := 
board_mk3080_INCLUDES         := 
kv_INCLUDES         := 
netmgr_INCLUDES         := 
armv7m_INCLUDES         := 
rtl8710bn_INCLUDES         := -I././platform/mcu/rtl8710bn/. -I././platform/mcu/rtl8710bn/../include -I././platform/mcu/rtl8710bn/../../../include/hal/soc -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include/lwip -I././platform/mcu/rtl8710bn/arch -I././platform/mcu/rtl8710bn/aos -I././platform/mcu/rtl8710bn/peripherals -I././platform/mcu/rtl8710bn/sdk -I././platform/mcu/rtl8710bn/sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/common/api/network/include -I././platform/mcu/rtl8710bn/sdk/component/common/api -I././platform/mcu/rtl8710bn/sdk/component/common/api/at_cmd -I././platform/mcu/rtl8710bn/sdk/component/common/api/platform -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/src -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wowlan -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/modules -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/sdio/realtek/sdio_host/inc -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/inic/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device/class -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/include -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/osdep -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hci -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/OUTSRC -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/wlan_ram_map/rom -I././platform/mcu/rtl8710bn/sdk/component/common/network -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/hal/rtl8711b -I././platform/mcu/rtl8710bn/. -I././platform/mcu/rtl8710bn/../include -I././platform/mcu/rtl8710bn/../../../include/hal/soc -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include/lwip -I././platform/mcu/rtl8710bn/arch -I././platform/mcu/rtl8710bn/aos -I././platform/mcu/rtl8710bn/peripherals -I././platform/mcu/rtl8710bn/sdk -I././platform/mcu/rtl8710bn/sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/common/api/network/include -I././platform/mcu/rtl8710bn/sdk/component/common/api -I././platform/mcu/rtl8710bn/sdk/component/common/api/at_cmd -I././platform/mcu/rtl8710bn/sdk/component/common/api/platform -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/src -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wowlan -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/modules -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/sdio/realtek/sdio_host/inc -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/inic/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device/class -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/include -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/osdep -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hci -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/OUTSRC -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/wlan_ram_map/rom -I././platform/mcu/rtl8710bn/sdk/component/common/network -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/hal/rtl8711b -I././platform/mcu/rtl8710bn/. -I././platform/mcu/rtl8710bn/../include -I././platform/mcu/rtl8710bn/../../../include/hal/soc -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/../../../kernel/protocols/net/include/lwip -I././platform/mcu/rtl8710bn/arch -I././platform/mcu/rtl8710bn/aos -I././platform/mcu/rtl8710bn/peripherals -I././platform/mcu/rtl8710bn/sdk -I././platform/mcu/rtl8710bn/sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/common/api/network/include -I././platform/mcu/rtl8710bn/sdk/component/common/api -I././platform/mcu/rtl8710bn/sdk/component/common/api/at_cmd -I././platform/mcu/rtl8710bn/sdk/component/common/api/platform -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/src -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wowlan -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/modules -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/sdio/realtek/sdio_host/inc -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/inic/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device/class -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/include -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/osdep -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hci -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/OUTSRC -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/wlan_ram_map/rom -I././platform/mcu/rtl8710bn/sdk/component/common/network -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/hal/rtl8711b
rtl8710bn_Peripheral_Drivers_INCLUDES         := -I././platform/mcu/rtl8710bn/peripherals/peripherals -I././platform/mcu/rtl8710bn/peripherals/../../include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/targets/hal/rtl8711b -I././platform/mcu/rtl8710bn/peripherals/../sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/peripherals/peripherals -I././platform/mcu/rtl8710bn/peripherals/../../include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/peripherals/../sdk/component/common/mbed/targets/hal/rtl8711b -I././platform/mcu/rtl8710bn/peripherals/../sdk/project/realtek_amebaz_va0_example/inc
rtl8710bn_SDK_INCLUDES         := -I././platform/mcu/rtl8710bn/sdk/../../../../include/hal/soc -I././platform/mcu/rtl8710bn/sdk/../../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/sdk/../ -I././platform/mcu/rtl8710bn/sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/common/api/network/include -I././platform/mcu/rtl8710bn/sdk/component/common/api -I././platform/mcu/rtl8710bn/sdk/component/common/api/at_cmd -I././platform/mcu/rtl8710bn/sdk/component/common/api/platform -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/src -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wowlan -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/modules -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/sdio/realtek/sdio_host/inc -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/inic/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device/class -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/include -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/osdep -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hci -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/OUTSRC -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/wlan_ram_map/rom -I././platform/mcu/rtl8710bn/sdk/component/common/network -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/../../../../include/hal/soc -I././platform/mcu/rtl8710bn/sdk/../../../../kernel/protocols/net/include -I././platform/mcu/rtl8710bn/sdk/../ -I././platform/mcu/rtl8710bn/sdk/project/realtek_amebaz_va0_example/inc -I././platform/mcu/rtl8710bn/sdk/component/os/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/common/api/network/include -I././platform/mcu/rtl8710bn/sdk/component/common/api -I././platform/mcu/rtl8710bn/sdk/component/common/api/at_cmd -I././platform/mcu/rtl8710bn/sdk/component/common/api/platform -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/src -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wowlan -I././platform/mcu/rtl8710bn/sdk/component/common/api/wifi/rtw_wpa_supplicant/wpa_supplicant -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/modules -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/sdio/realtek/sdio_host/inc -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/inic/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/usb_class/device/class -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/include -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/osdep -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hci -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/src/hal/OUTSRC -I././platform/mcu/rtl8710bn/sdk/component/common/drivers/wlan/realtek/wlan_ram_map/rom -I././platform/mcu/rtl8710bn/sdk/component/common/network -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/app/monitor/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/cmsis/device -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/ram_lib/crypto -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/fwlib/rom_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/os_dep/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libc/rom/string -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/std_lib/libgcc/rtl8195a/include -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/swlib/rtl_lib -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc -I././platform/mcu/rtl8710bn/sdk/component/soc/realtek/8711b/misc/os -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/api -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/hal_ext -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/cmsis/rtl8711b -I././platform/mcu/rtl8710bn/sdk/component/common/mbed/targets/hal/rtl8711b
net_INCLUDES         := -I./kernel/protocols/net/port/include -I./kernel/protocols/net/port/include
rhino_INCLUDES         := 
vcall_INCLUDES         := 
vfs_INCLUDES         := 
yloop_INCLUDES         := 
alicrypto_DEFINES          := 
auto_component_DEFINES          := 
cli_DEFINES          := 
digest_algorithm_DEFINES          := 
framework_DEFINES          := 
hal_DEFINES          := 
helloworld_DEFINES          := 
kernel_init_DEFINES          := 
vfs_device_DEFINES          := 
newlib_stub_DEFINES          := 
log_DEFINES          := 
board_mk3080_DEFINES          := 
kv_DEFINES          := 
netmgr_DEFINES          := 
armv7m_DEFINES          := 
rtl8710bn_DEFINES          := 
rtl8710bn_Peripheral_Drivers_DEFINES          := 
rtl8710bn_SDK_DEFINES          := 
net_DEFINES          := 
rhino_DEFINES          := 
vcall_DEFINES          := 
vfs_DEFINES          := 
yloop_DEFINES          := 
alicrypto_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -DCONFIG_CRYPT_MBED=1 -DCONFIG_DBG_CRYPT=1 -W -Wdeclaration-after-statement  -D_FILE_OFFSET_BITS=64 -DCONFIG_CRYPT_MBED=1 -DCONFIG_DBG_CRYPT=1 -W -Wdeclaration-after-statement  -D_FILE_OFFSET_BITS=64
auto_component_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
cli_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
digest_algorithm_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
framework_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
hal_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
helloworld_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
kernel_init_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
vfs_device_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
newlib_stub_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
log_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
board_mk3080_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
kv_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
netmgr_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
armv7m_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rtl8710bn_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing
rtl8710bn_Peripheral_Drivers_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rtl8710bn_SDK_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
net_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rhino_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
vcall_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
vfs_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
yloop_CFLAGS           :=     -DSYSINFO_APP_VERSION=\"app-1.0.0-20180424.1410\"       -DON_PRE2=1 -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3080\" -DSYSINFO_DEVICE_NAME=\"MK3080\" -L .//board/mk3080    -mcpu=cortex-m4 -march=armv7-m -mthumb -mthumb-interwork -mlittle-endian -DCONFIG_PLATFORM_8711B -DM3 -fno-short-enums                  -w    -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" -Wall -Werror -Wall -Werror
alicrypto_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
auto_component_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
cli_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
digest_algorithm_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
framework_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
hal_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
helloworld_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
kernel_init_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
vfs_device_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
newlib_stub_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
log_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
board_mk3080_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
kv_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
netmgr_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
armv7m_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rtl8710bn_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rtl8710bn_Peripheral_Drivers_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rtl8710bn_SDK_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
net_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
rhino_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
vcall_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
vfs_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
yloop_CXXFLAGS         :=                        -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3080/resources/ -DPLATFORM=\"mk3080\" 
alicrypto_ASMFLAGS         :=                        -ggdb
auto_component_ASMFLAGS         :=                        -ggdb
cli_ASMFLAGS         :=                        -ggdb
digest_algorithm_ASMFLAGS         :=                        -ggdb
framework_ASMFLAGS         :=                        -ggdb
hal_ASMFLAGS         :=                        -ggdb
helloworld_ASMFLAGS         :=                        -ggdb
kernel_init_ASMFLAGS         :=                        -ggdb
vfs_device_ASMFLAGS         :=                        -ggdb
newlib_stub_ASMFLAGS         :=                        -ggdb
log_ASMFLAGS         :=                        -ggdb
board_mk3080_ASMFLAGS         :=                        -ggdb
kv_ASMFLAGS         :=                        -ggdb
netmgr_ASMFLAGS         :=                        -ggdb
armv7m_ASMFLAGS         :=                        -ggdb
rtl8710bn_ASMFLAGS         :=                        -ggdb
rtl8710bn_Peripheral_Drivers_ASMFLAGS         :=                        -ggdb
rtl8710bn_SDK_ASMFLAGS         :=                        -ggdb
net_ASMFLAGS         :=                        -ggdb
rhino_ASMFLAGS         :=                        -ggdb
vcall_ASMFLAGS         :=                        -ggdb
vfs_ASMFLAGS         :=                        -ggdb
yloop_ASMFLAGS         :=                        -ggdb
alicrypto_RESOURCES        := 
auto_component_RESOURCES        := 
cli_RESOURCES        := 
digest_algorithm_RESOURCES        := 
framework_RESOURCES        := 
hal_RESOURCES        := 
helloworld_RESOURCES        := 
kernel_init_RESOURCES        := 
vfs_device_RESOURCES        := 
newlib_stub_RESOURCES        := 
log_RESOURCES        := 
board_mk3080_RESOURCES        := 
kv_RESOURCES        := 
netmgr_RESOURCES        := 
armv7m_RESOURCES        := 
rtl8710bn_RESOURCES        := 
rtl8710bn_Peripheral_Drivers_RESOURCES        := 
rtl8710bn_SDK_RESOURCES        := 
net_RESOURCES        := 
rhino_RESOURCES        := 
vcall_RESOURCES        := 
vfs_RESOURCES        := 
yloop_RESOURCES        := 
alicrypto_MAKEFILE         := ./security/alicrypto/alicrypto.mk
auto_component_MAKEFILE         := ./out/helloworld@mk3080/auto_component/auto_component.mk
cli_MAKEFILE         := ./tools/cli/cli.mk
digest_algorithm_MAKEFILE         := ./utility/digest_algorithm/digest_algorithm.mk
framework_MAKEFILE         := ././framework/common/common.mk
hal_MAKEFILE         := ./kernel/hal/hal.mk
helloworld_MAKEFILE         := ./example/helloworld/helloworld.mk
kernel_init_MAKEFILE         := ./kernel/init/init.mk
vfs_device_MAKEFILE         := ././kernel/vfs/device/device.mk
newlib_stub_MAKEFILE         := ./utility/libc/libc.mk
log_MAKEFILE         := ./utility/log/log.mk
board_mk3080_MAKEFILE         := ./board/mk3080/mk3080.mk
kv_MAKEFILE         := ./kernel/modules/fs/kv/kv.mk
netmgr_MAKEFILE         := ./framework/netmgr/netmgr.mk
armv7m_MAKEFILE         := ././platform/arch/arm/armv7m/armv7m.mk
rtl8710bn_MAKEFILE         := ././platform/mcu/rtl8710bn/rtl8710bn.mk
rtl8710bn_Peripheral_Drivers_MAKEFILE         := ././platform/mcu/rtl8710bn/peripherals/peripherals.mk
rtl8710bn_SDK_MAKEFILE         := ././platform/mcu/rtl8710bn/sdk/sdk.mk
net_MAKEFILE         := ./kernel/protocols/net/net.mk
rhino_MAKEFILE         := ./kernel/rhino/rhino.mk
vcall_MAKEFILE         := ./kernel/vcall/vcall.mk
vfs_MAKEFILE         := ./kernel/vfs/vfs.mk
yloop_MAKEFILE         := ./kernel/yloop/yloop.mk
alicrypto_PRE_BUILD_TARGETS:= 
auto_component_PRE_BUILD_TARGETS:= 
cli_PRE_BUILD_TARGETS:= 
digest_algorithm_PRE_BUILD_TARGETS:= 
framework_PRE_BUILD_TARGETS:= 
hal_PRE_BUILD_TARGETS:= 
helloworld_PRE_BUILD_TARGETS:= 
kernel_init_PRE_BUILD_TARGETS:= 
vfs_device_PRE_BUILD_TARGETS:= 
newlib_stub_PRE_BUILD_TARGETS:= 
log_PRE_BUILD_TARGETS:= 
board_mk3080_PRE_BUILD_TARGETS:= 
kv_PRE_BUILD_TARGETS:= 
netmgr_PRE_BUILD_TARGETS:= 
armv7m_PRE_BUILD_TARGETS:= 
rtl8710bn_PRE_BUILD_TARGETS:= 
rtl8710bn_Peripheral_Drivers_PRE_BUILD_TARGETS:= 
rtl8710bn_SDK_PRE_BUILD_TARGETS:= 
net_PRE_BUILD_TARGETS:= 
rhino_PRE_BUILD_TARGETS:= 
vcall_PRE_BUILD_TARGETS:= 
vfs_PRE_BUILD_TARGETS:= 
yloop_PRE_BUILD_TARGETS:= 
alicrypto_PREBUILT_LIBRARY := 
auto_component_PREBUILT_LIBRARY := 
cli_PREBUILT_LIBRARY := 
digest_algorithm_PREBUILT_LIBRARY := 
framework_PREBUILT_LIBRARY := 
hal_PREBUILT_LIBRARY := 
helloworld_PREBUILT_LIBRARY := 
kernel_init_PREBUILT_LIBRARY := 
vfs_device_PREBUILT_LIBRARY := 
newlib_stub_PREBUILT_LIBRARY := 
log_PREBUILT_LIBRARY := 
board_mk3080_PREBUILT_LIBRARY := 
kv_PREBUILT_LIBRARY := 
netmgr_PREBUILT_LIBRARY := 
armv7m_PREBUILT_LIBRARY := 
rtl8710bn_PREBUILT_LIBRARY := 
rtl8710bn_Peripheral_Drivers_PREBUILT_LIBRARY := 
rtl8710bn_SDK_PREBUILT_LIBRARY := 
net_PREBUILT_LIBRARY := 
rhino_PREBUILT_LIBRARY := 
vcall_PREBUILT_LIBRARY := 
vfs_PREBUILT_LIBRARY := 
yloop_PREBUILT_LIBRARY := 
alicrypto_TYPE             := 
auto_component_TYPE             := kernel
cli_TYPE             := kernel
digest_algorithm_TYPE             := share
framework_TYPE             := framework
hal_TYPE             := kernel
helloworld_TYPE             := 
kernel_init_TYPE             := kernel
vfs_device_TYPE             := 
newlib_stub_TYPE             := share
log_TYPE             := share
board_mk3080_TYPE             := kernel
kv_TYPE             := kernel
netmgr_TYPE             := framework
armv7m_TYPE             := 
rtl8710bn_TYPE             := kernel
rtl8710bn_Peripheral_Drivers_TYPE             := 
rtl8710bn_SDK_TYPE             := 
net_TYPE             := kernel
rhino_TYPE             := kernel
vcall_TYPE             := kernel
vfs_TYPE             := kernel
yloop_TYPE             := kernel
AOS_SDK_UNIT_TEST_SOURCES   		:=                                              
ALL_RESOURCES             		:= 
INTERNAL_MEMORY_RESOURCES 		:= 
EXTRA_TARGET_MAKEFILES 			:=       .//platform/mcu/rtl8710bn/download.mk .//platform/mcu/rtl8710bn/gen_crc_bin.mk
APPS_START_SECTOR 				:=  
BOOTLOADER_FIRMWARE				:=  
ATE_FIRMWARE				        :=  
APPLICATION_FIRMWARE				:=  
PARAMETER_1_IMAGE					:=  
PARAMETER_2_IMAGE					:=  
FILESYSTEM_IMAGE					:=  
WIFI_FIRMWARE						:=  
BT_PATCH_FIRMWARE					:=  
AOS_ROM_SYMBOL_LIST_FILE 		:= 
AOS_SDK_CHIP_SPECIFIC_SCRIPT		:=                       
AOS_SDK_CONVERTER_OUTPUT_FILE	:=                       
AOS_SDK_FINAL_OUTPUT_FILE 		:=                       
AOS_RAM_STUB_LIST_FILE 			:= 
