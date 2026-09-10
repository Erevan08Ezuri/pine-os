# Pine OS native architecture

```text
SDL shell and built-in applications
               ↓
ApplicationManager + Pine services
               ↓
        Platform interfaces
               ↓
DesktopBattery / Camera / Bluetooth / Audio / Network / USB
```

Applications derive from `Application` and are discovered by the home screen through
`ApplicationManager`; the launcher does not construct or switch on app implementations.
Each service exposes stable behavior and delegates hardware state to a narrow backend.
Only `src/platform/desktop` contains simulation behavior.

Configuration is loaded before platform construction, validated, and atomically saved.
Malformed files are replaced with safe defaults. FileService canonicalizes every path,
rejects absolute/traversing paths and symlink escapes, and exposes only `data/storage`.

The version is declared once by CMake's `project(... VERSION ...)`, then emitted into
the generated `Version.hpp` used by the system API and executable.

## Text input

`TextInputSession` owns editing semantics: cursor, selection, insertion, replacement,
UTF-8-safe backward deletion, newline policy, and input hints. `TextInputManager` owns
system focus, physical-keyboard detection, animated keyboard visibility, and the safe
content boundary. `OnScreenKeyboard` is rendered once by the shell above applications;
applications bind fields to sessions and never communicate with key widgets directly.

## Notes

The Notes UI is a normal in-shell application. `NotesService` keeps its in-memory model
responsive while a worker serializes each stable note ID to a separate JSON record.
Writes use temporary and backup files, three bounded retries, error logging, and an
explicit flush during app/system shutdown. Search and pin-first sorting operate locally.
