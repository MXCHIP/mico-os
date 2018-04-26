NAME := moc108

HOST_OPENOCD := moc108

ifeq ($(CONFIG_SOFTAP),1)
GLOBAL_CFLAGS += -DCONFIG_SOFTAP
endif

$(NAME)_TYPE := kernel

$(NAME)_COMPONENTS := platform/arch/arm/armv5
$(NAME)_COMPONENTS += libc rhino yloop modules.fs.kv alicrypto digest_algorithm
$(NAME)_COMPONENTS += platform/mcu/moc108/hal_init
$(NAME)_COMPONENTS += platform/mcu/moc108/mx108/mx378/driver/entry
$(NAME)_COMPONENTS += platform/mcu/moc108/aos/framework_runtime
$(NAME)_COMPONENTS += platform/mcu/moc108/aos/app_runtime
$(NAME)_COMPONENTS += prov
$(NAME)_COMPONENTS += hal


GLOBAL_DEFINES += CONFIG_MX108
GLOBAL_DEFINES += CONFIG_AOS_KV_MULTIPTN_MODE
GLOBAL_DEFINES += CONFIG_AOS_KV_PTN=6
GLOBAL_DEFINES += CONFIG_AOS_KV_SECOND_PTN=7
GLOBAL_DEFINES += CONFIG_AOS_KV_PTN_SIZE=4096
GLOBAL_DEFINES += CONFIG_AOS_KV_BUFFER_SIZE=8192
GLOBAL_DEFINES += CONFIG_AOS_CLI_BOARD
GLOBAL_DEFINES += CONFIG_AOS_FOTA_BREAKPOINT

GLOBAL_CFLAGS += -mcpu=arm968e-s \
                 -march=armv5te \
                 -mthumb -mthumb-interwork \
                 -mlittle-endian

GLOBAL_CFLAGS += -w

$(NAME)_CFLAGS  += -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration
$(NAME)_CFLAGS  += -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized
$(NAME)_CFLAGS  += -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable
$(NAME)_CFLAGS  += -Wno-unused-value -Wno-strict-aliasing


GLOBAL_INCLUDES += mx108/mx378/func/mxchip/lwip-2.0.2/port \
                   mx108/mx378/common \
                   mx108/mx378/app/config \
                   mx108/mx378/func/include \
                   mx108/mx378/os/include \
                   mx108/mx378/driver/include \
                   mx108/mx378/driver/common \
                   mx108/mx378/ip/common

$(NAME)_COMPONENTS += protocols.net

GLOBAL_LDFLAGS += -mcpu=arm968e-s \
                 -march=armv5te \
                 -mthumb -mthumb-interwork\
                 -mlittle-endian \
                 --specs=nosys.specs \
                 -nostartfiles \
                 $(CLIB_LDFLAGS_NANO_FLOAT)


BINS ?=

ifeq ($(APP),bootloader)
GLOBAL_LDFLAGS += -T platform/mcu/moc108/mx108/mx378/build/mx108_boot.ld
else

ifeq ($(BINS),)
GLOBAL_LDS_FILES += platform/mcu/moc108/mx108/mx378/build/mx108.ld.S
else ifeq ($(BINS),app)
GLOBAL_LDS_FILES += platform/mcu/moc108/mx108/mx378/build/mx108_app.ld.S
else ifeq ($(BINS),framework)
GLOBAL_LDS_FILES += platform/mcu/moc108/mx108/mx378/build/mx108_framework.ld.S
else ifeq ($(BINS),kernel)
GLOBAL_LDS_FILES += platform/mcu/moc108/mx108/mx378/build/mx108_kernel.ld.S
endif

endif

