import sys
import os
import shutil

src = Split('''
        aos/aos_main.c
        aos/qc_test.c
        mx108/mx378/app/app.c
        mx108/mx378/app/config/param_config.c
        mx108/mx378/app/ftp/ftpd.c
        mx108/mx378/app/ftp/vfs.c
        mx108/mx378/app/led/app_led.c
        mx108/mx378/app/net_work/app_lwip_tcp.c
        mx108/mx378/app/net_work/app_lwip_udp.c
        mx108/mx378/app/standalone-ap/sa_ap.c
        mx108/mx378/app/standalone-station/sa_station.c
        mx108/mx378/demo/ieee802_11_demo.c
        mx108/mx378/driver/common/dd.c
        mx108/mx378/driver/common/drv_model.c
        mx108/mx378/driver/dma/dma.c
        mx108/mx378/driver/driver.c
        mx108/mx378/driver/fft/fft.c
        mx108/mx378/driver/flash/flash.c
        mx108/mx378/driver/general_dma/general_dma.c
        mx108/mx378/driver/gpio/gpio.c
        mx108/mx378/driver/i2s/i2s.c
        mx108/mx378/driver/icu/icu.c
        mx108/mx378/driver/irda/irda.c
        mx108/mx378/driver/macphy_bypass/mac_phy_bypass.c
        mx108/mx378/driver/phy/phy_trident.c
        mx108/mx378/driver/pwm/pwm.c
        mx108/mx378/driver/saradc/saradc.c
        mx108/mx378/driver/sdcard/sdcard.c
        mx108/mx378/driver/sdcard/sdio_driver.c
        mx108/mx378/driver/sdio/sdio.c
        mx108/mx378/driver/sdio/sdma.c
        mx108/mx378/driver/sdio/sutil.c
        mx108/mx378/driver/spi/spi.c
        mx108/mx378/driver/spidma/spidma.c
        mx108/mx378/driver/sys_ctrl/sys_ctrl.c
        mx108/mx378/driver/uart/Retarget.c
        mx108/mx378/driver/uart/uart.c
        mx108/mx378/driver/usb/src/cd/mu_cntlr.c
        mx108/mx378/driver/usb/src/cd/mu_descs.c
        mx108/mx378/driver/usb/src/cd/mu_drc.c
        mx108/mx378/driver/usb/src/cd/mu_fc.c
        mx108/mx378/driver/usb/src/cd/mu_fun.c
        mx108/mx378/driver/usb/src/cd/mu_funex.c
        mx108/mx378/driver/usb/src/cd/mu_hc.c
        mx108/mx378/driver/usb/src/cd/mu_hdr.c
        mx108/mx378/driver/usb/src/cd/mu_hsdma.c
        mx108/mx378/driver/usb/src/cd/mu_hst.c
        mx108/mx378/driver/usb/src/cd/mu_list.c
        mx108/mx378/driver/usb/src/cd/mu_mdr.c
        mx108/mx378/driver/usb/src/cd/mu_pip.c
        mx108/mx378/driver/usb/src/drivers/comm/mu_comif.c
        mx108/mx378/driver/usb/src/drivers/hid/mu_hidif.c
        mx108/mx378/driver/usb/src/drivers/hid/mu_hidkb.c
        mx108/mx378/driver/usb/src/drivers/hid/mu_hidmb.c
        mx108/mx378/driver/usb/src/drivers/msd/mu_mapi.c
        mx108/mx378/driver/usb/src/drivers/msd/mu_mbot.c
        mx108/mx378/driver/usb/src/drivers/msd/mu_mscsi.c
        mx108/mx378/driver/usb/src/examples/msd/mu_msdfn.c
        mx108/mx378/driver/usb/src/hid/usb_hid.c
        mx108/mx378/driver/usb/src/lib/mu_bits.c
        mx108/mx378/driver/usb/src/lib/mu_stack.c
        mx108/mx378/driver/usb/src/lib/mu_stdio.c
        mx108/mx378/driver/usb/src/lib/mu_strng.c
        mx108/mx378/driver/usb/src/msc/usb_msd.c
        mx108/mx378/driver/usb/src/systems/none/afs/board.c
        mx108/mx378/driver/usb/src/systems/none/plat_uds.c
        mx108/mx378/driver/usb/src/uvc/usb_uvc.c
        mx108/mx378/driver/usb/src/uvc/uvc_driver.c
        mx108/mx378/driver/usb/usb.c
        mx108/mx378/driver/wdt/wdt.c
        mx108/mx378/func/bk7011_cal/bk7011_cal.c
        mx108/mx378/func/bk7011_cal/manual_cal.c
        mx108/mx378/func/fs_fat/disk_io.c
        mx108/mx378/func/fs_fat/ff.c
        mx108/mx378/func/fs_fat/playmode.c
        mx108/mx378/func/func.c
        mx108/mx378/func/hostapd-2.5/bk_patch/ddrv.c
        mx108/mx378/func/hostapd-2.5/bk_patch/signal.c
        mx108/mx378/func/hostapd-2.5/bk_patch/sk_intf.c
        mx108/mx378/func/hostapd-2.5/bk_patch/socket.c
        mx108/mx378/func/hostapd-2.5/src/common/hw_features_common.c
        mx108/mx378/func/hostapd-2.5/src/common/ieee802_11_common.c
        mx108/mx378/func/hostapd-2.5/src/common/wpa_common.c
        mx108/mx378/func/hostapd-2.5/src/crypto/aes-unwrap.c
        mx108/mx378/func/hostapd-2.5/src/crypto/rc4.c
        mx108/mx378/func/hostapd-2.5/src/crypto/sha1-pbkdf2.c
        mx108/mx378/func/hostapd-2.5/src/crypto/sha1-prf.c
        mx108/mx378/func/hostapd-2.5/src/crypto/tls_none.c
        mx108/mx378/func/hostapd-2.5/src/drivers/driver_beken.c
        mx108/mx378/func/hostapd-2.5/src/drivers/driver_common.c
        mx108/mx378/func/hostapd-2.5/src/drivers/drivers.c
        mx108/mx378/func/hostapd-2.5/src/eap_common/eap_common.c
        mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server.c
        mx108/mx378/func/hostapd-2.5/src/eap_server/eap_server_methods.c
        mx108/mx378/func/hostapd-2.5/src/eapol_auth/eapol_auth_sm.c
        mx108/mx378/func/hostapd-2.5/src/l2_packet/l2_packet_none.c
        mx108/mx378/func/hostapd-2.5/src/rsn_supp/preauth.c
        mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa.c
        mx108/mx378/func/hostapd-2.5/src/rsn_supp/wpa_ie.c
        mx108/mx378/func/hostapd-2.5/src/utils/common.c
        mx108/mx378/func/hostapd-2.5/src/utils/eloop.c
        mx108/mx378/func/hostapd-2.5/src/utils/os_none.c
        mx108/mx378/func/hostapd-2.5/src/utils/wpabuf.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/blacklist.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/bss.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/config.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/config_none.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/eap_register.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/events.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/main_supplicant.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/notify.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/wmm_ac.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_scan.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpa_supplicant.c
        mx108/mx378/func/hostapd-2.5/wpa_supplicant/wpas_glue.c
        mx108/mx378/func/hostapd_intf/hostapd_intf.c
        mx108/mx378/func/misc/fake_clock.c
        mx108/mx378/func/misc/target_util.c
        mx108/mx378/func/mxchip/lwip-2.0.2/port/ethernetif.c
        mx108/mx378/func/mxchip/lwip-2.0.2/port/net.c
        mx108/mx378/func/rf_test/rx_sensitivity.c
        mx108/mx378/func/rf_test/tx_evm.c
        mx108/mx378/func/rwnx_intf/rw_ieee80211.c
        mx108/mx378/func/rwnx_intf/rw_msdu.c
        mx108/mx378/func/rwnx_intf/rw_msg_rx.c
        mx108/mx378/func/rwnx_intf/rw_msg_tx.c
        mx108/mx378/func/sd_music/sdcard_test.c
        mx108/mx378/func/sdio_intf/sdio_intf.c
        mx108/mx378/func/sdio_trans/sdio_trans.c
        mx108/mx378/func/sim_uart/gpio_uart.c
        mx108/mx378/func/sim_uart/pwm_uart.c
        mx108/mx378/func/spidma_intf/spidma_intf.c
        mx108/mx378/func/temp_detect/temp_detect.c
        mx108/mx378/func/uart_debug/cmd_evm.c
        mx108/mx378/func/uart_debug/cmd_help.c
        mx108/mx378/func/uart_debug/cmd_reg.c
        mx108/mx378/func/uart_debug/cmd_rx_sensitivity.c
        mx108/mx378/func/uart_debug/command_line.c
        mx108/mx378/func/uart_debug/command_table.c
        mx108/mx378/func/uart_debug/udebug.c
        mx108/mx378/func/usb/fusb.c
        mx108/mx378/func/user_driver/BkDriverFlash.c
        mx108/mx378/func/user_driver/BkDriverGpio.c
        mx108/mx378/func/user_driver/BkDriverPwm.c
        mx108/mx378/func/user_driver/BkDriverUart.c
        mx108/mx378/func/user_driver/BkDriverWdg.c
        mx108/mx378/func/wlan_ui/wlan_ui.c
        mx108/mx378/func/wlan_ui/lsig_monitor.c
        mx108/mx378/os/mem_arch.c
        mx108/mx378/os/str_arch.c
        mx108/mico_api/MiCODrivers/MiCODriverFlash.c
        mx108/mico_api/MiCODrivers/MiCODriverGpio.c
        mx108/mico_api/MiCODrivers/MiCODriverPwm.c
        mx108/mico_api/MiCODrivers/MiCODriverUart.c
        mx108/mico_api/MiCODrivers/MiCODriverWdg.c
        mx108/mico_api/mico_cli.c
        mx108/mico_api/mxchipWNet.c
        mx108/mico_api/platform_stub.c
        aos/soc_impl.c
        aos/trace_impl.c
        hal/gpio.c
        hal/wdg.c
        hal/hw.c
        hal/flash.c
        hal/uart.c
        hal/i2c.c
        hal/ringbuf.c
        hal/StringUtils.c
        hal/wifi_port.c
        port/ota_port.c
''')

