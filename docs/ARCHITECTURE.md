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

## Finance

Finance is local-first and depends on the shared Pine shell, keyboard, Files sandbox,
notifications, network state, and security service. Monetary values are stored as signed
64-bit minor units through the `Money`/`Currency` abstraction; floating point is used only
for non-persistent calculator projections. `FinanceDatabase` owns schema migrations,
foreign keys, WAL durability, and indexes. Corrupt databases are preserved for recovery;
if storage is unavailable, the app remains usable in memory and surfaces a warning.

`FinanceService` is the transactional domain boundary for accounts, activity, linked
transfers, categories, budgets, bills, subscriptions, goals, and snapshots. The UI never
executes SQL. `AnalyticsEngine` performs local cash-flow/category/recurrence analysis.
`ImportExportManager` validates sandboxed paths, previews and deduplicates mapped CSV rows,
streams paginated exports, and uses SQLite's online backup API. File jobs run asynchronously.
`FinancialDataProvider` keeps optional future bank connections outside the core model; the
built-in manual provider works without a network and never requests banking credentials.
