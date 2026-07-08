#!/usr/bin/env python3
"""将 test_case 平铺 ini 迁入 steps/，并为各工站 flow 步骤生成 profiles/{Key}/steps/ 覆盖层。"""
from __future__ import annotations

import configparser
import re
import shutil
import sys
from pathlib import Path

FLOW_NAME = "总的测试流程.ini"
RESERVED = {FLOW_NAME, "总的测试流程"}
OVERLAY_SECTIONS = ("Send", "Timing", "Gate")


def has_content(path: Path) -> bool:
    if not path.is_file() or path.stat().st_size < 24:
        return False
    cp = configparser.ConfigParser()
    cp.read(path, encoding="utf-8-sig")
    return any(s.lower() in ("send", "meta", "hook", "timing", "gate") for s in cp.sections())


def resolve_step_source(root: Path, step_id: str) -> Path | None:
    for candidate in (root / "steps" / f"{step_id}.ini", root / f"{step_id}.ini"):
        if has_content(candidate):
            return candidate
    return None


def parse_flow_items(flow_ini: Path) -> list[str]:
    cp = configparser.ConfigParser()
    cp.read(flow_ini, encoding="utf-8-sig")
    raw = ""
    if cp.has_section("Flow"):
        raw = cp.get("Flow", "Items", fallback="")
    if not raw:
        return []
    raw = raw.strip().strip('"').strip("'")
    return [p.strip() for p in raw.split(",") if p.strip()]


def write_profile_overlay(src: Path, dst: Path, step_id: str) -> None:
    cp = configparser.ConfigParser()
    cp.optionxform = str
    cp.read(src, encoding="utf-8-sig")

    lines: list[str] = [
        "[Meta]",
        f"StepId={step_id}",
        f"Name={step_id}",
        "",
    ]
    for section in OVERLAY_SECTIONS:
        if not cp.has_section(section):
            continue
        lines.append(f"[{section}]")
        for key, value in cp.items(section):
            lines.append(f"{key}={value}")
        lines.append("")

    # Gate/N 多项卡控
    gate_sub = re.compile(r"^Gate/(\d+)$")
    for section in cp.sections():
        if gate_sub.match(section):
            lines.append(f"[{section}]")
            for key, value in cp.items(section):
                lines.append(f"{key}={value}")
            lines.append("")

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def parse_flow_stations(flow_ini: Path) -> dict[str, str]:
    cp = configparser.ConfigParser()
    cp.optionxform = str
    cp.read(flow_ini, encoding="utf-8-sig")
    mapping: dict[str, str] = {}
    if not cp.has_section("FlowStations"):
        return mapping
    for key, value in cp.items("FlowStations"):
        k = key.strip()
        v = value.strip()
        if k and v:
            mapping[k] = v
    return mapping


def rename_profile_dirs_to_display_names(root: Path) -> int:
    flow_ini = root / FLOW_NAME
    if not flow_ini.is_file():
        return 0
    profiles = root / "profiles"
    if not profiles.is_dir():
        return 0
    count = 0
    for key, display in parse_flow_stations(flow_ini).items():
        legacy = profiles / key
        target = profiles / display
        if not legacy.is_dir():
            continue
        if legacy.resolve() == target.resolve():
            continue
        if target.exists():
            for item in legacy.rglob("*"):
                if item.is_file():
                    rel = item.relative_to(legacy)
                    dst = target / rel
                    if not dst.exists():
                        dst.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(item, dst)
            shutil.rmtree(legacy)
        else:
            legacy.rename(target)
        print(f"Profile dir: {key} -> {display}")
        count += 1
    return count


def migrate_root(root: Path) -> tuple[int, int, int]:
    if not root.is_dir():
        return 0, 0
    steps = root / "steps"
    steps.mkdir(parents=True, exist_ok=True)

    lib_count = 0
    for src in root.glob("*.ini"):
        if src.name in RESERVED:
            continue
        dst = steps / src.name
        if not has_content(src) or has_content(dst):
            continue
        if dst.exists():
            dst.unlink()
        shutil.copy2(src, dst)
        print(f"Library: {src.name} -> {dst}")
        lib_count += 1

    profile_count = 0
    profiles = root / "profiles"
    if profiles.is_dir():
        for profile_dir in profiles.iterdir():
            if not profile_dir.is_dir():
                continue
            flow_ini = profile_dir / "flow.ini"
            if not flow_ini.is_file():
                continue
            station_steps = profile_dir / "steps"
            for step_id in parse_flow_items(flow_ini):
                overlay = station_steps / f"{step_id}.ini"
                if has_content(overlay):
                    continue
                source = resolve_step_source(root, step_id)
                if source is None:
                    print(f"Skip (no source): {profile_dir.name}/steps/{step_id}.ini")
                    continue
                write_profile_overlay(source, overlay, step_id)
                print(f"Profile: {profile_dir.name}/steps/{step_id}.ini")
                profile_count += 1

    rename_count = rename_profile_dirs_to_display_names(root)
    return lib_count, profile_count, rename_count


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    targets = [
        repo / "build" / "Desktop_Qt_5_15_2_MSVC2019_64bit-Release" / "bin" / "test_case",
        repo / "路特上位机运行环境" / "test_case",
    ]
    lib_total = profile_total = rename_total = 0
    for t in targets:
        lib_n, profile_n, rename_n = migrate_root(t)
        lib_total += lib_n
        profile_total += profile_n
        rename_total += rename_n
        print(f"{t}: library={lib_n}, profile_steps={profile_n}, rename_dirs={rename_n}")
    print(f"Done. library={lib_total}, profile_steps={profile_total}, rename_dirs={rename_total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