$(NAME)_INCLUDES := mx108/mx378/ip/common \
                    mx108/mx378/func/rf_test \
                    mx108/mico_api \
                    mx108/mx378/func/user_driver \
                    mx108/mico_api \
                    mx108/mico_api/MiCODrivers \
                    mx108/mx378/func/power_save \
                    mx108/mx378/ip/lmac/src/hal \
                    mx108/mx378/ip/lmac/src/mm \
                    mx108/mx378/driver/common/reg \
                    mx108/mx378/ip/lmac/src/ps \
                    mx108/mx378/ip/lmac/src/rd \
                    mx108/mx378/ip/lmac/src/rwnx \
                    mx108/mx378/ip/lmac/src/rx \
                    mx108/mx378/ip/lmac/src/scan \
                    mx108/mx378/ip/lmac/src/sta \
                    mx108/mx378/ip/lmac/src/tx \
                    mx108/mx378/ip/lmac/src/vif \
                    mx108/mx378/ip/lmac/src/rx/rxl \
                    mx108/mx378/ip/lmac/src/tx/txl \
                    mx108/mx378/ip/umac/src/bam \
                    mx108/mx378/ip/umac/src/llc \
                    mx108/mx378/ip/umac/src/me \
                    mx108/mx378/ip/umac/src/rxu \
                    mx108/mx378/ip/umac/src/scanu \
                    mx108/mx378/ip/umac/src/sm \
                    mx108/mx378/ip/umac/src/txu \
                    mx108/mx378/ip/ke \
                    mx108/mx378/ip/mac \
                    mx108/mx378/driver/sdio \
                    mx108/mx378/driver/common \
                    mx108/mx378/driver/include \
                    mx108/mx378/driver/uart \
                    mx108/mx378/driver/sys_ctrl \
                    mx108/mx378/func/sdio_intf \
                    mx108/mx378/driver/gpio \
                    mx108/mx378/ip/lmac/src/p2p \
                    mx108/mx378/ip/umac/src/apm \
                    mx108/mx378/driver/sdcard \
                    mx108/mx378/common \
                    mx108/mx378/ip/lmac/src/chan \
                    mx108/mx378/ip/lmac/src/td \
                    mx108/mx378/driver/common/reg \
                    mx108/mx378/driver/entry \
                    mx108/mx378/driver/dma \
                    mx108/mx378/driver/intc \
                    mx108/mx378/driver/phy \
                    mx108/mx378/driver/rc_beken \
                    mx108/mx378/func/sd_music \
                    mx108/mx378/func/hostapd-2.5/src/utils \
                    mx108/mx378/func/hostapd-2.5/src \
                    mx108/mx378/func/hostapd-2.5/bk_patch \
                    mx108/mx378/func/hostapd-2.5/src/ap \
                    mx108/mx378/app/standalone-ap \
                    mx108/mx378/func/hostapd-2.5/hostapd \
                    mx108/mx378/func/ethernet_intf \
                    mx108/mx378/app/standalone-station \
                    mx108/mx378/func/hostapd-2.5/src/common \
                    mx108/mx378/func/hostapd-2.5/src/drivers \
                    mx108/mx378/driver/usb/src/systems/none \
                    mx108/mx378/driver/usb/src/systems/none/afs \
                    mx108/mx378/driver/usb/include \
                    mx108/mx378/driver/usb/src/cd \
                    mx108/mx378/driver/usb/src/drivers/msd \
                    mx108/mx378/driver/usb/include/class \
                    mx108/mx378/app/net_work \
                    mx108/mx378/driver/usb/src/msc \
                    mx108/mx378/driver/usb \
                    mx108/mx378/driver/usb/src/hid \
                    mx108/mx378/driver/usb/src/drivers/hid \
                    mx108/mx378/driver/usb/src/uvc \
                    mx108/mx378/func/temp_detect \
                    mx108/mx378/ip/lmac/src/tpc \
                    mx108/mx378/ip/lmac/src/tdls \
                    mx108/mx378/ip/umac/src/mesh \
                    mx108/mx378/ip/umac/src/rc \
                    mx108/mx378/func/spidma_intf \
                    mx108/mx378/driver/general_dma \
                    mx108/mx378/driver/spidma \
                    mx108/mx378/func/rwnx_intf \
                    mx108/mx378/app \
                    mx108/mx378/app/ftp \
                    mx108/mx378/app/led


$(NAME)_SOURCES :=  aos/aos_main.c

$(NAME)_SOURCES += aos/qc_test.c

$(NAME)_INCLUDES += aos
                    
