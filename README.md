# 🔴 Kate Live Share

Real-time collaborative editing plugin for the [Kate](https://kate-editor.org/) text editor, inspired by VS Code Live Share.

> Type together, see each other's cursors, open files in sync — across the internet with zero configuration.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey)
![KDE](https://img.shields.io/badge/KDE-Frameworks%206-blue)
![Qt](https://img.shields.io/badge/Qt-6-green)

---

## Features

- 🟢 **Real-time co-editing** — changes sync instantly to all participants
- 🎨 **Colored remote cursors** — see exactly where everyone is
- 📂 **File sync** — when the host opens a file, guests open it too
- 🌐 **Internet sharing** — one click to create a public tunnel via [bore](https://github.com/ekzhang/bore), no port forwarding needed
- 👥 **Multi-guest** — multiple people can join the same session
- 🔌 **No account required** — fully self-hosted, no Microsoft servers

---

## How it works

```
Host ──────► bore.pub relay ◄────── Guest (any OS, any network)
```

The host starts a local WebSocket server. When sharing over the internet, [bore](https://github.com/ekzhang/bore) creates a free public tunnel to `bore.pub` — no port forwarding, no VPN, no account. Guests connect using a simple `bore.pub:PORT` address.

Edits are synchronized using **Operational Transform (OT)** — the host acts as the authority, transforms all concurrent edits, and broadcasts them to guests.

---

## Installation

### Linux

#### Dependencies

**Debian / Ubuntu:**
```bash
sudo apt install cmake extra-cmake-modules \
    qt6-websockets-dev libkf6texteditor-dev \
    libkf6coreaddons-dev libkf6i18n-dev libkf6xmlgui-dev
```

**Arch Linux:**
```bash
sudo pacman -S cmake extra-cmake-modules qt6-websockets \
               kf6-ktexteditor kf6-kcoreaddons kf6-ki18n kf6-kxmlgui
```

#### Build & Install
```bash
git clone https://github.com/Katziiii/kasher.git
cd kasher
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
kbuildsycoca6 --noincremental
```

---

### macOS

#### Dependencies
```bash
brew install cmake extra-cmake-modules kf6 qt6 kate
```

#### Build & Install
```bash
git clone https://github.com/Katziiii/kasher.git
cd kasher
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix)/lib/cmake"
make -j$(sysctl -n hw.ncpu)
sudo make install
```

---

### Enable the plugin in Kate

1. Open Kate
2. Go to **Settings → Configure Kate → Plugins**
3. Find **Live Share** and check it ✓
4. Click OK — the Live Share panel appears in the left sidebar

---

## Usage

### Hosting a session

1. Open a file in Kate
2. Open the **Live Share** panel (left sidebar)
3. Enter your display name
4. Set a port (default: `6789`) and click **Start Hosting**

**Same network (LAN):**
- Click **⎘ Copy Local Invite** and share the `192.168.x.x:6789` address

**Over the internet:**
- Click **🌐 Share via Internet**
- The `bore.pub:XXXXX` address is automatically copied to your clipboard
- Send it to your friend via any messenger

### Joining a session

1. Open the **Live Share** panel
2. Enter your display name
3. Paste the address (`192.168.x.x:6789` or `bore.pub:XXXXX`) in the Address field
4. Click **Join Session** — the file syncs immediately

### Stopping

Click **✕ Stop Session** in the panel. Guests can also disconnect independently.

---

## Internet sharing with bore

The **🌐 Share via Internet** button uses [bore](https://github.com/ekzhang/bore) to create a public tunnel. Only the **host** needs bore installed — guests just connect normally.

**Install bore on Linux:**
```bash
# Option 1 — via Cargo (Rust)
cargo install bore-cli

# Option 2 — direct binary
curl -L https://github.com/ekzhang/bore/releases/latest/download/bore-x86_64-unknown-linux-musl.tar.gz \
  | tar xz && sudo mv bore /usr/local/bin/
```

**Install bore on macOS:**
```bash
brew install bore-cli
```

---

## Architecture

```
plugin.cpp          KTextEditor::Plugin entry — view lifecycle, file switch events
session.cpp         Core: OT logic, document hooks, participant management
host_server.cpp     QWebSocketServer — host mode transport
guest_client.cpp    QWebSocket client — guest mode transport
ot_engine.cpp       Operational Transform: transform() for all op-type pairs
participant.cpp     Remote user with colored MovingRange cursor
tool_view.cpp       Kate side-panel UI + bore tunnel management
protocol.h          JSON message format constants
```

### OT model

The host is the single authority for conflict resolution:

1. Guest sends op with its known revision
2. Host transforms the op against all concurrent ops in history
3. Host commits, applies locally, broadcasts the transformed op to all peers
4. Guests apply received ops directly — the host already resolved conflicts

### Protocol (JSON over WebSocket)

| Message | Direction | Fields |
|---|---|---|
| `join` | Guest → Host | `author`, `name`, `color` |
| `sync` | Host → Guest | `content`, `rev`, `participants[]` |
| `op` | Any → Any | `op_type`, `pos`, `text`/`len`, `rev`, `author` |
| `cursor` | Any → Any | `line`, `col`, `author` |
| `file_open` | Host → Guests | `url`, `content`, `rev` |
| `participant_joined` | Host → Guests | `author`, `name`, `color` |
| `participant_left` | Host → Guests | `author` |
| `ack` | Host → Guest | `rev` |

---

## Known limitations

- No TLS — traffic is unencrypted. Use bore (encrypted tunnel) or a VPN for sensitive code
- No read-only guest mode (coming in v2)
- No shared terminal (coming in v2)
- Simultaneous edits at the exact same character position may cause minor flicker until the next keystroke resolves it

## Roadmap

- [ ] Read-only guest mode
- [ ] Shared terminal
- [ ] Client-side OT pending queue (zero-latency local feel)
- [ ] TLS support for direct connections
- [ ] Per-participant undo history

---

## Contributing

Pull requests are welcome. For major changes please open an issue first.

```bash
# Development build (with debug symbols)
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Run Kate from terminal to see plugin debug output:
```bash
kate --new-instance 2>&1
```

---

## License

MIT — see [LICENSE](LICENSE)

---

## Credits

Built with [KDE Frameworks 6](https://develop.kde.org/), [Qt 6](https://qt.io), and [bore](https://github.com/ekzhang/bore) for tunnel support.
