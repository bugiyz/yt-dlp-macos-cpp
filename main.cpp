#include <iostream>
#include <array>
#include <memory>
#include <string>
#include <cstdlib>

int main(int argc, char** argv) {
    const char* ytDlpPath  = "/opt/homebrew/bin/yt-dlp";     // result of: which yt-dlp
    const char* ffmpegPath = "/opt/homebrew/bin/ffmpeg";     // result of: which ffmpeg

    std::string url;
    if (argc >= 2) url = argv[1];
    else { std::cout << "Paste video URL: "; std::getline(std::cin, url); }
    if (url.empty()) { std::cerr << "No URL provided.\n"; return 1; }

    const char* downloads = "$HOME/Downloads";

    // Key fix: --ffmpeg-location points to Homebrew’s ffmpeg
    std::string cmd = std::string("\"") + ytDlpPath + "\" "
        + "--ffmpeg-location \"" + ffmpegPath + "\" "
        + "\"" + url + "\" "
        + "-f bv*+ba/b "
        + "-S vcodec:h264,res,ext "
        + "--merge-output-format mp4 "
        + "-P \"" + downloads + "\" "
        + "-o \"%(title)s.%(ext)s\" 2>&1";

    std::array<char, 4096> buffer{};
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) { std::cerr << "Failed to run yt-dlp\n"; return 1; }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) std::cout << buffer.data();

    std::system("open \"$HOME/Downloads\"");
    std::cout << "\nDone! Saved to ~/Downloads (Finder opened)\n";
    return 0;
}
