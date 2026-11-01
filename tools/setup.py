#!/usr/bin/env python3
"""Fetch the pinned reference source; no ROM is downloaded."""
import argparse
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
REV = "fda76b69837e6c5aae436e0d3e798989e49b06d2"
URL = "https://github.com/jkdansereau/goldeneye-pc-port.git"
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--check", action="store_true", help="Check an existing checkout without network access")
a = p.parse_args()
up = ROOT / "upstream/goldeneye-pc"
if not up.exists():
    if a.check:
        p.error("Reference checkout missing. Run python3 tools/setup.py first.")
    up.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", "--no-checkout", URL, str(up)], check=True)
    subprocess.run(["git", "-C", str(up), "checkout", "--detach", REV], check=True)
revision = subprocess.check_output(["git", "-C", str(up), "rev-parse", "HEAD"], text=True).strip()
if revision != REV:
    p.error("Reference revision differs; existing checkout was left untouched.")
if subprocess.check_output(["git", "-C", str(up), "status", "--porcelain"], text=True).strip():
    p.error("Reference checkout contains local changes; left untouched.")
(ROOT / "local/roms").mkdir(parents=True, exist_ok=True)
print("Reference source ready. Supply your own US ROM at local/roms/GoldenEye.n64.")
