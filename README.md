# yt-dlp-macos-cpp (Xcode CLI)

A tiny C++ command-line wrapper around yt-dlp that downloads videos on macOS and saves the final merged .mp4 (with audio) straight to ~/Downloads. 
Built as an Xcode Command Line Tool, it launches yt-dlp as a subprocess, streams progress to the console, and (optionally) opens Finder when done.

# Requirements (macOS)

Install the two Homebrew packages:

brew install yt-dlp

brew install ffmpeg


Verify:

yt-dlp --version

ffmpeg -version


Why both? yt-dlp fetches separate video+audio streams; ffmpeg merges them. Without ffmpeg, you’ll get a silent video.

# Build (Xcode)

Open the Xcode project (template: Command Line Tool, language: C++).

Put your code in main.cpp.

Make sure the absolute paths in code match your system (check in Terminal):

which yt-dlp

which ffmpeg


Typical Apple Silicon paths:

/opt/homebrew/bin/yt-dlp

/opt/homebrew/bin/ffmpeg

Run in Xcode.

Note: Xcode doesn’t inherit your shell PATH. That’s why the code passes --ffmpeg-location and uses absolute paths.

# Usage

You can pass a URL as an argument or paste it when prompted.

pass URL as an argument
./yt-dlp-macos-cpp "https://www.youtube.com/watch?v=EXAMPLE"

or run with no args and paste the URL when the program asks
./yt-dlp-macos-cpp

# What the program does

Calls yt-dlp with format sorting that prefers H.264 MP4 when possible.

Uses --ffmpeg-location to point directly to Homebrew’s ffmpeg.

Saves into ~/Downloads with filename template %(title)s.%(ext)s.

Streams live progress (the familiar yt-dlp progress lines) in Xcode’s console.

Optionally opens Finder → Downloads after completion.

# Output

Video: ~/Downloads/<Title>.mp4 (video+audio merged)

Audio-only (optional): If you add the flags below, you’ll get ~/Downloads/<Title>.mp3

To enable audio-only in your code, add to the yt-dlp args:

-x --audio-format mp3

# Common issues & fixes

Got video but no audio:
Install ffmpeg (brew install ffmpeg). If already installed, Xcode might not find it—ensure your code includes:

--ffmpeg-location "/opt/homebrew/bin/ffmpeg"


“yt-dlp not found”:
Use the absolute path from which yt-dlp in your code (e.g., /opt/homebrew/bin/yt-dlp).

File not in your project folder:
The app intentionally saves to ~/Downloads. Xcode’s working dir is a DerivedData path you don’t want to use.

Slow/blocked downloads:
Some sites rate-limit. Try again later or add --concurrent-fragments 4 for HLS sites.

# Example command (Terminal sanity check)

Use this once to confirm your environment is good (should produce an mp4 with audio in Downloads):

yt-dlp --ffmpeg-location /opt/homebrew/bin/ffmpeg \
  -f "bv*+ba/b" -S "vcodec:h264,res,ext" \
  --merge-output-format mp4 \
  -P ~/Downloads -o "%(title)s.%(ext)s" \
  "https://www.youtube.com/watch?v=EXAMPLE"

# macOS friendliness

Works on Apple Silicon and Intel Macs via Homebrew.

No Python/venv setup required (uses yt-dlp’s installed binary).

Uses $HOME so paths are username-agnostic.

Finder integration (open "$HOME/Downloads") for a native feel.

# Project structure (suggested)
.
├─ README.md
├─ .gitignore
└─ src/
   └─ main.cpp


.gitignore (minimal)

.DS_Store
/build
DerivedData/

# Legal

Only download content you have the right to download. Respect each site’s Terms of Service and local laws.

# License

MIT — do whatever you want, just include the license.
