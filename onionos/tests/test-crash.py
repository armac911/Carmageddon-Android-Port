#!/usr/bin/env python3
"""Verify controlled fatal-signal capture on native host or qemu ARM command."""
import subprocess,sys
r=subprocess.run(sys.argv[1:],capture_output=True,text=True)
assert r.returncode==139,(r.returncode,r.stderr)
assert 'Miyoo crash capture installed' in r.stderr,r.stderr
assert 'MIYOO_CRASH signal=0x' in r.stderr,r.stderr
assert ' pc=0x' in r.stderr and ' sp=0x' in r.stderr,r.stderr
assert 'MIYOO_CRASH maps begin' in r.stderr and 'MIYOO_CRASH maps end' in r.stderr,r.stderr
assert 'miyoo-crash-test' in r.stderr,r.stderr
print('PASS controlled SIGSEGV: terminal status 139, register context and process mappings captured')
