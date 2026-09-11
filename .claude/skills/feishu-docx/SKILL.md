---
name: feishu-docx
description: Export, write, and manage Feishu/Lark cloud documents. Supports docx, sheets, bitable, wiki, WeChat article import/export, drive management, and browser-based export for public or browser-readable docs. Use when reading/writing Feishu docs. When creating docs for humans to read, MUST follow the「写入云文档排版规范」section (table width, row limits, no mermaid)—do not paste GitHub-flavored Markdown verbatim.
---

# Feishu Docx Exporter

Export Feishu/Lark cloud documents to Markdown for AI analysis, writing, and automation.

## Setup (One-time)

```bash
pip install feishu-docx
feishu-docx config set --app-id YOUR_APP_ID --app-secret YOUR_APP_SECRET
```

> Token auto-refreshes. No user interaction required.

Optional features are split into extras so the core install stays lightweight. Quote extras in zsh and similar shells.

### Optional: Browser-Based Export

`export-browser` requires Playwright and a Chromium runtime:

```bash
pip install 'feishu-docx[browser]'
playwright install chromium
```

### Optional: PDF Export

PDF export requires the `pdf` extra:

```bash
pip install 'feishu-docx[pdf]'
```

## Export Documents

```bash
feishu-docx export "<FEISHU_URL>" -o ./output
```

The exported Markdown file will be saved with the document's title as filename.

If the document is public or only readable in your current browser session, prefer:

```bash
feishu-docx export-browser "<FEISHU_OR_LARK_URL>" -o ./output
```

Or reuse an existing Playwright session:

```bash
feishu-docx export-browser "<FEISHU_OR_LARK_URL>" --storage-state ./storage_state.json
```

### Supported Document Types

- **docx**: Feishu cloud documents → Markdown with images
- **sheet**: Spreadsheets → Markdown tables
- **bitable**: Multidimensional tables → Markdown tables
- **wiki**: Knowledge base nodes → Auto-resolved and exported
- **public/browser-readable docs**: Browser-based export with local images and attachments

## Command Reference

| Command | Description |
|---------|-------------|
| `feishu-docx export <URL>` | Export document to Markdown |
| `feishu-docx export <URL> --pdf` | Export document to Markdown and PDF |
| `feishu-docx export-browser <URL>` | Export in a real browser session with local assets |
| `feishu-docx export-wechat <URL>` | Export WeChat article to Markdown |
| `feishu-docx create <TITLE>` | Create new document |
| `feishu-docx create --url <URL>` | Create document from WeChat article |
| `feishu-docx write <URL>` | Append content to document |
| `feishu-docx update <URL>` | Update specific block |
| `feishu-docx drive ls` | List app or personal cloud-space files |
| `feishu-docx drive perm-show <TOKEN>` | Show public permission |
| `feishu-docx drive perm-members <TOKEN>` | List permission members |
| `feishu-docx drive clear` | Clear files with double confirmation |
| `feishu-docx export-wiki-space <URL>` | Batch export entire wiki space |
| `feishu-docx export-workspace-schema <ID>` | Export bitable database schema |
| `feishu-docx auth` | OAuth authorization |
| `feishu-docx config set` | Set credentials |
| `feishu-docx config show` | Show current config |
| `feishu-docx config clear` | Clear token cache |
| `feishu-docx tui` | Interactive TUI interface |

## Examples

### Export a wiki page

```bash
feishu-docx export "https://xxx.feishu.cn/wiki/ABC123" -o ./docs
```

### Export a document with custom filename

```bash
feishu-docx export "https://xxx.feishu.cn/docx/XYZ789" -o ./docs -n meeting_notes
```

### Export a public or browser-readable doc in a real browser session

```bash
feishu-docx export-browser "https://xxx.larkoffice.com/wiki/ABC123" -o ./browser_docs
```

### Export a document as PDF

```bash
feishu-docx export "https://xxx.feishu.cn/docx/XYZ789" --pdf
```

### Read content directly (recommended for AI Agent)

