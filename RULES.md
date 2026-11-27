# Gramm development rules

## Git and verification

- Use `codex/setup` for setup and feature development. Changes to
  `gramm/main` require the repository owner's permission.
- Establish a successful Windows x64 baseline before changing client behavior
  or branding.
- Commit build infrastructure, documentation, and functional changes with
  descriptive subjects. Record the Git SHA, GitHub Actions run and artifact,
  and the tests that ran.

## Telegram compatibility

- Keep Telegram's existing sign-in, session storage, privacy, and messaging
  flows. Add Music+ to the existing music player and keep voice and round-video
  playback working.
- Keep support for Telegram channel sponsored messages, read and typing
  indicators, and disappearing content.
- Use Gramm's name and original artwork for the fork. Disclose in the welcome
  screen and distribution description that Gramm uses the Telegram API.
- Use Gramm's own API ID and hash for distribution. Use the official sample
  credentials only for testing. Store project credentials outside Git.
  [Telegram API terms](https://core.telegram.org/api/terms) ·
  [Obtaining an API ID](https://core.telegram.org/api/obtaining_api_id)

## Distribution

- Preserve upstream copyright notices, `LICENSE`, `LEGAL`, and dependency
  notices. Publish the complete corresponding source, including build scripts
  and exact submodule versions, for binaries distributed to users.
- Package the x64 executable with its SHA-256 checksum and the source revision.
  Identify test and unsigned builds clearly.
