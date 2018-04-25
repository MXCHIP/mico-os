import argparse
import re
import os.path

# arguments
examples = """examples:
    python module_meminfo.py mapfile meminfo
"""
parser = argparse.ArgumentParser(
    description="Count module memory footprint",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)

parser.add_argument("map_file", help="gcc mapfile")
parser.add_argument("mem_info", help="dumpsys memory info file")
args = parser.parse_args()

text_sections = []
alloc_text = []
module_meminfo = {}

#get text section for gcc map file
with open(args.map_file, 'r') as f:
    s = f.read()
    # find the memory configuration
    mem_config_text = re.findall('Memory Configuration\n\nName             Origin             Length             Attributes\n([\s\S]+)\nLinker script and memory map', s)[0]
    # find the ROM configuration
    rom_config_text = re.findall('\w+\s+(0x\w+)\s+(0x\w+)\s+xr\n', mem_config_text)
    # get every ROM configuration's start - end address
    rom_config = []
    for rom in rom_config_text:
        rom_config += [{'start':int(rom[0], 16), 'end':int(rom[0], 16) + int(rom[1], 16)}]
    if len(rom_config) == 0:
        rom_config_text = re.findall('\*default\*\s+(0x\w+)\s+(0x\w+)\n',mem_config_text)
        for rom in rom_config_text:
            rom_config += [{'start':int(rom[0], 16), 'end':int(rom[0], 16) + int(rom[1], 16)}]
    
    # find memory map (without discard and debug sections)
    mem_map = re.findall('Linker script and memory map([\s\S]+?)OUTPUT\(\S+' + os.path.basename(args.map_file).replace('.map','.elf'), s)[0]
    text_sections = map(lambda arg : {'start':int(arg[0], 16), 'end':int(arg[0], 16) + int(arg[1], 16), 'module': arg[2]}, re.findall('(0x\w+)[ \t]+(0x\w+)[ \t]+.*[/\\\](\w+\.\w)(\(.+\.o\))?\n', mem_map))

#get memory alloc from memory info file
total_ram = 0
with open(args.mem_info, 'r') as f:
    s = f.read()
    alloc_text = re.findall('0x\w+\s+used+\s+(\d+)\s+\S+\s+(0x\w+)\s+',s)
    for section in text_sections:
        for i in range(len(alloc_text)-1, -1, -1):
            if section['start'] <= int(alloc_text[i][1],16) < section['end']:
                if section['module'] in module_meminfo:
                    module_meminfo[section['module']] += int(alloc_text[i][0])
                else:
                    module_meminfo[section['module']] = int(alloc_text[i][0])
                total_ram += int(alloc_text[i][0])
                #delete malloc info when find
                del alloc_text[i]

#get unknown memory alloc 
for malloc in alloc_text:
    if 'unknown' in module_meminfo:
        module_meminfo['unknown'] += int(malloc[0])
    else:
        module_meminfo['unknown'] = int(malloc[0])
    total_ram += int(malloc[0])

module_meminfo= sorted(module_meminfo.iteritems(), key=lambda d:d[1], reverse = True)
print '\n                  Module memory footprint'
print '|==================================================================|' 
print '| %-40s | %-10s | %-8s | '%('MODULE','RAM (bytes)', 'PCT(%)')
print '|==================================================================|' 
for (module,  memory) in module_meminfo:
    print('| %-40s | %-10d | %-8.2f |')%(module, memory, float(memory) / total_ram * 100)
print '|==================================================================|'    
print '| %-40s | %-10d | %-8.2f |'%('TOTAL (bytes)', total_ram, 100)
print '|==================================================================|'




