"""Local UE MCP client. Each invocation initializes a fresh editor session.

Usage: python Scripts/ue_mcp.py list_toolsets
       python Scripts/ue_mcp.py describe_toolset '{"toolset_name":"..."}'
       python Scripts/ue_mcp.py call_tool '{"toolset_name":"...","tool_name":"...","arguments":{}}'
"""
import json
import sys
import urllib.request


class UnrealMCP:
    def __init__(self, url="http://127.0.0.1:8000/mcp"):
        self.url = url
        self.session = None
        self.sequence = 0
        self.rpc("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                               "clientInfo": {"name": "codex-m1", "version": "1.0"}})
        self.rpc("notifications/initialized", {}, notification=True)

    def rpc(self, method, params, notification=False):
        self.sequence += 1
        body = {"jsonrpc": "2.0", "method": method, "params": params}
        if not notification:
            body["id"] = self.sequence
        headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        req = urllib.request.Request(self.url, json.dumps(body).encode(), headers)
        with urllib.request.urlopen(req, timeout=60) as response:
            self.session = response.headers.get("Mcp-Session-Id", self.session)
            payload = response.read().decode("utf-8")
        if not payload:
            return None
        data = json.loads(payload)
        if "error" in data:
            raise RuntimeError(data["error"])
        return data.get("result")

    def call(self, name, arguments=None):
        result = self.rpc("tools/call", {"name": name, "arguments": arguments or {}})
        if result.get("isError"):
            raise RuntimeError(result)
        return result

    def tool(self, toolset, name, arguments=None):
        return self.call("call_tool", {"toolset_name": toolset, "tool_name": name,
                                       "arguments": arguments or {}})


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    client = UnrealMCP()
    result = client.call(sys.argv[1], json.loads(sys.argv[2]) if len(sys.argv) > 2 else {})
    for item in result.get("content", []):
        if item.get("type") == "text":
            print(item["text"])
