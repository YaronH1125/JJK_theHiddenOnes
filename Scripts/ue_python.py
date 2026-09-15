"""Run an editor Python script over Epic's loopback remote execution protocol.

Usage: python Scripts/ue_python.py Scripts/M1_diag.py
Requires Python remote execution enabled in the editor (127.0.0.1, TTL 0).
"""
import argparse
import json
from pathlib import Path
import sys
import time


def run(script, engine="F:/GameStudy/UE_5.8"):
    # Enable the engine's existing Python transport through MCP after each editor restart.
    # Keep discovery and commands on loopback, without changing project config files.
    from ue_mcp import UnrealMCP
    client = UnrealMCP()
    client.tool('editor_toolset.toolsets.object.ObjectTools', 'set_properties', {
        'instance': {'refPath': '/Script/PythonScriptPlugin.Default__PythonScriptPluginSettings'},
        'values': json.dumps({'remoteExecutionMulticastBindAddress': '127.0.0.1',
                             'remoteExecutionMulticastTtl': 0, 'bRemoteExecution': True})})
    sys.path.insert(0, str(Path(engine) / "Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python"))
    import remote_execution
    remote = remote_execution.RemoteExecution()
    remote.start()
    try:
        deadline = time.monotonic() + 10
        project = Path(__file__).resolve().parents[1]
        nodes = []
        while time.monotonic() < deadline:
            nodes = [n for n in remote.remote_nodes
                     if Path(n.get("project_root", "")).resolve() == project]
            if nodes:
                break
            time.sleep(0.2)
        if len(nodes) != 1:
            raise RuntimeError(f"Expected one editor for {project}; discovered {remote.remote_nodes}")
        remote.open_command_connection(nodes[0]["node_id"])
        return remote.run_command(str(Path(script).resolve()).replace("\\", "/"),
                                  exec_mode=remote_execution.MODE_EXEC_FILE)
    finally:
        remote.stop()


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("script")
    parser.add_argument("--engine", default="F:/GameStudy/UE_5.8")
    args = parser.parse_args()
    result = run(args.script, args.engine)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["success"] else 1)
