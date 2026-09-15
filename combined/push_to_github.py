#!/usr/bin/env python3
"""
Push script for QuantumSIHCracker/sih-combined-system.
Runs silently — no token prompt needed.
Usage: python3 push_to_github.py
"""
import subprocess
import sys
from urllib.parse import quote

# Token stored here — rotate at https://github.com/settings/tokens after project
TOKEN = "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"

print("=== GitHub Push ===")
token_encoded = quote(TOKEN, safe="")

PUSHES = [
    {
        "local": "/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker",
        "remote": f"https://{token_encoded}@github.com/QuantumSIHCracker/sih-combined-system.git",
        "label": "sih-combined-system (workflow docs)",
        "force": True,
    },
]

for p in PUSHES:
    print(f"\n📤 Pushing: {p['label']} ...")
    cmd = ["git", "push", "-u"]
    if p.get("force"):
        cmd.append("--force")
    cmd += [p["remote"], "main"]
    result = subprocess.run(
        cmd,
        cwd=p["local"],
        capture_output=True,
        text=True
    )
    if result.returncode == 0:
        print(f"   ✅ Pushed successfully!")
        if result.stderr:
            print(f"   {result.stderr.strip()}")
    else:
        err = result.stderr.replace(token, "***TOKEN***")  # hide token in output
        print(f"   ❌ Failed: {err.strip()}")

print("\nDone.")
