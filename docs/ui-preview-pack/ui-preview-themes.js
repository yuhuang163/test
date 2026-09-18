/** 控制台预览配色 — 布局同 device-control-panel-final.html，仅换强调色与光晕 */
window.UI_PREVIEW_THEMES = {
  lavender: {
    id: "lavender",
    name: "品牌蓝",
    desc: "Momcozy Soft Medical-Tech：#4285F4 主色，浅底白卡片。",
    vars: {
      "--focus": "#3b82f6",
      "--focus-hover": "#2563eb",
      "--focus-soft": "#eaf3ff",
      "--focus-glow": "rgba(59, 130, 246, 0.22)",
      "--accent-cyan": "#62a3fb",
      "--border-accent": "rgba(59, 130, 246, 0.35)",
      "--glow-a": "rgba(59, 130, 246, 0.12)",
      "--glow-b": "rgba(98, 163, 251, 0.10)",
    },
  },
  indigo: {
    id: "indigo",
    name: "品牌蓝",
    current: true,
    desc: "定稿配色：Momcozy 组件主色 #3b82f6，浅底白卡片。",
    vars: {
      "--focus": "#3b82f6",
      "--focus-hover": "#2563eb",
      "--focus-soft": "#eaf3ff",
      "--focus-glow": "rgba(59, 130, 246, 0.22)",
      "--accent-cyan": "#62a3fb",
      "--border-accent": "rgba(59, 130, 246, 0.35)",
      "--glow-a": "rgba(59, 130, 246, 0.12)",
      "--glow-b": "rgba(98, 163, 251, 0.10)",
    },
  },
  steel: {
    id: "steel",
    name: "钢蓝工业",
    desc: "同布局，钢蓝强调。产线 HMI 熟悉感。",
    vars: {
      "--focus": "#5b9bd5",
      "--focus-soft": "rgba(91, 155, 213, 0.14)",
      "--focus-glow": "rgba(91, 155, 213, 0.32)",
      "--accent-cyan": "#7ec8e3",
      "--border-accent": "rgba(91, 155, 213, 0.38)",
      "--glow-a": "rgba(91, 155, 213, 0.08)",
      "--glow-b": "rgba(56, 120, 180, 0.05)",
    },
  },
  ice: {
    id: "ice",
    name: "冰蓝深黑",
    desc: "同布局，Nord 冰蓝强调。柔和可读。",
    vars: {
      "--focus": "#88c0d0",
      "--focus-soft": "rgba(136, 192, 208, 0.14)",
      "--focus-glow": "rgba(136, 192, 208, 0.32)",
      "--accent-cyan": "#8fbcbb",
      "--border-accent": "rgba(136, 192, 208, 0.38)",
      "--glow-a": "rgba(136, 192, 208, 0.08)",
      "--glow-b": "rgba(94, 129, 156, 0.05)",
    },
  },
  amber: {
    id: "amber",
    name: "琥珀 Forge",
    desc: "同布局，琥珀金强调。工厂警示辨识。",
    vars: {
      "--focus": "#e8a838",
      "--focus-soft": "rgba(232, 168, 56, 0.14)",
      "--focus-glow": "rgba(232, 168, 56, 0.3)",
      "--accent-cyan": "#f0c060",
      "--border-accent": "rgba(232, 168, 56, 0.38)",
      "--glow-a": "rgba(232, 168, 56, 0.07)",
      "--glow-b": "rgba(180, 120, 40, 0.04)",
    },
  },
  gold: {
    id: "gold",
    name: "黑金产线",
    desc: "同布局，金色强调。演示 / 大屏。",
    vars: {
      "--focus": "#c9a227",
      "--focus-soft": "rgba(201, 162, 39, 0.12)",
      "--focus-glow": "rgba(201, 162, 39, 0.28)",
      "--accent-cyan": "#dbb84a",
      "--border-accent": "rgba(201, 162, 39, 0.36)",
      "--glow-a": "rgba(201, 162, 39, 0.06)",
      "--glow-b": "rgba(160, 130, 30, 0.04)",
    },
  },
};

window.applyUiPreviewTheme = function applyUiPreviewTheme(themeId) {
  const theme = window.UI_PREVIEW_THEMES[themeId] || window.UI_PREVIEW_THEMES.lavender;
  const lines = Object.entries(theme.vars).map(([k, v]) => `  ${k}: ${v};`);
  let el = document.getElementById("themeOverride");
  if (!el) {
    el = document.createElement("style");
    el.id = "themeOverride";
    document.head.appendChild(el);
  }
  el.textContent = `:root {\n${lines.join("\n")}\n}`;
  document.body.dataset.theme = theme.id;
  const title = document.querySelector("title");
  if (title) title.textContent = `Production Test Control Panel · ${theme.name}`;
  return theme;
};

(function bootstrapTheme() {
  const q = new URLSearchParams(location.search).get("theme") || "indigo";
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", () => window.applyUiPreviewTheme(q));
  } else {
    window.applyUiPreviewTheme(q);
  }
})();