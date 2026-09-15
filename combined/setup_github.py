#!/usr/bin/env python3
"""
GitHub Repository Setup Script for QuantumSIHCracker Organization.
Creates all 4 PRIVATE repositories, GitHub teams, invites members, sets branch protection.

Prerequisites:
    pip install PyGithub

Usage:
    python setup_github.py --token YOUR_GITHUB_PAT
    OR: export GITHUB_TOKEN=YOUR_PAT && python setup_github.py
"""

import argparse
import os
import subprocess
import sys

# ─────────────────────────────────────────────────────────────────────────────
# TEAM MEMBERS — email : (github_username or None, repo_team)
# ─────────────────────────────────────────────────────────────────────────────
TEAM_MEMBERS = [
    # (email,                   github_username,         team)
    # Fill in real emails before running — do NOT commit real emails to GitHub
    ("HARDWARE_LEAD_EMAIL",    "arpitkumar81008-cmd",   "hardware"),   # Arpit — HW lead
    ("HARDWARE_MEMBER_EMAIL",  None,                    "hardware"),   # Akshat — HW member
    ("SERVER_HEAD_EMAIL",      None,                    "server"),     # Server head
    ("ML_HEAD_EMAIL",          None,                    "ml"),         # ML head
]

REPOS = [
    {
        "name": "sih-hardware-firmware",
        "description": "ESP32-S3 firmware, INMP441 mic, on-device KWS (TENet INT8 TFLite) — SIH 2026",
        "topics": ["esp32-s3", "firmware", "keyword-spotting", "tflite-micro", "arduino", "sih2026"],
        "team": "hardware",
        "local_path": None,
    },
    {
        "name": "sih-ml-models",
        "description": "KWS model training, MFCC pipeline, INT8 TFLite optimization — SIH 2026",
        "topics": ["machine-learning", "keyword-spotting", "tflite", "tensorflow", "mfcc", "sih2026"],
        "team": "ml",
        "local_path": None,
    },
    {
        "name": "sih-server-backend",
        "description": "FastAPI server, Silero VAD, Faster-Whisper ASR, live dashboard — SIH 2026",
        "topics": ["fastapi", "whisper", "vad", "python", "speech-to-text", "sih2026"],
        "team": "server",
        "local_path": None,
    },
    {
        "name": "sih-combined-system",
        "description": "Combined workflow docs, protocol spec, integration — SIH 2026",
        "topics": ["sih2026", "voice-assistant", "esp32", "edge-ai", "hackathon"],
        "team": "all",
        "local_path": "/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker",
    },
]


