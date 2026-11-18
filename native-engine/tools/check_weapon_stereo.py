"""Run standalone weapon projection and material-prefix checks without upstream."""
from pathlib import Path
import os
import subprocess


def main():
    target = Path(__file__).resolve().parents[1]
    out = target.parent / 'local' / 'validation'
    out.mkdir(parents=True, exist_ok=True)
    for name, sources in (
        ('weapon-stereo', ['tests/weapon_stereo.cpp']),
        ('dynamic-stereo', ['tests/dynamic_stereo.cpp']),
        ('gyro-filter', ['tests/gyro.cpp']),
        ('motion-sensor', ['tests/motion_sensor.cpp','src/native_motion_sensor.cpp']),
        ('batch-prefix', ['tests/batch_prefix.cpp', 'src/pica_inputs.cpp']),
    ):
        binary = out / name
        subprocess.run([
            os.environ.get('CXX', 'g++'), '-std=c++20', '-O2',
            '-Wall', '-Wextra', '-Werror', '-I' + str(target / 'include'),
            *(str(target / source) for source in sources), '-o', str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
