#!/usr/bin/env python3
"""Check retail BSP scalar conversion and execute the original model sorter."""
import csv
import struct
import zlib
from pathlib import Path
import build
import build_probe
from native_fixture import canonical_rom
from native_game_assets import convert_model, model_counts, original_helpers


def main():
    out = build.BUILD / 'game-assets-host'
    out.mkdir(exist_ok=True)
    rom = canonical_rom(build.ROOT / 'local/roms/GoldenEye.n64')
    counts, helpers = model_counts(), original_helpers()
    records = bytearray()
    models = 0
    weapon_counts = {}
    for row in csv.reader((build.UP / 'scripts/filelist.u.csv').open()):
        name = Path(row[2]).stem
        if not name or name[0] not in 'CGP' or not name.endswith('Z') or '_stan' in name:
            continue
        address, size = map(int, row[:2])
        if not size:
            continue
        raw = zlib.decompress(rom[address + 2:address + size], -15)
        count = next(counts[n.lower()] for n in (name, name[1:], name[:-1], name[1:-1]) if n.lower() in counts)
        native, _ = convert_model(raw, *count, helpers)
        nodes, _ = helpers['build_nodes'](raw, len(raw), *count)
        found = 0
        for node in nodes.values():
            if node.op != 9:
                continue
            d = node.data
            # Point, normal, active axis and RW-pool index are independent
            # retail references; an inverse-byte-swap check alone misses fields.
            expected = struct.unpack_from('>6f', raw, d) + struct.unpack_from('>hH', raw, d + 32)
            actual = struct.unpack_from('<6f', native, d) + struct.unpack_from('<hH', native, d + 32)
            assert actual == expected, (name, hex(d), actual, expected)
            assert 0 <= actual[6] <= 3, (name, hex(d), actual[6])
            records += struct.pack('<6fhH', *actual)
            found += 1
        models += bool(found)
        if name in ('GwppkZ', 'GwppksilZ', 'Gak47Z', 'GsniperrifleZ'):
            weapon_counts[name] = found
    assert all(weapon_counts.get(n, 0) for n in ('GwppkZ', 'GwppksilZ', 'Gak47Z', 'GsniperrifleZ'))
    fixture = out / 'model-bsp.dat'
    fixture.write_bytes(records)
    arm_layout = out / 'model-bsp-arm-layout.c'
    arm_layout.write_text('#include <ultra64.h>\n#include "game/model.h"\n'
        '_Static_assert(__builtin_offsetof(ModelRoData_BSPRecord, reserved)==32, "BSP axis at retail +0x20");\n'
        '_Static_assert(__builtin_offsetof(ModelRoData_BSPRecord, RwDataIndex)==34, "BSP RW index at retail +0x22");\n')
    build_probe.run([build.CC, *build_probe.engine_args(), '-c', arm_layout, '-o', arm_layout.with_suffix('.o')])
    objects = []
    for source in (build.source_path(Path('src/game/model.c')), build.UP / 'src/game/matrixmath.c', build.TARGET / 'tests/model_bsp_host.c'):
        obj = out / ('bsp-' + source.name + '.o')
        build_probe.run(['gcc', *build_probe.engine_args(True), '-c', source, '-o', obj])
        objects.append(obj)
    binary = out / 'model-bsp-check'
    build_probe.run(['gcc', '-no-pie', '-Wl,--gc-sections', *objects, '-lm', '-o', binary])
    print(f'PASS retail/native BSP planes, axis selectors and RW indices: {len(records)//28} nodes in {models} models; weapons {weapon_counts}')
    print(build_probe.run([binary, fixture]).stdout)


if __name__ == '__main__':
    main()
