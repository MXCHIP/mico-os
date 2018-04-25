src     = Split('''
        arch_main.c
        boot_handlers.S
        boot_vectors.S
        ll.S
        ../intc/intc.c 
''')

component = aos_component('entry', src)
incs = Split('''
        ../../app
        ../../app/config
        ../../func/include
        ../../os/include
        ../../ip/lmac/src/rwnx
        ../../ip/ke
        ../../ip/mac
        ../../../../aos 
''')

component.add_includes(*incs)
