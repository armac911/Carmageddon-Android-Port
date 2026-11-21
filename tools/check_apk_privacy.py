#!/usr/bin/env python3
"""Check a built APK for personal or machine-identifying information before
sharing it.

Usage: check_apk_privacy.py APK [EXTRA_PATTERN ...]

Always checks every file in the APK for absolute home/temp paths
(/Users/..., /home/..., C:\\Users\\..., /var/folders/...), the current
user name, home directory and host name, and fails if any native library
still carries debug sections. EXTRA_PATTERNs (for example your name, email
or git user) are passed on the command line so they never live in the repo.
Exit status 0 = clean, 1 = findings.
"""
import getpass, os, re, socket, sys, zipfile

def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    apk = sys.argv[1]
    literals = {p for p in sys.argv[2:] if p}
    for value in (getpass.getuser(), os.path.expanduser('~'), socket.gethostname().split('.')[0]):
        if value and len(value) >= 3 and value != '/':
            literals.add(value)
    path_pattern = re.compile(rb'(/Users/[A-Za-z0-9._-]+|/home/[A-Za-z0-9._-]+|[A-Za-z]:\\\\Users\\\\[A-Za-z0-9._-]+|/var/folders/[A-Za-z0-9_]+)')
    findings = []
    with zipfile.ZipFile(apk) as archive:
        for info in archive.infolist():
            data = archive.read(info)
            for literal in sorted(literals):
                # Check both UTF-8 and the UTF-16 form used in resources.arsc/AXML.
                for encoded in (literal.encode(), literal.encode('utf-16-le')):
                    if encoded.lower() in data.lower():
                        findings.append(f'{info.filename}: contains a configured personal value')
                        break
            for match in sorted(set(path_pattern.findall(data)))[:3]:
                findings.append(f'{info.filename}: absolute path {match.decode(errors="replace")}')
            if info.filename.endswith('.so') and data[:4] == b'\x7fELF' and (b'.debug_info' in data or b'.debug_line' in data):
                findings.append(f'{info.filename}: native library still has debug sections')
    for line in sorted(set(findings)):
        print('FOUND', line)
    if not findings:
        print(f'CLEAN {os.path.basename(apk)}: no personal values, absolute user paths or native debug sections')
    return 1 if findings else 0

if __name__ == '__main__':
    sys.exit(main())
