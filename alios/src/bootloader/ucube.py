src =Split(''' 
    main.c
    menu.c
    getline.c
    update_for_ota.c
    ymodem.c
''')
component =aos_component('APP_bootloader', src)


global_includes =Split(''' 
    .
''')
for i in global_includes:
    component.add_global_includes(i)

global_macros =Split(''' 
    AOS_NO_WIFI
    BOOTLOADER
''')
for i in global_macros:
    component.add_global_macros(i)