$(NAME)_SOURCES +=  mx108/mx378/app/app.c \
                    mx108/mx378/app/config/param_config.c \
                    mx108/mx378/app/ftp/ftpd.c \
                    mx108/mx378/app/ftp/vfs.c \
                    mx108/mx378/app/led/app_led.c \
                    mx108/mx378/app/net_work/app_lwip_tcp.c \
                    mx108/mx378/app/net_work/app_lwip_udp.c \
                    mx108/mx378/app/standalone-ap/sa_ap.c \
                    mx108/mx378/app/standalone-station/sa_station.c \
                    mx108/mx378/demo/ieee802_11_demo.c \
                    mx108/mx378/driver/common/dd.c \
                    mx108/mx378/driver/common/drv_model.c \
                    mx108/mx378/driver/dma/dma.c \
                    mx108/mx378/driver/driver.c \
                    mx108/mx378/driver/fft/fft.c \
                    mx108/mx378/driver/flash/flash.c \
                    mx108/mx378/driver/general_dma/general_dma.c \
                    mx108/mx378/driver/gpio/gpio.c \
                    mx108/mx378/driver/i2s/i2s.c \
                    mx108/mx378/driver/icu/icu.c \
                    mx108/mx378/driver/irda/irda.c \
                    mx108/mx378/driver/macphy_bypass/mac_phy_bypass.c \
                    mx108/mx378/driver/phy/phy_trident.c \
                    mx108/mx378/driver/pwm/pwm.c \
                    mx108/mx378/driver/saradc/saradc.c \
                    mx108/mx378/driver/sdcard/sdcard.c \
                    mx108/mx378/driver/sdcard/sdio_driver.c \
                    mx108/mx378/driver/sdio/sdio.c \
                    mx108/mx378/driver/sdio/sdma.c \
                    mx108/mx378/driver/sdio/sutil.c \
                    mx108/mx378/driver/spi/spi.c \
                    mx108/mx378/driver/spidma/spidma.c \
                    mx108/mx378/driver/sys_ctrl/sys_ctrl.c \
                    mx108/mx378/driver/uart/Retarget.c \
                    mx108/mx378/driver/uart/uart.c \
                    mx108/mx378/driver/usb/src/cd/mu_cntlr.c \
                    mx108/mx378/driver/usb/src/cd/mu_descs.c \
                    mx108/mx378/driver/usb/src/cd/mu_drc.c \
                    mx108/mx378/driver/usb/src/cd/mu_fc.c \
                    mx108/mx378/driver/usb/src/cd/mu_fun.c \
                    mx108/mx378/driver/usb/src/cd/mu_funex.c \
                    mx108/mx378/driver/usb/src/cd/mu_hc.c \
                    mx108/mx378/driver/usb/src/cd/mu_hdr.c \
                    mx108/mx378/driver/usb/src/cd/mu_hsdma.c \
                    mx108/mx378/driver/usb/src/cd/mu_hst.c \
                    mx108/mx378/driver/usb/src/cd/mu_list.c \
                    mx108/mx378/driver/usb/src/cd/mu_mdr.c \
                    mx108/mx378/driver/usb/src/cd/mu_pip.c \
                    mx108/mx378/driver/usb/src/drivers/comm/mu_comif.c \
                    mx108/mx378/driver/usb/src/drivers/hid/mu_hidif.c \
                    mx108/mx378/driver/usb/src/drivers/hid/mu_hidkb.c \
                    mx108/mx378/driver/usb/src/drivers/hid/mu_hidmb.c \
                    mx108/mx378/driver/usb/src/drivers/msd/mu_mapi.c \
                    mx108/mx378/driver/usb/src/drivers/msd/mu_mbot.c \
                    mx108/mx378/driver/usb/src/drivers/msd/mu_mscsi.c \
                    mx108/mx378/driver/usb/src/examples/msd/mu_msdfn.c \
                    mx108/mx378/driver/usb/src/hid/usb_hid.c \
                    mx108/mx378/driver/usb/src/lib/mu_bits.c \
                    mx108/mx378/driver/usb/src/lib/mu_stack.c \
                    mx108/mx378/driver/usb/src/lib/mu_stdio.c \
                    mx108/mx378/driver/usb/src/lib/mu_strng.c \
                    mx108/mx378/driver/usb/src/msc/usb_msd.c \
                    mx108/mx378/driver/usb/src/systems/none/afs/board.c \
                    mx108/mx378/driver/usb/src/systems/none/plat_uds.c \
                    mx108/mx378/driver/usb/src/uvc/usb_uvc.c \
                    mx108/mx378/driver/usb/src/uvc/uvc_driver.c \
                    mx108/mx378/driver/usb/usb.c \
                    mx108/mx378/driver/wdt/wdt.c \
                    mx108/mx378/func/bk7011_cal/bk7011_cal.c \
                    mx108/mx378/func/bk7011_cal/manual_cal.c \
                    mx108/mx378/func/fs_fat/disk_io.c \
                    mx108/mx378/func/fs_fat/ff.c \
                    mx108/mx378/func/fs_fat/playmode.c \
                    mx108/mx378/func/func.c \
                    mx108/mx378/func/hostapd-2.5/bk_patch/ddrv.c \
                    mx108/mx378/func/hostapd-2.5/bk_patch/signal.c \
                    mx108/mx378/func/hostapd-2.5/bk_patch/sk_intf.c \
                    mx108/mx378/func/hostapd-2.5/bk_patch/socket.c \
                    mx108/mx378/func/hostapd-2.5/src/common/hw_features_common.c \
                    mx108/mx378/func/hostapd-2.5/src/common/ieee802_11_common.c \
                    mx108/mx378/func/hostapd-2.5/src/common/wpa_common.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/aes-unwrap.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/rc4.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/sha1-pbkdf2.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/sha1-prf.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/tls_none.c \
                    mx108/mx378/func/hostapd-2.5/src/drivers/driver_beken.c \
                    mx108/mx378/func/hostapd-2.5/src/drivers/driver_common.c \
                    mx108/mx378/func/hostapd-2.5/src/drivers/drivers.c \
                    mx108/mx378/func/hostapd-2.5/src/eap_common/eap_common.c \
                    mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server.c \
                    mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server_methods.c \
                    mx108/mx378/func/hostapd-2.5/src/eapol_auth/eapol_auth_sm.c \
                    mx108/mx378/func/hostapd-2.5/src/l2_packet/l2_packet_none.c \
                    mx108/mx378/func/hostapd-2.5/src/rsn_supp/preauth.c \
                    mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa.c \
                    mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa_ie.c \
                    mx108/mx378/func/hostapd-2.5/src/utils/common.c \
                    mx108/mx378/func/hostapd-2.5/src/utils/eloop.c \
                    mx108/mx378/func/hostapd-2.5/src/utils/os_none.c \
                    mx108/mx378/func/hostapd-2.5/src/utils/wpabuf.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/blacklist.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/bss.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/config.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/config_none.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/eap_register.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/events.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/main_supplicant.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/notify.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/wmm_ac.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_scan.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_supplicant.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpas_glue.c \
                    mx108/mx378/func/hostapd_intf/hostapd_intf.c \
                    mx108/mx378/func/misc/fake_clock.c \
                    mx108/mx378/func/misc/target_util.c \
                    mx108/mx378/func/mxchip/lwip-2.0.2/port/ethernetif.c \
                    mx108/mx378/func/mxchip/lwip-2.0.2/port/net.c \
                    mx108/mx378/func/rf_test/rx_sensitivity.c \
                    mx108/mx378/func/rf_test/tx_evm.c \
                    mx108/mx378/func/rwnx_intf/rw_ieee80211.c \
                    mx108/mx378/func/rwnx_intf/rw_msdu.c \
                    mx108/mx378/func/rwnx_intf/rw_msg_rx.c \
                    mx108/mx378/func/rwnx_intf/rw_msg_tx.c \
                    mx108/mx378/func/sd_music/sdcard_test.c \
                    mx108/mx378/func/sdio_intf/sdio_intf.c \
                    mx108/mx378/func/sdio_trans/sdio_trans.c \
                    mx108/mx378/func/sim_uart/gpio_uart.c \
                    mx108/mx378/func/sim_uart/pwm_uart.c \
                    mx108/mx378/func/spidma_intf/spidma_intf.c \
                    mx108/mx378/func/temp_detect/temp_detect.c \
                    mx108/mx378/func/uart_debug/cmd_evm.c \
                    mx108/mx378/func/uart_debug/cmd_help.c \
                    mx108/mx378/func/uart_debug/cmd_reg.c \
                    mx108/mx378/func/uart_debug/cmd_rx_sensitivity.c \
                    mx108/mx378/func/uart_debug/command_line.c \
                    mx108/mx378/func/uart_debug/command_table.c \
                    mx108/mx378/func/uart_debug/udebug.c \
                    mx108/mx378/func/usb/fusb.c \
                    mx108/mx378/func/user_driver/BkDriverFlash.c \
                    mx108/mx378/func/user_driver/BkDriverGpio.c \
                    mx108/mx378/func/user_driver/BkDriverPwm.c \
                    mx108/mx378/func/user_driver/BkDriverUart.c \
                    mx108/mx378/func/user_driver/BkDriverWdg.c \
                    mx108/mx378/func/wlan_ui/wlan_ui.c \
                    mx108/mx378/func/wlan_ui/lsig_monitor.c \
                    mx108/mx378/os/mem_arch.c \
                    mx108/mx378/os/str_arch.c \
                    mx108/mico_api/MiCODrivers/MiCODriverFlash.c \
                    mx108/mico_api/MiCODrivers/MiCODriverGpio.c \
                    mx108/mico_api/MiCODrivers/MiCODriverPwm.c \
                    mx108/mico_api/MiCODrivers/MiCODriverUart.c \
                    mx108/mico_api/MiCODrivers/MiCODriverWdg.c \
                    mx108/mico_api/mico_cli.c \
                    mx108/mico_api/mxchipWNet.c \
                    mx108/mico_api/platform_stub.c \
                    aos/soc_impl.c \
                    aos/trace_impl.c

