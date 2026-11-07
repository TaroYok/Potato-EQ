# Security

## What the plugin does and doesn't do
- **No network access.** Potato EQ never connects to the internet. There's no telemetry, no license check
  and no auto-updater (JUCE's networking and web-browser modules are compiled out).
- **No files written** outside your DAW project. Settings and learned Auto EQ data are saved inside the project.
- **Untrusted project data is validated.** Learned spectrum data loaded from a project must be well-formed,
  finite and in range, or it's discarded (the rest of the project still loads). Malformed state can't crash
  the plugin; this is covered by the automated tests.
- The cross-track features only share spectrum levels between Potato EQ instances in the same DAW process.
  Nothing leaves your computer.

## Build and supply chain
- **Pinned dependencies.** JUCE is fetched at an exact commit hash, and every GitHub Action is pinned to a
  full commit SHA, so a changed or hijacked upstream release can't slip into a build. Dependabot opens
  reviewable pull requests when those pins have updates.
- **Least-privilege CI.** Build jobs only get read access to the repository. Only the release job (run when
  a `v*` tag is pushed) may publish.
- **Checksums.** Installer builds include `SHA256SUMS.txt`, so a download can be checked against the build that made it.
- **Hardened binaries.** Windows builds use Control Flow Guard, ASLR and DEP. macOS/Linux builds use stack
  protectors and `_FORTIFY_SOURCE`. Linux also uses full RELRO and a non-executable stack. The Windows build uses a
  static C++ runtime, so nothing extra needs installing.

## Not done yet (needs paid accounts)
- **Code signing.** The Windows installer isn't signed (SmartScreen will warn), and the macOS package isn't
  signed or notarized (Gatekeeper will ask you to approve it). Signing needs a code-signing certificate
  (e.g. Microsoft Trusted Signing) and an Apple Developer account. CI can sign automatically once those
  exist, with the credentials stored as encrypted GitHub secrets.

## Recommended repository settings (owner only)
- Turn on **two-factor authentication** for your GitHub account.
- Add a **branch protection rule** for `main`: require the "Build plugin" checks to pass, and block force-pushes.
- Keep the repository private, or review who has write access.

## Reporting a problem
Open a private security advisory in this repository (Security tab → "Report a vulnerability").
