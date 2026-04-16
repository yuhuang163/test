@echo off
cd .\factory_protocol
for %%f in (*.proto) do (
    echo Processing %%f
    ..\Python39\python.exe "..\generator\nanopb_generator.py" %%f
)
echo All files processed.
pause