ifneq ($(wildcard $(CURDIR)librwnx),)
ifeq ($(bkdebug),1)
$(NAME)_PREBUILT_LIBRARY := librwnx/librwnx_debug.a
else
$(NAME)_PREBUILT_LIBRARY := librwnx/librwnx.a
endif
else
$(NAME)_SOURCES	 += mx108/mx378/ip/common/co_dlist.c \
                    mx108/mx378/ip/common/co_list.c \
                    mx108/mx378/ip/common/co_math.c \
                    mx108/mx378/ip/common/co_pool.c \
                    mx108/mx378/ip/common/co_ring.c \
                    mx108/mx378/ip/ke/ke_env.c \
                    mx108/mx378/ip/ke/ke_event.c \
                    mx108/mx378/ip/ke/ke_msg.c \
                    mx108/mx378/ip/ke/ke_queue.c \
                    mx108/mx378/ip/ke/ke_task.c \
                    mx108/mx378/ip/ke/ke_timer.c \
                    mx108/mx378/ip/lmac/src/chan/chan.c \
                    mx108/mx378/ip/lmac/src/hal/hal_desc.c \
                    mx108/mx378/ip/lmac/src/hal/hal_dma.c \
                    mx108/mx378/ip/lmac/src/hal/hal_machw.c \
                    mx108/mx378/ip/lmac/src/hal/hal_mib.c \
                    mx108/mx378/ip/lmac/src/mm/mm.c \
                    mx108/mx378/ip/lmac/src/mm/mm_bcn.c \
                    mx108/mx378/ip/lmac/src/mm/mm_task.c \
                    mx108/mx378/ip/lmac/src/mm/mm_timer.c \
                    mx108/mx378/ip/lmac/src/p2p/p2p.c \
                    mx108/mx378/ip/lmac/src/ps/ps.c \
                    mx108/mx378/ip/lmac/src/rd/rd.c \
                    mx108/mx378/ip/lmac/src/rwnx/rwnx.c \
                    mx108/mx378/ip/lmac/src/rx/rx_swdesc.c \
                    mx108/mx378/ip/lmac/src/rx/rxl/rxl_cntrl.c \
                    mx108/mx378/ip/lmac/src/rx/rxl/rxl_hwdesc.c \
                    mx108/mx378/ip/lmac/src/scan/scan.c \
                    mx108/mx378/ip/lmac/src/scan/scan_shared.c \
                    mx108/mx378/ip/lmac/src/scan/scan_task.c \
                    mx108/mx378/ip/lmac/src/sta/sta_mgmt.c \
                    mx108/mx378/ip/lmac/src/td/td.c \
                    mx108/mx378/ip/lmac/src/tdls/tdls.c \
                    mx108/mx378/ip/lmac/src/tdls/tdls_task.c \
                    mx108/mx378/ip/lmac/src/tpc/tpc.c \
                    mx108/mx378/ip/lmac/src/tx/tx_swdesc.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_buffer.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_buffer_shared.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_cfm.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_cntrl.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_frame.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_frame_shared.c \
                    mx108/mx378/ip/lmac/src/tx/txl/txl_hwdesc.c \
                    mx108/mx378/ip/lmac/src/vif/vif_mgmt.c \
                    mx108/mx378/ip/mac/mac.c \
                    mx108/mx378/ip/mac/mac_ie.c \
                    mx108/mx378/ip/umac/src/apm/apm.c \
                    mx108/mx378/ip/umac/src/apm/apm_task.c \
                    mx108/mx378/ip/umac/src/bam/bam.c \
                    mx108/mx378/ip/umac/src/bam/bam_task.c \
                    mx108/mx378/ip/umac/src/me/me.c \
                    mx108/mx378/ip/umac/src/me/me_mgmtframe.c \
                    mx108/mx378/ip/umac/src/me/me_mic.c \
                    mx108/mx378/ip/umac/src/me/me_task.c \
                    mx108/mx378/ip/umac/src/me/me_utils.c \
                    mx108/mx378/ip/umac/src/rc/rc.c \
                    mx108/mx378/ip/umac/src/rc/rc_basic.c \
                    mx108/mx378/ip/umac/src/rxu/rxu_cntrl.c \
                    mx108/mx378/ip/umac/src/scanu/scanu.c \
                    mx108/mx378/ip/umac/src/scanu/scanu_shared.c \
                    mx108/mx378/ip/umac/src/scanu/scanu_task.c \
                    mx108/mx378/ip/umac/src/sm/sm.c \
                    mx108/mx378/ip/umac/src/sm/sm_task.c \
                    mx108/mx378/ip/umac/src/txu/txu_cntrl.c 
