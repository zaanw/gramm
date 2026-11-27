# Build Gramm for Windows x64

## GitHub Actions

Pushes to `codex/setup` trigger [Gramm Windows x64](../.github/workflows/win.yml). The job checks out
recursive submodules, prepares the pinned upstream dependencies, and builds the
Telegram CMake target in Debug on `windows-2025` with MSVC 14.44. The first
build also compiles dependencies such as Qt and FFmpeg.

GitHub requires a `workflow_dispatch` trigger on the default branch to offer
manual runs. The setup branch uses push triggers while the project is being
set up. [GitHub Actions event reference](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#workflow_dispatch)

Open the run for the desired commit from
[GitHub Actions](https://github.com/zaanw/gramm/actions?query=branch%3Acodex%2Fsetup).
A successful build produces the artifact
`gramm-windows-x64-<commit SHA>` containing `Gramm.exe`, the
license notices, `SHA256SUMS.txt`, and `build-info.txt`. The packaging step
checks the PE machine type is AMD64 before uploading. This workflow produces an
unsigned build using Telegram's limited test API credentials.

[The initial upstream baseline build](https://github.com/zaanw/gramm/actions/runs/45833040278) produced `Telegram.exe` at commit `74ebb4a`.

## Build locally

Install Git, Python 3.10, CMake 3.25 or newer, Ninja, and Visual Studio with
the C++ desktop workload, MSVC 14.44 x64 tools, and Windows SDK 10.0.26100.0.
Use a short build path to keep dependency paths within Windows path limits.
See the [upstream Windows guide](building-win.md) for Visual Studio setup.

From an x64 Native Tools Command Prompt initialized with
`vcvars64.bat -vcvars_ver=14.44`, enter the following in an empty build
directory.

~~~bat
mkdir ThirdParty
mkdir Libraries\win64
git clone --branch codex/setup --recurse-submodules https://github.com/zaanw/gramm.git gramm
cd gramm
call Telegram\build\prepare\win.bat skip-release silent
cd Telegram
call configure.bat -G "Ninja Multi-Config" -D TDESKTOP_API_TEST=ON -D CMAKE_CONFIGURATION_TYPES=Debug -D CMAKE_MSVC_DEBUG_INFORMATION_FORMAT= -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON -D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON
cmake --build ..\out --config Debug --target Telegram --parallel
~~~

Stop at the first failed command and inspect its error before continuing. The
baseline executable is `grammoutDebugTelegram.exe`. Match the
`Get-FileHash -Algorithm SHA256` output for a downloaded executable to that
artifact's `SHA256SUMS.txt` before testing.

## Runtime checks

1. Run the executable with `-workdir` pointing at a new, existing directory
   reserved for the test profile.
2. Confirm the welcome screen opens and the phone and QR sign-in screens
   remain reachable. With a consenting test account and appropriate API
   credentials, confirm sign-in, reopen the saved session, send and receive a
   message, and sign out.
3. On a Music+ build, queue music from multiple chats, change the order,
   remove entries, and confirm that the next entry starts when a song ends.
   Verify play/pause, previous/next, voice-message playback, account switching,
   and the Gramm Settings entry.
4. Record what passed, what failed, and what remains untested against the commit
   in the artifact.

For distribution, configure with Gramm's own `TDESKTOP_API_ID` and
`TDESKTOP_API_HASH` in place of `TDESKTOP_API_TEST=ON`. Use the upstream
dependency preparation without `skip-release` to build Release. Publish the
matching source and license notices with the distribution. See [RULES.md](../RULES.md).
