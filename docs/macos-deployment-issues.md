# macOS Deployment — Known Issues & Fixes

This document records issues discovered during macOS CI packaging and local deployment testing (February 2026), along with the fixes applied.

## Issues Found & Fixed in CI

### 1. `codesign --deep` deprecated for signing (macOS 13+)

**Problem:** The original CI used `codesign --force --deep --sign` to sign the entire `.app` bundle in one pass. Apple deprecated `--deep` for signing as of macOS 13 because it applies all options (including entitlements) uniformly to nested code, which produces invalid bundles.

**Fix:** Replaced with inside-out signing per Apple's current requirements:
1. Sign all `.dylib` files in `Contents/Frameworks/`
2. Sign `.framework` bundles (binary inside first, then the bundle itself)
3. Sign Qt plugins in `Contents/PlugIns/`
4. Sign the main app bundle last — only the main executable gets entitlements

Note: `codesign --deep` is still valid for **verification** (`--verify`).

**References:**
- [CODESIGN(1) man page](https://keith.github.io/xcode-man-pages/codesign.1.html)
- [Resolving common notarization issues — Apple Developer](https://developer.apple.com/documentation/security/resolving-common-notarization-issues)

### 2. `macdeployqt` has a known framework-signing bug

**Problem:** Qt's `macdeployqt` signs binaries inside `.framework` bundles but does NOT sign the enclosing `.framework` directory itself. This causes `codesign` to report the app as "not signed at all" when verifying.

**Fix:** Do not pass `-codesign` or `-sign-for-notarization` to `macdeployqt`. Instead, run `macdeployqt` without any signing, then sign everything manually using the inside-out approach.

**References:**
- [GDATASoftwareAG/macdeployqt fork](https://github.com/GDATASoftwareAG/macdeployqt) (community fix)
- [Qt for macOS Deployment — Qt 6 Documentation](https://doc.qt.io/qt-6/macos-deployment.html)

### 3. `install_name_tool` ordering with `macdeployqt`

**Problem:** The rpath `@executable_path/../Frameworks` was added to the binary *before* `macdeployqt` ran. Since `macdeployqt` also adds this rpath, it resulted in a duplicate rpath warning that was silently suppressed by `|| true`.

**Fix:**
- Run `macdeployqt` first (it sets up rpaths)
- Add the rpath defensively *after* `macdeployqt`, filtering out only the "would duplicate" warning instead of suppressing all errors

### 4. ONNX Runtime dylib copy missed symlinks

**Problem:** `cp onnxruntime/lib/libonnxruntime.*.dylib` only copies the versioned file (e.g., `libonnxruntime.1.24.1.dylib`). The unversioned symlink (`libonnxruntime.dylib -> libonnxruntime.1.24.1.dylib`) was not copied, which could break if a future ORT version changes its install name scheme.

**Fix:** Changed to `cp -a onnxruntime/lib/libonnxruntime*.dylib` which:
- Uses a broader glob to match both versioned and unversioned files
- Preserves symlinks with `-a` flag

### 5. Notarization ran on every push

**Problem:** The notarization step ran on every push to `master` (not just releases), wasting Apple API calls and adding 5-15 minutes to CI.

**Fix:** Restricted notarization to tag pushes only:
```yaml
if: runner.os == 'macOS' && env.CODESIGN_IDENTITY != '' && startsWith(github.ref, 'refs/tags/')
```

## Runtime Warnings (Non-Critical)

The following warnings appear at runtime but do **not** affect functionality:

### `Unknown property gridline-width`

Qt 6 no longer supports the `gridline-width` CSS property in stylesheets. This produces warnings but the table widget renders correctly.

### `QMetaObject::connectSlotsByName: No matching signal for on_pushButton_prev_clicked()`

Slot naming convention (`on_<objectName>_<signal>`) triggers Qt's auto-connect mechanism, but the matching signal does not exist on the widget. These slots are connected manually elsewhere in the code, so the warning is harmless.

Affected slots:
- `on_pushButton_prev_clicked()`
- `on_pushButton_next_clicked()`
- `on_usageTimer_timeout()`
- `on_usageTimerReset_clicked()`

### `Populating font family aliases took N ms. Replace uses of missing font family "Consolas"`

The stylesheet references the "Consolas" font which is not available on macOS. Qt falls back to a system monospace font. This causes a one-time startup delay (~100ms) while Qt resolves font aliases.

## macOS Gatekeeper Notes (macOS 15+)

As of macOS 15 (Sequoia):
- **Control-click bypass removed.** Users can no longer right-click to bypass Gatekeeper for unsigned apps.
- **Stricter enforcement in macOS 15.1+.** Users must navigate to System Settings > Privacy & Security to approve blocked apps.
- **Drag-and-drop checks in macOS 15.4+.** Gatekeeper alerts also trigger when dragging quarantined files into applications.

Proper Developer ID signing + notarization is effectively **mandatory** for macOS distribution.
