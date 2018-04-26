src = Split('''
    uradar_test.c 
    lowpan6_test.c 
    sid_router_test.c 
    vector_router_test.c 
    lwip_adapter_test.c 
    urmesh_test.c 
    sid_allocator_test.c 
    rsid_allocator_test.c 
    misc_test.c 
    mesh_mgmt_test.c 
    hal_mesh_test.c 
    diags_test.c 
    mcast_test.c 
    1hop_test.c 
    1mobile_test.c 
    topology_line_test.c 
    layer_routing_line_topology.c
    layer_routing_2mobile.c
    dest_unreachable.c 
    asymmetric_test.c 
    topo_test.c 
    cli_test.c 
    test_common_functions.c
    scales_5nodes_test.c 
    scales_10nodes_test.c 
    scales_20nodes_test.c 
    scales_30nodes_test.c 
    4super_7nodes_test.c 
    lowpower_test.c 
    leader_discover_test.c
''')

component = aos_component('mesh_test', src)
component.add_includes('include','../../../../../tools/dda')

component.add_comp_deps('kernel/protocols/mesh')
component.add_comp_deps('tools/dda')








