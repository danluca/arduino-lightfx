import subprocess
from datetime import datetime

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
buildTime=datetime.now()

# print("'-w'")
# print("'-std=gnu++17'")
# print("'-O2'")
print("'-DGIT_COMMIT=\"%s\"'" % revision)
print("'-DGIT_COMMIT_SHORT=\"%s\"'" % revision[0:8])
print("'-DGIT_BRANCH=\"%s\"'" % branch)
print(f"'-DBUILD_TIME=\"{buildTime:%Y-%m-%d %H:%M:%S}\"'")
