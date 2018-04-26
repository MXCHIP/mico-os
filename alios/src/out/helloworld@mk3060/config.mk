AOS_SDK_MAKEFILES           		+= ./security/alicrypto/alicrypto.mk ./out/helloworld@mk3060/auto_component/auto_component.mk ./tools/cli/cli.mk ./utility/digest_algorithm/digest_algorithm.mk ./kernel/hal/hal.mk ./example/helloworld/helloworld.mk ./kernel/init/init.mk ././kernel/vfs/device/device.mk ./utility/libc/libc.mk ./security/libid2/libid2.mk ./security/libkm/libkm.mk ./utility/log/log.mk ./board/mk3060/mk3060.mk ./kernel/modules/fs/kv/kv.mk ./security/plat_gen/plat_gen.mk ././platform/arch/arm/armv5/armv5.mk ././platform/mcu/moc108/moc108.mk ././platform/mcu/moc108/aos/app_runtime/app_runtime.mk ././platform/mcu/moc108/aos/framework_runtime/framework_runtime.mk ././platform/mcu/moc108/hal_init/hal_init.mk ././platform/mcu/moc108/mx108/mx378/driver/entry/entry.mk ./kernel/protocols/net/net.mk ./security/prov/prov.mk ./kernel/rhino/rhino.mk ./kernel/vcall/vcall.mk ./kernel/vfs/vfs.mk ./kernel/yloop/yloop.mk
TOOLCHAIN_NAME            		:= GCC
AOS_SDK_LDFLAGS             		+= -Wl,--gc-sections -Wl,--cref -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian --specs=nosys.specs -nostartfiles --specs=nano.specs -u _printf_float -uapp_info -uframework_info
AOS_SDK_LDS_FILES                     += platform/mcu/moc108/mx108/mx378/build/mx108.ld.S
AOS_SDK_LDS_INCLUDES                  += .//board/mk3060/memory.ld.S
RESOURCE_CFLAGS					+= -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\" -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"
AOS_SDK_LINK_SCRIPT         		+= 
AOS_SDK_LINK_SCRIPT_CMD    	 	+= 
AOS_SDK_PREBUILT_LIBRARIES 	 	+= ./security/libid2/lib/ARM968E-S/libid2.a ./security/libkm/lib/ARM968E-S/libkm.a ./security/plat_gen/lib/ARM968E-S/libplat_gen.a ././platform/mcu/moc108/librwnx/librwnx.a ./security/prov/lib/ARM968E-S/libprov.a
AOS_SDK_CERTIFICATES       	 	+= 
AOS_SDK_PRE_APP_BUILDS      		+= 
AOS_SDK_LINK_FILES          		+=                                                      
AOS_SDK_INCLUDES           	 	+=                           -I./security/alicrypto/./libalicrypto/inc -I./tools/cli/include -I./utility/digest_algorithm/. -I./security/libid2/include -I./security/libkm/include -I./board/mk3060/. -I./kernel/modules/fs/kv/include -I./security/plat_gen/include -I././platform/arch/arm/armv5/./gcc -I././platform/mcu/moc108/mx108/mx378/func/mxchip/lwip-2.0.2/port -I././platform/mcu/moc108/mx108/mx378/common -I././platform/mcu/moc108/mx108/mx378/app/config -I././platform/mcu/moc108/mx108/mx378/func/include -I././platform/mcu/moc108/mx108/mx378/os/include -I././platform/mcu/moc108/mx108/mx378/driver/include -I././platform/mcu/moc108/mx108/mx378/driver/common -I././platform/mcu/moc108/mx108/mx378/ip/common -I././platform/mcu/moc108/mx108/mx378/driver/entry/. -I./kernel/protocols/net/include -I./kernel/protocols/net/port/include -I./security/prov/include -I./kernel/rhino/core/include -I./kernel/vcall/./mico/include -I./kernel/vfs/include -I./include -I./example/helloworld
AOS_SDK_DEFINES             		+=                      -DBUILD_BIN -DCONFIG_ALICRYPTO -DHAVE_NOT_ADVANCED_FORMATE -DCONFIG_AOS_CLI -DAOS_HAL -DAOS_NO_WIFI -DSTDIO_UART=0 -DAOS_KV -DCONFIG_MX108 -DCONFIG_AOS_KV_MULTIPTN_MODE -DCONFIG_AOS_KV_PTN=6 -DCONFIG_AOS_KV_SECOND_PTN=7 -DCONFIG_AOS_KV_PTN_SIZE=4096 -DCONFIG_AOS_KV_BUFFER_SIZE=8192 -DCONFIG_AOS_CLI_BOARD -DCONFIG_AOS_FOTA_BREAKPOINT -DWITH_LWIP -DCONFIG_NET_LWIP -DVCALL_RHINO -DAOS_VFS -DAOS_LOOP
COMPONENTS                		:= alicrypto auto_component cli digest_algorithm hal helloworld kernel_init vfs_device newlib_stub libid2 libkm log board_mk3060 kv plat_gen armv5 moc108 app_runtime framework_runtime hal_init entry net prov rhino vcall vfs yloop
PLATFORM_DIRECTORY        		:= mk3060
APP_FULL                  		:= helloworld
PLATFORM                  		:= mk3060
HOST_MCU_FAMILY                  	:= moc108
SUPPORT_BINS                          := yes
APP                       		:= helloworld
HOST_OPENOCD              		:= moc108
JTAG              		        := jlink
HOST_ARCH                 		:= ARM968E-S
NO_BUILD_BOOTLOADER           	:= 
NO_BOOTLOADER_REQUIRED         	:= 
alicrypto_LOCATION         := ./security/alicrypto/
auto_component_LOCATION         := ./out/helloworld@mk3060/auto_component/
cli_LOCATION         := ./tools/cli/
digest_algorithm_LOCATION         := ./utility/digest_algorithm/
hal_LOCATION         := ./kernel/hal/
helloworld_LOCATION         := ./example/helloworld/
kernel_init_LOCATION         := ./kernel/init/
vfs_device_LOCATION         := ././kernel/vfs/device/
newlib_stub_LOCATION         := ./utility/libc/
libid2_LOCATION         := ./security/libid2/
libkm_LOCATION         := ./security/libkm/
log_LOCATION         := ./utility/log/
board_mk3060_LOCATION         := ./board/mk3060/
kv_LOCATION         := ./kernel/modules/fs/kv/
plat_gen_LOCATION         := ./security/plat_gen/
armv5_LOCATION         := ././platform/arch/arm/armv5/
moc108_LOCATION         := ././platform/mcu/moc108/
app_runtime_LOCATION         := ././platform/mcu/moc108/aos/app_runtime/
framework_runtime_LOCATION         := ././platform/mcu/moc108/aos/framework_runtime/
hal_init_LOCATION         := ././platform/mcu/moc108/hal_init/
entry_LOCATION         := ././platform/mcu/moc108/mx108/mx378/driver/entry/
net_LOCATION         := ./kernel/protocols/net/
prov_LOCATION         := ./security/prov/
rhino_LOCATION         := ./kernel/rhino/
vcall_LOCATION         := ./kernel/vcall/
vfs_LOCATION         := ./kernel/vfs/
yloop_LOCATION         := ./kernel/yloop/
alicrypto_SOURCES          += ./libalicrypto/ali_crypto.c ./libalicrypto/mbed/asym/rsa.c ./libalicrypto/mbed/cipher/aes.c ./libalicrypto/mbed/hash/hash.c ./libalicrypto/mbed/mac/hmac.c ./libalicrypto/sw/ali_crypto_rand.c ./libalicrypto/test/ali_crypto_test.c ./libalicrypto/test/ali_crypto_test_aes.c ./libalicrypto/test/ali_crypto_test_comm.c ./libalicrypto/test/ali_crypto_test_hash.c ./libalicrypto/test/ali_crypto_test_hmac.c ./libalicrypto/test/ali_crypto_test_rand.c ./libalicrypto/test/ali_crypto_test_rsa.c ./mbedtls/library/aes.c ./mbedtls/library/asn1parse.c ./mbedtls/library/bignum.c ./mbedtls/library/hash_wrap.c ./mbedtls/library/hmac.c ./mbedtls/library/mbedtls_alt.c ./mbedtls/library/md5.c ./mbedtls/library/oid.c ./mbedtls/library/rsa.c ./mbedtls/library/sha1.c ./mbedtls/library/sha256.c ./mbedtls/library/threading.c 
auto_component_SOURCES          +=  component_init.c testcase_register.c
cli_SOURCES          += cli.c dumpsys.c 
digest_algorithm_SOURCES          += CheckSumUtils.c crc.c digest_algorithm.c md5.c 
hal_SOURCES          += ota.c wifi.c 
helloworld_SOURCES          += helloworld.c 
kernel_init_SOURCES          += aos_init.c 
vfs_device_SOURCES          += vfs_adc.c vfs_gpio.c vfs_i2c.c vfs_pwm.c vfs_rtc.c vfs_spi.c vfs_uart.c vfs_wdg.c 
newlib_stub_SOURCES          += newlib_stub.c 
libid2_SOURCES          += 
libkm_SOURCES          += 
log_SOURCES          += log.c 
board_mk3060_SOURCES          += board.c 
kv_SOURCES          += kvmgr.c 
plat_gen_SOURCES          += 
armv5_SOURCES          += gcc/port_c.c gcc/port_s.S 
moc108_SOURCES          += aos/aos_main.c aos/qc_test.c aos/soc_impl.c aos/trace_impl.c hal/StringUtils.c hal/flash.c hal/gpio.c hal/hw.c hal/i2c.c hal/mesh_wifi_hal.c hal/ringbuf.c hal/uart.c hal/wdg.c hal/wifi_port.c mx108/mico_api/MiCODrivers/MiCODriverFlash.c mx108/mico_api/MiCODrivers/MiCODriverGpio.c mx108/mico_api/MiCODrivers/MiCODriverPwm.c mx108/mico_api/MiCODrivers/MiCODriverUart.c mx108/mico_api/MiCODrivers/MiCODriverWdg.c mx108/mico_api/mico_cli.c mx108/mico_api/mxchipWNet.c mx108/mico_api/platform_stub.c mx108/mx378/app/app.c mx108/mx378/app/config/param_config.c mx108/mx378/app/ftp/ftpd.c mx108/mx378/app/ftp/vfs.c mx108/mx378/app/led/app_led.c mx108/mx378/app/net_work/app_lwip_tcp.c mx108/mx378/app/net_work/app_lwip_udp.c mx108/mx378/app/standalone-ap/sa_ap.c mx108/mx378/app/standalone-station/sa_station.c mx108/mx378/demo/ieee802_11_demo.c mx108/mx378/driver/common/dd.c mx108/mx378/driver/common/drv_model.c mx108/mx378/driver/dma/dma.c mx108/mx378/driver/driver.c mx108/mx378/driver/fft/fft.c mx108/mx378/driver/flash/flash.c mx108/mx378/driver/general_dma/general_dma.c mx108/mx378/driver/gpio/gpio.c mx108/mx378/driver/i2s/i2s.c mx108/mx378/driver/icu/icu.c mx108/mx378/driver/irda/irda.c mx108/mx378/driver/macphy_bypass/mac_phy_bypass.c mx108/mx378/driver/phy/phy_trident.c mx108/mx378/driver/pwm/pwm.c mx108/mx378/driver/saradc/saradc.c mx108/mx378/driver/sdcard/sdcard.c mx108/mx378/driver/sdcard/sdio_driver.c mx108/mx378/driver/sdio/sdio.c mx108/mx378/driver/sdio/sdma.c mx108/mx378/driver/sdio/sutil.c mx108/mx378/driver/spi/spi.c mx108/mx378/driver/spidma/spidma.c mx108/mx378/driver/sys_ctrl/sys_ctrl.c mx108/mx378/driver/uart/Retarget.c mx108/mx378/driver/uart/uart.c mx108/mx378/driver/usb/src/cd/mu_cntlr.c mx108/mx378/driver/usb/src/cd/mu_descs.c mx108/mx378/driver/usb/src/cd/mu_drc.c mx108/mx378/driver/usb/src/cd/mu_fc.c mx108/mx378/driver/usb/src/cd/mu_fun.c mx108/mx378/driver/usb/src/cd/mu_funex.c mx108/mx378/driver/usb/src/cd/mu_hc.c mx108/mx378/driver/usb/src/cd/mu_hdr.c mx108/mx378/driver/usb/src/cd/mu_hsdma.c mx108/mx378/driver/usb/src/cd/mu_hst.c mx108/mx378/driver/usb/src/cd/mu_list.c mx108/mx378/driver/usb/src/cd/mu_mdr.c mx108/mx378/driver/usb/src/cd/mu_pip.c mx108/mx378/driver/usb/src/drivers/comm/mu_comif.c mx108/mx378/driver/usb/src/drivers/hid/mu_hidif.c mx108/mx378/driver/usb/src/drivers/hid/mu_hidkb.c mx108/mx378/driver/usb/src/drivers/hid/mu_hidmb.c mx108/mx378/driver/usb/src/drivers/msd/mu_mapi.c mx108/mx378/driver/usb/src/drivers/msd/mu_mbot.c mx108/mx378/driver/usb/src/drivers/msd/mu_mscsi.c mx108/mx378/driver/usb/src/examples/msd/mu_msdfn.c mx108/mx378/driver/usb/src/hid/usb_hid.c mx108/mx378/driver/usb/src/lib/mu_bits.c mx108/mx378/driver/usb/src/lib/mu_stack.c mx108/mx378/driver/usb/src/lib/mu_stdio.c mx108/mx378/driver/usb/src/lib/mu_strng.c mx108/mx378/driver/usb/src/msc/usb_msd.c mx108/mx378/driver/usb/src/systems/none/afs/board.c mx108/mx378/driver/usb/src/systems/none/plat_uds.c mx108/mx378/driver/usb/src/uvc/usb_uvc.c mx108/mx378/driver/usb/src/uvc/uvc_driver.c mx108/mx378/driver/usb/usb.c mx108/mx378/driver/wdt/wdt.c mx108/mx378/func/bk7011_cal/bk7011_cal.c mx108/mx378/func/bk7011_cal/manual_cal.c mx108/mx378/func/fs_fat/disk_io.c mx108/mx378/func/fs_fat/ff.c mx108/mx378/func/fs_fat/playmode.c mx108/mx378/func/func.c mx108/mx378/func/hostapd-2.5/bk_patch/ddrv.c mx108/mx378/func/hostapd-2.5/bk_patch/signal.c mx108/mx378/func/hostapd-2.5/bk_patch/sk_intf.c mx108/mx378/func/hostapd-2.5/bk_patch/socket.c mx108/mx378/func/hostapd-2.5/src/common/hw_features_common.c mx108/mx378/func/hostapd-2.5/src/common/ieee802_11_common.c mx108/mx378/func/hostapd-2.5/src/common/wpa_common.c mx108/mx378/func/hostapd-2.5/src/crypto/aes-unwrap.c mx108/mx378/func/hostapd-2.5/src/crypto/crypto_ali.c mx108/mx378/func/hostapd-2.5/src/crypto/rc4.c mx108/mx378/func/hostapd-2.5/src/crypto/sha1-pbkdf2.c mx108/mx378/func/hostapd-2.5/src/crypto/sha1-prf.c mx108/mx378/func/hostapd-2.5/src/crypto/tls_none.c mx108/mx378/func/hostapd-2.5/src/drivers/driver_beken.c mx108/mx378/func/hostapd-2.5/src/drivers/driver_common.c mx108/mx378/func/hostapd-2.5/src/drivers/drivers.c mx108/mx378/func/hostapd-2.5/src/eap_common/eap_common.c mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server.c mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server_methods.c mx108/mx378/func/hostapd-2.5/src/eapol_auth/eapol_auth_sm.c mx108/mx378/func/hostapd-2.5/src/l2_packet/l2_packet_none.c mx108/mx378/func/hostapd-2.5/src/rsn_supp/preauth.c mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa.c mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa_ie.c mx108/mx378/func/hostapd-2.5/src/utils/common.c mx108/mx378/func/hostapd-2.5/src/utils/eloop.c mx108/mx378/func/hostapd-2.5/src/utils/os_none.c mx108/mx378/func/hostapd-2.5/src/utils/wpabuf.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/blacklist.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/bss.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/config.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/config_none.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/eap_register.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/events.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/main_supplicant.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/notify.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/wmm_ac.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_scan.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_supplicant.c mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpas_glue.c mx108/mx378/func/hostapd_intf/hostapd_intf.c mx108/mx378/func/misc/fake_clock.c mx108/mx378/func/misc/target_util.c mx108/mx378/func/mxchip/lwip-2.0.2/port/ethernetif.c mx108/mx378/func/mxchip/lwip-2.0.2/port/net.c mx108/mx378/func/rf_test/rx_sensitivity.c mx108/mx378/func/rf_test/tx_evm.c mx108/mx378/func/rwnx_intf/rw_ieee80211.c mx108/mx378/func/rwnx_intf/rw_msdu.c mx108/mx378/func/rwnx_intf/rw_msg_rx.c mx108/mx378/func/rwnx_intf/rw_msg_tx.c mx108/mx378/func/sd_music/sdcard_test.c mx108/mx378/func/sdio_intf/sdio_intf.c mx108/mx378/func/sdio_trans/sdio_trans.c mx108/mx378/func/sim_uart/gpio_uart.c mx108/mx378/func/sim_uart/pwm_uart.c mx108/mx378/func/spidma_intf/spidma_intf.c mx108/mx378/func/temp_detect/temp_detect.c mx108/mx378/func/uart_debug/cmd_evm.c mx108/mx378/func/uart_debug/cmd_help.c mx108/mx378/func/uart_debug/cmd_reg.c mx108/mx378/func/uart_debug/cmd_rx_sensitivity.c mx108/mx378/func/uart_debug/command_line.c mx108/mx378/func/uart_debug/command_table.c mx108/mx378/func/uart_debug/udebug.c mx108/mx378/func/usb/fusb.c mx108/mx378/func/user_driver/BkDriverFlash.c mx108/mx378/func/user_driver/BkDriverGpio.c mx108/mx378/func/user_driver/BkDriverPwm.c mx108/mx378/func/user_driver/BkDriverUart.c mx108/mx378/func/user_driver/BkDriverWdg.c mx108/mx378/func/wlan_ui/lsig_monitor.c mx108/mx378/func/wlan_ui/wlan_ui.c mx108/mx378/os/mem_arch.c mx108/mx378/os/str_arch.c port/ota_port.c 
app_runtime_SOURCES          += app_runtime.c 
framework_runtime_SOURCES          += framework_runtime.c 
hal_init_SOURCES          += hal_init.c 
entry_SOURCES          += ../intc/intc.c arch_main.c boot_handlers.S boot_vectors.S ll.S 
net_SOURCES          += api/api_lib.c api/api_msg.c api/err.c api/netbuf.c api/netdb.c api/netifapi.c api/sockets.c api/tcpip.c apps/tftp/tftp_client.c apps/tftp/tftp_common.c apps/tftp/tftp_ota.c apps/tftp/tftp_server.c core/def.c core/dns.c core/inet_chksum.c core/init.c core/ip.c core/ipv4/autoip.c core/ipv4/dhcp.c core/ipv4/etharp.c core/ipv4/icmp.c core/ipv4/igmp.c core/ipv4/ip4.c core/ipv4/ip4_addr.c core/ipv4/ip4_frag.c core/ipv6/dhcp6.c core/ipv6/ethip6.c core/ipv6/icmp6.c core/ipv6/inet6.c core/ipv6/ip6.c core/ipv6/ip6_addr.c core/ipv6/ip6_frag.c core/ipv6/mld6.c core/ipv6/nd6.c core/mem.c core/memp.c core/netif.c core/pbuf.c core/raw.c core/stats.c core/sys.c core/tcp.c core/tcp_in.c core/tcp_out.c core/timeouts.c core/udp.c netif/ethernet.c netif/slipif.c port/sys_arch.c 
prov_SOURCES          += 
rhino_SOURCES          += common/k_fifo.c common/k_trace.c core/k_buf_queue.c core/k_dyn_mem_proc.c core/k_err.c core/k_event.c core/k_idle.c core/k_mm.c core/k_mm_blk.c core/k_mm_debug.c core/k_mutex.c core/k_obj.c core/k_pend.c core/k_queue.c core/k_ringbuf.c core/k_sched.c core/k_sem.c core/k_stats.c core/k_sys.c core/k_task.c core/k_task_sem.c core/k_tick.c core/k_time.c core/k_timer.c core/k_workqueue.c 
vcall_SOURCES          += aos/aos_rhino.c mico/mico_rhino.c 
vfs_SOURCES          += device.c select.c vfs.c vfs_file.c vfs_inode.c vfs_register.c 
yloop_SOURCES          += local_event.c yloop.c 
alicrypto_CHECK_HEADERS    += 
auto_component_CHECK_HEADERS    += 
cli_CHECK_HEADERS    += 
digest_algorithm_CHECK_HEADERS    += 
hal_CHECK_HEADERS    += 
helloworld_CHECK_HEADERS    += 
kernel_init_CHECK_HEADERS    += 
vfs_device_CHECK_HEADERS    += 
newlib_stub_CHECK_HEADERS    += 
libid2_CHECK_HEADERS    += 
libkm_CHECK_HEADERS    += 
log_CHECK_HEADERS    += 
board_mk3060_CHECK_HEADERS    += 
kv_CHECK_HEADERS    += 
plat_gen_CHECK_HEADERS    += 
armv5_CHECK_HEADERS    += 
moc108_CHECK_HEADERS    += 
app_runtime_CHECK_HEADERS    += 
framework_runtime_CHECK_HEADERS    += 
hal_init_CHECK_HEADERS    += 
entry_CHECK_HEADERS    += 
net_CHECK_HEADERS    += 
prov_CHECK_HEADERS    += 
rhino_CHECK_HEADERS    += 
vcall_CHECK_HEADERS    += 
vfs_CHECK_HEADERS    += 
yloop_CHECK_HEADERS    += 
alicrypto_INCLUDES         := -I./security/alicrypto/./mbedtls/include/mbedtls -I./security/alicrypto/./libalicrypto/mbed/inc -I./security/alicrypto/./libalicrypto/sw -I./security/alicrypto/./mbedtls/include -I./security/alicrypto/./libalicrypto/test/inc -I./security/alicrypto/./mbedtls/include/mbedtls -I./security/alicrypto/./libalicrypto/mbed/inc -I./security/alicrypto/./libalicrypto/sw -I./security/alicrypto/./mbedtls/include -I./security/alicrypto/./libalicrypto/test/inc
auto_component_INCLUDES         := 
cli_INCLUDES         := 
digest_algorithm_INCLUDES         := 
hal_INCLUDES         := 
helloworld_INCLUDES         := 
kernel_init_INCLUDES         := 
vfs_device_INCLUDES         := -I././kernel/vfs/device/../include/device/ -I././kernel/vfs/device/../include/ -I././kernel/vfs/device/../../hal/soc/ -I././kernel/vfs/device/../include/device/ -I././kernel/vfs/device/../include/ -I././kernel/vfs/device/../../hal/soc/
newlib_stub_INCLUDES         := 
libid2_INCLUDES         := 
libkm_INCLUDES         := 
log_INCLUDES         := 
board_mk3060_INCLUDES         := 
kv_INCLUDES         := 
plat_gen_INCLUDES         := 
armv5_INCLUDES         := 
moc108_INCLUDES         := -I././platform/mcu/moc108/mx108/mx378/ip/common -I././platform/mcu/moc108/mx108/mx378/func/rf_test -I././platform/mcu/moc108/mx108/mico_api -I././platform/mcu/moc108/mx108/mx378/func/user_driver -I././platform/mcu/moc108/mx108/mico_api -I././platform/mcu/moc108/mx108/mico_api/MiCODrivers -I././platform/mcu/moc108/mx108/mx378/func/power_save -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/hal -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/mm -I././platform/mcu/moc108/mx108/mx378/driver/common/reg -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/ps -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/rd -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/rwnx -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/rx -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/scan -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/sta -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/tx -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/vif -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/rx/rxl -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/tx/txl -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/bam -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/llc -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/me -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/rxu -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/scanu -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/sm -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/txu -I././platform/mcu/moc108/mx108/mx378/ip/ke -I././platform/mcu/moc108/mx108/mx378/ip/mac -I././platform/mcu/moc108/mx108/mx378/driver/sdio -I././platform/mcu/moc108/mx108/mx378/driver/common -I././platform/mcu/moc108/mx108/mx378/driver/include -I././platform/mcu/moc108/mx108/mx378/driver/uart -I././platform/mcu/moc108/mx108/mx378/driver/sys_ctrl -I././platform/mcu/moc108/mx108/mx378/func/sdio_intf -I././platform/mcu/moc108/mx108/mx378/driver/gpio -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/p2p -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/apm -I././platform/mcu/moc108/mx108/mx378/driver/sdcard -I././platform/mcu/moc108/mx108/mx378/common -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/chan -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/td -I././platform/mcu/moc108/mx108/mx378/driver/common/reg -I././platform/mcu/moc108/mx108/mx378/driver/entry -I././platform/mcu/moc108/mx108/mx378/driver/dma -I././platform/mcu/moc108/mx108/mx378/driver/intc -I././platform/mcu/moc108/mx108/mx378/driver/phy -I././platform/mcu/moc108/mx108/mx378/driver/rc_beken -I././platform/mcu/moc108/mx108/mx378/func/sd_music -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/src/utils -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/src -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/bk_patch -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/src/ap -I././platform/mcu/moc108/mx108/mx378/app/standalone-ap -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/hostapd -I././platform/mcu/moc108/mx108/mx378/func/ethernet_intf -I././platform/mcu/moc108/mx108/mx378/app/standalone-station -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/src/common -I././platform/mcu/moc108/mx108/mx378/func/hostapd-2.5/src/drivers -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/systems/none -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/systems/none/afs -I././platform/mcu/moc108/mx108/mx378/driver/usb/include -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/cd -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/drivers/msd -I././platform/mcu/moc108/mx108/mx378/driver/usb/include/class -I././platform/mcu/moc108/mx108/mx378/app/net_work -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/msc -I././platform/mcu/moc108/mx108/mx378/driver/usb -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/hid -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/drivers/hid -I././platform/mcu/moc108/mx108/mx378/driver/usb/src/uvc -I././platform/mcu/moc108/mx108/mx378/func/temp_detect -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/tpc -I././platform/mcu/moc108/mx108/mx378/ip/lmac/src/tdls -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/mesh -I././platform/mcu/moc108/mx108/mx378/ip/umac/src/rc -I././platform/mcu/moc108/mx108/mx378/func/spidma_intf -I././platform/mcu/moc108/mx108/mx378/driver/general_dma -I././platform/mcu/moc108/mx108/mx378/driver/spidma -I././platform/mcu/moc108/mx108/mx378/func/rwnx_intf -I././platform/mcu/moc108/mx108/mx378/app -I././platform/mcu/moc108/mx108/mx378/app/ftp -I././platform/mcu/moc108/mx108/mx378/app/led -I././platform/mcu/moc108/aos
app_runtime_INCLUDES         := 
framework_runtime_INCLUDES         := 
hal_init_INCLUDES         := 
entry_INCLUDES         := -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../app -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../app/config -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../func/include -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../os/include -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../ip/lmac/src/rwnx -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../ip/ke -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../ip/mac -I././platform/mcu/moc108/mx108/mx378/driver/entry/../../../../aos
net_INCLUDES         := -I./kernel/protocols/net/port/include -I./kernel/protocols/net/port/include
prov_INCLUDES         := 
rhino_INCLUDES         := 
vcall_INCLUDES         := 
vfs_INCLUDES         := 
yloop_INCLUDES         := 
alicrypto_DEFINES          := 
auto_component_DEFINES          := 
cli_DEFINES          := 
digest_algorithm_DEFINES          := 
hal_DEFINES          := 
helloworld_DEFINES          := 
kernel_init_DEFINES          := 
vfs_device_DEFINES          := 
newlib_stub_DEFINES          := 
libid2_DEFINES          := 
libkm_DEFINES          := 
log_DEFINES          := 
board_mk3060_DEFINES          := 
kv_DEFINES          := 
plat_gen_DEFINES          := 
armv5_DEFINES          := 
moc108_DEFINES          := 
app_runtime_DEFINES          := 
framework_runtime_DEFINES          := 
hal_init_DEFINES          := 
entry_DEFINES          := 
net_DEFINES          := 
prov_DEFINES          := 
rhino_DEFINES          := 
vcall_DEFINES          := 
vfs_DEFINES          := 
yloop_DEFINES          := 
alicrypto_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -DCONFIG_CRYPT_MBED=1 -DCONFIG_DBG_CRYPT=1 -W -Wdeclaration-after-statement  -D_FILE_OFFSET_BITS=64 -DCONFIG_CRYPT_MBED=1 -DCONFIG_DBG_CRYPT=1 -W -Wdeclaration-after-statement  -D_FILE_OFFSET_BITS=64
auto_component_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
cli_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
digest_algorithm_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
hal_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
helloworld_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
kernel_init_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
vfs_device_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
newlib_stub_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
libid2_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Os -Wall -Werror -Os
libkm_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Os -Wall -Werror -Os
log_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
board_mk3060_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
kv_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
plat_gen_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Os -Wall -Werror -Os
armv5_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -marm -mthumb-interwork
moc108_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-strict-aliasing
app_runtime_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
framework_runtime_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
hal_init_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
entry_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -marm -marm
net_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
prov_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Os -Wall -Werror -Os
rhino_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
vcall_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -marm -Wall -Werror -marm
vfs_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
yloop_CFLAGS           :=             -DSYSINFO_PRODUCT_MODEL=\"ALI_AOS_MK3060\" -DSYSINFO_DEVICE_NAME=\"MK3060\"    -mcpu=arm968e-s -march=armv5te -mthumb -mthumb-interwork -mlittle-endian -w       -DSYSINFO_KERNEL_VERSION=\"AOS-R-1.2.1\"    -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -std=gnu11  -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" -Wall -Werror -Wall -Werror
alicrypto_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
auto_component_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
cli_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
digest_algorithm_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
hal_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
helloworld_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
kernel_init_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
vfs_device_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
newlib_stub_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
libid2_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
libkm_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
log_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
board_mk3060_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
kv_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
plat_gen_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
armv5_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
moc108_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
app_runtime_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
framework_runtime_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
hal_init_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
entry_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
net_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
prov_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
rhino_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
vcall_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
vfs_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
yloop_CXXFLAGS         :=                            -ggdb -Os -Wall -Wfatal-errors -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-rtti -fno-exceptions   -DAOS_SDK_VERSION_MAJOR=3 -DAOS_SDK_VERSION_MINOR=2 -DAOS_SDK_VERSION_REVISION=3 -Iout/helloworld@mk3060/resources/ -DPLATFORM=\"mk3060\" 
alicrypto_ASMFLAGS         :=                            -ggdb
auto_component_ASMFLAGS         :=                            -ggdb
cli_ASMFLAGS         :=                            -ggdb
digest_algorithm_ASMFLAGS         :=                            -ggdb
hal_ASMFLAGS         :=                            -ggdb
helloworld_ASMFLAGS         :=                            -ggdb
kernel_init_ASMFLAGS         :=                            -ggdb
vfs_device_ASMFLAGS         :=                            -ggdb
newlib_stub_ASMFLAGS         :=                            -ggdb
libid2_ASMFLAGS         :=                            -ggdb
libkm_ASMFLAGS         :=                            -ggdb
log_ASMFLAGS         :=                            -ggdb
board_mk3060_ASMFLAGS         :=                            -ggdb
kv_ASMFLAGS         :=                            -ggdb
plat_gen_ASMFLAGS         :=                            -ggdb
armv5_ASMFLAGS         :=                            -ggdb
moc108_ASMFLAGS         :=                            -ggdb
app_runtime_ASMFLAGS         :=                            -ggdb
framework_runtime_ASMFLAGS         :=                            -ggdb
hal_init_ASMFLAGS         :=                            -ggdb
entry_ASMFLAGS         :=                            -ggdb
net_ASMFLAGS         :=                            -ggdb
prov_ASMFLAGS         :=                            -ggdb
rhino_ASMFLAGS         :=                            -ggdb
vcall_ASMFLAGS         :=                            -ggdb
vfs_ASMFLAGS         :=                            -ggdb
yloop_ASMFLAGS         :=                            -ggdb
alicrypto_RESOURCES        := 
auto_component_RESOURCES        := 
cli_RESOURCES        := 
digest_algorithm_RESOURCES        := 
hal_RESOURCES        := 
helloworld_RESOURCES        := 
kernel_init_RESOURCES        := 
vfs_device_RESOURCES        := 
newlib_stub_RESOURCES        := 
libid2_RESOURCES        := 
libkm_RESOURCES        := 
log_RESOURCES        := 
board_mk3060_RESOURCES        := 
kv_RESOURCES        := 
plat_gen_RESOURCES        := 
armv5_RESOURCES        := 
moc108_RESOURCES        := 
app_runtime_RESOURCES        := 
framework_runtime_RESOURCES        := 
hal_init_RESOURCES        := 
entry_RESOURCES        := 
net_RESOURCES        := 
prov_RESOURCES        := 
rhino_RESOURCES        := 
vcall_RESOURCES        := 
vfs_RESOURCES        := 
yloop_RESOURCES        := 
alicrypto_MAKEFILE         := ./security/alicrypto/alicrypto.mk
auto_component_MAKEFILE         := ./out/helloworld@mk3060/auto_component/auto_component.mk
cli_MAKEFILE         := ./tools/cli/cli.mk
digest_algorithm_MAKEFILE         := ./utility/digest_algorithm/digest_algorithm.mk
hal_MAKEFILE         := ./kernel/hal/hal.mk
helloworld_MAKEFILE         := ./example/helloworld/helloworld.mk
kernel_init_MAKEFILE         := ./kernel/init/init.mk
vfs_device_MAKEFILE         := ././kernel/vfs/device/device.mk
newlib_stub_MAKEFILE         := ./utility/libc/libc.mk
libid2_MAKEFILE         := ./security/libid2/libid2.mk
libkm_MAKEFILE         := ./security/libkm/libkm.mk
log_MAKEFILE         := ./utility/log/log.mk
board_mk3060_MAKEFILE         := ./board/mk3060/mk3060.mk
kv_MAKEFILE         := ./kernel/modules/fs/kv/kv.mk
plat_gen_MAKEFILE         := ./security/plat_gen/plat_gen.mk
armv5_MAKEFILE         := ././platform/arch/arm/armv5/armv5.mk
moc108_MAKEFILE         := ././platform/mcu/moc108/moc108.mk
app_runtime_MAKEFILE         := ././platform/mcu/moc108/aos/app_runtime/app_runtime.mk
framework_runtime_MAKEFILE         := ././platform/mcu/moc108/aos/framework_runtime/framework_runtime.mk
hal_init_MAKEFILE         := ././platform/mcu/moc108/hal_init/hal_init.mk
entry_MAKEFILE         := ././platform/mcu/moc108/mx108/mx378/driver/entry/entry.mk
net_MAKEFILE         := ./kernel/protocols/net/net.mk
prov_MAKEFILE         := ./security/prov/prov.mk
rhino_MAKEFILE         := ./kernel/rhino/rhino.mk
vcall_MAKEFILE         := ./kernel/vcall/vcall.mk
vfs_MAKEFILE         := ./kernel/vfs/vfs.mk
yloop_MAKEFILE         := ./kernel/yloop/yloop.mk
alicrypto_PRE_BUILD_TARGETS:= 
auto_component_PRE_BUILD_TARGETS:= 
cli_PRE_BUILD_TARGETS:= 
digest_algorithm_PRE_BUILD_TARGETS:= 
hal_PRE_BUILD_TARGETS:= 
helloworld_PRE_BUILD_TARGETS:= 
kernel_init_PRE_BUILD_TARGETS:= 
vfs_device_PRE_BUILD_TARGETS:= 
newlib_stub_PRE_BUILD_TARGETS:= 
libid2_PRE_BUILD_TARGETS:= 
libkm_PRE_BUILD_TARGETS:= 
log_PRE_BUILD_TARGETS:= 
board_mk3060_PRE_BUILD_TARGETS:= 
kv_PRE_BUILD_TARGETS:= 
plat_gen_PRE_BUILD_TARGETS:= 
armv5_PRE_BUILD_TARGETS:= 
moc108_PRE_BUILD_TARGETS:= 
app_runtime_PRE_BUILD_TARGETS:= 
framework_runtime_PRE_BUILD_TARGETS:= 
hal_init_PRE_BUILD_TARGETS:= 
entry_PRE_BUILD_TARGETS:= 
net_PRE_BUILD_TARGETS:= 
prov_PRE_BUILD_TARGETS:= 
rhino_PRE_BUILD_TARGETS:= 
vcall_PRE_BUILD_TARGETS:= 
vfs_PRE_BUILD_TARGETS:= 
yloop_PRE_BUILD_TARGETS:= 
alicrypto_PREBUILT_LIBRARY := 
auto_component_PREBUILT_LIBRARY := 
cli_PREBUILT_LIBRARY := 
digest_algorithm_PREBUILT_LIBRARY := 
hal_PREBUILT_LIBRARY := 
helloworld_PREBUILT_LIBRARY := 
kernel_init_PREBUILT_LIBRARY := 
vfs_device_PREBUILT_LIBRARY := 
newlib_stub_PREBUILT_LIBRARY := 
libid2_PREBUILT_LIBRARY := ./security/libid2/lib/ARM968E-S/libid2.a
libkm_PREBUILT_LIBRARY := ./security/libkm/lib/ARM968E-S/libkm.a
log_PREBUILT_LIBRARY := 
board_mk3060_PREBUILT_LIBRARY := 
kv_PREBUILT_LIBRARY := 
plat_gen_PREBUILT_LIBRARY := ./security/plat_gen/lib/ARM968E-S/libplat_gen.a
armv5_PREBUILT_LIBRARY := 
moc108_PREBUILT_LIBRARY := ././platform/mcu/moc108/librwnx/librwnx.a
app_runtime_PREBUILT_LIBRARY := 
framework_runtime_PREBUILT_LIBRARY := 
hal_init_PREBUILT_LIBRARY := 
entry_PREBUILT_LIBRARY := 
net_PREBUILT_LIBRARY := 
prov_PREBUILT_LIBRARY := ./security/prov/lib/ARM968E-S/libprov.a
rhino_PREBUILT_LIBRARY := 
vcall_PREBUILT_LIBRARY := 
vfs_PREBUILT_LIBRARY := 
yloop_PREBUILT_LIBRARY := 
alicrypto_TYPE             := 
auto_component_TYPE             := kernel
cli_TYPE             := kernel
digest_algorithm_TYPE             := share
hal_TYPE             := kernel
helloworld_TYPE             := 
kernel_init_TYPE             := kernel
vfs_device_TYPE             := 
newlib_stub_TYPE             := share
libid2_TYPE             := 
libkm_TYPE             := 
log_TYPE             := share
board_mk3060_TYPE             := kernel
kv_TYPE             := kernel
plat_gen_TYPE             := 
armv5_TYPE             := kernel
moc108_TYPE             := kernel
app_runtime_TYPE             := app
framework_runtime_TYPE             := framework
hal_init_TYPE             := kernel
entry_TYPE             := kernel
net_TYPE             := kernel
prov_TYPE             := 
rhino_TYPE             := kernel
vcall_TYPE             := kernel
vfs_TYPE             := kernel
yloop_TYPE             := kernel
AOS_SDK_UNIT_TEST_SOURCES   		:=                                                      
ALL_RESOURCES             		:= 
INTERNAL_MEMORY_RESOURCES 		:= 
EXTRA_TARGET_MAKEFILES 			:=       .//build/aos_standard_targets.mk .//platform/mcu/moc108/gen_crc_bin.mk
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
