# macOS Code Signing & Notarization Setup

This document explains how to configure GitHub Actions secrets so that CI can sign and notarize macOS builds with a Developer ID certificate. Once set up, release DMGs will pass Gatekeeper without any warnings.

## Prerequisites

- An [Apple Developer Program](https://developer.apple.com/programs/) membership (US$99/year)
- A **Developer ID Application** certificate issued by Apple
- Xcode or Keychain Access on a Mac (to export the certificate)

## Required Secrets

Add the following secrets to your GitHub repository under **Settings > Secrets and variables > Actions**:

| Secret | Description |
|---|---|
| `MACOS_CERTIFICATE` | Base64-encoded `.p12` file containing the Developer ID Application certificate and private key |
| `MACOS_CERTIFICATE_PASSWORD` | Password used when exporting the `.p12` file |
| `APPLE_ID` | Apple ID email used for notarization (e.g., `you@example.com`) |
| `APPLE_ID_PASSWORD` | App-specific password for the Apple ID (NOT your regular Apple ID password) |
| `APPLE_TEAM_ID` | 10-character Apple Developer Team ID |

## Step-by-Step Setup

### 1. Export the Developer ID Certificate as `.p12`

You need the **Developer ID Application** certificate with its private key.

**Option A — Using Keychain Access:**
1. Open **Keychain Access** on your Mac
2. In the sidebar, select **login** keychain and the **My Certificates** category
3. Find **Developer ID Application: Your Name (TEAM_ID)**
4. Right-click the certificate > **Export...**
5. Choose **Personal Information Exchange (.p12)** format
6. Set a strong password (this becomes `MACOS_CERTIFICATE_PASSWORD`)
7. Save the file (e.g., `certificate.p12`)

**Option B — Using the command line:**
```bash
# List available signing identities
security find-identity -v -p codesigning

# Export (replace the hash with your certificate's SHA-1)
security export -k ~/Library/Keychains/login.keychain-db \
  -t identities -f pkcs12 -o certificate.p12
```

### 2. Base64-Encode the Certificate

```bash
base64 -i certificate.p12 | pbcopy
```

This copies the base64 string to your clipboard. Paste it as the value of the `MACOS_CERTIFICATE` secret on GitHub.

> **Security note:** Delete the `.p12` file from disk after you have added the secret.

### 3. Create an App-Specific Password

Apple requires an **app-specific password** for notarization (your regular Apple ID password will not work).

1. Go to [appleid.apple.com](https://appleid.apple.com/)
2. Sign in and navigate to **Sign-In and Security > App-Specific Passwords**
3. Click **Generate an app-specific password**
4. Give it a label (e.g., `GitHub Actions Notarization`)
5. Copy the generated password and save it as the `APPLE_ID_PASSWORD` secret

### 4. Find Your Team ID

```bash
# If you have Xcode installed:
xcrun simctl list 2>/dev/null | head -1
# Or check https://developer.apple.com/account → Membership Details
```

Your Team ID is a 10-character alphanumeric string (e.g., `A1B2C3D4E5`).

### 5. Add Secrets to GitHub

Go to your repository on GitHub:

1. **Settings** > **Secrets and variables** > **Actions**
2. Click **New repository secret** for each:
   - `MACOS_CERTIFICATE` — the base64 string from step 2
   - `MACOS_CERTIFICATE_PASSWORD` — the password from step 1
   - `APPLE_ID` — your Apple ID email
   - `APPLE_ID_PASSWORD` — the app-specific password from step 3
   - `APPLE_TEAM_ID` — the Team ID from step 4

## How It Works

The CI workflow (`.github/workflows/ci-release.yml`) handles signing and notarization automatically:

1. **Certificate import** — Decodes the `.p12` from the secret, creates a temporary keychain, and imports the certificate
2. **Code signing** — Signs the `.app` bundle with `Developer ID Application`, hardened runtime (`--options runtime`), and an Apple timestamp (`--timestamp`). The DMG is also signed separately.
3. **Notarization** — Submits the signed DMG to Apple's notary service via `xcrun notarytool` and waits for approval
4. **Stapling** — Attaches the notarization ticket to the DMG via `xcrun stapler`, so Gatekeeper can verify it offline
5. **Cleanup** — Deletes the temporary keychain

### PR / Fork Builds (No Secrets)

When secrets are not available (e.g., pull requests from forks), the workflow gracefully falls back to **ad-hoc signing** (`codesign --sign -`). The build will not break — it simply won't be notarized. This is the same behavior as before this feature was added.

## Verification

After a release build completes, download the DMG and verify:

```bash
# Check code signature
codesign --verify --deep --strict --verbose=2 /Volumes/YoloLabel/YoloLabel.app

# Check notarization
spctl --assess --type open --context context:primary-signature YoloLabel-macOS.dmg

# Check stapled ticket
stapler validate YoloLabel-macOS.dmg
```

## Troubleshooting

### "Developer ID Application" identity not found
- Ensure the `.p12` file contains both the certificate **and** the private key
- Verify the certificate is a **Developer ID Application** type (not Developer ID Installer or other types)

### "The user name or passphrase you entered is not correct"
- The `MACOS_CERTIFICATE_PASSWORD` secret is empty or does not match the `.p12` export password
- **Secret name matters**: verify the name is exactly `MACOS_CERTIFICATE_PASSWORD` (not `MACOS_CERTIFICATE_PWD` or other variations). Use `gh secret list` to check registered secret names.
- If you forgot the password, re-export the certificate from Keychain Access with a new password and update both `MACOS_CERTIFICATE` and `MACOS_CERTIFICATE_PASSWORD` secrets

### Notarization fails with "Invalid credentials"
- Confirm `APPLE_ID_PASSWORD` is an **app-specific password**, not your regular Apple ID password
- Ensure the Apple ID is associated with the same team as the Developer ID certificate

### App crashes with "Could not load the Qt platform plugin" after signing

This happens when hardened runtime is enabled (`--options runtime`) but not all nested binaries are re-signed with the same Developer ID.

**Root cause:** `macdeployqt` copies Qt plugins (e.g., `libqcocoa.dylib`) into `Contents/PlugIns/`. These plugins retain Qt Company's original signature. With hardened runtime, macOS enforces **library validation** — all loaded dylibs must be signed by the same Team ID as the main executable. If a plugin is signed by a different team (Qt Company vs. your Developer ID), the app fails to load it at runtime.

**Solution:** Sign **all** dylibs under `Contents/` (not just `Contents/Frameworks/`):
```bash
# Wrong: only signs Frameworks, misses PlugIns
find YoloLabel.app/Contents/Frameworks -name '*.dylib' -exec codesign ...

# Correct: signs everything under Contents (Frameworks + PlugIns)
find YoloLabel.app/Contents -name '*.dylib' -exec codesign ...
```

**How to diagnose:** Compare signing authorities across binaries:
```bash
# Check main binary
codesign -d --verbose=2 YoloLabel.app/Contents/MacOS/YoloLabel 2>&1 | grep Authority

# Check a plugin
codesign -d --verbose=2 YoloLabel.app/Contents/PlugIns/platforms/libqcocoa.dylib 2>&1 | grep Authority
```
If the `Authority` lines show different Team IDs, the plugin needs to be re-signed.

### Notarization fails with "Hardened Runtime not enabled"
- The `--options runtime` flag must be present in the `codesign` command
- Ensure all nested binaries (dylibs in Frameworks/ **and** PlugIns/) are also signed with `--options runtime`

### Notarization times out
- Apple's notary service usually completes within 5-15 minutes, but can occasionally take longer
- Re-running the workflow will resubmit the DMG
