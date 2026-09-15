#!/usr/bin/env python3
"""
Push script for QuantumSIHCracker/sih-combined-system.
Token is read from .env file — never committed to GitHub.
Usage: python3 push_to_github.py
"""
import subprocess
import sys
import os
from urllib.parse import quote

# Read token from local .env file (never committed — in .gitignore)
_env_path = os.path.join(os.path.dirname(__file__), ".env")
if os.path.exists(_env_path):
    for line in open(_env_path):
        if line.startswith("GITHUB_TOKEN="):
            os.environ["GITHUB_TOKEN"] = line.strip().split("=", 1)[1]

TOKEN = os.environ.get("GITHUB_TOKEN", "")
if not TOKEN:
    print("❌ No token found. Create combined/.env with: GITHUB_TOKEN=ghp_yourtoken")
    sys.exit(1)

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
