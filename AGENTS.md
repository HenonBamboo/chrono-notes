# Repository Guidelines

## Project Structure & Module Organization

ChronoNotes is a Windows desktop app built with Qt 6 Quick/QML and C++17. Core C++ modules live in `src/`: `qt_note_app.*` is the application facade, `note_store.*` handles SQLite persistence, `note_view.*` builds filtered row projections, `config.*` manages local settings, `ai_client.*` calls OpenAI-compatible APIs, and `backup_service.*` handles import/export. QML UI components live in `qml/`. Tests live in `tests/`, with QML interaction tests under `tests/qml/`. Visual assets and Windows resources are in `assets/`; packaging scripts are in `tools/`; project notes are in `docs/`.

## Build, Test, and Development Commands

Configure a local MinGW build:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="<qt-mingw-path>"
```

Build the app:

```powershell
cmake --build build -j 6
```

Run the desktop app:

```powershell
.\build\ChronoNotes.exe
```

Run all registered tests:

```powershell
ctest --test-dir build --output-on-failure
```

Package a release:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/package_release.ps1 -BuildDir build -OutputDir dist
```

## Coding Style & Naming Conventions

Use C++17 and Qt idioms. Keep C++ files paired as `snake_case.h/.cpp`; QML components use `PascalCase.qml`. Use four-space indentation in C++ and QML. Prefer small, behavior-focused modules over broad utility classes. Keep UI copy, state names, and model roles consistent between `src/qt_note_app.*` and `qml/`.

## Testing Guidelines

C++ tests use Qt Test and are registered through CTest: `note_store_tests`, `note_view_tests`, `project_tree_model_tests`, `config_tests`, and `qt_note_app_tests`. QML tests use `qmltestrunner` via `qml_interaction_tests`. Add tests beside related coverage, using names that describe observable behavior. For data-path tests, set isolated data directories rather than touching real `data/`.

## Commit & Pull Request Guidelines

Recent commits use short imperative titles, for example `Fix Windows executable icon resource` and `Polish README and remove local paths`. Follow that style: one focused change per commit, sentence case, no noisy prefixes unless the project later adopts them. Pull requests should include a concise summary, test evidence, linked issues when relevant, and screenshots or screen recordings for UI changes.

## Security & Configuration Tips

Do not commit API keys, local Qt SDKs, build outputs, runtime databases, or personal config files. Keep AI endpoint settings in local config only. Build directories such as `build/`, `cmake-build-*`, `dist/`, and runtime `data/` should stay out of source control unless explicitly required.
