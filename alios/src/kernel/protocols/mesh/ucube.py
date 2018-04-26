src = Split('''
   src/core/umesh.c 
   src/core/mesh/mesh_mgmt.c 
   src/core/mesh/network_mgmt.c 
   src/core/mesh/link_mgmt.c 
   src/core/mesh/network_data.c 
   src/core/mesh/mesh_forwarder.c 
   src/core/mesh/address_mgmt.c 
   src/core/mesh/fragments.c 
   src/core/mesh/mcast.c 
   src/core/routing/router_mgr.c 
   src/core/routing/sid_router.c 
   src/core/routing/ssid_allocator.c 
   src/core/security/keys_mgr.c 
   src/ip/ip_address.c 
   src/hal/interfaces.c 
   src/hal/umesh_hal.c 
   src/hal/wifi/80211_frame.c 
   src/hal/wifi/wifi.c 
   src/utilities/message.c 
   src/utilities/timer.c 
   src/utilities/memory.c 
   src/utilities/configs.c 
   src/utilities/maths.c 
   src/utilities/mac_whitelist.c
   src/tools/cli.c
''')

component = aos_component('mesh', src)
component.add_global_includes('include')
component.add_comp_deps('kernel/yloop')

if aos_global_config.get('SDK') != '1':
    component.add_sources('src/platform/aos.c')
else:
    component.add_sources('src/platform/loop.c')

if aos_global_config.get('MESHDEBUG') == None:
    aos_global_config.set('MESHDEBUG','1')
if aos_global_config.get('MESHDEBUG') == '1':
    component.add_sources('src/tools/diags.c', 'src/utilities/logging.c')
    component.add_global_macros('CONFIG_AOS_MESH_DEBUG')

if aos_global_config.get('MESHSUPER') == None:
    aos_global_config.set('MESHSUPER','1')
if aos_global_config.get('MESHSUPER') == '1':
    component.add_sources('src/core/routing/vector_router.c', 'src/core/routing/rsid_allocator.c')
    component.add_global_macros('CONFIG_AOS_MESH_SUPER')

if aos_global_config.get('MESHLOWPOWER') == None:
    aos_global_config.set('MESHLOWPOWER','0')
if aos_global_config.get('MESHLOWPOWER') == '1':
    component.add_sources('src/core/mesh/lowpower_mgmt.c')
    component.add_global_macros('CONFIG_AOS_MESH_LOWPOWER')


if aos_global_config.get('MESHAUTH') == None:
    aos_global_config.set('MESHAUTH','0')
if aos_global_config.get('MESHAUTH') == '1':
    component.add_sources('src/core/security/auth_dot1x.c','src/core/security/auth_eap.c','src/core/security/auth_mgmt.c','src/core/security/auth_relay.c')
    component.add_macros('CONFIG_AOS_MESH_AUTH')


if aos_global_config.get('CONFIG_AOS_MESH_TAPIF') == '1':
    component.add_sources('src/ip/tapif_adapter.c')
    component.add_macros('CONFIG_AOS_MESH_TAPIF')

if aos_global_config.get('LWIP') == None:
    aos_global_config.set('LWIP','1')
if aos_global_config.get('LWIP') == '1':
    component.add_sources('src/ip/lwip_adapter.c', 'src/ip/compress6.c', 'src/tools/cli_ip.c')
else:
    component.add_sources('src/utilities/mem/pbuf.c', 'src/utilities/mem/def.c')

if aos_global_config.compiler == 'gcc':
    component.add_cflags('-Wall')
    component.add_cflags('-Werror')
   
component.add_global_macros('CONFIG_AOS_MESH')    
