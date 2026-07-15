# -*- coding: utf-8 -*-
from pathlib import Path
p = Path(r"d:/C/new_production_h_develop/platform/settings/widgets/test_case_edit_dialog.cpp")
lines = p.read_text(encoding="utf-8").splitlines(keepends=True)
# remove orphaned lines that start after initRangeMultiGateTable closing brace
# Find the pattern: line with only "}\n" after initRange then orphan starting with "    table->horizontalHeader"
out = []
i = 0
while i < len(lines):
    if (
        i + 1 < len(lines)
        and lines[i].rstrip("\r\n") == "}"
        and lines[i + 1].lstrip().startswith("table->horizontalHeader()->setStretchLastSection")
        and i > 0
        and "initRangeMultiGateTable" in "".join(lines[max(0, i - 40) : i])
    ):
        # skip until next function definition after orphaned closing brace
        # current lines[i] is the good closing of initRangeMultiGateTable - keep it
        out.append(lines[i])
        i += 1
        # skip orphan lines until we hit "void fillGateOpCombo"
        while i < len(lines) and not lines[i].startswith("void fillGateOpCombo"):
            i += 1
        continue
    out.append(lines[i])
    i += 1
p.write_text("".join(out), encoding="utf-8", newline="\n")
print("done", len(lines), "->", len(out))
