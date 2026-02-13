# Project Overview

YOLO-Label is a GUI image-labeling tool for creating YOLO object detection training datasets. Built with **C++17 and Qt 6**, it runs on Windows, Linux, and macOS.

Key design: uses a **two-click method** (not drag-and-drop) to define bounding boxes, reducing wrist strain during long labeling sessions. Keyboard shortcuts (A/D for prev/next image, W/S for prev/next class) enable rapid workflow.

## Architecture

- **MainWindow** (`mainwindow.cpp/h`): Application logic, UI controls, file I/O, keyboard shortcuts
- **label_img** (`label_img.cpp/h`): Custom QLabel widget for image display, mouse-based bounding box drawing, contrast adjustment
- **Build**: qmake (`YoloLabel.pro`), CI via GitHub Actions (`.github/workflows/ci-release.yml`)
- **Output format**: YOLO annotation `.txt` files with normalized bounding box coordinates

# Project Guidelines

## Git Configuration
- All commits must be authored by `developer0hye <developer.0hye@gmail.com>`
- All commits must include `Signed-off-by` line to pass DCO check (always use `git commit -s`)

## Branching & PR Workflow
- Always create a new branch before starting any task (never work directly on `master`)
- Branch naming convention: `<type>/<short-description>` (e.g., `feat/add-dark-mode`, `fix/label-offset-bug`, `ci/add-linting`)
- Once the task is complete, push the branch and create a Pull Request to `master`
- Each branch should contain a single, focused unit of work
- Do not start a new task on the same branch — create a new branch for each task
- **When working on an existing PR** (e.g., fixing issues, adding changes), push commits directly to that PR's branch instead of creating a new PR. Only create a separate PR if explicitly requested. For cross-repository (fork) PRs, add the contributor's fork as a remote (e.g., `git remote add <user> <fork-url>`) and push to that remote's branch.
- **Before merging a PR**, check if `master` has advanced since the PR was created. If so, check for conflicts (e.g., `git merge-tree`) and rebase or merge `master` into the PR branch to resolve them before merging.
- **Never use `git checkout` or `git switch` to change branches.** Use `git worktree` to work on multiple branches simultaneously in separate directories
  - Create a worktree: `git worktree add ../Yolo_Label-<branch-name> -b <type>/<short-description>`
  - Work inside the worktree directory, not the main repo
  - Remove after merge: `git worktree remove ../Yolo_Label-<branch-name>`

## CI/CD
- CI builds and tests target stable, widely-used OS versions (e.g., `windows-latest`, `ubuntu-22.04`, `macos-latest` — these are examples only; if unsure about current runner availability, search the web for the latest GitHub Actions runner images before updating)

## Library & Framework Updates
- When upgrading libraries or frameworks (e.g., Qt 5 → Qt 6), **existing functionality must remain identical**. Only replace deprecated APIs with their direct equivalents — do not add, remove, or alter any user-facing behavior.
- **Visual rendering must also remain identical.** Framework upgrades can change how widgets are styled or drawn (e.g., checkbox indicators, selection highlights, frame styles, gradient support). After any upgrade, review all UI elements — especially those with custom stylesheets — and verify they render the same as before. Fix any visual regressions with explicit styles.

## Language
- All commit messages, PR descriptions, code comments, and documentation must be written in **English only**
