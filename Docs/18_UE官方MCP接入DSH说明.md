# UE 官方 MCP 接入 DSH 说明

本文记录如何把 **UE 5.8 引擎自带的官方 MCP 服务**（`ModelContextProtocol` 插件，非第三方）接入 DSH，
让 AI 助手能直接读取和操作本项目正在运行的 UE 编辑器。

结论：**已配置完成**。DSH 重启会话后会出现 `mcp__unreal__*` 工具。

---

## 1. 这套东西是什么

UE 5.8 引擎自带一个实验性插件，在编辑器进程内起一个 **Streamable HTTP 的 MCP 服务器**：

| 项 | 值 |
| --- | --- |
| 插件路径 | `F:\GameStudy\UE_5.8\Engine\Plugins\Experimental\ModelContextProtocol` |
| 插件显示名 | Unreal MCP（`CreatedBy: Epic Games, Inc.`） |
| 端点 | `http://127.0.0.1:8000/mcp` |
| 默认端口 / 路径 | `DefaultServerPort = 8000`、`DefaultServerUrlPath = "/mcp"`（见 `ModelContextProtocol.h`） |
| 协议版本 | 支持 `2025-11-25` / `2025-06-18` / `2024-11-05` |

**端口和路径是引擎里的常量，不是随机分配的**，所以 DSH 侧可以直接写死静态地址
（这一点和 OpenDesign 那个必须动态探测端口的 MCP 完全不同）。

本项目侧的配置（已存在，本次未改动）：

- `JJK_theHiddenOnes.uproject` 已启用 `ModelContextProtocol` 与 `AllToolsets` 两个插件（Editor 目标）。
- `Config/DefaultEditorPerProjectUserSettings.ini`：

  ```ini
  [/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
  bAutoStartServer=True
  ServerPortNumber=8000
  ServerUrlPath=/mcp
  ```

  `bAutoStartServer=True` 是关键：**编辑器一启动就自动开端口**，不需要手动敲控制台命令。
  默认值本来是 `false`，本项目已打开。

## 2. DSH 侧配置

写在这里：`C:\Users\yaeonh\.dsh\profiles\desktop\cordis.patch.yml`

```yaml
- insert:
    - id: mcp-unreal
      name: '@deepseek-ai/dsh-mcp-client'
      config:
        serverName: unreal
        transport: streamable-http
        url: http://127.0.0.1:8000/mcp
        toolCallTimeoutMs: 180000
        reconnect:
          enabled: true
          initialDelayMs: 2000
          maxDelayMs: 30000
          maxAttempts: 120
```

设计说明：

- 用 `streamable-http`：服务由 UE 编辑器提供，不是子进程，所以不能用 stdio 方式拉起。
- **故意不设 `failOnStartupError`**：UE 没开的时候 DSH 照常启动，连接失败交给 `reconnect` 后台重试，
  编辑器一开起来工具就自动出现。设成 `true` 会导致「没开 UE 就打不开 DSH」。
- `maxAttempts: 120`（约 40–60 分钟）是按「大工程编辑器启动慢」放宽的。
- `toolCallTimeoutMs: 180000`：资产/蓝图类调用比默认 60s 慢。

改完这个文件需要**重启 DSH 会话**才生效（MCP 客户端在 host 启动阶段挂载）。

## 3. 工具形态：只有 3 个原生工具

```
mcp__unreal__list_toolsets     # 列出所有工具集
mcp__unreal__describe_toolset  # 查看某个工具集里的函数及其入参 schema
mcp__unreal__call_tool         # 真正调用
```

这是插件的「工具搜索」模式（`bEnableToolSearch=true`）：**不把几百个 UE 能力逐个注册成原生工具**，
而是先列工具集、再看 schema、最后用 `call_tool` 二次分派。好处是不必为每个请求塞进海量工具定义。

本项目当前实测：**52 个工具集**，覆盖 Actor / 资产 / 蓝图 / 材质 / UMG / GAS / Sequencer /
StateTree / Niagara / PCG / 物理资产 / 自动化测试 / 配置读写 / 日志 等。

### 正确用法

```jsonc
// 1) 找工具集
{ "name": "list_toolsets", "arguments": {} }

// 2) 看某个工具集有哪些函数、入参 schema
{ "name": "describe_toolset", "arguments": { "toolset_name": "editor_toolset.toolsets.scene.SceneTools" } }

// 3) 调用
{
  "name": "call_tool",
  "arguments": {
    "toolset_name": "editor_toolset.toolsets.scene.SceneTools",
    "tool_name": "get_current_level",   // ← 去掉工具集前缀的裸函数名
    "arguments": {}
  }
}
```

### 三个实测踩过的坑

1. **`tool_name` 必须去掉工具集前缀。** 传 `editor_toolset.toolsets.scene.SceneTools.get_current_level`
   会得到 `Unknown tool ...`。只传 `get_current_level`。
2. **可选 struct 参数不能显式传 `null`。** 例如 `find_actors` 的 `root` / `actor_type` / `bounds`
   虽然有 `default: null`，但显式传 `null` 会报
   `could not convert incoming function input params Json to a UStruct`。这类参数直接**省略**。
3. **schema 的 `required` 不完全可信。** 有些函数（如 `find_actors` 的 `tag`、`collision_channels`）
   标着可选，实际不传会报错。先按 schema 传，报错就补齐。

## 4. 自检与排障

```powershell
node C:\Users\yaeonh\.dsh\mcp\_verify_ue_mcp.mjs
```

该脚本检查：端点可达 → 协议协商 → 3 个原生工具在位 → 工具集注册表可读 → 真实调用编辑器（读当前关卡）。
全 PASS 时退出码为 0。`mcp__unreal__*` 工具没出现或报错时先跑它。

排查顺序：

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| 自检连不上 8000 | UE 编辑器没开 | 开编辑器；确认 `bAutoStartServer=True` |
| 编辑器开着但端口不通 | 服务器的自动启动被关掉了 | 编辑器控制台执行 `ModelContextProtocol.StartServer`，或检查上面那个 ini |
| 8000 被别的程序占了 | 端口冲突 | 编辑器控制台 `ModelContextProtocol.StartServer <别的端口>`，并同步改 `cordis.patch.yml` 的 `url` |
| DSH 里没有 `mcp__unreal__*` | 会话还没重启 | 重启 DSH 会话 |
| DSH 里工具在但一直失败 | 编辑器重启过，端口/会话变了 | 等 `reconnect`（最长 30s 一次重试），或重启会话 |

相关控制台命令（编辑器内 `~` 或输出日志命令框）：

- `ModelContextProtocol.StartServer [port]` / `ModelContextProtocol.StopServer`
- `ModelContextProtocol.RefreshTools`
- `ModelContextProtocol.GenerateClientConfig <ClaudeCode|Cursor|VSCode|Gemini|Codex|All>`
  —— 官方自带的客户端配置生成器，可以生成别的 MCP 客户端的配置文件。

## 5. 相关文件

| 路径 | 说明 |
| --- | --- |
| `~\.dsh\profiles\desktop\cordis.patch.yml` | DSH 侧 MCP 挂载配置（含 OpenDesign 与 unreal 两条） |
| `~\.dsh\mcp\_verify_ue_mcp.mjs` | UE MCP 连接自检脚本 |
| `~\.dsh\mcp\_validate_patch.mjs` | 校验 `cordis.patch.yml` 结构与 `mcp-unreal` 字段 |
| `Config\DefaultEditorPerProjectUserSettings.ini` | UE 侧自动启动与端口设置 |
| `JJK_theHiddenOnes.uproject` | 插件启用清单 |
