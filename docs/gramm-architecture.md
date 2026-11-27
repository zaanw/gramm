# Telegram Desktop architecture in Gramm

Gramm starts from the Telegram Desktop source at
`1fdd0ea143accc8b5d917e95144cfc5f73e06395`. The root CMake project loads the
`cmake` submodule and delegates application targets, source lists and resource
generation to `Telegram/CMakeLists.txt`. Git submodule pins and
`Telegram/build/prepare/prepare.py` select the companion libraries and native
dependencies. The UI is Qt-based and uses reactive `rpl` producers for events
and widget state. See the [upstream source atlas](../Telegram/SourceFiles/AGENTS.md).

| Area | Source and responsibility |
| --- | --- |
| Startup | `core/launcher.*` and `core/application.*` initialize the process, launch windows, and manage shared services. `platform/win` handles Windows integration. |
| Authorization and accounts | `intro/` contains the welcome, QR, phone, code, and cloud-password pages. `main/` owns accounts and logged-in sessions. |
| Protocol and account data | `mtproto/` manages transport, data centers and auth keys. `api/` and `apiwrap.*` translate API results into changes in `data/`. `storage/` persists accounts and settings and implements transfers. |
| Settings | `settings/` contains the section builder and search. `settings/sections/` holds pages and the main Settings menu. Gramm Settings belongs in this framework so it uses the existing navigation and search. |
| Music | `media/player/` owns the playlist, playback state, controls, and player UI. `Media::Player::Instance` manages separate song and voice playback state through the shared media engine. Music+ belongs on the song path and uses existing session media, streaming, and OS controls. |
| Branding | Welcome and About views, localized strings under `Telegram/Resources`, and Windows resources and metadata define the user-facing identity. The fork's app icon and product branding belong at these entry points. |

Source paths in the table are relative to `Telegram/SourceFiles` unless
specified otherwise. The [official Telegram Desktop repository](https://github.com/telegramdesktop/tdesktop)
maintains the source and dependency credits. The [Telegram API terms](https://core.telegram.org/api/terms)
define the compatibility and branding requirements for third-party clients.
