import argparse

# arguments
examples = """examples:
    python log_parse.py input.txt leaks.txt       # parse input.txt save to leaks.txt
"""
parser = argparse.ArgumentParser(
    description="parse input.txt out.txt",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)

parser.add_argument("input", help="input file")
parser.add_argument("output", help="output file")

args = parser.parse_args()
input_stream = open(args.input, "r")
output_stream = open(args.output, "w")

backtrace = False
for line in input_stream:
    if line.strip() == '':
        continue
    else:
        if "linuxhost.elf]" in line:
            continue
        else:
            output_stream.write(line.split(":")[0].replace('+', '`')+"\n")
input_stream.close()
output_stream.close()
