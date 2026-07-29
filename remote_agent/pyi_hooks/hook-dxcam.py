# Ensure dxcam Cython extension is a binary, not archived .c source (null bytes).
from pathlib import Path

from PyInstaller.utils.hooks import (
    collect_data_files,
    collect_dynamic_libs,
    collect_submodules,
    get_package_paths,
)

datas = [
    (src, dest)
    for src, dest in collect_data_files("dxcam")
    if not str(src).endswith((".c", ".pyx"))
]
binaries = list(collect_dynamic_libs("dxcam"))

_pkg = Path(get_package_paths("dxcam")[1])
for pyd in (_pkg / "processor").glob("_numpy_kernels*.pyd"):
    binaries.append((str(pyd), str(Path("dxcam") / "processor")))

hiddenimports = [
    m for m in collect_submodules("dxcam") if m != "dxcam.processor._numpy_kernels"
]
