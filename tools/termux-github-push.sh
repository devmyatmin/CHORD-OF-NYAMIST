#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

SOURCE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
REPO_URL="https://github.com/devmyatmin/CHORD-OF-NYAMIST.git"
BRANCH="dx11-live-cube-mainline"
CLONE_DIR="${NYAMIST_GITHUB_CLONE_DIR:-$HOME/nyamist-github-upload}"

if ! command -v git >/dev/null 2>&1; then
  echo "git is missing. Install it with: pkg install git"
  exit 2
fi

if [ ! -d "$CLONE_DIR/.git" ]; then
  git clone --branch "$BRANCH" "$REPO_URL" "$CLONE_DIR"
else
  git -C "$CLONE_DIR" fetch origin "$BRANCH"
  git -C "$CLONE_DIR" checkout "$BRANCH"
  git -C "$CLONE_DIR" pull --ff-only origin "$BRANCH"
fi

# Copy source without build output, local Git metadata, generated APKs, or secrets.
find "$CLONE_DIR" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cd "$SOURCE_DIR"
tar --exclude='./.git' \
    --exclude='./build' --exclude='./build-*' --exclude='./cmake-build-*' \
    --exclude='./android-app/.gradle' --exclude='./android-app/app/build' \
    --exclude='*/__pycache__' --exclude='*.pyc' \
    --exclude='./.nyamist-cache' \
    -cf - . | tar -xf - -C "$CLONE_DIR"

cd "$CLONE_DIR"
git add -A
if git diff --cached --quiet; then
  echo "No source changes to commit."
else
  if ! git config user.name >/dev/null || ! git config user.email >/dev/null; then
    echo "Set your Git identity first: git config --global user.name 'Your Name'"
    echo "and git config --global user.email 'your-github-email'"
    exit 3
  fi
  git commit -m "Sync DX11-first NYAMIST runtime and live cube workloads"
fi
git push -u origin "$BRANCH"
echo "Uploaded branch: $BRANCH"
echo "Repository: https://github.com/devmyatmin/CHORD-OF-NYAMIST/tree/$BRANCH"
