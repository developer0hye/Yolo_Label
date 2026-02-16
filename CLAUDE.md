# Project Overview

YOLO-Label is a GUI image-labeling tool for creating YOLO object detection training datasets. Built with **C++17 and Qt 6**, it runs on Windows, Linux, and macOS.

Key design: uses a **two-click method** (not drag-and-drop) to define bounding boxes, reducing wrist strain during long labeling sessions. Keyboard shortcuts (A/D for prev/next image, W/S for prev/next class) enable rapid workflow.

## Architecture

- **MainWindow** (`mainwindow.cpp/h`): Application logic, UI controls, file I/O, keyboard shortcuts
- **label_img** (`label_img.cpp/h`): Custom QLabel widget for image display, mouse-based bounding box drawing, contrast adjustment
- **YoloDetector** (`yolo_detector.cpp/h`): ONNX Runtime-based YOLO inference engine for pseudo labeling (see below)
- **Build**: qmake (`YoloLabel.pro`), CI via GitHub Actions (`.github/workflows/ci-release.yml`)
- **Output format**: YOLO annotation `.txt` files with normalized bounding box coordinates

## Pseudo Labeling (Auto-Label)

Local YOLO object detection that auto-generates bounding boxes from an Ultralytics `.onnx` model. Loading a single `.onnx` file is sufficient — class names, input size, and model configuration are all read from the ONNX metadata embedded by Ultralytics.

### Design Principle

