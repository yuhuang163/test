#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将 mainwindow.ui 中扁平调试 Tab 重组为「顶层分组 + 组内子 Tab」."""

from __future__ import annotations

import copy
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_PATH = ROOT / "mainwindow.ui"

GROUPS = [
    (
        "tab_debug_group_production",
        "产测",
        "verticalLayout_debug_production",
        "tabWidget_debug_production",
        ["tab_4", "tab", "tab_3"],
    ),
    (
        "tab_debug_group_peripheral",
        "外设",
        "verticalLayout_debug_peripheral",
        "tabWidget_debug_peripheral",
        ["tab_13", "tab_11", "tab_6", "tab_5"],
    ),
    (
        "tab_debug_group_dongle",
        "Dongle",
        "verticalLayout_debug_dongle",
        "tabWidget_debug_dongle",
        ["tab_dongle_suction", "tab_dongle_at"],
    ),
    (
        "tab_debug_group_ota",
        "OTA/压测",
        "verticalLayout_debug_ota",
        "tabWidget_ota",
        ["tab_7", "tab_8", "tab_16", "tab_2", "tab_9"],
    ),
    (
        "tab_debug_group_misc",
        "其它",
        "verticalLayout_debug_misc",
        "tabWidget_debug_misc",
        ["tab_12", "tab_15", "tab_14"],
    ),
]

TAB_PAGE_NAMES = {name for *_rest, names in GROUPS for name in names}


def _is_tab_page(widget: ET.Element) -> bool:
    if widget.tag != "widget" or widget.get("class") != "QWidget":
        return False
    name = widget.get("name", "")
    if name in TAB_PAGE_NAMES:
        return True
    for attr in widget.findall("attribute"):
        if attr.get("name") == "title":
            return True
    return False


def _collect_pages(tab_widget: ET.Element) -> dict[str, ET.Element]:
    pages: dict[str, ET.Element] = {}

    def walk(elem: ET.Element, parent: ET.Element | None) -> None:
        if elem.tag == "widget" and elem.get("class") == "QWidget":
            name = elem.get("name", "")
            if name in TAB_PAGE_NAMES:
                pages[name] = elem
                if parent is not None:
                    parent.remove(elem)
                return
        for child in list(elem):
            walk(child, elem)

    for child in list(tab_widget):
        if child.tag != "widget":
            continue
        name = child.get("name", "")
        if name == "tab_ota_group":
            walk(child, tab_widget)
            tab_widget.remove(child)
        elif _is_tab_page(child) and name in TAB_PAGE_NAMES:
            pages[name] = child
            tab_widget.remove(child)

    return pages


def _vbox_zero_margins(name: str) -> ET.Element:
    layout = ET.Element("layout", {"class": "QVBoxLayout", "name": name})
    for prop_name, value in (
        ("spacing", "0"),
        ("leftMargin", "8"),
        ("topMargin", "4"),
        ("rightMargin", "8"),
        ("bottomMargin", "0"),
    ):
        prop = ET.SubElement(layout, "property", {"name": prop_name})
        ET.SubElement(prop, "number").text = value
    return layout


def _make_inner_tab_widget(name: str, pages: list[ET.Element]) -> ET.Element:
    inner = ET.Element("widget", {"class": "QTabWidget", "name": name})
    idx_prop = ET.SubElement(inner, "property", {"name": "currentIndex"})
    ET.SubElement(idx_prop, "number").text = "0"
    scroll_prop = ET.SubElement(inner, "property", {"name": "usesScrollButtons"})
    ET.SubElement(scroll_prop, "bool").text = "true"
    for page in pages:
        inner.append(page)
    return inner


def _make_group_shell(
    group_name: str,
    title: str,
    layout_name: str,
    inner_name: str,
    pages: list[ET.Element],
) -> ET.Element:
    shell = ET.Element("widget", {"class": "QWidget", "name": group_name})
    attr = ET.SubElement(shell, "attribute", {"name": "title"})
    ET.SubElement(attr, "string").text = title

    layout = _vbox_zero_margins(layout_name)
    item = ET.SubElement(layout, "item")
    inner = _make_inner_tab_widget(inner_name, pages)
    item.append(inner)
    shell.append(layout)
    return shell


def restructure() -> None:
    tree = ET.parse(UI_PATH)
    root = tree.getroot()
    tab_widget = None
    for elem in root.iter("widget"):
        if elem.get("class") == "QTabWidget" and elem.get("name") == "tabWidget":
            tab_widget = elem
            break
    if tab_widget is None:
        raise RuntimeError("未找到 tabWidget")

    pages = _collect_pages(tab_widget)
    missing = [n for *_g, names in GROUPS for n in names if n not in pages]
    if missing:
        raise RuntimeError(f"缺少 Tab 页面: {missing}")

    current_idx = tab_widget.find("property[@name='currentIndex']")
    if current_idx is not None:
        num = current_idx.find("number")
        if num is not None:
            num.text = "0"

    for child in list(tab_widget):
        if child.tag == "widget":
            tab_widget.remove(child)

    for group_name, title, layout_name, inner_name, page_names in GROUPS:
        group_pages = [copy.deepcopy(pages[name]) for name in page_names]
        tab_widget.append(
            _make_group_shell(group_name, title, layout_name, inner_name, group_pages)
        )

    ET.indent(tree, space=" ", level=0)
    tree.write(UI_PATH, encoding="utf-8", xml_declaration=True)
    print(f"已重组 {UI_PATH}，共 {len(GROUPS)} 个顶层分组。")


if __name__ == "__main__":
    try:
        restructure()
    except Exception as exc:
        print(f"错误: {exc}", file=sys.stderr)
        sys.exit(1)