def setup_repos(token):
    try:
        from github import Github, GithubException, Auth
    except ImportError:
        print("Installing PyGithub...")
        subprocess.run([sys.executable, "-m", "pip", "install", "PyGithub"], check=True)
        from github import Github, GithubException, Auth

    # Use new Auth API (fixes deprecation warning)
    g = Github(auth=Auth.Token(token))

    # ── Find org ──────────────────────────────────────────────────────────────
    try:
        org = g.get_organization("QuantumSIHCracker")
        print(f"✅ Organization found: {org.login}")
    except Exception as e:
        print(f"❌ Cannot find 'QuantumSIHCracker': {e}")
        print("   Check: does your token have 'admin:org' scope?")
        return

    # ── Create GitHub teams ───────────────────────────────────────────────────
    github_teams = {}
    for tname in ["hardware", "ml", "server", "all-teams"]:
        existing = {t.name: t for t in org.get_teams()}
        if tname in existing:
            github_teams[tname] = existing[tname]
            print(f"   Team '{tname}' already exists")
        else:
            t = org.create_team(tname, privacy="secret")
            github_teams[tname] = t
            print(f"✅ Created team: '{tname}'")

    # ── Invite members by email or username ──────────────────────────────────
    for email, username, team in TEAM_MEMBERS:
        # Try by username first (gives immediate access), fall back to email invite
        invited = False
        if username:
            try:
                user = g.get_user(username)
                org.invite_user(user)
                print(f"✅ Invited @{username} ({email}) to org")
                invited = True
            except Exception as e:
                if "already" in str(e).lower():
                    print(f"   @{username} already a member")
                    invited = True
                else:
                    print(f"   ⚠️  Username invite failed for @{username}: {e}")

        if not invited:
            # Invite by email directly
            try:
                org.invite_user(email=email)
                print(f"✅ Org invite sent to {email} (team: {team})")
            except Exception as e:
                if "already" in str(e).lower():
                    print(f"   {email} already invited/member")
                else:
                    print(f"   ⚠️  Email invite failed for {email}: {e}")

    # ── Create repos ──────────────────────────────────────────────────────────
    for rc in REPOS:
        name = rc["name"]
        print(f"\n📦 {name}  [PRIVATE]")

        try:
            repo = org.get_repo(name)
            repo.edit(private=True)
            print(f"   Already exists — confirmed PRIVATE ✅")
        except GithubException:
            repo = org.create_repo(
                name=name,
                description=rc["description"],
                private=True,
                auto_init=True,
                has_issues=True,
                has_projects=True,
                has_wiki=False,
            )
            print(f"   Created (private) ✅  {repo.html_url}")

        # Set topics
        try:
            repo.replace_topics(rc["topics"])
            print(f"   Topics set ✅")
        except Exception:
            pass

        # Create dev branch
        try:
            sha = repo.get_branch("main").commit.sha
            repo.create_git_ref("refs/heads/dev", sha)
            print(f"   'dev' branch created ✅")
        except Exception as e:
            print(f"   'dev' branch: {'already exists' if 'exists' in str(e) else e}")

        # Branch protection on main
        try:
            repo.get_branch("main").edit_protection(
                required_approving_review_count=1,
                enforce_admins=False,
                dismiss_stale_reviews=True,
            )
            print(f"   Branch protection on 'main' ✅")
        except Exception as e:
            print(f"   ⚠️  Branch protection: {e}")

        # Grant team push access
        for tname in (["all-teams"] + ([rc["team"]] if rc["team"] != "all" else [])):
            t = github_teams.get(tname)
            if t:
                try:
                    t.add_to_repos(repo)
                    t.set_repo_permission(repo, "push")
                    print(f"   Team '{tname}' → push access ✅")
                except Exception as e:
                    print(f"   ⚠️  Team '{tname}': {e}")

        # Add individual collaborators (only those with known GitHub usernames)
        repo_team = rc["team"]
        for email, username, member_team in TEAM_MEMBERS:
            if not username:
                continue  # Can only add collaborators by username, not email
            # Add to repo if member is on this team, hardware team (always), or repo is combined
            if repo_team == "all" or member_team == repo_team or member_team == "hardware":
                try:
                    repo.add_to_collaborators(username, permission="push")
                    print(f"   @{username} → collaborator (push) ✅")
                except Exception as e:
                    print(f"   ⚠️  @{username}: {e}")

        # Push local content (sih-combined-system only)
        if rc["local_path"] and os.path.exists(rc["local_path"]):
            print(f"   Pushing local content to GitHub...")
            r = subprocess.run(
                ["git", "push", "-u", "origin", "main"],
                cwd=rc["local_path"], capture_output=True, text=True
            )
            if r.returncode == 0:
                print(f"   Pushed ✅")
            else:
                print(f"   ⚠️  Push failed: {r.stderr.strip()}")
                print(f"       Run manually: cd \"{rc['local_path']}\" && git push -u origin main")

    print("\n" + "="*60)
    print("🎉 ALL DONE — 4 private repos created.")
    print("="*60)
    print("\nNext steps:")
    print("  1. Hardware (this machine): push firmware code")
    print("     cd /your/firmware && git remote add origin https://github.com/QuantumSIHCracker/sih-hardware-firmware.git")
    print("     git push -u origin main")
    print("  2. ML Team: accept org invite email → clone sih-ml-models")
    print("  3. Server Team: accept org invite email → clone sih-server-backend")
    print("  4. Workflow docs: already pushed to sih-combined-system ✅")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--token", help="GitHub PAT")
    args = parser.parse_args()

    token = args.token or os.environ.get("GITHUB_TOKEN")
    if not token:
        print("❌ GitHub Personal Access Token required.\n")
        print("Steps to get one:")
        print("  1. Go to → https://github.com/settings/tokens/new")
        print("  2. Note: 'SIH Setup'  |  Expiration: 90 days")
        print("  3. Select scopes:")
        print("       ✅ repo          (full control of private repositories)")
        print("       ✅ admin:org     (create repos + invite members to org)")
        print("       ✅ read:user")
        print("  4. Click 'Generate token' and copy it")
        print("  5. Run: python setup_github.py --token ghp_YOURTOKEN\n")
        sys.exit(1)

    setup_repos(token)