src.append('mx108/mx378/func/hostapd-2.5/src/crypto/crypto_ali.c')
src.append('hal/mesh_wifi_hal.c')

incs = Split('''
        mx108/mx378/ip/common
        mx108/mx378/func/rf_test
        mx108/mico_api
        mx108/mx378/func/user_driver
        mx108/mico_api
        mx108/mico_api/MiCODrivers
        mx108/mx378/func/power_save
        mx108/mx378/ip/lmac/src/hal
        mx108/mx378/ip/lmac/src/mm
        mx108/mx378/driver/common/reg
        mx108/mx378/ip/lmac/src/ps
        mx108/mx378/ip/lmac/src/rd
        mx108/mx378/ip/lmac/src/rwnx
        mx108/mx378/ip/lmac/src/rx
        mx108/mx378/ip/lmac/src/scan
        mx108/mx378/ip/lmac/src/sta
        mx108/mx378/ip/lmac/src/tx
        mx108/mx378/ip/lmac/src/vif
        mx108/mx378/ip/lmac/src/rx/rxl
        mx108/mx378/ip/lmac/src/tx/txl
        mx108/mx378/ip/umac/src/bam
        mx108/mx378/ip/umac/src/llc
        mx108/mx378/ip/umac/src/me
        mx108/mx378/ip/umac/src/rxu
        mx108/mx378/ip/umac/src/scanu
        mx108/mx378/ip/umac/src/sm
        mx108/mx378/ip/umac/src/txu
        mx108/mx378/ip/ke
        mx108/mx378/ip/mac
        mx108/mx378/driver/sdio
        mx108/mx378/driver/common
        mx108/mx378/driver/include
        mx108/mx378/driver/uart
        mx108/mx378/driver/sys_ctrl
        mx108/mx378/func/sdio_intf
        mx108/mx378/driver/gpio
        mx108/mx378/ip/lmac/src/p2p
        mx108/mx378/ip/umac/src/apm
        mx108/mx378/driver/sdcard
        mx108/mx378/common
        mx108/mx378/ip/lmac/src/chan
        mx108/mx378/ip/lmac/src/td
        mx108/mx378/driver/common/reg
        mx108/mx378/driver/entry
        mx108/mx378/driver/dma
        mx108/mx378/driver/intc
        mx108/mx378/driver/phy
        mx108/mx378/driver/rc_beken
        mx108/mx378/func/sd_music
        mx108/mx378/func/hostapd-2.5/src/utils
        mx108/mx378/func/hostapd-2.5/src
        mx108/mx378/func/hostapd-2.5/bk_patch
        mx108/mx378/func/hostapd-2.5/src/ap
        mx108/mx378/app/standalone-ap
        mx108/mx378/func/hostapd-2.5/hostapd
        mx108/mx378/func/ethernet_intf
        mx108/mx378/app/standalone-station
        mx108/mx378/func/hostapd-2.5/src/common
        mx108/mx378/func/hostapd-2.5/src/drivers
        mx108/mx378/driver/usb/src/systems/none
        mx108/mx378/driver/usb/src/systems/none/afs
        mx108/mx378/driver/usb/include
        mx108/mx378/driver/usb/src/cd
        mx108/mx378/driver/usb/src/drivers/msd
        mx108/mx378/driver/usb/include/class
        mx108/mx378/app/net_work
        mx108/mx378/driver/usb/src/msc
        mx108/mx378/driver/usb
        mx108/mx378/driver/usb/src/hid
        mx108/mx378/driver/usb/src/drivers/hid
        mx108/mx378/driver/usb/src/uvc
        mx108/mx378/func/temp_detect
        mx108/mx378/ip/lmac/src/tpc
        mx108/mx378/ip/lmac/src/tdls
        mx108/mx378/ip/umac/src/mesh
        mx108/mx378/ip/umac/src/rc
        mx108/mx378/func/spidma_intf
        mx108/mx378/driver/general_dma
        mx108/mx378/driver/spidma
        mx108/mx378/func/rwnx_intf
        mx108/mx378/app
        mx108/mx378/app/ftp
        mx108/mx378/app/led
        aos
''')