endif

ifeq ($(CONFIG_SOFTAP),1)
$(NAME)_SOURCES	 += mx108/mx378/func/hostapd-2.5/src/ap/ap_config.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ap_drv_ops.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ap_list.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ap_mlme.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/authsrv.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/beacon.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/bss_load.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/dfs.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/drv_callbacks.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/eap_user_db.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/hostapd.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/hw_features.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ieee802_11.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ieee802_11_auth.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ieee802_11_ht.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ieee802_11_shared.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/ieee802_1x.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/pmksa_cache_auth.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/sta_info.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/tkip_countermeasures.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/utils.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/wmm.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/wpa_auth.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/wpa_auth_glue.c \
                    mx108/mx378/func/hostapd-2.5/src/ap/wpa_auth_ie.c \
                    mx108/mx378/func/hostapd-2.5/hostapd/main_none.c \
                    mx108/mx378/func/hostapd-2.5/wpa_supplicant/ap.c \
                    mx108/mx378/func/mxchip/dhcpd/dhcp-server-main.c \
                    mx108/mx378/func/mxchip/dhcpd/dhcp-server.c 
endif

ifeq ($(WPA_CRYPTO),1)
$(NAME)_SOURCES	 += mx108/mx378/func/hostapd-2.5/src/crypto/aes-internal-dec.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/aes-internal-enc.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/aes-internal.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/aes-wrap.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/md5-internal.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/md5.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/sha1-internal.c \
                    mx108/mx378/func/hostapd-2.5/src/crypto/sha1.c 
else
$(NAME)_SOURCES	 += mx108/mx378/func/hostapd-2.5/src/crypto/crypto_ali.c
endif

$(NAME)_SOURCES	 += hal/gpio.c \
                    hal/wdg.c \
                    hal/hw.c \
                    hal/flash.c \
					hal/uart.c \
					hal/i2c.c \
					hal/ringbuf.c \
                    hal/StringUtils.c \
					hal/wifi_port.c \
                    port/ota_port.c

#ifneq (,$(filter protocols.mesh,$(COMPONENTS)))
$(NAME)_SOURCES +=  hal/mesh_wifi_hal.c
#endif



