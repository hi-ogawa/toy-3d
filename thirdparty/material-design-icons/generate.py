FILE = 'codepoints'

lines = open(FILE).read().split('\n')[:-1]
pairs = [l.split(' ') for l in lines]
codepoints = sorted(y for x, y in pairs)
icon_min = codepoints[0]
icon_max = codepoints[-1]

print(f"""\
#pragma once

#define MD_ICON_MIN 0x{icon_min}
#define MD_ICON_MAX 0x{icon_max}

#define MD_FOR_EACH_ICON(_) \\\
""")

for x, y in pairs:
  print(f'_({x}, u8"\\u{y}") \\')