global_incs = Split('''
        mx108/mx378/func/mxchip/lwip-2.0.2/port
        mx108/mx378/common
        mx108/mx378/app/config
        mx108/mx378/func/include
        mx108/mx378/os/include
        mx108/mx378/driver/include
        mx108/mx378/driver/common
        mx108/mx378/ip/common
        mx108/mx378/driver/entry
        #security/alicrypto/libalicrypto/inc/
''')
component = aos_mcu_component('moc108', src)
component.add_includes(*incs)
component.add_global_includes(*global_incs)

deps = Split('''
        platform/arch/arm/armv5
        utility/libc
        kernel/rhino
        kernel/yloop
        kernel/modules/fs/kv
        security/alicrypto
        utility/digest_algorithm
        platform/mcu/moc108/hal_init
        platform/mcu/moc108/mx108/mx378/driver/entry
        platform/mcu/moc108/aos/framework_runtime
        platform/mcu/moc108/aos/app_runtime
        security/prov
        kernel/hal
        kernel/protocols/net
        kernel/vcall
        kernel/init
''')
component.add_comp_deps(*deps)


global_macro = Split('''
        CONFIG_MX108
        CONFIG_AOS_KV_MULTIPTN_MODE
        CONFIG_AOS_KV_PTN=6
        CONFIG_AOS_KV_SECOND_PTN=7
        CONFIG_AOS_KV_PTN_SIZE=4096
        CONFIG_AOS_KV_BUFFER_SIZE=8192
        CONFIG_AOS_CLI_BOARD
        CONFIG_AOS_FOTA_BREAKPOINT
        STDIO_UART=0
''')
for macro in global_macro:
    component.add_global_macros(macro)

