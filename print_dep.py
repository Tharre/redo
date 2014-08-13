#!/usr/bin/env python3

# quick and dirty utility to make the binary mess, produced by my redo program
# into something more friendly to the human eye
# requires termcolor (pip install termcolor)

import struct
import sys
import hashlib
from binascii import hexlify
from os.path import basename
from termcolor import colored

def convert_back(s):
    return s.replace('!', '/')

if (len(sys.argv) < 2):
    print("Need an argument.")
    exit(1)

hasher = hashlib.sha1()
file = open(sys.argv[1], 'rb')
magic = file.read(4)
hash = file.read(20)
subdeps = file.read()
org_file = convert_back(basename(sys.argv[1]))

hash_str = str(hexlify(hash), 'ascii')

with open(org_file, 'rb') as f:
    buf = f.read()
    hasher.update(buf)

print("Target: " + org_file)
print("Hash: " + hash_str + " ", end="")
if hasher.hexdigest() == hash_str:
    print(colored(u"\u2714", "green", attrs=['bold']))
else:
    print(colored(u"\u2718", "red", attrs=['bold']))

print("Magic number: " + str(struct.unpack('i', magic)[0]))
print("Dependencies:")
start = True
thing = ""
for byte in subdeps:
    if start:
        print("    " + chr(byte) + "-", end="")
        start = False
    elif byte == 0:
        start = True
        print(thing)
        thing = ""
    else:
        thing += chr(byte)
        continue
