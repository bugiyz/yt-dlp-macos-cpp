# awesomeyt (macOS Terminal CLI)

`awesomeyt` is a local-only Bash wrapper around `yt-dlp` + `ffmpeg`.

- No backend/server
- Single-video downloads by default (`--no-playlist`)
- Video mode by default, audio mode available (`--audio`)
- Safe defaults with strict Bash error handling

## Requirements

Install dependencies with Homebrew:

```bash
brew update
brew install yt-dlp ffmpeg
```

## Install `awesomeyt`

From this repo root:

```bash
# Create local bin dir
mkdir -p "$HOME/.local/bin"

# Install script
cp ./awesomeyt "$HOME/.local/bin/awesomeyt"

# Make executable
chmod +x "$HOME/.local/bin/awesomeyt"

# Add ~/.local/bin to PATH for zsh (if missing)
grep -qxF 'export PATH="$HOME/.local/bin:$PATH"' "$HOME/.zshrc" || \
  echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.zshrc"

# Reload shell config for current terminal
source "$HOME/.zshrc"
hash -r
```

## Usage

```bash
awesomeyt
awesomeyt <url>
awesomeyt --audio <url>
awesomeyt --video <url>
awesomeyt --dir "<folder>" <url>
awesomeyt -h
```

## Defaults

- Default mode: `video`
- Default output directory: `~/Downloads/AwesomeYT`
- Default format: `-f "bv*+ba/b"`
- Output template: `%(title).200s [%(id)s].%(ext)s`
- Always uses: `--newline --progress --no-playlist`

Audio mode adds:

```bash
-x --audio-format mp3 --audio-quality 0
```

## Quick Test Plan

```bash
# 1) Help
awesomeyt --help

# 2) Prompt flow (paste URL when asked)
awesomeyt

# 3) Video mode with direct URL
awesomeyt "https://www.youtube.com/watch?v=VIDEO_ID"

# 4) Audio mode
awesomeyt --audio "https://www.youtube.com/watch?v=VIDEO_ID"

# 5) Custom directory
awesomeyt --dir "$HOME/Desktop/AwesomeYTTest" "https://www.youtube.com/watch?v=VIDEO_ID"

# 6) URL validation (should fail)
awesomeyt ""
```

## Legal

Only download content you own or have permission to download.
