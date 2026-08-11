# Calcium Client

A modern, polished homebrew software-distribution client for PS4 systems running a compatible homebrew environment. Calcium Client provides a clean dark-themed interface for discovering, installing, and launching legally obtained homebrew applications from configured repositories.

---

## Features

- **Application catalog** — Browse, search, and filter homebrew apps by category
- **App detail pages** — Descriptions, screenshots, changelogs, firmware compatibility, and download sizes
- **Package management** — Download, SHA-256 verify, extract, and install ZIP packages
- **Installed apps view** — Track installed versions, launch apps, and uninstall
- **Downloads page** — Live progress, cancellation, and retry support
- **Settings page** — Configure repositories, paths, log level, and hash verification
- **Multi-repository** — Add as many JSON repositories as you like
- **Desktop simulation** — Full UI and package workflow on Linux/macOS/Windows via SDL2 (no PS4 required)
- **Robust error handling** — Network failures, bad metadata, corrupt packages, and storage errors all produce clear user-facing messages

---

## Documentation

| Document | Description |
|---|---|
| [docs/building.md](docs/building.md) | How to build for desktop and PS4 |
| [docs/repository-format.md](docs/repository-format.md) | Repository JSON schema reference |
| [docs/adding-apps.md](docs/adding-apps.md) | How to add applications to a repository |
| [docs/testing.md](docs/testing.md) | Running the test suite |

---

## Quick Start (desktop)

```bash
git clone https://github.com/example/calcium-client
cd calcium-client/CalciumClient
cmake -B build -DCALCIUM_BUILD_DESKTOP=ON -DCALCIUM_BUILD_TESTS=ON
cmake --build build
./build/bin/calcium-client --config resources/config.default.json
```

See [docs/building.md](docs/building.md) for full prerequisites and options.

---

## Architecture Overview

```
┌─────────────────────────────────────────┐
│              UI Layer                   │
│  (Screens, Widgets, Theme, Renderer)    │
├─────────────────────────────────────────┤
│           Application Core             │
│  (AppManager, Launcher, Application)   │
├───────────┬───────────┬─────────────────┤
│ Packages  │Repository │   Config        │
│ (Download │ (Index,   │   (Settings,    │
│  Install  │  Metadata │   Persistence)  │
│  Verify)  │  Parser)  │                 │
├───────────┴───────────┴─────────────────┤
│       Platform Abstraction Layer        │
│  (INetwork, IFilesystem, IDisplay,      │
│   IPlatform, ILauncher)                 │
├────────────────────┬────────────────────┤
│  Desktop (SDL2)    │   PS4 (Orbis SDK)  │
└────────────────────┴────────────────────┘
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.

This project does not implement, distribute, or document any PS4 security exploit, jailbreak chain, DRM bypass, piracy mechanism, or unauthorized PSN access. It assumes the console is already running a supported homebrew environment.
