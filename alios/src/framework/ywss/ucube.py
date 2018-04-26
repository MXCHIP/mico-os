src = Split('''
    awss.c 
    enrollee.c 
    sha256.c 
    zconfig_utils.c 
    zconfig_ieee80211.c 
    wifimgr.c 
    ywss_utils.c
    zconfig_ut_test.c 
    registrar.c 
    zconfig_protocol.c 
    zconfig_vendor_common.c
''')

component = aos_component('ywss', src)
component.add_macros('DEBUG')

component.add_global_macros('CONFIG_YWSS')