#!/usr/bin/env python3
"""
Secure push script — reads token from input, pushes without exposing it in shell history.
Usage: python3 push_to_github.py
"""
import subprocess
import sys
import getpass

print("=== Secure GitHub Push ===")
print("This script won't save your token anywhere.\n")

token = getpass.getpass("Paste your GitHub token (hidden): ").strip()
if not token:
    print("No token entered. Exiting.")
    sys.exit(1)

PUSHES = [
    {
        "local": "/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker",
        "remote": f"https://{token}@github.com/QuantumSIHCracker/sih-combined-system.git",
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