```bash
# Output content to stdout instead of saving to file
feishu-docx export "https://xxx.feishu.cn/wiki/ABC123" --stdout
# or use short flag
feishu-docx export "https://xxx.feishu.cn/wiki/ABC123" -c
```

### Export with Block IDs (for later updates)

```bash
# Include block IDs as HTML comments in the Markdown output
feishu-docx export "https://xxx.feishu.cn/wiki/ABC123" --with-block-ids
# or use short flag
feishu-docx export "https://xxx.feishu.cn/wiki/ABC123" -b
```

### Batch Export Entire Wiki Space

```bash
# Export all documents in a wiki space (auto-extract space_id from URL)
feishu-docx export-wiki-space "https://xxx.feishu.cn/wiki/ABC123" -o ./wiki_backup

# Specify depth limit
feishu-docx export-wiki-space "https://xxx.feishu.cn/wiki/ABC123" -o ./docs --max-depth 3

# Export with Block IDs for later updates
feishu-docx export-wiki-space "https://xxx.feishu.cn/wiki/ABC123" -o ./docs -b
```

### Export Database Schema

```bash
# Export bitable/workspace database schema as Markdown
feishu-docx export-workspace-schema <workspace_id>

# Specify output file
feishu-docx export-workspace-schema <workspace_id> -o ./schema.md
```

## Write Documents (CLI)

### Create Document

```bash
# Create empty document
feishu-docx create "我的笔记"

# Create with Markdown content
feishu-docx create "会议记录" -c "# 会议纪要\n\n- 议题一\n- 议题二"

# Create from Markdown file
feishu-docx create "周报" -f ./weekly_report.md

# Create in specific folder
feishu-docx create "笔记" --folder fldcnXXXXXX

# Create from a WeChat article URL
feishu-docx create --url "https://mp.weixin.qq.com/s/xxxxx"
```

**如何获取 folder token**:
1. 云空间文件夹：URL `https://xxx.feishu.cn/drive/folder/fldcnXXXXXX` → token 为 `fldcnXXXXXX`
2. **知识库 wiki 子节点**：URL `https://xxx.feishu.cn/wiki/YcR6wqDKEiuAEUk67fQcke3rnMf` → `--folder` 直接用 wiki 节点 token（`YcR6wqDKEiuAEUk67fQcke3rnMf`），文档会挂在该节点下
3. 企业域（如 momcozy-in.feishu.cn）同样适用；个人文档用 `--auth-mode oauth`

### Append Content to Existing Document

```bash
# Append Markdown content
feishu-docx write "https://xxx.feishu.cn/docx/xxx" -c "## 新章节\n\n内容"

# Append from file
feishu-docx write "https://xxx.feishu.cn/docx/xxx" -f ./content.md
```

## Manage Drive Files

```bash
# List app cloud-space documents
feishu-docx drive ls --auth-mode tenant --type docx

# List personal cloud-space documents
feishu-docx drive ls --auth-mode oauth --type docx

# Show public permission
feishu-docx drive perm-show "https://xxx.feishu.cn/docx/ABC123"

# List permission members
feishu-docx drive perm-members "https://xxx.feishu.cn/docx/ABC123"

# Clear files with double confirmation
feishu-docx drive clear --type docx
```

### Update Specific Block

```bash
# Step 1: Export with Block IDs
feishu-docx export "https://xxx.feishu.cn/docx/xxx" -b -o ./

# Step 2: Find block ID from HTML comments
# <!-- block:blk123abc -->
# # Heading
# <!-- /block -->

# Step 3: Update the specific block
feishu-docx update "https://xxx.feishu.cn/docx/xxx" -b blk123abc -c "新内容"
```

> **Tip for AI Agents**: When you need to update a specific section:
> 1. Export with `-b` to get block IDs
> 2. Find the target block ID from HTML comments
> 3. Use `feishu-docx update` with that block ID

## 写入云文档排版规范（必读）

