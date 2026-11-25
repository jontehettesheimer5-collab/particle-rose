# Terminal32

An iSH-based local Linux terminal customized by 32e4. On opening, a pink particle flower blooms and transforms into “made by 32e4”, then reveals the terminal. The intro lasts about seven seconds. Reduce Motion uses a short static credit.

This build uses the upstream iSH emulator, terminal, settings, sessions, Linux filesystem and networking. It supports iPhone and iPad in portrait and landscape. The installed app is named Terminal32 and has a separate bundle identifier from the video app.

## Chat and file import

The Chat tab has a direct file picker. Tap + to select one or more files up to 20 MB each from the Files app, including iCloud and other document providers. Imported copies are kept in the app and exposed inside Linux under `/imports`. The folder icon lists previously imported files. Select a file, then tap its play button or say “execute the file”. Output and the actual exit code appear in chat. The Terminal tab provides the full shell and interactive program input.

Groq Settings accepts an API key and model ID. Keys are stored in the device Keychain and are never embedded in the source or IPA. The default model is `openai/gpt-oss-120b`. Chat requests go directly to Groq over HTTPS. File content is sent when the assistant reads a selected file, and execution output is included in tool results. Execution requires an explicit run request and runs at most once per chat turn. The model cannot issue arbitrary shell commands. JavaScript and Lua execution require their interpreters to be installed.

The native UI uses system typography, adaptive layouts, 44-point controls, selectable message text, concise status feedback, and native sheets. Design guidance was read from Emil Kowalski's `emil-design-eng` and `apple-design` skills at https://github.com/emilkowalski/skills, commit `64eb3deb9a5fbfa041faf783e0bc3fd635872e9c`. The flower intro still supports Reduce Motion.

The build includes a simulator-only regression check for Keychain, mocked Groq tool calls, file picker presentation, native import and script execution with a filename containing an apostrophe and spaces. The test checks actual shell output and exit status. Groq API access with a real account and physical-device file providers require device verification.

## Python and tools

On the first launch, run `setup-dev` in the terminal with an internet connection. It installs Python 3, pip, requests, Bash, Git, curl, wget, nano and the OpenSSH client. It creates a persistent Python virtual environment. Open a new shell afterward or run `. /root/.venv/bin/activate`.

Try `python -c 'import requests; print(requests.__version__)'`, `pip install rich`, `python script.py`, `git --version`, or `nano script.py`. Use `pkg install <package>` or Alpine's `apk add <package>` for Linux packages. Package availability follows the bundled Alpine/iSH repositories. Some packages with native extensions, unsupported syscalls, or large resource requirements may fail or run slowly. iOS may suspend the app in the background.

The base build is pinned to iSH commit `ce96e698f1820b9d8db47e87c501ecb89119d7ce`. `prepare.py` applies the native intro, display name, defaults, and app-container fallback. `prepare-root.py` adds developer setup commands to the root archive. The workflow builds the app and uploads an unsigned IPA and its corresponding source.

## License and attribution

Based on iSH by Theodore Dubois and the iSH contributors, https://github.com/ish-app/ish. iSH's license, additional iOS terms, notices and bundled component licenses remain in the corresponding source archive. Terminal32 additions are GPL-3.0-or-later. This project is a modified derivative and is not an official iSH release. The “made by 32e4” credit identifies these customizations.

## Installation

Download the Terminal32 unsigned IPA artifact from a successful workflow run, extract it, and sign it using a valid certificate/profile for your iPhone or iPad. The app falls back to its own Documents directory when an app-group entitlement is unavailable. Files extension integration depends on the signing profile's app-group support.
