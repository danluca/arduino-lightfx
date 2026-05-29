import os
import subprocess
from datetime import datetime


def emit_build_flags():
    revision = (
        subprocess.check_output(["git", "rev-parse", "HEAD"])
        .strip()
        .decode("utf-8")
    )
    branch = (
        subprocess.check_output(["git", "branch", "--show-current"])
        .strip()
        .decode("utf-8")
    )
    build_time = datetime.now()

    print("'-DGIT_COMMIT=\"%s\"'" % revision)
    print("'-DGIT_COMMIT_SHORT=\"%s\"'" % revision[0:8])
    print("'-DGIT_BRANCH=\"%s\"'" % branch)
    print(f"'-DBUILD_TIME=\"{build_time:%Y-%m-%d %H:%M:%S}\"'")


def configure_linker_map(env):
    map_path = os.environ.get("PLATFORMIO_LINKER_MAP")

    if map_path:
        map_dir = os.path.dirname(map_path)
        if map_dir:
            os.makedirs(map_dir, exist_ok=True)
        env.Append(LINKFLAGS=[f"-Wl,-Map,{map_path}"])
        print(f"Linker map enabled: {map_path}")


if "Import" in globals():
    Import("env")
    configure_linker_map(env)
else:
    emit_build_flags()
