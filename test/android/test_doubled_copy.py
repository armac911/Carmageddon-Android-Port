#!/usr/bin/env python3
"""Exercise the actual engine function without linking the full game or assets."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/DETHRACE/common/graphics.c").read_text()
start = source.index("void DRPixelmapDoubledCopy(")
brace = source.index("{", start)
depth = 1
end = brace + 1
while depth:
    if source[end] == "{": depth += 1
    elif source[end] == "}": depth -= 1
    end += 1
function = source[start:end]
fixture = (Path(__file__).parent / "doubled_copy_fixture.c").read_text()
with tempfile.TemporaryDirectory(prefix="carma-masked-copy-") as directory:
    test = Path(directory) / "test.c"
    executable = Path(directory) / "test"
    test.write_text(fixture.replace("/* ENGINE_FUNCTION */", function))
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-g", "-O1",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    str(test), "-o", str(executable)], check=True)
    # Fixture uses stack storage only. LeakSanitizer cannot enumerate /proc
    # tasks in the managed build environment; retain address and UB checks.
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = environment.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
    subprocess.run([str(executable)], check=True, env=environment)
