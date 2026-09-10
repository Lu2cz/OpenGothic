"""Merge only explicitly audited container inventory entries into an untouched save."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

p=argparse.ArgumentParser()
for name in ["source","donor","entries","output"]:
    p.add_argument("--"+name,type=Path,required=True)
a=p.parse_args()
original=hashlib.sha256(a.source.read_bytes()).digest()
entries=set(json.loads(a.entries.read_text()))
assert entries and all("/mobsi/" in n and n.endswith("/inventory") for n in entries)
assert not a.output.exists(),"Never overwrite an existing save"
with zipfile.ZipFile(a.source) as src,zipfile.ZipFile(a.donor) as donor:
    assert entries<=set(donor.namelist()),"Missing recovered inventory"
    header=src.read("header")
    tag=b"OpenGothic/Save\0"
    assert header.startswith(tag)
    offset=len(tag)+2  # uint16 version; next field is the length-prefixed name
    length=struct.unpack_from("<I",header,offset)[0]
    title=b"Archolos loot recovery"
    header=header[:offset]+struct.pack("<I",len(title))+title+header[offset+4+length:]
    with zipfile.ZipFile(a.output,"x",compression=zipfile.ZIP_DEFLATED) as dst:
        for info in src.infolist():
            data=header if info.filename=="header" else (donor.read(info.filename) if info.filename in entries else src.read(info.filename))
            dst.writestr(info,data)
        for name in entries-set(src.namelist()):
            dst.writestr(name,donor.read(name))
with zipfile.ZipFile(a.output) as dst,zipfile.ZipFile(a.source) as src:
    assert dst.testzip() is None
    for name in src.namelist():
        if name not in entries and name!="header":
            assert dst.read(name)==src.read(name),f"Unexpected change: {name}"
assert hashlib.sha256(a.source.read_bytes()).digest()==original
print(f"PASS: recovered {len(entries)} container inventories; all gameplay state outside them is byte-identical: {a.output}")