global_cflags = Split('''
        -mcpu=arm968e-s
        -march=armv5te
        -mthumb
        -mthumb-interwork
        -mlittle-endian
        -w
        -MD
''')
for cflags in global_cflags:
    component.add_global_cflags(cflags)


local_cflags = Split('''
        -Wall
        -Werror
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-implicit-function-declaration
        -Wno-type-limits-Wno-sign-compare
        -Wno-pointer-sign
        -Wno-uninitialized
        -Wno-return-type
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-unused-value
        -Wno-strict-aliasing
        -Wall
        -Werror
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-implicit-function-declaration
        -Wno-type-limits
        -Wno-sign-compare
        -Wno-pointer-sign
        -Wno-uninitialized
        -Wno-return-type
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-unused-value
        -Wno-strict-aliasing
        -Wall
        -Werror
        -Wno-unused-variable
        -Wno-unused-parameter
        -Wno-implicit-function-declaration
        -Wno-type-limits
        -Wno-sign-compare
        -Wno-pointer-sign
        -Wno-uninitialized
        -Wno-return-type
        -Wno-unused-function
        -Wno-unused-but-set-variable
        -Wno-unused-value
        -Wno-strict-aliasing
''')
for cflags in local_cflags:
    component.add_cflags(cflags)

global_ldflags = Split('''
        -mcpu=arm968e-s
        -march=armv5te
        -mthumb
        -mthumb-interwork
        -mlittle-endian
        -w
        -marm
        --static
        --specs=nosys.specs 
        -nostartfiles
        -u _printf_float 
        -uapp_info 
        -uframework_info
''')

for ldflag in global_ldflags:
    component.add_global_ldflags(ldflag)

aos_global_config.add_ld_files('mx108/mx378/build/mx108.ld.S')

component.add_prebuilt_libs('librwnx/librwnx.a')
component.set_global_arch('ARM968E-S')

