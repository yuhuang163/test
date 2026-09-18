/* eslint-disable no-unused-vars */
/** 设备控制台预览 — 绑定 window.UI_PREVIEW_DATA（mainwindow.ui） */

(function () {
  const NESTED_KEYS = {
    tab_debug_group_business: "tabWidget_debug_business",
    tab_debug_group_peripheral: "tabWidget_debug_peripheral",
    tab_debug_group_dongle: "tabWidget_debug_dongle",
    tab_debug_group_ota: "tabWidget_ota",
    tab_debug_group_misc: "tabWidget_debug_misc",
  };

  const NAV_LABEL = {
    connect: "连接",
    tab_device_info: "基础/外设",
    tab_debug_group_business: "业务控制",
    tab_debug_group_peripheral: "外设",
    tab_debug_group_dongle: "Dongle",
    tab_debug_group_ota: "OTA/压测",
    tab_debug_group_misc: "其它",
  };

  const PRIMARY = [
    "connect",
    "tab_debug_group_business",
    "tab_debug_group_peripheral",
    "tab_debug_group_dongle",
    "tab_debug_group_ota",
    "tab_debug_group_misc",
  ];

  const NAV_ICONS = {
    connect:
      '<svg viewBox="0 0 24 24"><path d="M10 13a3 3 0 004.24 0l1.41-1.41a3 3 0 10-4.24-4.24L9.76 9.76"/><path d="M14 11a3 3 0 00-4.24 0L8.35 12.41a3 3 0 004.24 4.24l1.41-1.41"/></svg>',
    tab_device_info:
      '<svg viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="2"/><path d="M9 9h6M9 13h4"/></svg>',
    tab_debug_group_business:
      '<svg viewBox="0 0 24 24"><path d="M5 7h14"/><path d="M5 12h14"/><path d="M5 17h14"/><circle cx="9" cy="7" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="11" cy="17" r="2"/></svg>',
    tab_debug_group_peripheral:
      '<svg viewBox="0 0 24 24"><rect x="7" y="7" width="10" height="10" rx="1.5"/><path d="M12 7V4"/><path d="M12 20v-3"/><path d="M7 12H4"/><path d="M20 12h-3"/><path d="M10 10h4"/><path d="M10 14h2"/></svg>',
    tab_debug_group_dongle:
      '<svg viewBox="0 0 24 24"><path d="M9 4h6a1 1 0 011 1v6h2.5L17 20H7L5.5 11H8V5a1 1 0 011-1z"/><path d="M10 7h4"/><path d="M9.5 14h5"/></svg>',
    tab_debug_group_ota:
      '<svg viewBox="0 0 24 24"><path d="M12 4v7"/><path d="M9 8l3-3 3 3"/><rect x="5" y="14" width="14" height="6" rx="1"/><path d="M9 17h6"/></svg>',
    tab_debug_group_misc:
      '<svg viewBox="0 0 24 24"><path d="M14.7 6.3a1 1 0 000 1.4l1.6 1.6a1 1 0 001.4 0l3.77-3.77a6 6 0 01-7.94 7.94l-6.91 6.91a2.12 2.12 0 01-3-3l6.91-6.91a6 6 0 017.94-7.94l-3.76 3.76z"/></svg>',
  };

  const ROW_TITLE_RULES = [
    [/工厂|产测|出厂|关机|重启|曲目/, "工厂与产测"],
    [/sn/i, "序列号读写"],
    [/亮白|清洁|休眠|刷牙|禁止休眠/, "工作模式"],
    [/吸力|补偿/, "吸力流程"],
    [/mac|三元|设备id|产品id|密钥/, "设备元数据"],
    [/校准|采集|六轴|转化/, "校准与采集"],
    [/查询|读取|获取(?!整机|板子)/, "状态查询"],
    [/广播|配网|Wi-Fi|wifi/i, "无线连接"],
    [/OTA|升级|压测/, "升级与压测"],
  ];

  const HOST_VER = "V1.6.5";
  const state = { primary: "connect", tertiaryIndex: 0, filter: "" };
  let activeLog = "host";
  let logSamples = null;

  function esc(s) {
    return String(s ?? "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function btnClass(text) {
    if (/停止|断开|关闭|退出|关机|重启|失败|复位|重置|删除|禁止|卸载|清除|出厂|关闭Dongle/.test(text)) {
      return "btn btn-danger";
    }
    if (/启动|开启|打开|启用|Enable/.test(text)) {
      return "btn btn-success";
    }
    if (/测试|刷新|读取参数/.test(text)) {
      return "btn btn-secondary";
    }
    if (/连接|开始|发送|读取|搜索|写入|设置|查询|升级|获取|保存|确定|执行|绑定|进入工厂|退出工厂|产测/.test(text)) {
      return "btn btn-primary";
    }
    return "btn btn-secondary";
  }

  /** 开/关、进/退、连/断等成对按钮 */
  const SWITCH_PAIR_RULES = [
    [/^进入/, /^退出/],
    [/^连接(?!后)/, /^断开/],
    [/^开启/, /^关闭/],
    [/^打开/, /^关闭/],
    [/^启动|^开始(?!搜索)/, /^停止/],
    [/^启用/, /^禁用|^禁止/],
  ];

  function matchSwitchPair(a, b) {
    for (const [onRe, offRe] of SWITCH_PAIR_RULES) {
      if (onRe.test(a) && offRe.test(b)) return { on: a, off: b };
      if (onRe.test(b) && offRe.test(a)) return { on: b, off: a };
    }
    if (/^开始/.test(a) && /停止/.test(b)) return { on: a, off: b };
    if (/^开始/.test(b) && /停止/.test(a)) return { on: b, off: a };
    if (/^进入休眠/.test(a) && /^禁止休眠/.test(b)) return { on: a, off: b };
    if (/^进入休眠/.test(b) && /^禁止休眠/.test(a)) return { on: b, off: a };
    for (const [onSuf, offSuf] of [
      ["开启", "关闭"],
      ["打开", "关闭"],
    ]) {
      if (a.endsWith(onSuf) && b.endsWith(offSuf) && a.slice(0, -onSuf.length) === b.slice(0, -offSuf.length)) {
        return { on: a, off: b };
      }
      if (b.endsWith(onSuf) && a.endsWith(offSuf) && b.slice(0, -onSuf.length) === a.slice(0, -offSuf.length)) {
        return { on: b, off: a };
      }
    }
    if (a.length > 1 && b.length > 1 && a.slice(0, -1) === b.slice(0, -1)) {
      if (a.endsWith("开") && b.endsWith("关")) return { on: a, off: b };
      if (a.endsWith("关") && b.endsWith("开")) return { on: b, off: a };
    }
    return null;
  }

  function isToggleCombo(field) {
    const items = (field?.items || []).map(String);
    if (items.length !== 2) return false;
    const pair = new Set(items);
    return pair.has("关") && pair.has("开");
  }

  function fieldObjAttr(field) {
    return field?.objectName ? ` data-obj="${esc(field.objectName)}"` : "";
  }

  function wrapSegmentedSwitch(inner) {
    return `<div class="connect-actions connect-actions-full">${inner}</div>`;
  }

  function renderSegmentedSwitch(onLabel, offLabel, onText, offText, attrs) {
    const extra = attrs ? ` ${attrs}` : "";
    return `<div class="segmented lg" data-seg-switch data-state="off"${extra}>
      <button type="button" class="active" data-seg="on" data-action="${esc(onLabel)}">${esc(onText || onLabel)}</button>
      <button type="button" data-seg="off" data-action="${esc(offLabel)}">${esc(offText || offLabel)}</button>
    </div>`;
  }

  function renderOnOffSwitch(onAction, offAction, attrs) {
    return renderSegmentedSwitch(onAction, offAction, "开", "关", attrs);
  }

  function featureLabelFromPair(onAction, offAction, fallback = "") {
    const on = String(onAction || "").trim();
    const off = String(offAction || "").trim();
    const map = {
      "进入工厂模式|退出工厂模式": "工厂模式",
      "产测完成写入|产测未完成写入": "产测标志",
      "进入休眠模式|禁止休眠模式": "休眠模式",
      "进入吸力测试|退出吸力测试": "吸力测试",
      "吸力补偿开|吸力补偿关": "吸力补偿",
      "开启吸力读取|关闭吸力读取": "吸力读取",
      "显示峰检辅助线|隐藏峰检辅助线": "峰检辅助线",
      "开始交替运行|立即停止": "交替运行",
    };
    const hit = map[`${on}|${off}`];
    if (hit) return hit;

    const mOn = on.match(/^进入(.+)$/);
    const mOff = off.match(/^退出(.+)$/);
    if (mOn && mOff && mOn[1] === mOff[1]) return mOn[1];

    const mOpen = on.match(/^开启(.+)$/);
    const mClose = off.match(/^关闭(.+)$/);
    if (mOpen && mClose && mOpen[1] === mClose[1]) return mOpen[1];

    const mShow = on.match(/^显示(.+)$/);
    const mHide = off.match(/^隐藏(.+)$/);
    if (mShow && mHide && mShow[1] === mHide[1]) return mShow[1];

    const mEnable = on.match(/^启用(.+)$/);
    const mDisable = off.match(/^禁用(.+)$/);
    if (mEnable && mDisable && mEnable[1] === mDisable[1]) return mEnable[1];

    if (on.endsWith("开") && off.endsWith("关") && on.slice(0, -1) === off.slice(0, -1)) {
      return on.slice(0, -1);
    }

    return fallback || on.replace(/^(进入|开启|启用|显示)/, "") || on;
  }

  function renderLabeledOnOffField(label, onAction, offAction, attrs) {
    const lbl = label || featureLabelFromPair(onAction, offAction);
    return `<div class="switch-field field-span">
      <span class="inline-label">${esc(lbl)}</span>
      ${wrapSegmentedSwitch(renderOnOffSwitch(onAction, offAction, attrs))}
    </div>`;
  }

  /** connect-field-grid 内：label + 开/关；可选右侧附加按钮（如「读取」） */
  function renderGridOnOffRow(label, onAction, offAction, attrs, trailingHtml) {
    const lbl = label || featureLabelFromPair(onAction, offAction);
    const switchWrap = wrapSegmentedSwitch(renderOnOffSwitch(onAction, offAction, attrs));
    const control = trailingHtml
      ? `<div class="onoff-row">${switchWrap}${trailingHtml}</div>`
      : switchWrap;
    return `<label>${esc(lbl)}</label><div class="onoff-control">${control}</div>`;
  }

  function renderGridCheckboxSwitchRow(label, field, trailingHtml) {
    const lbl = label || checkboxFeatureLabel(field);
    const switchWrap = wrapSegmentedSwitch(renderCheckboxSegmented(field));
    const control = trailingHtml
      ? `<div class="onoff-row">${switchWrap}${trailingHtml}</div>`
      : switchWrap;
    return `<label>${esc(lbl)}</label><div class="onoff-control">${control}</div>`;
  }

  function checkboxFeatureLabel(field) {
    const text = String(field?.text || field?.label || "选项").trim();
    return (
      text
        .replace(/Dongle$/i, "")
        .replace(/^是否/u, "")
        .trim() || text
    );
  }

  function renderToggleComboSegmented(field) {
    const items = (field?.items || []).map(String);
    const onIdx = items.indexOf("开");
    const offIdx = items.indexOf("关");
    const onLabel = onIdx >= 0 ? "开" : items[0];
    const offLabel = offIdx >= 0 ? "关" : items[1];
    return renderSegmentedSwitch(onLabel, offLabel, onLabel, offLabel, fieldObjAttr(field).trim());
  }

  function renderCheckboxSegmented(field) {
    const text = field?.text || "选项";
    const obj = fieldObjAttr(field).trim();
    return renderOnOffSwitch(`启用${text}`, `禁用${text}`, obj);
  }

  function updateSegmentedVisual(seg) {
    if (!seg) return;
    const onBtn = seg.querySelector('[data-seg="on"]');
    const offBtn = seg.querySelector('[data-seg="off"]');
    if (!onBtn || !offBtn) return;
    onBtn.classList.remove("active", "danger");
    offBtn.classList.remove("active", "danger");
    if (seg.dataset.state === "on") {
      offBtn.classList.add("active", "danger");
    } else {
      onBtn.classList.add("active");
    }
  }

  function inferRowTitle(buttons, index) {
    const text = buttons.join(" ");
    for (const [re, title] of ROW_TITLE_RULES) {
      if (re.test(text)) return title;
    }
    return `操作组 ${index + 1}`;
  }

  function normalizeGroupBox(gb) {
    if (!gb) return gb;
    const buttonTexts = new Set((gb.button_rows || []).flat());
    const controlObjNames = new Set(
      (gb.controls || []).map((c) => c.objectName).filter(Boolean)
    );
    return {
      ...gb,
      controls: (gb.controls || []).filter(
        (c) => !(c.type === "QPushButton" && buttonTexts.has(c.text))
      ),
      fields: (gb.fields || []).filter(
        (f) => !(f.field?.objectName && controlObjNames.has(f.field.objectName) && !f.label)
      ),
      children: (gb.children || []).map((ch) =>
        ch.type === "QGroupBox" ? normalizeGroupBox(ch) : ch
      ),
    };
  }

  function renderInput(field) {
    if (!field) return "";
    const t = field.type;
    const mono = /mac|sn|command|hex/i.test(field.objectName || "") ? " mono" : "";
    if (t === "QComboBox") {
      if (isToggleCombo(field)) return renderToggleComboSegmented(field);
      const items = (field.items || []).map((i) => `<option>${esc(i)}</option>`).join("");
      return `<select class="select${mono}" data-obj="${esc(field.objectName)}">${items}</select>`;
    }
    if (t === "QCheckBox") {
      return renderCheckboxSegmented(field);
    }
    if (t === "QSpinBox" || t === "QDoubleSpinBox") {
      return `<input class="input mono" type="number" value="${esc(field.value || "0")}" data-obj="${esc(field.objectName)}" />`;
    }
    if (t === "QPlainTextEdit" || t === "QTextEdit") {
      return `<textarea class="input" rows="2" placeholder="${esc(field.placeholder || "")}" data-obj="${esc(field.objectName)}"></textarea>`;
    }
    const val = field.value ? ` value="${esc(field.value)}"` : "";
    const ph = field.placeholder ? ` placeholder="${esc(field.placeholder)}"` : "";
    return `<input class="input${mono}"${val}${ph} data-obj="${esc(field.objectName)}" />`;
  }

  function renderButton(text, forceClass, visibleText, iconId) {
    const cls = forceClass || btnClass(text);
    const icon = iconId ? `<svg class="ico"><use href="#${iconId}" /></svg>` : "";
    return `<button type="button" class="${cls}" data-action="${esc(text)}">${icon}<span>${esc(visibleText || text)}</span></button>`;
  }

  function renderButtonRow(buttons) {
    if (!buttons?.length) return "";
    const parts = [];
    let i = 0;
    while (i < buttons.length) {
      if (i + 1 < buttons.length) {
        const pair = matchSwitchPair(buttons[i], buttons[i + 1]);
        if (pair) {
          parts.push(renderLabeledOnOffField("", pair.on, pair.off));
          i += 2;
          continue;
        }
      }
      parts.push(renderButton(buttons[i]));
      i += 1;
    }
    return `<div class="btn-row">${parts.join("")}</div>`;
  }

  function renderSection(title, inner) {
    if (!inner?.trim()) return "";
    const hideLabel = !title || title === "参数" || title === "选项";
    return `<div class="page-block">${hideLabel ? "" : `<div class="section-label">${esc(title)}</div>`}${inner}</div>`;
  }

  function renderParamRow(label, fieldHtml, actionBtn) {
    return `<div class="param-row"><label>${esc(label)}</label>${fieldHtml}${actionBtn || "<span></span>"}</div>`;
  }

  function renderFieldsGrid(fields) {
    if (!fields?.length) return "";
    return fields
      .map((f) => {
        if (f.field?.type === "QCheckBox") {
          const lbl = f.label || checkboxFeatureLabel(f.field);
          return `<div class="field-span connect-field-grid">${renderGridCheckboxSwitchRow(lbl, f.field)}</div>`;
        }
        if (f.field) {
          const lbl = f.label || "";
          return renderParamRow(lbl, renderInput(f.field), "");
        }
        if (f.label) return renderParamRow(f.label, "<span></span>", "");
        return "";
      })
      .join("");
  }

  function renderTable(t, compact) {
    const name = t.objectName || t.note || "表格";
    const rows = [
      ["产品名称", "Pump-E"],
      ["软件版本", "v1.0.3"],
      ["硬件版本", "v1.0.1"],
      ["资源版本", "v1.0.0"],
    ];
    const body = rows
      .map(([k, v]) => `<div class="fake-table-row"><span>${esc(k)}</span><span>${esc(v)}</span></div>`)
      .join("");
    const caption = compact ? "" : `<div class="fake-table-caption">${esc(t.note || name)}</div>`;
    const cls = compact ? "fake-table fake-table-compact" : "fake-table";
    return `<div class="${cls}" data-obj="${esc(name)}">${caption}
      <div class="fake-table-h"><span>测试项</span><span>测试值</span></div>${body}</div>`;
  }

  function renderControlsInline(controls) {
    if (!controls?.length) return "";
    const parts = [];
    let i = 0;
    while (i < controls.length) {
      const c = controls[i];
      const next = controls[i + 1];
      if (c.type === "QLabel" && next && isToggleCombo(next)) {
        parts.push(
          `<div class="switch-field field-span"><span class="inline-label">${esc(c.text)}</span>${wrapSegmentedSwitch(renderToggleComboSegmented(next))}</div>`
        );
        i += 2;
        continue;
      }
      if (c.type === "QCheckBox") {
        parts.push(
          `<div class="switch-field field-span"><span class="inline-label">${esc(checkboxFeatureLabel(c))}</span>${wrapSegmentedSwitch(renderCheckboxSegmented(c))}</div>`
        );
        i += 1;
        continue;
      }
      if (c.type === "QPushButton") {
        const btns = [];
        while (i < controls.length && controls[i].type === "QPushButton") {
          btns.push(controls[i].text);
          i += 1;
        }
        parts.push(renderButtonRow(btns));
        continue;
      }
      if (c.type === "QLabel") parts.push(`<span class="inline-label">${esc(c.text)}</span>`);
      else if (c.note?.startsWith("[表格:")) parts.push(renderTable(c));
      else parts.push(renderInput(c));
      i += 1;
    }
    return `<div class="control-inline">${parts.join("")}</div>`;
  }

  function renderStructuredBody(gb) {
    let html = "";
    const rows = gb.button_rows || [];
    if (rows.length === 1 && rows[0].length) {
      html += renderSection("操作", renderButtonRow(rows[0]));
    } else {
      rows.forEach((row, i) => {
        if (row.length) html += renderSection(inferRowTitle(row, i), renderButtonRow(row));
      });
    }
    const orphanBtns = (gb.controls || []).filter((c) => c.type === "QPushButton").map((c) => c.text);
    const nonBtns = (gb.controls || []).filter((c) => c.type !== "QPushButton");
    if (orphanBtns.length) {
      html += renderSection(rows.length ? "其它操作" : "操作", renderButtonRow(orphanBtns));
    }
    if (nonBtns.length) html += renderSection("选项", renderControlsInline(nonBtns));
    const fields = renderFieldsGrid(gb.fields);
    if (fields.trim()) html += renderSection("参数", fields);
    (gb.tables || []).forEach((t) => {
      html += renderSection("结果", renderTable(t));
    });
    (gb.children || []).forEach((ch) => {
      if (ch.type === "QGroupBox") {
        const child = normalizeGroupBox(ch);
        html += renderSection(child.title || "子模块", renderStructuredBody(child));
      } else {
        html += renderContentItem(ch, true);
      }
    });
    return html;
  }

  function countGroupControls(gb) {
    if (!gb) return 0;
    const node = normalizeGroupBox(gb);
    let n = (node.button_rows || []).flat().length;
    n += (node.controls || []).length;
    n += (node.fields || []).length;
    n += (node.tables || []).length;
    (node.children || []).forEach((ch) => {
      if (ch.type === "QGroupBox") n += countGroupControls(ch);
    });
    return n;
  }

  function cardMeta() {
    return "";
  }

  function renderGroupBox(gb, extraClass) {
    if (!gb) return "";
    const node = normalizeGroupBox(gb);
    const title = node.title || node.objectName || "功能";
    const sub = cardMeta(node);
    const cls = extraClass ? `card ${extraClass}` : "card";
    return `<section class="${cls}" data-obj="${esc(node.objectName || "")}">
      <div class="card-head"><div><h3>${esc(title)}</h3>${sub ? `<span class="card-sub">${esc(sub)}</span>` : ""}</div></div>
      <div class="card-body">${renderStructuredBody(node)}</div>
    </section>`;
  }

  function renderContentItem(node, nested) {
    if (!node) return "";
    if (node.kind === "action_bundle") {
      const body = node.rows.map((row) => renderButtonRow(row)).join("");
      const title = inferBundleTitle(node.rows);
      if (nested) return renderSection(title, body);
      return `<section class="card"><div class="card-head"><h3>${esc(title)}</h3></div><div class="card-body action-body">${body}</div></section>`;
    }
    if (node.kind === "button_row") {
      if (nested) return renderSection("操作", renderButtonRow(node.buttons));
      return `<section class="card"><div class="card-head"><h3>操作</h3></div><div class="card-body action-body">${renderButtonRow(node.buttons)}</div></section>`;
    }
    if (node.kind === "form_row") {
      if (nested) return renderSection("参数", renderFieldsGrid(node.fields));
      return `<section class="card"><div class="card-head"><h3>参数</h3></div><div class="card-body">${renderFieldsGrid(node.fields)}</div></section>`;
    }
    if (node.type === "QGroupBox") return renderGroupBox(node);
    if (node.type === "QSplitter") {
      return `<div class="split-panels">${(node.parts || []).map((p) => renderContentItem(p, false)).join("")}</div>`;
    }
    if (node.type === "QTabWidget") return "";
    if (node.note?.startsWith("[表格:")) {
      return `<section class="card"><div class="card-body">${renderTable(node)}</div></section>`;
    }
    if (node.type === "QPushButton") {
      return `<section class="card"><div class="card-body">${renderButtonRow([node.text])}</div></section>`;
    }
    if (node.content) return (node.content || []).map((c) => renderContentItem(c, nested)).join("");
    const inline = renderControlsInline([node]);
    if (!inline.trim()) return "";
    return nested ? renderSection("选项", inline) : `<section class="card"><div class="card-body">${inline}</div></section>`;
  }

  function inferBundleTitle(rows) {
    const text = (rows || []).flat().join(" ");
    if (/电机/.test(text)) return "电机操作";
    if (/六轴|校准|采集/.test(text)) return "校准与采集";
    if (/OTA|升级/.test(text)) return "升级";
    return inferRowTitle(rows?.[0] || [], 0);
  }

  function coalesceContent(items) {
    const out = [];
    for (const item of items || []) {
      const last = out[out.length - 1];
      if (item.kind === "button_row") {
        if (last?.kind === "action_bundle") last.rows.push(item.buttons);
        else out.push({ kind: "action_bundle", rows: [item.buttons] });
        continue;
      }
      if (item.type === "QPushButton") {
        if (last?.kind === "action_bundle") last.rows.push([item.text]);
        else out.push({ kind: "action_bundle", rows: [[item.text]] });
        continue;
      }
      out.push(item);
    }
    return out;
  }

  function renderContent(items) {
    const list = coalesceContent(items);
    if (!list.length) return '<p class="hint">（此页暂无可见控件）</p>';
    return `<div class="card-grid pack-grid">${list.map((i) => renderContentItem(i, false)).join("")}</div>`;
  }

  function getTabEntry(id) {
    if (id === "connect") return null;
    return (window.UI_PREVIEW_DATA.tabWidget.tabs || []).find((t) => t.objectName === id);
  }

  function getNestedTabWidget(tabEntry) {
    if (!tabEntry) return null;
    const key = NESTED_KEYS[tabEntry.objectName];
    if (key && tabEntry[key]) return tabEntry[key];
    for (const item of tabEntry.content || []) {
      if (item.type === "QTabWidget") return item;
    }
    return null;
  }

  function getDeviceInfoPart(objectName) {
    const tab = getTabEntry("tab_device_info");
    const splitter = (tab?.content || []).find((x) => x.type === "QSplitter");
    return (splitter?.parts || []).find((p) => p.objectName === objectName) || null;
  }

  function getBasicInfoGroupBox() {
    return getDeviceInfoPart("groupBox_11");
  }

  function getPeriphStateGroupBox() {
    return getDeviceInfoPart("groupBox_2");
  }

  function getPeripheralTabsWithState(nested) {
    const tabs = nested?.tabs || [];
    if (tabs.some((t) => t.objectName === "tab_periph_state")) return tabs;
    return [
      {
        objectName: "tab_periph_state",
        title: "外设状态",
        content: [],
        _isPeriphState: true,
      },
      ...tabs,
    ];
  }

  function findField(section, objectName) {
    return (section.fields || []).find(
      (f) => f.objectName === objectName || f.field?.objectName === objectName
    );
  }

  function stripGroupTitleNum(title) {
    return String(title ?? "").replace(/^\d+[、.．]\s*/, "");
  }

  function renderConnectHint(text) {
    if (!text) return "";
    return `<span class="connect-hint">${esc(text)}</span>`;
  }

  function renderConnectFieldStack(field, hint) {
    if (!field) return "";
    return `<div class="connect-field-stack">${renderInput(field)}${renderConnectHint(hint)}</div>`;
  }

  function renderConnectBlock(step, title, inner) {
    if (!inner?.trim()) return "";
    return `<div class="connect-block">
      <div class="connect-block-head">
        <span class="connect-step">${step}</span>
        <span class="connect-block-title">${esc(title)}</span>
      </div>
      <div class="connect-field-grid">${inner}</div>
    </div>`;
  }

  function connectFieldLabel(fieldEntry, fallback) {
    const raw = fieldEntry?.label || fallback || "";
    return String(raw).replace(/[：:]\s*$/, "").trim();
  }

  function renderConnectFieldRow(label, inner, extraClass) {
    const cls = extraClass ? ` connect-field-row ${extraClass}` : " connect-field-row";
    const labelHtml = label ? `<label>${esc(label)}</label>` : "";
    return `<div class="${cls.trim()}">${labelHtml}<div class="connect-field-control">${inner}</div></div>`;
  }

  function renderConnectFieldTip(text) {
    if (!text) return "";
    return `<p class="connect-field-tip">${esc(text)}</p>`;
  }

  const CONNECT_FLOW_STEPS = [
    { step: 1, label: "打开串口", icon: "i-plug", hint: "选择串口并点击「连接」" },
    { step: 2, label: "设置条件", icon: "i-filter", hint: "按需调整名称与信号条件" },
    { step: 3, label: "搜索设备", icon: "i-refresh", hint: "点击「开始搜索设备」" },
    { step: 4, label: "选择设备", icon: "i-device", hint: "从列表中选择要连接的设备" },
  ];

  function renderConnectFlowGuide(activeStep, completedThrough) {
    const nodes = CONNECT_FLOW_STEPS.map(({ step, label, icon }) => {
      let state = "is-pending";
      if (step <= completedThrough) state = "is-done";
      else if (step === activeStep) state = "is-active";
      return `<li class="connect-flow-node ${state}" data-flow-step="${step}">
        <span class="connect-flow-icon-wrap"><svg class="ico"><use href="#${icon}" /></svg></span>
        <span class="connect-flow-label">${esc(label)}</span>
      </li>`;
    }).join("");
    const hint = CONNECT_FLOW_STEPS.find((s) => s.step === activeStep)?.hint || "连接完成";
    return `<nav class="connect-flow-guide" aria-label="连接流程">
      <ol class="connect-flow-track">${nodes}</ol>
      <p class="connect-flow-hint" data-connect-flow-hint>${esc(hint)}</p>
    </nav>`;
  }

  function renderConnectSearchStatus() {
    return `<div class="connect-live-status connect-search-status" data-connect-status data-state="idle">
      <span class="connect-live-dot"></span>
      <div class="connect-live-copy">
        <span class="connect-live-text">准备好后，点击开始搜索</span>
      </div>
    </div>`;
  }

  function renderConnectDevicePick() {
    return `<div class="connect-device-zone" data-connect-zone data-state="idle">
      <label class="connect-device-select-label">已发现设备</label>
      <select class="select connect-device-select" data-obj="pick_device" disabled aria-label="已发现设备">
        <option value="" selected disabled>— 请先完成搜索，再选择设备 —</option>
      </select>
      <div class="connect-pick-result" data-connect-pick-result hidden>
        <span class="connect-live-dot"></span>
        <div class="connect-live-copy">
          <span class="connect-live-text">已连接</span>
          <span class="connect-live-mac" data-connect-mac-label></span>
        </div>
      </div>
    </div>`;
  }

  function renderConnectStepCard(step, title, status, statusClass, bodyHtml, cardClass) {
    const badge = status
      ? `<span class="connect-step-status ${statusClass || ""}">${esc(status)}</span>`
      : "";
    return `<section class="card connect-step-card ${cardClass || ""}" data-connect-step="${step}">
      <div class="card-head">
        <div><h3><span class="connect-step">${step}</span>${esc(title)}</h3></div>
        ${badge}
      </div>
      <div class="card-body connect-step-fields">${bodyHtml}</div>
    </section>`;
  }

  function renderConnectPage() {
    const h = window.UI_PREVIEW_DATA.header_connection_area;
    const g5 = h.groupBox_5;
    const comField = findField(g5, "comNameCombo");
    const nameField = findField(g5, "name_range");
    const macField = findField(g5, "macInput");
    const manualConnectLabel = "设备地址";
    const resetChk = findField(g5, "is_reset_dongle");

    const resetObj = resetChk?.field ? fieldObjAttr(resetChk.field).trim() : "";
    const resetSwitch = resetChk?.field
      ? wrapSegmentedSwitch(
          renderOnOffSwitch("启用连接后自动复位Dongle", "禁用连接后自动复位Dongle", resetObj)
        )
      : "";

    const step1 = renderConnectStepCard(
      1,
      "打开串口",
      "已连接",
      "ok",
      `${renderConnectFieldRow(
        "串口",
        `<div class="connect-inline-tools">
          ${comField ? renderInput(comField.field) : ""}
          ${wrapSegmentedSwitch(renderSegmentedSwitch("连接Dongle", "断开Dongle", "连接", "断开"))}
        </div>`
      )}${renderConnectFieldRow(checkboxFeatureLabel(resetChk?.field) || "连接后自动复位", resetSwitch)}`,
      "is-done"
    );

    const step2 = renderConnectStepCard(
      2,
      "设置条件",
      "当前步骤",
      "active",
      `<div class="connect-filter-body">
        ${renderConnectFieldRow(
          connectFieldLabel(nameField, "名称筛选"),
          nameField ? renderInput(nameField.field) : ""
        )}${renderConnectFieldRow(
          "最低信号",
          `<div class="connect-rssi-cell" data-rssi-row>
            <input type="range" class="connect-rssi-input" name="rssi_range" min="-100" max="-20" value="-60" step="1" aria-label="最低信号阈值" />
            <span class="connect-rssi-value" data-obj="rssi_range_value">-60 dBm</span>
          </div>`
        )}
      </div>`,
      "is-active"
    );

    const step3 = renderConnectStepCard(
      3,
      "搜索设备",
      "待搜索",
      "pending",
      `${renderConnectFieldRow(
        "",
        renderButton("开始搜索设备", "btn btn-primary", null, "i-refresh"),
        "connect-field-row-action"
      )}${renderConnectSearchStatus()}`,
      "is-ready"
    );

    const step4 = renderConnectStepCard(
      4,
      "选择设备",
      "",
      "",
      `${renderConnectDevicePick()}${renderConnectFieldTip("选好设备后会自动连接")}
      ${renderConnectFieldRow(
        `${manualConnectLabel}（可选）`,
        `<div class="connect-inline-tools connect-mac-tools">
          ${macField ? renderInput(macField.field) : ""}
          ${renderButton("连接产品", "btn btn-primary btn-sm", null, "i-link")}
          ${renderButton("测试(开发用)", "btn btn-secondary btn-sm", "测试", "i-flask")}
        </div>`
      )}`,
      "is-ready"
    );

    return `<div class="connect-page" data-obj="groupBox_5">
      ${renderConnectFlowGuide(2, 1)}
      <div class="connect-steps">${step1}${step2}${step3}${step4}</div>
    </div>`;
  }

  function renderPeriphStatePage() {
    const gb = getPeriphStateGroupBox();
    if (!gb) return '<p class="hint err">外设状态（groupBox_2）未在预览数据中找到</p>';
    return `<div class="card-grid periph-state-grid">${renderGroupBox(gb)}</div>`;
  }

  function findGroupBox(items, objectName) {
    for (const item of items || []) {
      if (item.type === "QGroupBox" && (!objectName || item.objectName === objectName)) return item;
      const nested = findGroupBox(item.content || item.children, objectName);
      if (nested) return nested;
    }
    return null;
  }

  function renderBizCard(obj, title, sub, inner, extraClass) {
    return `<section class="card ${extraClass || "biz-card"}" data-obj="${esc(obj)}">
      <div class="card-head"><div><h3>${esc(title)}</h3>${sub ? `<span class="card-sub">${esc(sub)}</span>` : ""}</div></div>
      <div class="card-body">${inner}</div>
    </section>`;
  }

  function renderBizBlock(title, inner) {
    if (!inner?.trim()) return "";
    const head = title
      ? `<div class="connect-block-head"><span class="connect-block-title">${esc(title)}</span></div>`
      : "";
    return `<div class="connect-block">${head}${inner}</div>`;
  }

  function renderReadListRow(label, action) {
    return `<div class="read-list-row"><span>${esc(label)}</span>${renderButton(action, "btn btn-secondary", "读取")}</div>`;
  }

  function renderIoPair(label, writeText, readText, field) {
    const input = renderInput(
      field || {
        type: "QLineEdit",
        objectName: "",
        placeholder: label,
      }
    );
    return `<div class="action-pair io-row">
      <label>${esc(label)}</label>
      ${input}
      ${renderButton(readText, "btn btn-secondary", "读取")}
      ${renderButton(writeText, "btn btn-primary", "写入")}
    </div>`;
  }

  function renderSchemeSection(num, title, body, extraClass) {
    return `<section class="scheme-card${extraClass ? ` ${extraClass}` : ""}">
      <span class="scheme-num">${num}</span>
      <h3>${esc(title)}</h3>
      <div class="scheme-body">${body}</div>
    </section>`;
  }

  function renderDesignSchemePage() {
    const switches = [
      ["工厂模式", "进入工厂模式", "退出工厂模式"],
      ["产测标志", "产测完成写入", "产测未完成写入"],
      ["休眠模式", "进入休眠模式", "禁止休眠模式"],
      ["吸力测试", "进入吸力测试", "退出吸力测试"],
      ["吸力补偿", "吸力补偿开", "吸力补偿关"],
      ["吸力读取", "开启吸力读取", "关闭吸力读取"],
      ["峰检辅助线", "显示峰检辅助线", "隐藏峰检辅助线"],
      ["交替运行", "开始交替运行", "立即停止"],
    ]
      .map(([lbl, on, off]) => renderLabeledOnOffField(lbl, on, off))
      .join("");

    const checks = [
      ["连接后自动复位 Dongle", "启用连接后自动复位Dongle", "禁用连接后自动复位Dongle"],
      ["仅绑定 SN", "启用仅绑定SN", "禁用仅绑定SN"],
    ]
      .map(
        ([lbl, on, off]) =>
          `<div class="scheme-check-row"><span>${esc(lbl)}</span>${wrapSegmentedSwitch(renderOnOffSwitch(on, off))}</div>`
      )
      .join("");

    const ioRows = [
      ["整机 SN", "SN20260916001"],
      ["MAC", "DC-20-C1-00-3C-49"],
      ["设备 ID", "SUBPID-1024"],
      ["产品 ID", "SKU-M8"],
    ]
      .map(
        ([lbl, val]) =>
          `<div class="io-row"><label>${esc(lbl)}</label><input class="input mono" value="${esc(val)}" />${renderButton("读取", "btn btn-secondary", "读取")}${renderButton("写入", "btn btn-primary", "写入")}</div>`
      )
      .join("");

    const readList = ["读固件版本", "读设备名称", "产测完成读取", "读取设备配置信息"]
      .map((t) => renderReadListRow(t, t))
      .join("");

    const plotSvg = renderSuctionWave(1, 0.9, 0, "#3b82f6")
      + renderSuctionWave(2, 0.7, 1.2, "#10b981")
      + renderSuctionWave(3, 0.55, 2.4, "#f59e0b");

    const stats = ["最高", "最低", "有效", "漏峰", "弱峰", "最大峰", "频率", "CH1"]
      .map((k) => `<div class="stat-cell"><b>${esc(k)}</b><span>${k === "频率" ? "1.2Hz" : k === "CH1" ? "—" : "-28.5"}</span></div>`)
      .join("");

    const flowSteps = [
      { n: 1, t: "串口", s: "done" },
      { n: 2, t: "搜索", s: "done" },
      { n: 3, t: "RSSI", s: "done" },
      { n: 4, t: "选设备", s: "active" },
      { n: 5, t: "连产品", s: "" },
      { n: 6, t: "绑定", s: "" },
    ]
      .map(
        (x) =>
          `<div class="scheme-flow-step ${x.s}"><div class="dot">${x.n}</div><span>${esc(x.t)}</span></div>`
      )
      .join("");

    const navMini = PRIMARY.filter((id) => id !== "design_scheme")
      .slice(0, 5)
      .map((id, i) => {
        const cls = i === 0 ? "nav-item active" : "nav-item";
        return `<div class="${cls}">${esc(NAV_LABEL[id] || id)}</div>`;
      })
      .join("");

    return `<div class="scheme-page">
      <header class="scheme-hero">
        <h1>上位机 UI 控件设计方案</h1>
        <div class="scheme-tags"><span>简洁</span><span>专业</span><span>温馨</span><span>高效</span></div>
      </header>
      <div class="scheme-grid">
        ${renderSchemeSection(1, "互斥开关", `<div class="scheme-switch-grid">${switches}</div>`)}
        ${renderSchemeSection(2, "Checkbox 转开关", `<div class="scheme-check-list">${checks}</div>`)}
        ${renderSchemeSection(
          3,
          "Combo 转开关",
          `<div class="scheme-check-list">
            <div class="scheme-check-row"><span>设备模式</span>${wrapSegmentedSwitch(renderSegmentedSwitch("用户模式", "工厂模式", "用户", "工厂"))}</div>
            <div class="scheme-check-row"><span>OTA 升级</span>${wrapSegmentedSwitch(renderOnOffSwitch("开启OTA", "关闭OTA"))}</div>
          </div>`
        )}
        ${renderSchemeSection(4, "写入 + 读取", ioRows)}
        ${renderSchemeSection(5, "单次读取", `<div class="read-list">${readList}</div>`)}
        ${renderSchemeSection(
          6,
          "持续读取",
          `<div class="live-read-panel">
            ${renderLabeledOnOffField("吸力读取", "开启吸力读取", "关闭吸力读取")}
            <div class="live-ch-row">
              <div class="live-ch-box"><b>CH1</b><span>-28.5 kPa</span></div>
              <div class="live-ch-box"><b>CH2</b><span>-31.2 kPa</span></div>
              <div class="live-ch-box"><b>CH3</b><span>-- kPa</span></div>
            </div>
            <div class="live-interval-row"><span>数据间隔</span><input class="input mono" value="100" /><span>ms</span><span>包间隔 12 ms</span></div>
          </div>`
        )}
        ${renderSchemeSection(
          7,
          "实时曲线",
          `<div class="scheme-plot-mini"><svg viewBox="0 0 640 100" preserveAspectRatio="none">${plotSvg}</svg></div>
           ${renderLabeledOnOffField("跟随刷新", "开启跟随", "关闭跟随")}`,
          "span-2"
        )}
        ${renderSchemeSection(8, "实时统计", `<div class="stat-grid">${stats}</div>`)}
        ${renderSchemeSection(
          9,
          "参数输入",
          `<div class="scheme-param-form">
            <label>测试次数</label><input class="input mono" type="number" value="10" />
            <label>测试间隔</label><input class="input mono" type="number" value="1000" /><span style="font-size:10px;color:var(--text-dim)">ms</span>
            <label>工作模式</label><select class="select"><option>泵阀交替</option></select>
            <label>设备名称</label><input class="input" value="M9-001" />
          </div>`
        )}
        ${renderSchemeSection(
          10,
          "参数表",
          `<table class="scheme-table"><thead><tr><th>序号</th><th>项目</th><th>值</th><th>操作</th></tr></thead>
          <tbody>
            <tr><td>1</td><td>泵电流</td><td>120 mA</td><td><span class="tbl-act">编辑</span></td></tr>
            <tr><td>2</td><td>阀开时间</td><td>80 ms</td><td><span class="tbl-act">编辑</span></td></tr>
          </tbody></table>
          <div class="scheme-btn-row">${renderButton("新增", "btn btn-secondary btn-sm")}${renderButton("保存", "btn btn-primary btn-sm")}</div>`
        )}
        ${renderSchemeSection(
          11,
          "单次按钮",
          `<div class="scheme-btn-row">${renderButton("连接", "btn btn-primary")}${renderButton("搜索", "btn btn-primary")}${renderButton("发送模式", "btn btn-primary")}${renderButton("设置 OSR", "btn btn-secondary")}${renderButton("设置全部", "btn btn-secondary")}</div>`
        )}
        ${renderSchemeSection(
          12,
          "危险按钮",
          `<div class="scheme-danger-grid">
            ${renderButton("断开", "btn btn-danger")}
            ${renderButton("停止测试", "btn btn-danger")}
            ${renderButton("重启设备", "btn btn-danger")}
            ${renderButton("恢复出厂", "btn btn-danger")}
          </div>`
        )}
        ${renderSchemeSection(
          13,
          "导出",
          `<div class="scheme-btn-row">${renderButton("导出吸力曲线 PNG", "btn btn-secondary")}${renderButton("导出测试结果 CSV", "btn btn-secondary")}${renderButton("导出日志 TXT", "btn btn-secondary")}</div>`
        )}
        ${renderSchemeSection(14, "连接流程", `<div class="scheme-flow-bar">${flowSteps}</div>`)}
        ${renderSchemeSection(15, "导航", `<div class="scheme-nav-mini">${navMini}</div><input class="input" type="search" placeholder="搜索功能…" style="margin-top:6px;height:30px;font-size:11px" />`)}
        ${renderSchemeSection(
          16,
          "状态展示",
          `<div class="scheme-status-mini">
            <div class="scheme-status-row"><span><i class="dot"></i> 电机</span><b>运行中</b></div>
            <div class="scheme-status-row"><span><i class="dot"></i> 泵/阀</span><b>正常</b></div>
            <div class="scheme-status-row"><span>电量</span><b>85%</b></div>
            <div class="scheme-status-row"><span>温度</span><b>36.5 ℃</b></div>
            <div class="scheme-status-row"><span>电流</span><b>1.24 A</b></div>
          </div>`
        )}
        ${renderSchemeSection(
          17,
          "日志",
          `<div class="scheme-log-mini">
            <div><span class="ts">14:32:01</span> 串口已连接 COM3</div>
            <div><span class="ts">14:32:03</span> 发现设备 M9-001</div>
            <div><span class="ts">14:32:08</span> 设备信息读取成功</div>
          </div>
          <div class="scheme-check-row" style="margin-top:4px"><span>自动滚动</span>${wrapSegmentedSwitch(renderOnOffSwitch("开启自动滚动", "关闭自动滚动"))}</div>
          ${renderButton("清空", "btn btn-secondary btn-sm")}`
        )}
      </div>
    </div>`;
  }

  function renderProductModeForm(gb) {
    if (!gb) return "";
    const controls = gb.controls || [];
    let fields = "";
    const btns = [];
    let i = 0;
    while (i < controls.length) {
      const c = controls[i];
      const next = controls[i + 1];
      if (c.type === "QLabel" && next && next.type !== "QPushButton" && next.type !== "QLabel") {
        fields += renderParamRow(c.text, renderInput(next), "");
        i += 2;
        continue;
      }
      if (c.type === "QPushButton") {
        btns.push(c.text);
        i += 1;
        continue;
      }
      i += 1;
    }
    const send = btns.length
      ? `<div class="connect-actions connect-actions-full">${btns
          .map((t) => renderButton(t, "btn btn-primary", t === "发送吸力测试" ? "发送模式" : t))
          .join("")}</div>`
      : "";
    return fields + send;
  }

  function renderBasicInfoBizCard() {
    const gb = getBasicInfoGroupBox();
    if (!gb) return "";
    const node = normalizeGroupBox(gb);
    const btnCtrl = (node.controls || []).find((c) => c.type === "QPushButton");
    const table = (node.tables || [])[0];
    const inner = `<div class="biz-basic-body">
      <div class="biz-basic-toolbar">${btnCtrl ? renderButton(btnCtrl.text, "btn btn-primary") : ""}</div>
      ${table ? renderTable(table, true) : ""}
    </div>`;
    return renderBizCard(node.objectName || "groupBox_11", "基础属性", "", inner, "biz-card biz-basic-card");
  }

  function renderBusinessPage(items) {
    const gb = findGroupBox(items, "groupBox_business_control") || findGroupBox(items);
    if (!gb) return renderContent(items);

    const basicCard = renderBasicInfoBizCard();
    const suction = (gb.children || []).find((c) => c.objectName === "groupBox_root_suction_test");
    const meta = (gb.children || []).find((c) => c.objectName === "groupBox_6");
    const setMode = (gb.fields || []).find((f) => f.field?.objectName === "set_mode");
    const brushLog = (gb.fields || []).find((f) => f.field?.objectName === "show_bursh_log");
    const modeParams = `<div class="connect-field-grid">
      <label>设备模式</label>${setMode ? renderInput(setMode.field) : ""}
      <label>固件版本</label><input class="input mono" value="v1.0.3" readonly data-obj="soft_version" />
      ${brushLog ? renderGridCheckboxSwitchRow("串口指令", brushLog.field) : ""}
      <div class="connect-actions field-span">${renderButton("读固件版本", "btn btn-secondary")}${renderButton("读设备名称", "btn btn-secondary")}</div>
    </div>`;

    const factoryCard = renderBizCard(
      "groupBox_business_factory",
      "工厂与电源",
      "",
      `<div class="connect-field-grid">
        ${renderGridOnOffRow("工厂模式", "进入工厂模式", "退出工厂模式")}
        ${renderGridOnOffRow("产测标志", "产测完成写入", "产测未完成写入", "", renderButton("产测完成读取", "btn btn-secondary", "读取"))}
        <label>电源</label>
        <div class="connect-actions">${renderButton("关机", "btn btn-danger")}${renderButton("重启", "btn btn-danger")}${renderButton("恢复出厂设置", "btn btn-danger")}</div>
      </div>`
    );

    const modeCard = renderBizCard(
      "groupBox_business_mode",
      "工作模式",
      "",
      renderBizBlock(
        "",
        `<div class="mode-grid">
          ${renderButton("进入亮白模式")}
          ${renderButton("清洁模式")}
          ${renderButton("开启刷牙流程")}
          ${renderButton("获取当前曲目", "btn btn-secondary")}
          ${renderLabeledOnOffField("休眠模式", "进入休眠模式", "禁止休眠模式")}
        </div>
        ${modeParams}`
      )
    );

    const suctionCard = renderBizCard(
      suction?.objectName || "groupBox_root_suction_test",
      "产品模式控制",
      "",
      renderBizBlock("", suction ? renderProductModeForm(suction) : "") +
        renderBizBlock(
          "吸力测试",
          `${renderLabeledOnOffField("吸力测试", "进入吸力测试", "退出吸力测试")}
          ${renderLabeledOnOffField("吸力补偿", "吸力补偿开", "吸力补偿关")}`
        ),
      "biz-card card-span biz-suction-card"
    );

    const keyField = (meta?.fields || []).find((f) => f.field?.objectName === "kTlvKey_data")?.field || {
      type: "QLineEdit",
      objectName: "kTlvKey_data",
      placeholder: "三元密钥",
    };
    const metaRows = meta
      ? `<div class="meta-pairs">
        ${renderIoPair("整机 SN", "写入整机的sn", "获取整机的sn", { type: "QLineEdit", objectName: "sn_whole", placeholder: "整机 SN" })}
        ${renderIoPair("板子 SN", "写入板子sn", "获取板子sn", { type: "QLineEdit", objectName: "sn_board", placeholder: "板子 SN" })}
        ${renderIoPair("MAC", "写入mac地址", "获取mac地址", { type: "QLineEdit", objectName: "meta_mac", placeholder: "设备 MAC" })}
        ${renderIoPair("设备 ID", "写入设备id", "获取设备id", { type: "QLineEdit", objectName: "meta_subpid", placeholder: "设备 ID" })}
        ${renderIoPair("产品 ID", "写入产品id", "获取产品id", { type: "QLineEdit", objectName: "meta_skuid", placeholder: "产品 ID" })}
        ${renderIoPair("三元组", "写入三元组密钥", "获取三元组信息", { ...keyField, placeholder: keyField.placeholder || "三元密钥" })}
      </div>`
      : "";
    const metaCard = meta
      ? renderBizCard(
          meta.objectName || "groupBox_6",
          "序列号与设备元数据",
          "",
          metaRows,
          "biz-card card-span"
        )
      : "";

    return `<div class="card-grid business-grid">${basicCard}${factoryCard}${modeCard}${suctionCard}${metaCard}</div>`;
  }

  function findPreviewField(gb, objectName) {
    return (gb?.fields || []).find((f) => f.field?.objectName === objectName)?.field;
  }

  function renderSuctionChip(label, obj, value, extraClass) {
    return `<label class="suction-chip${extraClass ? " " + extraClass : ""}"><span>${esc(label)}</span>
      <input class="input mono" type="number" value="${esc(value)}" data-obj="${esc(obj)}" /></label>`;
  }

  function renderSuctionSelectChip(label, field, selectedIndex, extraClass) {
    const items = (field?.items || []).map(
      (item, idx) => `<option${idx === selectedIndex ? " selected" : ""}>${esc(item)}</option>`
    );
    return `<label class="suction-chip${extraClass ? " " + extraClass : ""}"><span>${esc(label)}</span>
      <select class="select" data-obj="${esc(field?.objectName || "")}">${items.join("")}</select></label>`;
  }

  function renderSuctionWave(ch, amp, phase, color) {
    const pts = [];
    for (let i = 0; i <= 160; i += 1) {
      const t = i / 16;
      const cycle = ((t + phase) % 1.25) / 1.25;
      let v = -1.2;
      if (cycle < 0.2) v = -1.2 - amp * Math.sin((cycle / 0.2) * Math.PI);
      else if (cycle < 0.4) v = -1.2 - amp * 0.12 * Math.exp(-(cycle - 0.2) * 10);
      const x = 44 + (t / 10) * 580;
      const y = 18 + ((5 - v) / 45) * 236;
      pts.push(`${x.toFixed(1)},${y.toFixed(1)}`);
    }
    return `<path d="M${pts.join(" L")}" fill="none" stroke="${color}" stroke-width="${ch === 1 ? 1.8 : 1.35}" />`;
  }

  function renderSuctionPlot() {
    const yOf = (v) => 18 + ((5 - v) / 45) * 236;
    const xOf = (t) => 44 + (t / 10) * 580;
    const yTicks = [-40, -35, -30, -25, -20, -15, -10, -5, 0, 5];
    const xTicks = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    const grid = xTicks
      .map((t) => `<line x1="${xOf(t)}" y1="18" x2="${xOf(t)}" y2="254" stroke="rgba(255,255,255,0.06)" />`)
      .join("") +
      yTicks
        .map((v) => `<line x1="44" y1="${yOf(v)}" x2="624" y2="${yOf(v)}" stroke="rgba(255,255,255,0.06)" />`)
        .join("");
    const yLabels = yTicks
      .map((v) => `<text x="40" y="${yOf(v) + 3}" text-anchor="end" fill="#72728a" font-size="10">${v}</text>`)
      .join("");
    const xLabels = xTicks
      .map((t) => `<text x="${xOf(t)}" y="270" text-anchor="middle" fill="#72728a" font-size="10">${t}</text>`)
      .join("");
    return `<div class="suction-plot" data-obj="dongleSuctionPlotHost">
      <div class="suction-plot-legend">
        <span><b style="background:#3B82F6"></b>CH1</span>
        <span><b style="background:#4ade80"></b>CH2</span>
        <span><b style="background:#fbbf24"></b>CH3</span>
      </div>
      <svg viewBox="0 0 640 280" preserveAspectRatio="none" aria-label="吸力曲线">
        ${grid}
        <line x1="44" y1="${yOf(-36)}" x2="624" y2="${yOf(-36)}" stroke="rgba(248,113,113,0.55)" stroke-dasharray="5 4" />
        <line x1="44" y1="${yOf(-10)}" x2="624" y2="${yOf(-10)}" stroke="rgba(251,191,36,0.5)" stroke-dasharray="4 4" />
        ${renderSuctionWave(1, 34.5, 0, "#3B82F6")}
        ${renderSuctionWave(2, 31.2, 0.08, "#4ade80")}
        ${renderSuctionWave(3, 27.6, 0.16, "#fbbf24")}
        <line x1="44" y1="18" x2="44" y2="254" stroke="rgba(255,255,255,0.18)" />
        <line x1="44" y1="254" x2="624" y2="254" stroke="rgba(255,255,255,0.18)" />
        ${yLabels}${xLabels}
        <text x="332" y="278" text-anchor="middle" fill="#72728a" font-size="10">X (s)</text>
        <text x="14" y="140" text-anchor="middle" fill="#72728a" font-size="10" transform="rotate(-90 14 140)">kPa</text>
      </svg>
    </div>`;
  }

  function renderSuctionSegmentTable() {
    const rows = [];
    for (let i = 0; i < 8; i += 1) {
      const work = i % 2 === 0;
      rows.push(`<tr>
        <td>${work ? "工作时段" : "间歇时段"}</td>
        <td class="${work ? "" : "dim"}">${work ? "20000" : "-"}</td>
        <td class="${work ? "" : "dim"}">${work ? "50" : "-"}</td>
        <td>${work ? "5000" : "0"}</td>
      </tr>`);
    }
    return `<table class="suction-seg-table" data-obj="tableDongleSegments">
      <thead><tr><th>类型</th><th>频率(Hz)</th><th>占空比(%)</th><th>运行时间(ms)</th></tr></thead>
      <tbody>${rows.join("")}</tbody>
    </table>`;
  }

  function renderSuctionSpecialPage(items) {
    const peak = findGroupBox(items, "groupBox_dongle_suction_peak_params");
    const pump = findGroupBox(items, "groupBox_dongle_at_pump");
    const osr = findPreviewField(peak, "dongleSuctionOsrCombo") || {
      type: "QComboBox",
      objectName: "dongleSuctionOsrCombo",
      items: ["1 · 1024X / 8ms", "2 · 2048X / 10ms", "3 · 4096X / 11ms", "4 · 8192X / 16ms（默认）"],
    };
    const drive = findPreviewField(pump, "comboDongleDriveType") || {
      type: "QComboBox",
      objectName: "comboDongleDriveType",
      items: ["泵阀驱动", "直线电机驱动"],
    };
    return `<div class="suction-grid">
      <section class="card suction-peak" data-obj="groupBox_dongle_suction_peak_params">
        <div class="card-head"><div><h3>峰检参数</h3></div></div>
        <div class="card-body">
          <div class="suction-toolbar">
            ${renderLabeledOnOffField("吸力读取", "开启吸力读取", "关闭吸力读取")}
            ${renderButton("清空吸力曲线", "btn btn-secondary")}
            ${renderButton("跟随最新", "btn btn-secondary")}
            ${renderSuctionSelectChip("OSR档位", osr, 3, "suction-chip-wide")}
            ${renderButton("设置OSR")}
          </div>
          <div class="suction-param-row">
            ${renderSuctionChip("目标峰值(kPa)", "dongleSuctionPeakTargetSpin", "-36")}
            ${renderSuctionChip("峰值容差(kPa)", "dongleSuctionPeakToleranceSpin", "2.6")}
            ${renderSuctionChip("计频线(kPa)", "dongleSuctionPeakDipStartSpin", "-10")}
            ${renderSuctionChip("漏峰间隔(s)", "dongleSuctionPeakMaxGapSpin", "2.5")}
            ${renderButton("导出吸力曲线PNG", "btn btn-secondary")}
          </div>
          <div class="suction-param-row">
            ${renderSuctionChip("X范围(s)", "dongleSuctionXWindowSpin", "10")}
            ${renderSuctionChip("X刻度(s)", "dongleSuctionXTickSpin", "1")}
            ${renderSuctionChip("Y下限(kPa)", "dongleSuctionYMinSpin", "-40")}
            ${renderSuctionChip("Y上限(kPa)", "dongleSuctionYMaxSpin", "5")}
            ${renderSuctionChip("Y刻度(kPa)", "dongleSuctionYTickSpin", "5")}
          </div>
        </div>
      </section>
      <section class="card suction-live">
        <div class="card-body">
          <div class="suction-live-side">
            <span class="suction-stat interval">包间隔：-- ms</span>
            ${renderLabeledOnOffField("峰检辅助线", "显示峰检辅助线", "隐藏峰检辅助线")}
          </div>
          <div class="suction-live-ch">
            <div class="live-ch-row">
              <div class="live-ch-box"><b>CH1</b><span class="suction-stat">-- kPa</span></div>
              <div class="live-ch-box"><b>CH2</b><span class="suction-stat">-- kPa</span></div>
              <div class="live-ch-box"><b>CH3</b><span class="suction-stat">-- kPa</span></div>
            </div>
            <div class="stat-grid" style="margin-top:6px">
              <div class="stat-cell"><b>最高</b><span>--</span></div>
              <div class="stat-cell"><b>最低</b><span>--</span></div>
              <div class="stat-cell"><b>有效</b><span>0</span></div>
              <div class="stat-cell"><b>频率</b><span>--</span></div>
            </div>
          </div>
        </div>
      </section>
      <section class="card suction-pump" data-obj="groupBox_dongle_at_pump">
        <div class="card-head"><div><h3>泵/阀参数设置</h3></div></div>
        <div class="card-body">
          <div class="suction-pump-row">
            ${renderSuctionSelectChip("驱动方式", drive, 0)}
            <label class="suction-chip"><span>日志上报间隔(ms)</span>
              <input class="input mono" value="1000" data-obj="DongleAtFgPrint" /></label>
            ${renderButton("设置", "btn btn-primary", "设置")}
          </div>
          <div class="suction-pump-row">
            ${renderSuctionChip("每轮段数", "spinDongleSegmentCount", "8")}
            ${renderButton("读取设备配置信息", "btn btn-secondary")}
            ${renderButton("导出参数CSV", "btn btn-secondary")}
            <span class="suction-rate">频率：3.00 次/分钟</span>
          </div>
          <div class="suction-pump-row">
            <label class="suction-chip"><span>总运行(ms，0=不限)</span>
              <input class="input mono" value="0" data-obj="DongleAtPumpTotal" /></label>
            ${renderButton("设置", "btn btn-primary")}
          </div>
          <div class="suction-pump-row">
            ${renderButton("设置全部内容")}
            ${renderLabeledOnOffField("交替运行", "开始交替运行", "立即停止")}
          </div>
        </div>
      </section>
      <section class="card suction-plot-card" data-obj="dongleSuctionPlotHost">
        <div class="card-body">${renderSuctionPlot()}</div>
      </section>
      <section class="card suction-table-card" data-obj="tableDongleSegments">
        <div class="card-body">${renderSuctionSegmentTable()}</div>
      </section>
    </div>`;
  }

  function pageTitle(primary, nestedTab) {
    if (primary === "connect") return "设备连接";
    if (primary === "tab_debug_group_business") return "产品业务控制";
    if (primary === "tab_debug_group_peripheral" && nestedTab) return `外设 · ${nestedTab.title}`;
    if (nestedTab && nestedTab.title !== NAV_LABEL[primary]) return `${NAV_LABEL[primary]} · ${nestedTab.title}`;
    return NAV_LABEL[primary] || primary;
  }

  function pageSubtitle(primary, nestedTab) {
    if (primary === "connect") return "按流程完成：打开串口 → 设置条件 → 搜索 → 选择设备";
    if (primary === "tab_debug_group_business") return "基础属性与产品业务控制";
    if (nestedTab?.objectName === "tab_dongle_suction") return "峰检参数、吸力曲线与泵/阀多段循环";
    if (primary === "tab_debug_group_peripheral" && nestedTab?._isPeriphState) {
      return "外设测试结果表格与测试外设操作";
    }
    if (primary === "tab_debug_group_peripheral" && nestedTab) {
      return `${nestedTab.title} 外设调试`;
    }
    if (nestedTab && nestedTab.title !== NAV_LABEL[primary]) return nestedTab.title;
    return "";
  }

  function renderPageHeader(primary, nestedTab) {
    const el = document.getElementById("pageHeader");
    if (!el) return;
    const sub = pageSubtitle(primary, nestedTab);
    const showFilter = primary !== "connect";
    const title = `<h1>${esc(pageTitle(primary, nestedTab))}</h1>${sub ? `<p>${esc(sub)}</p>` : ""}`;
    el.className = showFilter ? "page-header-row" : "page-header";
    el.innerHTML = showFilter
      ? `<div class="page-header">${title}</div>
         <input class="input cmd-filter" id="cmdFilter" type="search" placeholder="筛选操作…" value="${esc(state.filter)}" />`
      : title;
    const filter = document.getElementById("cmdFilter");
    if (filter) {
      filter.addEventListener("input", () => {
        state.filter = filter.value;
        applyCmdFilter(state.filter);
      });
    }
  }

  function applyCmdFilter(q) {
    const needle = String(q || "").trim().toLowerCase();
    document.querySelectorAll("#mainContent .card").forEach((card) => {
      let visible = 0;
      card.querySelectorAll(".action-pair, .segmented, .connect-actions-full, .btn-row > .btn, .btn-row > .connect-actions-full, .connect-actions > .btn, .mode-grid > .btn, .suction-toolbar .connect-actions-full, .suction-param-row .btn, .suction-pump-row .btn, .suction-pump-row .connect-actions-full").forEach((el) => {
        const text = (el.textContent || "").toLowerCase();
        const show = !needle || text.includes(needle);
        el.classList.toggle("is-filtered", !show);
        if (show) visible += 1;
      });
      const hasFields = card.querySelector(".input, .select, .fake-table");
      card.classList.toggle("is-filtered", Boolean(needle) && visible === 0 && !hasFields);
    });
  }

  function playContentEnter(root) {
    if (!root || window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    root.classList.remove("is-entering");
    void root.offsetWidth;
    root.classList.add("is-entering");
    window.setTimeout(() => root.classList.remove("is-entering"), 520);
  }

  function renderMain() {
    const main = document.getElementById("mainContent");
    if (!window.UI_PREVIEW_DATA) {
      main.innerHTML = '<p class="hint err">请先运行 scripts/generate_ui_preview_html.py</p>';
      return;
    }

    let nestedTab = null;
    let html = "";

    if (state.primary === "connect") {
      renderPageHeader("connect");
      main.innerHTML = renderConnectPage();
      initConnectRssiSliders(main);
      initConnectFlow(main);
      renderTertiaryBar(null);
      initSegmentedSwitches(main);
      applyCmdFilter(state.filter);
      playContentEnter(main);
      return;
    }

    if (state.primary === "tab_device_info") {
      state.primary = "tab_debug_group_business";
    }

    const tab = getTabEntry(state.primary);
    if (!tab) {
      main.innerHTML = '<p class="hint err">Tab 未找到</p>';
      return;
    }

    let contentItems = tab.content || [];
    const nested = getNestedTabWidget(tab);
    const nestedTabs =
      nested?.tabs?.length && state.primary === "tab_debug_group_peripheral"
        ? getPeripheralTabsWithState(nested)
        : nested?.tabs || [];

    if (nestedTabs.length) {
      const ti = Math.min(state.tertiaryIndex, nestedTabs.length - 1);
      nestedTab = nestedTabs[ti];
      if (!nestedTab._isPeriphState) contentItems = nestedTab.content || [];
    }

    renderPageHeader(state.primary, nestedTab);

    if (state.primary === "tab_debug_group_business") {
      main.innerHTML = renderBusinessPage(contentItems);
    } else if (nestedTab?._isPeriphState) {
      main.innerHTML = renderPeriphStatePage();
    } else if (nestedTab?.objectName === "tab_dongle_suction") {
      main.innerHTML = renderSuctionSpecialPage(contentItems);
    } else {
      main.innerHTML = renderContent(contentItems);
    }

    renderTertiaryBar(nestedTabs.length > 1 ? nestedTabs : null);
    initSegmentedSwitches(main);
    applyCmdFilter(state.filter);
    playContentEnter(main);
  }

  function renderTertiaryBar(tabs) {
    const bar = document.getElementById("tertiaryBar");
    if (!bar) return;
    if (!tabs) {
      bar.innerHTML = "";
      bar.className = "tertiary-bar hidden";
      return;
    }
    bar.className = "tertiary-bar";
    bar.innerHTML = tabs
      .map((t, i) => {
        const cls = i === state.tertiaryIndex ? "tertiary-pill active" : "tertiary-pill";
        return `<button type="button" class="${cls}" data-idx="${i}">${esc(t.title)}</button>`;
      })
      .join("");
    bar.querySelectorAll(".tertiary-pill").forEach((btn) => {
      btn.addEventListener("click", () => {
        state.tertiaryIndex = Number(btn.dataset.idx);
        renderMain();
      });
    });
  }

  function renderPrimaryNav() {
    const nav = document.getElementById("navPrimary");
    nav.innerHTML = PRIMARY.map((id) => {
      const cls = id === state.primary ? "nav-item active" : "nav-item";
      const icon = NAV_ICONS[id] || "";
      return `<button type="button" class="${cls}" data-id="${id}">${icon}${esc(NAV_LABEL[id] || id)}</button>`;
    }).join("");
    if (nav.dataset.bound === "1") return;
    nav.dataset.bound = "1";
    nav.addEventListener("click", (e) => {
      const el = e.target.closest("[data-id]");
      if (!el || !nav.contains(el)) return;
      state.primary = el.dataset.id;
      state.tertiaryIndex = 0;
      renderPrimaryNav();
      renderHeader();
      renderMain();
    });
  }

  function renderHeader() {
    const h = window.UI_PREVIEW_DATA?.header_connection_area;
    const comField = h ? findField(h.groupBox_5, "comNameCombo") : null;

    const com = comField ? "COM3" : "—";
    const metrics = document.getElementById("headerMetrics");
    metrics.innerHTML = `
      <span class="header-pill ok"><span class="dot ok"></span>设备在线</span>
      <span class="header-pill purple"><span class="dot"></span>${esc(com)}</span>
      <span class="header-pill blue" data-dongle-status><span class="dot"></span>Dongle 已连接</span>
      <span class="header-pill"><span class="dot"></span>协议: <strong>V3.0</strong></span>
      <span class="header-pill"><span class="dot"></span>固件: <strong>1.2.3</strong></span>`;

    const brandUser = document.getElementById("brandUserLabel") || document.getElementById("brandUserName");
    if (brandUser) {
      const name = window.UI_PREVIEW_DATA?.meta?.operatorName || "工程师";
      brandUser.textContent = name;
      brandUser.title = name;
    }
  }

  function ts() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, "0");
    return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
  }

  function appendLog(html, cls) {
    const logView = document.getElementById("logView");
    const line = document.createElement("div");
    line.className = cls ? `log-line is-new ${cls}` : "log-line is-new";
    line.innerHTML = html;
    logView.appendChild(line);
    logView.scrollTop = logView.scrollHeight;
  }

  function initLogSamples() {
    const logView = document.getElementById("logView");
    if (!logView) return;
    logSamples = {
      host: logView.innerHTML,
      dongle: [
        `<div class="log-line"><span class="ts">14:32:01</span> <span class="msg">AT+CONN OK</span></div>`,
        `<div class="log-line"><span class="ts">14:32:02</span> <span class="msg">AT+BLEMTU=247</span></div>`,
        `<div class="log-line"><span class="ts">14:32:04</span> <span class="msg">RSSI -45, addr B4:56:5D:BF:53:71</span></div>`,
        `<div class="log-line warn"><span class="ts">14:32:06</span> <span class="msg">AT+LOG: packet interval 12ms &gt; threshold</span></div>`,
        `<div class="log-line"><span class="ts">14:32:09</span> <span class="msg">AT+ADC ch1=1024 ch2=998</span></div>`,
        `<div class="log-line"><span class="ts">14:32:11</span> <span class="msg">passthrough TX 64B</span></div>`,
      ].join(""),
    };
  }

  function showLogTab(name) {
    const logView = document.getElementById("logView");
    if (!logView || !logSamples) return;
    logView.innerHTML = logSamples[name] || logSamples.host;
    logView.scrollTop = logView.scrollHeight;
  }

  function updateRssiSliderVisual(row) {
    const input = row.querySelector(".connect-rssi-input");
    const valueEl = row.querySelector(".connect-rssi-value");
    if (!input || !valueEl) return;
    const min = Number(input.min);
    const max = Number(input.max);
    const value = Number(input.value);
    const pct = ((value - min) / (max - min)) * 100;
    input.style.setProperty("--rssi-pct", `${pct}%`);
    valueEl.textContent = `${value} dBm`;
    valueEl.classList.remove("is-strong", "is-mid", "is-weak");
    if (value >= -50) valueEl.classList.add("is-strong");
    else if (value >= -70) valueEl.classList.add("is-mid");
    else valueEl.classList.add("is-weak");
  }

  const CONNECT_PREVIEW_DEVICES = ["B4:56:5D:BF:53:71", "B4:56:5D:BF:53:66", "DC:20:C1:00:3C:49"];

  function updateConnectFlowGuide(scope, activeStep, completedThrough) {
    const root = scope || document;
    root.querySelectorAll("[data-flow-step]").forEach((el) => {
      const step = Number(el.dataset.flowStep);
      el.classList.remove("is-done", "is-active", "is-pending");
      if (step <= completedThrough) el.classList.add("is-done");
      else if (step === activeStep) el.classList.add("is-active");
      else el.classList.add("is-pending");
    });
    const hintEl = root.querySelector("[data-connect-flow-hint]");
    if (hintEl) {
      const hint = CONNECT_FLOW_STEPS.find((s) => s.step === activeStep)?.hint || "连接完成，可以开始测试";
      hintEl.textContent = hint;
    }
    root.querySelectorAll("[data-connect-step]").forEach((card) => {
      const step = Number(card.dataset.connectStep);
      card.classList.remove("is-done", "is-active", "is-ready");
      if (step <= completedThrough) card.classList.add("is-done");
      else if (step === activeStep) card.classList.add("is-active");
      else card.classList.add("is-ready");
    });
  }

  function setConnectStepBadge(scope, step, text, statusClass) {
    const badge = scope.querySelector(`[data-connect-step="${step}"] .connect-step-status`);
    if (!badge) return;
    badge.textContent = text;
    badge.className = `connect-step-status ${statusClass || ""}`.trim();
  }

  function setConnectLiveStatus(root, state, text, mac) {
    const scope = root || document;
    const status = scope.querySelector("[data-connect-status]");
    const zone = scope.querySelector("[data-connect-zone]");
    const pickResult = scope.querySelector("[data-connect-pick-result]");
    if (status) {
      status.dataset.state = state;
      const textEl = status.querySelector(".connect-live-text");
      if (textEl) textEl.textContent = text;
    }
    if (zone) zone.dataset.state = state;
    if (pickResult) {
      const pickText = pickResult.querySelector(".connect-live-text");
      const macEl = pickResult.querySelector("[data-connect-mac-label]");
      if (state === "connected" && mac) {
        pickResult.hidden = false;
        if (pickText) pickText.textContent = "已连接";
        if (macEl) macEl.textContent = mac;
      } else {
        pickResult.hidden = true;
        if (macEl) macEl.textContent = "";
      }
    }
  }

  function initConnectFlow(root) {
    const scope = root || document;
    const pick = scope.querySelector('[data-obj="pick_device"]');
    const searchBtn = scope.querySelector('[data-action="开始搜索设备"]');
    if (!pick || pick.dataset.bound === "1") return;
    pick.dataset.bound = "1";

    let flowActive = 2;
    let flowDone = 1;
    updateConnectFlowGuide(scope, flowActive, flowDone);

    const advanceFlow = (active, done) => {
      flowActive = active;
      flowDone = Math.max(flowDone, done);
      updateConnectFlowGuide(scope, flowActive, flowDone);
    };

    const dongleSeg = scope.querySelector('[data-connect-step="1"] [data-seg-switch]');
    if (dongleSeg) {
      // 分段控件的 off 状态表示左侧按钮激活，此处左侧是“连接”。
      dongleSeg.dataset.state = "off";
      updateSegmentedVisual(dongleSeg);
    }
    if (dongleSeg && dongleSeg.dataset.flowBound !== "1") {
      dongleSeg.dataset.flowBound = "1";
      dongleSeg.addEventListener("click", () => {
        window.setTimeout(() => {
          const connected = dongleSeg.dataset.state === "off";
          const headerStatus = document.querySelector("[data-dongle-status]");
          if (connected) {
            setConnectStepBadge(scope, 1, "已连接", "ok");
            if (headerStatus) headerStatus.innerHTML = '<span class="dot"></span>Dongle 已连接';
            advanceFlow(2, 1);
          } else {
            setConnectStepBadge(scope, 1, "未连接", "pending");
            if (headerStatus) headerStatus.innerHTML = '<span class="dot"></span>Dongle 未连接';
            flowDone = 0;
            advanceFlow(1, 0);
            setConnectLiveStatus(scope, "idle", "请先连接串口");
            pick.disabled = true;
            pick.classList.remove("is-ready", "is-connected");
            pick.innerHTML = `<option value="" selected disabled>— 请先完成搜索，再选择设备 —</option>`;
          }
        }, 0);
      });
    }

    scope.querySelectorAll('[data-connect-step="2"] input, [data-connect-step="2"] .connect-rssi-input').forEach((el) => {
      if (el.dataset.flowBound === "1") return;
      el.dataset.flowBound = "1";
      const markReady = () => {
        if (flowDone >= 2) return;
        setConnectStepBadge(scope, 2, "已设置", "ok");
        advanceFlow(3, 2);
      };
      el.addEventListener("input", markReady);
      el.addEventListener("change", markReady);
    });

    if (searchBtn && searchBtn.dataset.connectBound !== "1") {
      searchBtn.dataset.connectBound = "1";
      searchBtn.addEventListener("click", () => {
        if (flowDone < 1) {
          setConnectLiveStatus(scope, "idle", "请先连接串口");
          advanceFlow(1, flowDone);
          return;
        }
        if (flowDone < 2) {
          setConnectStepBadge(scope, 2, "使用默认", "ok");
        }
        pick.innerHTML =
          `<option value="" selected disabled>— 请选择要连接的设备 —</option>` +
          CONNECT_PREVIEW_DEVICES.map((mac) => `<option value="${esc(mac)}">${esc(mac)}</option>`).join("");
        pick.disabled = false;
        pick.classList.add("is-ready");
        setConnectLiveStatus(scope, "scanned", `已找到 ${CONNECT_PREVIEW_DEVICES.length} 台设备，请在下一步选择`);
        setConnectStepBadge(scope, 3, "已找到设备", "ok");
        setConnectStepBadge(scope, 4, "请选择", "active");
        advanceFlow(4, 3);
        pick.focus();
      });
    }

    pick.addEventListener("change", () => {
      const mac = String(pick.value || "").trim();
      if (!mac) return;
      const macInput = scope.querySelector('[data-obj="macInput"]');
      if (macInput) macInput.value = mac;
      setConnectLiveStatus(scope, "connected", "已连接", mac);
      appendLog(
        `<span class="ts">${ts()}</span> <span class="msg">正在连接设备 ${esc(mac)}</span>`,
        "ok"
      );
      appendLog(`<span class="ts">${ts()}</span> <span class="msg ok">设备连接成功</span>`, "ok");
      setConnectStepBadge(scope, 4, "已连接", "ok");
      advanceFlow(4, 4);
      pick.classList.add("is-connected");
      const onlinePill = document.querySelector("#headerMetrics .header-pill.ok");
      if (onlinePill) onlinePill.innerHTML = `<span class="dot ok"></span>设备已连接`;
    });
  }

  function initConnectRssiSliders(root) {
    (root || document).querySelectorAll("[data-rssi-row]").forEach((row) => {
      const input = row.querySelector(".connect-rssi-input");
      if (!input || input.dataset.bound === "1") return;
      input.dataset.bound = "1";
      const sync = () => updateRssiSliderVisual(row);
      input.addEventListener("input", sync);
      input.addEventListener("change", sync);
      sync();
    });
  }

  function bindLogUi() {
    initLogSamples();
    document.getElementById("logClear")?.addEventListener("click", () => {
      const logView = document.getElementById("logView");
      if (!logView) return;
      logView.innerHTML = "";
      if (logSamples) logSamples[activeLog] = "";
    });
    document.querySelectorAll(".log-tab").forEach((tab) => {
      tab.addEventListener("click", () => {
        document.querySelectorAll(".log-tab").forEach((t) => t.classList.remove("active"));
        tab.classList.add("active");
        activeLog = tab.dataset.log || "host";
        showLogTab(activeLog);
      });
    });
    document.getElementById("mainContent")?.addEventListener("click", (e) => {
      const segBtn = e.target.closest("[data-seg-switch] button[data-seg]");
      if (segBtn) {
        const seg = segBtn.closest("[data-seg-switch]");
        seg.dataset.state = segBtn.dataset.seg === "on" ? "off" : "on";
        updateSegmentedVisual(seg);
        appendLog(`<span class="ts">${ts()}</span> <span class="dir-tx">TX</span> <span class="msg">${esc(segBtn.dataset.action)}</span>`);
        return;
      }
      const btn = e.target.closest("[data-action]");
      if (!btn || btn.closest("[data-seg-switch]")) return;
      const action = btn.dataset.action || "";
      const pair = btn.closest(".action-pair");
      const field = pair?.querySelector("input, select, textarea");
      if (field) field.classList.remove("invalid");

      if (btn.classList.contains("btn-danger") && btn.dataset.armed !== "1") {
        btn.dataset.armed = "1";
        btn.dataset.label = btn.textContent;
        btn.textContent = "再点确认";
        window.setTimeout(() => {
          if (btn.dataset.armed === "1") {
            btn.dataset.armed = "";
            btn.textContent = btn.dataset.label || action;
          }
        }, 2500);
        return;
      }
      if (btn.dataset.armed === "1") {
        btn.dataset.armed = "";
        btn.textContent = btn.dataset.label || action;
      }

      if (field && /写入|设置/.test(action) && !String(field.value || "").trim()) {
        field.classList.add("invalid");
        field.focus();
        appendLog(
          `<span class="ts">${ts()}</span> <span class="msg">${esc("未填写" + (pair.querySelector("label")?.textContent || "参数") + "，已取消写入")}</span>`,
          "warn"
        );
        return;
      }
      if (field && /读取|获取/.test(action)) {
        field.value = mockReadValue(action);
        appendLog(`<span class="ts">${ts()}</span> <span class="dir-rx">RX</span> <span class="msg">${esc(action)} → ${esc(field.value)}</span>`);
        return;
      }
      const extra = field && String(field.value || "").trim() ? ` ${esc(field.value.trim())}` : "";
      appendLog(`<span class="ts">${ts()}</span> <span class="dir-tx">TX</span> <span class="msg">${esc(action)}${extra}</span>`);
    });
    document.addEventListener("keydown", (e) => {
      if (e.key !== "/" || e.target.closest("input, textarea, select")) return;
      const filter = document.getElementById("cmdFilter");
      if (!filter) return;
      e.preventDefault();
      filter.focus();
    });
  }

  function mockReadValue(action) {
    if (/mac/i.test(action)) return "DC-20-C1-00-3C-49";
    if (/设备id/i.test(action)) return "SUBPID-1024";
    if (/产品id/i.test(action)) return "SKU-M8";
    if (/三元/i.test(action)) return "1111111234567891";
    if (/整机/i.test(action)) return "SN20260916001";
    if (/板子/i.test(action)) return "BD20260916088";
    return "OK";
  }

  function initSegmentedSwitches(root) {
    (root || document).querySelectorAll("[data-seg-switch]").forEach(updateSegmentedVisual);
  }

  function clampLayout(v, min, max) {
    return Math.min(max, Math.max(min, v));
  }

  function bindLayoutDrag(handle, bodyClass, onDelta, onDone, onStart) {
    if (!handle || handle.dataset.dragBound === "1") return;
    handle.dataset.dragBound = "1";

    const startDrag = (clientX, clientY) => {
      const ctx = { startX: clientX, startY: clientY };
      if (onStart) onStart(ctx);
      handle.classList.add("is-dragging");
      document.body.classList.add(bodyClass);

      const onMove = (ev) => {
        onDelta(ev.clientX - ctx.startX, ev.clientY - ctx.startY, ctx);
      };
      const onUp = () => {
        handle.classList.remove("is-dragging");
        document.body.classList.remove(bodyClass);
        document.removeEventListener("mousemove", onMove);
        document.removeEventListener("mouseup", onUp);
        window.removeEventListener("blur", onUp);
        if (onDone) onDone();
      };

      document.addEventListener("mousemove", onMove);
      document.addEventListener("mouseup", onUp);
      window.addEventListener("blur", onUp);
    };

    handle.addEventListener("mousedown", (e) => {
      if (e.button !== 0) return;
      e.preventDefault();
      startDrag(e.clientX, e.clientY);
    });
    handle.addEventListener(
      "touchstart",
      (e) => {
        if (!e.changedTouches?.[0]) return;
        e.preventDefault();
        startDrag(e.changedTouches[0].clientX, e.changedTouches[0].clientY);
      },
      { passive: false }
    );
  }

  function initLayoutResizers() {
    const app = document.getElementById("appLayout") || document.querySelector(".app");
    if (!app || app.dataset.layoutBound === "1") return;
    app.dataset.layoutBound = "1";

    const KEY = "ui-preview-panel-layout";
    const layout = { sidebarW: 242, railW: 320, statusH: null };
    try {
      const saved = JSON.parse(localStorage.getItem(KEY) || "{}");
      if (Number.isFinite(saved.sidebarW)) layout.sidebarW = saved.sidebarW;
      if (Number.isFinite(saved.railW)) layout.railW = saved.railW;
      if (Number.isFinite(saved.statusH)) layout.statusH = saved.statusH;
    } catch (_) {
      /* ignore */
    }

    const saveLayout = () => {
      try {
        localStorage.setItem(KEY, JSON.stringify(layout));
      } catch (_) {
        /* ignore */
      }
    };

    const applyLayout = () => {
      app.style.setProperty("--sidebar-w", `${layout.sidebarW}px`);
      app.style.setProperty("--rail-w", `${layout.railW}px`);
      document.documentElement.style.setProperty("--sidebar-w", `${layout.sidebarW}px`);
      document.documentElement.style.setProperty("--rail-w", `${layout.railW}px`);
      if (layout.statusH != null) {
        app.style.setProperty("--status-panel-h", `${layout.statusH}px`);
      } else {
        app.style.removeProperty("--status-panel-h");
      }
    };
    applyLayout();

    bindLayoutDrag(
      document.getElementById("sidebarSplit"),
      "layout-resizing-col",
      (dx, _dy, ctx) => {
        layout.sidebarW = clampLayout(ctx.baseSidebar + dx, 200, 380);
        applyLayout();
      },
      saveLayout,
      (ctx) => {
        ctx.baseSidebar = layout.sidebarW;
      }
    );

    bindLayoutDrag(
      document.getElementById("railColSplit"),
      "layout-resizing-col",
      (dx, _dy, ctx) => {
        layout.railW = clampLayout(ctx.baseRail - dx, 260, 520);
        applyLayout();
      },
      saveLayout,
      (ctx) => {
        ctx.baseRail = layout.railW;
      }
    );

    const rowSplit = document.getElementById("railRowSplit");
    const statusPanel = document.getElementById("statusPanel");
    const rail = rowSplit?.closest(".rail");
    if (rowSplit && statusPanel && rail) {
      bindLayoutDrag(
        rowSplit,
        "layout-resizing-row",
        (_dx, dy, ctx) => {
          const maxH = Math.max(140, rail.clientHeight - 156);
          layout.statusH = clampLayout(ctx.baseStatusH + dy, 140, maxH);
          applyLayout();
        },
        saveLayout,
        (ctx) => {
          ctx.baseStatusH = statusPanel.getBoundingClientRect().height;
        }
      );
    }
  }

  function init() {
    if (!window.UI_PREVIEW_DATA) {
      document.getElementById("mainContent").innerHTML =
        '<p class="hint err">未加载 ui-preview-data.js</p>';
      return;
    }
    renderPrimaryNav();
    renderHeader();
    renderMain();
    bindLogUi();
    initLayoutResizers();
    document.body.classList.add("is-app-ready");
    const note = document.getElementById("designNote");
    if (note) {
      note.textContent = `薰衣草紫控制台 · ${window.UI_PREVIEW_DATA.source} · 点击按钮写入日志`;
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
