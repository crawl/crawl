import sys

def extract_keys(filename):
    keys = set()
    previous = ''
    with open(filename) as infile:
        while True:
            line = infile.readline()
            if not line:
                break

            line = line.strip()
            if previous == '%%%%':
                keys.add(line)
            previous = line
    return keys

if len(sys.argv) != 3:
    print "Usage: dbm-compare.py <file1> <file2>"
    exit(1)

file1 = sys.argv[1]
file2 = sys.argv[2]

#print "Comparing keys in", file1, "vs", file2

keys1 = extract_keys(file1)
keys2 = extract_keys(file2)

print "Missing from", file2
print keys1.difference(keys2)

print "Extra in", file2
print keys2.difference(keys1)