tool_chain = aos_global_config.create_tool_chain()
tool_chain.set_prefix('arm-none-eabi-')
tool_chain.set_cppflags('-DSYSINFO_PRODUCT_MODEL=\\"ALI_AOS_MK3060\\" -DSYSINFO_DEVICE_NAME=\\"MK3060\\"')
tool_chain.set_linkcom('$LINK -o $TARGET $LDFLAGS -Wl,-Map,$MAPFILE -Wl,--whole-archive -Wl,--start-group $LIBS  -Wl,--end-group -Wl,--no-whole-archive -Wl,--gc-sections -Wl,--cref $LINKFLAGS')

aos_global_config.tool_chain_config(tool_chain)


class ota_bin(aos_command):
    def __init__(self, aos_global_config):
        self.config = aos_global_config

    def dispatch_action(self, target, source, env):
        ota_out = target[0].get_path()
        crc_out = ota_out.replace('.ota', '_crc')
        phony_target = ota_out + '.phony'

        if sys.platform.startswith('linux'):
            encrypt = os.path.join(os.getcwd(), 'platform/mcu', aos_global_config.mcu_family, 'encrypt_linux')
        elif sys.platform.startswith('darwin'):
            encrypt = os.path.join(os.getcwd(), 'platform/mcu', aos_global_config.mcu_family, 'encrypt_osx')
        elif sys.platform.startswith('win'):
            encrypt = os.path.join(os.getcwd(), 'platform/mcu', aos_global_config.mcu_family, 'encrypt_win')
        else:
            print('%s encrypt unsupport...' % sys.platform)

        env.Command(crc_out, source[0].get_path(), encrypt + ' ' + source[0].get_path() + ' 0 0 0 0')

        if sys.platform.startswith('linux'):
            xz = '/usr/bin/xz'
            if not os.path.exists(xz):
                env.Command(phony_target, crc_out, Copy(ota_out, '$SOURCE'))
                print("xz need be installed")
            else:
                tmp = ota_out + '.tmp'
                env.Command(tmp, crc_out, Copy('$TARGET', '$SOURCE'))
                xz_cmd = xz + ' -f --lzma2=dict=32KiB --check=crc32 -k ' + tmp
                env.Command(phony_target, tmp, xz_cmd)
                env.Command(phony_target, tmp, Move(ota_out, '$SOURCE'))
        else:
            env.Command(phony_target, crc_out, Copy(ota_out, '$SOURCE'))


binary = aos_global_config.toolchain.binary
target = binary.replace('elf', 'ota.bin')
source = binary.replace('.elf', '.bin')
component.add_command(target, source, ota_bin(aos_global_config))


class debug_config(aos_command):
    def __init__(self, aos_global_config):
        self.config = aos_global_config

    def dispatch_action(self, target, source, env):
        gdb_config_file = target[0].get_path()
        cwd = os.getcwd()
        with open(gdb_config_file, 'w') as f:
            f.write('set remotetimeout 20\n')

            gdb_init_string = 'shell -c "trap \\\\"\\\\" 2;"'
            if sys.platform.startswith('win'):
                f.write('shell build/cmd/win32/taskkill /F /IM openocd.exe\n')
                gdb_init_string += os.path.join(cwd, 'build/OpenOCD/Win32/openocd').replace(os.path.sep, '/')
            elif sys.platform.startswith('darwin'):
                f.write('shell killall openocd\n')
                gdb_init_string += os.path.join(cwd, 'build/OpenOCD/OSX/openocd')
            else:
                f.write('shell killall openocd_run\n')
                gdb_init_string += os.path.join(cwd, 'build/OpenOCD/Linux64/openocd')

            gdb_init_string += '"'
            gdb_init_string += ' -f ' + os.path.join(cwd, 'build/OpenOCD/interface/jlink.cfg').replace(os.path.sep, '/')
            gdb_init_string += ' -f ' + os.path.join(cwd, 'build/OpenOCD/moc108/moc108.cfg').replace(os.path.sep, '/')
            gdb_init_string += ' -f ' + os.path.join(cwd, 'build/OpenOCD/moc108/moc108_gdb_jtag.cfg').replace(os.path.sep, '/')
            gdb_init_string += ' -l ' + os.path.join(cwd, 'out/openocd.log').replace(os.path.sep, '/')
            gdb_init_string += ' &"\n'
            f.write(gdb_init_string)

target = '.gdbinit'
source = binary.replace('.elf', '.bin')
component.add_command(target, source, debug_config(aos_global_config))