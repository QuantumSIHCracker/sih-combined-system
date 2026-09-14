#!/usr/bin/env python3
"""
GitHub Repository Setup Script for Quantum-SIH-Cracker Organization.

This script helps create all 4 repositories on GitHub with proper settings.
Run this AFTER creating the GitHub organization and authenticating.

Prerequisites:
    pip install PyGithub
    
Usage:
    python setup_github.py --token YOUR_GITHUB_PAT
    
    OR set environment variable:
    export GITHUB_TOKEN=YOUR_PAT
    python setup_github.py
"""

import argparse
import os
import subprocess
import sys

REPOS = [
    {
        "name": "sih-hardware-firmware",
        "description": "ESP32-S3 firmware, INMP441 microphone integration, on-device KWS (TENet INT8 TFLite) — Smart India Hackathon 2026",
        "topics": ["esp32-s3", "firmware", "keyword-spotting", "tflite-micro", "arduino", "sih2026"],
        "local_path": None,  # Separate repo — Hardware Team creates this on their machine
    },
    {
        "name": "sih-ml-models",
        "description": "Keyword Spotting model training pipeline, MFCC features, INT8 TFLite optimization — Smart India Hackathon 2026",
        "topics": ["machine-learning", "keyword-spotting", "tflite", "tensorflow", "mfcc", "sih2026"],
        "local_path": None,  # ML Team creates this on their machine
    },
    {
        "name": "sih-server-backend",
        "description": "FastAPI server, Silero VAD, Faster-Whisper ASR, live telemetry dashboard — Smart India Hackathon 2026",
        "topics": ["fastapi", "whisper", "voice-activity-detection", "python", "speech-to-text", "sih2026"],
        "local_path": None,  # Server Team creates this on their machine
    },
    {
        "name": "sih-combined-system",
        "description": "Combined workflow, integration docs, protocol specification — Smart India Hackathon 2026 | Quantum-SIH-Cracker",
        "topics": ["sih2026", "voice-assistant", "esp32", "edge-ai", "hackathon"],
        "local_path": "/home/arpit_ubuntu/New WorkFlow/SIH-Quantum-Cracker",  # This machine
    },
]

def setup_repos(token):
    """Create all repos and configure branch protection."""
    try:
        from github import Github, GithubException
    except ImportError:
        print("PyGithub not installed. Installing...")
        subprocess.run([sys.executable, "-m", "pip", "install", "PyGithub"], check=True)
        from github import Github, GithubException

    g = Github(token)
    
    try:
        org = g.get_organization("Quantum-SIH-Cracker")
        print(f"✅ Found organization: {org.login}")
    except Exception as e:
        print(f"❌ Could not find organization 'Quantum-SIH-Cracker': {e}")
        print("   Make sure you've created the organization on GitHub first.")
        return

    for repo_config in REPOS:
        name = repo_config["name"]
        print(f"\n📦 Setting up repo: {name}")
        
        try:
            # Check if repo already exists
            repo = org.get_repo(name)
            print(f"   Repo already exists: {repo.html_url}")
        except:
            # Create new repo
            repo = org.create_repo(
                name=name,
                description=repo_config["description"],
                private=False,
                auto_init=True,  # Creates with README
                has_issues=True,
                has_projects=True,
                has_wiki=False,
            )
            print(f"   ✅ Created: {repo.html_url}")
        
        # Set topics
        try:
            repo.replace_topics(repo_config["topics"])
            print(f"   ✅ Topics set: {', '.join(repo_config['topics'])}")
        except Exception as e:
            print(f"   ⚠️  Could not set topics: {e}")
        
        # Create dev branch
        try:
            main_sha = repo.get_branch("main").commit.sha
            repo.create_git_ref(f"refs/heads/dev", main_sha)
            print(f"   ✅ Created 'dev' branch")
        except Exception as e:
            if "already exists" in str(e):
                print(f"   'dev' branch already exists")
            else:
                print(f"   ⚠️  Could not create dev branch: {e}")
        
        # Set default branch to main
        try:
            repo.edit(default_branch="main")
            print(f"   ✅ Default branch: main")
        except Exception as e:
            print(f"   ⚠️  Could not set default branch: {e}")
        
        # Push local content to combined-system repo
        if repo_config["local_path"] and os.path.exists(repo_config["local_path"]):
            print(f"\n📤 Pushing local content to {name}...")
            local_path = repo_config["local_path"]
            
            result = subprocess.run(
                ["git", "push", "-u", "origin", "main"],
                cwd=local_path,
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                print(f"   ✅ Pushed to GitHub!")
            else:
                print(f"   ⚠️  Push failed: {result.stderr}")
                print(f"   Try manually: cd '{local_path}' && git push -u origin main")
    
    print("\n\n🎉 GitHub Setup Complete!")
    print("\nNext steps for each team:")
    print("  Hardware Team → clone: https://github.com/Quantum-SIH-Cracker/sih-hardware-firmware")
    print("  ML Team       → clone: https://github.com/Quantum-SIH-Cracker/sih-ml-models")
    print("  Server Team   → clone: https://github.com/Quantum-SIH-Cracker/sih-server-backend")
    print("  Combined      → clone: https://github.com/Quantum-SIH-Cracker/sih-combined-system")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Setup GitHub repos for Quantum-SIH-Cracker")
    parser.add_argument("--token", help="GitHub Personal Access Token")
    args = parser.parse_args()
    
    token = args.token or os.environ.get("GITHUB_TOKEN")
    if not token:
        print("❌ GitHub PAT required. Get one from: https://github.com/settings/tokens")
        print("   Required scopes: repo, admin:org")
        print("\nUsage:")
        print("   python setup_github.py --token YOUR_TOKEN")
        print("   OR: export GITHUB_TOKEN=YOUR_TOKEN && python setup_github.py")
        sys.exit(1)
    
    setup_repos(token)
