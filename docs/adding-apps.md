# Adding Applications to a Repository

This guide walks through the complete process of packaging a homebrew application and adding it to a Calcium-compatible repository.

---

## 1. Prepare the package archive

Calcium installs from standard ZIP archives. The archive must have a single top-level directory:

```
myapp-1.0.0.zip
└── myapp-1.0.0/
    ├── eboot.bin           ← PS4 ELF / homebrew binary
    ├── sce_sys/
    │   ├── icon0.png       ← 512×512 PNG app icon
    │   ├── pic1.png        ← optional 1920×1080 background image
    │   └── param.sfo       ← PS4 system file with title/content ID
    └── data/               ← any additional assets
        └── ...
```

Create the archive:

```bash
# Put all files under a versioned directory first.
mkdir -p myapp-1.0.0/sce_sys
cp eboot.bin myapp-1.0.0/
cp sce_sys/icon0.png myapp-1.0.0/sce_sys/
cp sce_sys/param.sfo  myapp-1.0.0/sce_sys/

# Create the ZIP.
zip -r myapp-1.0.0.zip myapp-1.0.0/
```

---

## 2. Compute the SHA-256 hash

```bash
# Linux / macOS
sha256sum myapp-1.0.0.zip
# Output: abc123...  myapp-1.0.0.zip

# PowerShell (Windows)
(Get-FileHash myapp-1.0.0.zip -Algorithm SHA256).Hash.ToLower()
```

Record the 64-character hex string — you will need it for the metadata.

---

## 3. Get the file sizes

```bash
# Download size (the ZIP):
wc -c < myapp-1.0.0.zip          # bytes

# Installed size (unzipped contents):
du -sb myapp-1.0.0/ | cut -f1    # bytes (Linux)
```

---

## 4. Write the metadata object

Add an entry to your repository's `index.json` under the `apps` array:

```json
{
  "id":               "com.example.myapp",
  "name":             "My App",
  "version":          "1.0.0",
  "min_firmware":     "9.00",
  "author":           "Your Name",
  "license":          "MIT",
  "category":         "utility",
  "tags":             ["utility", "demo"],
  "description":      "A short one-line description shown in the catalog.",
  "long_description": "A full description that appears on the detail page.\n\nSupports multiple paragraphs.",
  "changelog":        "v1.0.0:\n- Initial release",
  "icon_url":         "https://repo.example.com/icons/myapp.png",
  "screenshots":      [],
  "download_url":     "https://repo.example.com/packages/myapp-1.0.0.zip",
  "download_size":    2097152,
  "installed_size":   4194304,
  "sha256":           "abc123...64hexchars...",
  "content_id":       "UP0001-MYAPP00000_00-MYAPP0001000000",
  "compatibility": {
    "status":          "verified",
    "tested_firmware": ["9.00"],
    "notes":           ""
  },
  "updated_at":       "2026-08-10T12:00:00Z"
}
```

### Category values

Use one of these standard categories for consistent filtering:

| Value | Description |
|---|---|
| `emulator` | Multi-system or single-system emulators |
| `utility` | System tools, file managers, FTP servers, etc. |
| `media` | Media players, image viewers, music players |
| `game` | Homebrew games and game engines |
| `tool` | Developer tools, debuggers, modding utilities |

Custom category values are accepted and will appear in the category filter.

---

## 5. Upload files to your server

```bash
# Upload the package.
scp myapp-1.0.0.zip user@repo.example.com:/var/www/calcium-repo/packages/

# Upload the icon.
scp icon0.png user@repo.example.com:/var/www/calcium-repo/icons/myapp.png

# Upload the updated index.
scp index.json user@repo.example.com:/var/www/calcium-repo/
```

---

## 6. Validate your index

Test the index locally using Calcium Client's desktop mode:

```bash
# Point the config at your local index file.
cat > /tmp/test-config.json << 'EOF'
{
  "repositories": [
    {
      "id": "my-repo",
      "name": "My Repo",
      "url": "file:///path/to/your/index.json",
      "enabled": true
    }
  ]
}
EOF

./build/bin/calcium-client --config /tmp/test-config.json
```

Your app should appear in the catalog. Check `calcium.log` for any validation warnings.

You can also validate the JSON schema directly with `python3`:

```python
import json, sys

with open("index.json") as f:
    index = json.load(f)

assert index.get("schema_version") == "1.0", "Wrong schema_version"
assert isinstance(index.get("apps"), list), "apps must be an array"

for app in index["apps"]:
    assert app.get("id"),           f"Missing id in {app}"
    assert app.get("name"),         f"Missing name in {app['id']}"
    assert app.get("version"),      f"Missing version in {app['id']}"
    assert app.get("author"),       f"Missing author in {app['id']}"
    assert app.get("download_url"), f"Missing download_url in {app['id']}"

print(f"OK — {len(index['apps'])} app(s) validated.")
```

---

## 7. Releasing an update

1. Build and package the new version as `myapp-1.1.0.zip`
2. Compute the new SHA-256 hash
3. Update the metadata entry:
   - `version`: `"1.1.0"`
   - `download_url`: new ZIP URL
   - `sha256`: new hash
   - `download_size` / `installed_size`: updated sizes
   - `changelog`: add a new section at the top
   - `updated_at`: current timestamp
4. Upload the new ZIP and updated `index.json`

Users who already have `1.0.0` installed will see an **UPDATE** badge on the app's card in the catalog.

---

## Content ID format

The `content_id` field follows the PS4 title ID format:

```
UP0001-MYAPP00000_00-MYAPP0001000000
│      │             │
│      │             └── 16-char unique ID (uppercase alphanumeric)
│      └── Title ID (9 chars: letter + 4 digits + underscore + 2 digits)
└── Region + publisher prefix
```

For homebrew you can generate a placeholder content ID. If blank, the launch feature is disabled on PS4 (but all other features work normally on desktop).

---

## Local/offline repositories

For testing or air-gapped environments, use `file://` URLs:

```json
{
  "id":      "local-repo",
  "name":    "Local Test Repo",
  "url":     "file:///home/user/my-repo/index.json",
  "enabled": true
}
```

The `file://` scheme is fully supported and does not require a network connection.