This feature targets full compatibility with the [Ultralytics](https://github.com/ultralytics/ultralytics) project's ONNX export. When modifying or extending the pseudo labeling feature:

1. **Always check the upstream Ultralytics source** for the latest supported models, ONNX export format, and metadata schema. Key references:
   - Model export: `ultralytics/engine/exporter.py` — how metadata is embedded in ONNX
   - ONNX predict: `ultralytics/nn/autobackend.py` — how metadata is read back and used for inference
   - Postprocessing: `ultralytics/utils/ops.py` (`non_max_suppression`) and `ultralytics/models/yolo/detect/predict.py`
   - Supported models list: https://docs.ultralytics.com/models/
2. **Ultralytics may add new model versions, change ONNX metadata keys, or alter output tensor layouts.** Before making changes, search the Ultralytics repo and docs for the current state rather than relying solely on what is documented here.
3. **The goal is: a user exports any Ultralytics object detection model to `.onnx` and loads it in YOLO-Label — it just works.** No separate class names file, no manual configuration.

### ONNX Metadata

Ultralytics embeds metadata as string key-value pairs in ONNX `metadata_props` during export. The detector reads these via `Ort::Session::GetModelMetadata()` to configure itself automatically. Key fields (check `ultralytics/engine/exporter.py` for the latest schema):

| Key | Example | Usage |
|---|---|---|
| `names` | `"{0: 'person', 1: 'car', ...}"` | Auto-populate class list (Python dict format, parsed in C++) |
| `task` | `"detect"` | Validate model type; reject non-detection models |
| `stride` | `"32"` | Model stride |
| `imgsz` | `"[640, 640]"` | Input resolution for dynamic-shape models |
| `end2end` | `"True"` | Skip NMS when model has NMS baked in |
| `description` | `"Ultralytics YOLOv8n model"` | Detect specific YOLO version |

### Inference Pipeline (`yolo_detector.cpp`)

The C++ inference code references the Ultralytics Python implementation but maximizes use of ONNX metadata to make a single `.onnx` file self-sufficient for detection:

1. **Load**: `Ort::Session` loads the model. Metadata is read via `GetModelMetadata()` → `GetCustomMetadataMapKeysAllocated()` / `LookupCustomMetadataMapAllocated()`.
2. **Version detection**: First checks metadata `description` for specific version, then falls back to output shape heuristic (`dim1 > dim2` → V5-style, else V8-style). End-to-end is a boolean flag (`endToEnd`), not a version. The `YoloVersion` enum and `detectVersionFromMetadata()` may need new entries when Ultralytics releases new model architectures.
3. **Preprocess**: Letterbox resize via `QImage::scaled()`, gray padding (114/255), HWC→CHW, normalize to [0,1].
4. **Postprocess**: Three paths depending on model output tensor layout:
   - **V5-style**: `[B, N, C+5]` — has objectness score, row-major layout
   - **V8-style**: `[B, C+4, N]` — no objectness, transposed (column-major per detection). Used by V8, V11, V12, V26 and likely future versions that share this layout.
   - **End-to-end**: `[1, maxDet, 6]` — `[x1, y1, x2, y2, score, class_id]`, NMS already applied
   - **When adding support for a new model**, check whether its ONNX output layout matches an existing path or requires a new `postprocess*()` function.
5. **NMS**: Pure C++ greedy NMS (class-aware, sorted by confidence). Skipped for end-to-end models.
6. **Output**: `DetectionResult` with normalized [0,1] coordinates matching `ObjectLabelingBox.box` format.

### Conditional Build

ONNX Runtime is optional. When `ONNXRUNTIME_DIR` points to a valid installation, `YoloLabel.pro` defines `ONNXRUNTIME_AVAILABLE` and compiles the detector. Without it, the app builds normally without the auto-label UI. See `scripts/download_onnxruntime.sh` for downloading pre-built ONNX Runtime binaries.

# Project Guidelines

## Git Configuration
- All commits must use the local git config `user.name` and `user.email` for both author and committer. Verify with `git config user.name` and `git config user.email` before committing.
- All commits must include `Signed-off-by` line to pass DCO check (always use `git commit -s`). The `Signed-off-by` name must match the commit author.
- If the local git config `user.name` is **not** `developer0hye`, you **MUST** ask the user to confirm their identity before the first commit or push in the session. Once confirmed, do not ask again for the rest of the session.

## Branching & PR Workflow
- Always create a new branch before starting any task (never work directly on `master`)
- **NEVER edit files or commit directly to `master`.** All changes — including documentation and config files — must go through a PR. Always create the worktree/branch first, then make all edits inside the worktree directory. Editing files on `master` and then copying them to a worktree causes merge conflicts when `master` advances.
- Branch naming convention: `<type>/<short-description>` (e.g., `feat/add-dark-mode`, `fix/label-offset-bug`, `ci/add-linting`)
- Once the task is complete, push the branch and create a Pull Request to `master`
- Each branch should contain a single, focused unit of work
- Do not start a new task on the same branch — create a new branch for each task
- **When working on an existing PR** (e.g., fixing issues, adding changes), push commits directly to that PR's branch instead of creating a new PR. Only create a separate PR if explicitly requested. For cross-repository (fork) PRs, add the contributor's fork as a remote (e.g., `git remote add <user> <fork-url>`) and push to that remote's branch.
- **Before merging a PR**, review its description. If the body is empty or lacks sufficient detail (e.g., no explanation of what changed or why), rewrite it using `gh pr edit <number> --body "..."`. A good PR description should include: a summary of what the PR does and why, a list of key changes, and any relevant context (e.g., related issues, shortcuts added, behavioral changes). Look at the actual commits and code diff to write an accurate description.
- **Before merging a PR**, search for related issues using `gh issue list` and the PR's topic/keywords. If related issues exist, mention them in the PR description (e.g., "Related: #70, #55"). Do not use auto-close keywords (`Closes`, `Fixes`) unless explicitly instructed — only reference issues for context. Only mention issues that are directly related to the PR's changes.
- **Before merging a PR**, check if `master` has advanced since the PR was created. If so, check for conflicts (e.g., `git merge-tree`) and rebase or merge `master` into the PR branch to resolve them before merging.
- **Never use `git checkout` or `git switch` to change branches.** Use `git worktree` to work on multiple branches simultaneously in separate directories
  - Create a worktree: `git worktree add ../Yolo_Label-<branch-name> -b <type>/<short-description>`
  - Work inside the worktree directory, not the main repo
  - **Do NOT remove a worktree immediately after completing a task.** If you delete the worktree while your working directory is still inside it, all subsequent commands will fail because the path no longer exists. Only remove a worktree after you have confirmed the user wants it removed, or when starting a new task (at which point you will create a new worktree and move into that directory).
  - **When removing a worktree, you MUST return to the main repo directory first.** You cannot remove a worktree while your working directory is inside it. Always run: `cd /Users/yhkwon/Documents/Projects/Yolo_Label && git worktree remove ../Yolo_Label-<branch-name>`
- **After merging a PR**, always run `git pull` in the main repo directory (`/Users/yhkwon/Documents/Projects/Yolo_Label`) to keep local `master` up to date. New worktrees branch off local `master`, so a stale `master` causes missing commits in new branches.

## CI/CD
- CI builds and tests target stable, widely-used OS versions (e.g., `windows-latest`, `ubuntu-22.04`, `macos-latest` — these are examples only; if unsure about current runner availability, search the web for the latest GitHub Actions runner images before updating)

## Releases
- When creating a GitHub Release, include a **Contributors** section in the release notes that credits all external contributors (anyone other than `developer0hye`) since the previous release.
- Use `git log <prev-tag>..HEAD --format='%an' | sort -u` to find contributors, then list each with their GitHub profile link (`[@username](https://github.com/username)`) and a brief summary of what they contributed.

## Library & Framework Updates
- When upgrading libraries or frameworks (e.g., Qt 5 → Qt 6), **existing functionality must remain identical**. Only replace deprecated APIs with their direct equivalents — do not add, remove, or alter any user-facing behavior.
- **Visual rendering must also remain identical.** Framework upgrades can change how widgets are styled or drawn (e.g., checkbox indicators, selection highlights, frame styles, gradient support). After any upgrade, review all UI elements — especially those with custom stylesheets — and verify they render the same as before. Fix any visual regressions with explicit styles.

## Language
- All commit messages, PR descriptions, code comments, and documentation must be written in **English only**
