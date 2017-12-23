
import sys

def quoted(line):
    line = line.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
    return '"' + line + '"'

_, source, readme = sys.argv

for line in open(source):
    if line != '"<make, insert README here>"\n':
        sys.stdout.write(line)
        continue

    # Found the line to replace with the readme.
    for line in open(readme):
        sys.stdout.write(quoted(line) + '\n')
    sys.stdout.write('"\\n"\n')

