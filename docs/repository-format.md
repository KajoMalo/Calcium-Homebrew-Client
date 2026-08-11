# Repository Format

Calcium Client uses a simple JSON-based repository format. A repository is a single JSON file served over HTTP/HTTPS, or accessible via a `file://` path for local/offline use.

---

## Index file structure

```json
{
  "schema_version": "1.0",
  "repository": {
    "name":        "My Homebrew Repository",
    "description": "A curated collection of PS4 homebrew.",
    "maintainer":  "Your Name",
    "url":         "https://repo.example.com/index.json"
  },
  "apps": [ ... ]
}
```

### Top-level fields

| Field | Type | Required | Description |
|---|---|---|---|
| `schema_version` | string | Yes | Must be `"1.0"` for this version of the client |
| `repository` | object | No | Metadata about the repository itself |
| `apps` | array | Yes | Array of app metadata objects |

---

## App metadata object

```json
{
  "id":               "com.example.myapp",
  "name":             "My App",
  "version":          "1.2.3",
  "min_firmware":     "9.00",
  "author":           "Author Name",
  "license":          "MIT",
  "category":         "utility",
  "tags":             ["utility", "network"],
  "description":      "One-line summary shown in the catalog.",
  "long_description": "Full description shown on the detail page. Supports plain text.",
  "changelog":        "v1.2.3:\n- Fixed a bug\n- Added a feature",
  "icon_url":         "https://repo.example.com/icons/myapp.png",
  "screenshots":      [
    "https://repo.example.com/screenshots/myapp-1.png",
    "https://repo.example.com/screenshots/myapp-2.png"
  ],
  "download_url":     "https://repo.example.com/packages/myapp-1.2.3.zip",
  "download_size":    5242880,
  "installed_size":   10485760,
  "sha256":           "64-character lowercase hex SHA-256 of the package file",
  "content_id":       "UP0001-MYAPP00000_00-MYAPP0001230000",
  "compatibility": {
    "status":           "verified",
    "tested_firmware":  ["9.00", "11.00"],
    "notes":            "Optional compatibility notes."
  },
  "updated_at":       "2026-07-15T10:30:00Z"
}
```

### Required fields

| Field | Type | Description |
|---|---|---|
| `id` | string | Unique reverse-DNS or slug identifier. Must match `[a-zA-Z0-9][a-zA-Z0-9._\-]{1,127}` |
| `name` | string | Display name shown in the UI |
| `version` | string | Version string (no enforced format, but semantic versioning is recommended) |
| `author` | string | Author or team name |
| `download_url` | string | Full `https://` URL to the package ZIP file |

### Optional fields

| Field | Type | Default | Description |
|---|---|---|---|
| `min_firmware` | string | `""` | Minimum PS4 firmware required, e.g. `"9.00"` |
| `license` | string | `""` | SPDX license identifier, e.g. `"MIT"`, `"GPLv2"` |
| `category` | string | `""` | One of: `emulator`, `utility`, `media`, `game`, `tool`, or any custom value |
| `tags` | string[] | `[]` | Free-form search tags |
| `description` | string | `""` | Short one-line summary |
| `long_description` | string | `""` | Multi-paragraph description (plain text) |
| `changelog` | string | `""` | Release notes for the current version |
| `icon_url` | string | `""` | URL to a PNG/JPEG icon (recommended: 256×256) |
| `screenshots` | string[] | `[]` | URLs to screenshot images |
| `download_size` | integer | `0` | Package file size in bytes |
| `installed_size` | integer | `0` | Estimated disk usage after installation, in bytes |
| `sha256` | string | `""` | Lowercase hex SHA-256 of the package file. Strongly recommended. If present, must be exactly 64 hex characters. |
| `content_id` | string | `""` | PS4 Content ID in the format `PPPPPP-XXXXXXX_XX-XXXXXXXXXXXXXXXX` |
| `compatibility` | object | — | See below |
| `updated_at` | string | `""` | ISO 8601 timestamp of the last update |

### Compatibility object

| Field | Type | Description |
|---|---|---|
| `status` | string | One of: `verified`, `partial`, `broken`, `unknown` |
| `tested_firmware` | string[] | Firmware versions this app has been tested on |
| `notes` | string | Human-readable compatibility notes |

---

## Package format

Packages are standard ZIP archives. The archive must contain a single top-level directory named after the app. The client strips this top-level directory when installing:

```
myapp-1.2.3.zip
└── myapp-1.2.3/          ← top-level directory (stripped on install)
    ├── eboot.bin          ← main executable (PS4) or platform binary
    ├── sce_sys/
    │   ├── icon0.png
    │   └── param.sfo
    └── assets/
        └── ...
```

Both **Stored** (method 0) and **Deflate** (method 8) compression methods are supported. Other compression methods will cause the installation to fail with an error.

---

## SHA-256 verification

Providing a `sha256` hash is strongly recommended. The client verifies the downloaded package before installation. If the hash does not match, the package is deleted and an error is shown to the user.

Generate the hash with:

```bash
# Linux / macOS
sha256sum myapp-1.2.3.zip

# PowerShell (Windows)
Get-FileHash myapp-1.2.3.zip -Algorithm SHA256
```

---

## Hosting a repository

Any static file server works. The index JSON file must:

1. Be served with `Content-Type: application/json` (most servers do this automatically for `.json` files)
2. Be accessible over HTTPS (recommended) or HTTP
3. Have CORS headers if accessed from a web-based client (not required for Calcium Client)

### Minimal nginx example

```nginx
server {
    listen 443 ssl;
    server_name repo.example.com;

    root /var/www/calcium-repo;
    index index.json;

    location / {
        add_header Cache-Control "public, max-age=300";
        try_files $uri $uri/ =404;
    }
}
```

### Recommended directory layout on the server

```
/var/www/calcium-repo/
├── index.json          ← repository index (served as the repo URL)
├── icons/
│   ├── myapp.png
│   └── otherapp.png
├── screenshots/
│   ├── myapp-1.png
│   └── myapp-2.png
└── packages/
    ├── myapp-1.2.3.zip
    └── otherapp-2.0.0.zip
```

---

## Versioning and updates

When you release a new version of an app:

1. Upload the new package ZIP to your server
2. Update the app's `version`, `download_url`, `sha256`, `download_size`, and `changelog` fields in `index.json`
3. Update `updated_at` to the current timestamp

Calcium Client compares the `version` string in the repository against the `version` stored in the local installed database. If they differ, an update badge is shown in the catalog.

---

## Validation

The `MetadataParser::validate()` function enforces:

- `id`, `name`, `version`, `author`, `download_url` must not be empty
- `id` must match `[a-zA-Z0-9][a-zA-Z0-9._\-]{1,127}`
- `download_url` must begin with `http`
- `sha256`, if present, must be exactly 64 lowercase hex characters

Apps that fail validation are skipped with a warning logged; they do not prevent the rest of the index from loading.