`feishu-docx create/write` 把 Markdown 转成飞书 Block 时，**不支持** Markdown/HTML 指定列宽；默认三列表格列宽很窄、长路径会竖排换行。**Agent 写稿时必须按下列规则预处理**，不要直接把仓库 README / 本地 `.md` 原样 `-f` 上传。

### 表格

| 规则 | 说明 |
| --- | --- |
| **每张表 ≤ 5 行数据**（不含表头） | 超过约 8 行会被飞书拆成两张碎表（最后一两行单独成表） |
| **表头尽量短** | 用「测试 / 生产」代替「测试环境 / 生产环境」；用「项目」代替过长列名 |
| **单元格内缩短文案** | 路径去掉反引号；能缩写就缩写（如 `deploy-test/`）；不要把完整 `C:\inetpub\...` 和长 URL 塞进同一格 |
| **宽对照表改用双栏列表** | 环境对照、配置对照等：用「### 测试」「### 生产」两个小标题 + 无序列表，比三列表更宽、更易读 |
| **禁止宽表硬撑** | 列数 > 3、或任一格超过 ~25 字时，改列表或拆成多张表 |
| **脚本速查等小表** | 仅 2 列、≤4 行时可保留表格；第一列用短名 |

**环境对照推荐写法（代替三列表）：**

```markdown
### 测试环境
- 目录：fwq-deploy-test/
- 脚本：打包部署-测试.bat
- 端口：8801
- 域名：fctp-test.luteos.site

### 生产环境
- 目录：fwq-deploy/
- 脚本：打包部署-生产.bat
- 端口：8800
- 域名：fctp.luteos.com
```

### 列宽（API 限制 + 交付说明）

- **CLI 无法自动拉宽列**；`-f` / `-c` 写入后列宽由飞书客户端默认值决定。
- 文档交付给用户时，在文末加一句：**「请在飞书客户端点表格 → 拖动列边界至内容可读；或全选表格使用自动调整。」**
- 若用户强调「表格必须一次到位、列很宽」：改用**飞书电子表格**（sheet）放对照表，云文档正文只放链接；或让用户在定稿后手动拖一次列宽（运维/部署类文档可接受）。

### 其他格式禁忌

| 禁止 | 改用 |
| --- | --- |
| Mermaid / flowchart | **步骤 1～N** 加粗标题，或简短无序列表 |
| 正文 `# 一级标题` | `create "标题"` 已生成文档名；正文从 `##` 或加粗导语开始，避免双标题 |
| Markdown 有序列表 `1. 2. 3.` | 易全显示为「1.」；用 **步骤 1 · …** 或 `-` 列表 |
| 同一表混超长英文路径 + 中文 | 路径单独放代码块或列表项 |

### 写入工作流（Agent）

1. 先写/改 `*-飞书版.md`（按本规范），**不要**用含 Mermaid、宽表、双 `#` 的仓库原稿。
2. 个人文档：`feishu-docx create "标题" -f ./xxx-飞书版.md --auth-mode oauth`
3. 导出自检：`feishu-docx export "<URL>" --auth-mode oauth -c`，检查是否出现**两张表头相同的碎表**、是否出现 `flowchart TD`  plaintext。
4. 若表格仍窄：在回复中说明需在飞书 UI 拖列宽，或下一版改为双栏列表重写。

### 本地完整版 vs 飞书版

- 仓库 `docs/*.md`：可保留 Mermaid、宽表、完整路径（给 Git / Cursor 看）。
- 上传飞书：单独维护 `*-飞书版.md`，或写入前按本节改写。

---

## Tips

- Images and attachments auto-download to `{doc_title}/` folder when local assets are available
- Prefer `export-browser` for public share links or browser-readable docs
- Use `--stdout` or `-c` for direct content output (recommended for agents)
- Use `-b` to export with block IDs for later updates
- Token auto-refreshes, no re-auth needed
- For Lark (overseas): add `--lark` flag
- `tenant_access_token` manages app cloud space, `user_access_token` manages personal cloud space
