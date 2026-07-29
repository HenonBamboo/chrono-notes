#include "note_store.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

static int failures = 0;

static void expect_int(const char *name, int expected, int actual) {
    if (expected != actual) {
        printf("FAIL %s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

static void expect_wstr(const char *name, const wchar_t *expected, const wchar_t *actual) {
    if (wcscmp(expected, actual) != 0) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

static void expect_true(const char *name, int actual) {
    expect_int(name, 1, actual ? 1 : 0);
}

static void expect_last_error(const char *name) {
    const wchar_t *error = note_store_last_error();
    if (error == NULL || error[0] == L'\0') {
        printf("FAIL %s: expected a diagnostic error\n", name);
        failures++;
    }
}

static bool execute_sql(const QString &path, const QStringList &statements) {
    const QString connection_name = QStringLiteral("note_store_test_%1")
                                        .arg(QUuid::createUuid().toString(QUuid::Id128));
    bool success = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
        database.setDatabaseName(path);
        success = database.open();
        if (success) {
            QSqlQuery query(database);
            for (const QString &statement : statements) {
                if (!query.exec(statement)) {
                    success = false;
                    break;
                }
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection_name);
    return success;
}

static int sqlite_scalar_int(const QString &path, const QString &statement) {
    const QString connection_name = QStringLiteral("note_store_test_%1")
                                        .arg(QUuid::createUuid().toString(QUuid::Id128));
    int result = -1;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(statement) && query.next()) {
                result = query.value(0).toInt();
            }
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connection_name);
    return result;
}

static void test_add_filter_and_toggle(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);

    NoteEvent *daily = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"完成界面草图");
    NoteEvent *weekly = note_store_add(&store, NOTE_STAGE_WEEK, L"2026-W21", L"完成课程设计");

    expect_int("store count after add", 2, store.count);
    expect_int("first id", 1, daily->id);
    expect_int("second id", 2, weekly->id);

    int count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-17", filtered, 8);
    expect_int("daily filter count", 1, count);
    expect_wstr("daily text", L"完成界面草图", filtered[0]->text);

    expect_int("daily initially open", 0, daily->completed);
    expect_int("toggle daily", 1, note_store_toggle(&store, daily->id));
    expect_int("daily completed", 1, daily->completed);
}

static void test_save_and_load_roundtrip(void) {
    const wchar_t *path = L"note_store_test_data.txt";
    NoteStore store;
    NoteStore loaded;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_MONTH, L"2026-05", L"中文内容 roundtrip");
    note_store_toggle(&store, event->id);

    expect_int("save succeeds", 1, note_store_save(&store, path));

    note_store_init(&loaded);
    expect_int("load succeeds", 1, note_store_load(&loaded, path));
    expect_int("loaded count", 1, loaded.count);

    int count = note_store_filter(&loaded, NOTE_STAGE_MONTH, L"2026-05", filtered, 8);
    expect_int("loaded month count", 1, count);
    expect_int("loaded completed", 1, filtered[0]->completed);
    expect_wstr("loaded text", L"中文内容 roundtrip", filtered[0]->text);

    _wremove(path);
}

static void test_update_text_preserves_event_identity(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_YEAR, L"2026", L"旧内容");
    note_store_toggle(&store, event->id);

    expect_int("update text succeeds", 1, note_store_update_text(&store, event->id, L"新内容"));
    expect_int("update missing id fails", 0, note_store_update_text(&store, 999, L"不存在"));
    expect_int("update empty text fails", 0, note_store_update_text(&store, event->id, L""));

    int count = note_store_filter(&store, NOTE_STAGE_YEAR, L"2026", filtered, 8);
    expect_int("updated filter count", 1, count);
    expect_int("updated id preserved", event->id, filtered[0]->id);
    expect_int("updated completion preserved", 1, filtered[0]->completed);
    expect_wstr("updated text", L"新内容", filtered[0]->text);
}

static void test_completed_events_get_time_and_sink(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *first = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"第一条");
    NoteEvent *second = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"第二条");
    NoteEvent *third = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"第三条");

    expect_int("complete second succeeds", 1, note_store_toggle(&store, second->id));
    expect_int("second completed", 1, second->completed);
    expect_int("completed time set", 1, second->completed_at > 0);

    int count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-17", filtered, 8);
    expect_int("sorted count", 3, count);
    second->completed_at = 500;
    first->created_at = 100;
    third->created_at = 300;

    count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-17", filtered, 8);
    expect_int("newer incomplete first", third->id, filtered[0]->id);
    expect_int("older incomplete second", first->id, filtered[1]->id);
    expect_int("completed sinks below open", second->id, filtered[2]->id);

    expect_int("uncomplete second succeeds", 1, note_store_toggle(&store, second->id));
    expect_int("completed time cleared", 0, second->completed_at);
}

static void test_new_open_events_sort_to_top(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *old_event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"old");
    NoteEvent *new_event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-17", L"new");
    old_event->created_at = 100;
    new_event->created_at = 200;

    int count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-17", filtered, 8);
    expect_int("new first count", 2, count);
    expect_int("new event first", new_event->id, filtered[0]->id);
    expect_int("old event second", old_event->id, filtered[1]->id);

    old_event->created_at = 300;
    new_event->created_at = 300;
    count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-17", filtered, 8);
    expect_int("newer id wins tie", new_event->id, filtered[0]->id);
}

static void test_complete_all_marks_current_stage_only(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *daily_one = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-18", L"daily one");
    NoteEvent *daily_two = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-18", L"daily two");
    NoteEvent *weekly = note_store_add(&store, NOTE_STAGE_WEEK, L"2026-W21", L"weekly");

    expect_int("complete all current stage", 2, note_store_complete_all(&store, NOTE_STAGE_DAY, L"2026-05-18"));
    expect_int("daily one completed", 1, daily_one->completed);
    expect_int("daily two completed", 1, daily_two->completed);
    expect_int("daily one has time", 1, daily_one->completed_at > 0);
    expect_int("weekly untouched", 0, weekly->completed);

    int count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-18", filtered, 8);
    expect_int("complete all filtered count", 2, count);
    expect_int("complete all empty stage", 0, note_store_complete_all(&store, NOTE_STAGE_MONTH, L"2026-05"));
}

static void test_toggle_all_completes_then_uncompletes(void) {
    NoteStore store;

    note_store_init(&store);
    NoteEvent *first = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-18", L"first");
    NoteEvent *second = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-18", L"second");
    NoteEvent *other = note_store_add(&store, NOTE_STAGE_WEEK, L"2026-W21", L"other");

    expect_int("toggle all completes current", 2, note_store_toggle_all(&store, NOTE_STAGE_DAY, L"2026-05-18"));
    expect_int("first completed", 1, first->completed);
    expect_int("second completed", 1, second->completed);
    expect_int("other untouched after complete", 0, other->completed);
    expect_int("first completed time set", 1, first->completed_at > 0);

    expect_int("toggle all uncompletes current", 2, note_store_toggle_all(&store, NOTE_STAGE_DAY, L"2026-05-18"));
    expect_int("first uncompleted", 0, first->completed);
    expect_int("second uncompleted", 0, second->completed);
    expect_int("first completed time cleared", 0, first->completed_at);
    expect_int("other still untouched", 0, other->completed);
}

static void test_restore_deleted_event_preserves_identity(void) {
    NoteStore store;
    NoteEvent *filtered[8];

    note_store_init(&store);
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-18", L"可撤销事件");
    note_store_toggle(&store, event->id);
    NoteEvent snapshot = *event;

    expect_int("delete snapshot source", 1, note_store_delete(&store, snapshot.id));
    expect_int("restore snapshot", 1, note_store_restore(&store, &snapshot));
    expect_int("restore duplicate rejected", 0, note_store_restore(&store, &snapshot));

    int count = note_store_filter(&store, NOTE_STAGE_DAY, L"2026-05-18", filtered, 8);
    expect_int("restored filter count", 1, count);
    expect_int("restored id preserved", snapshot.id, filtered[0]->id);
    expect_int("restored completed preserved", snapshot.completed, filtered[0]->completed);
    expect_int("restored completed time preserved", snapshot.completed_at, filtered[0]->completed_at);
    expect_wstr("restored text preserved", L"可撤销事件", filtered[0]->text);
}

static void test_store_grows_beyond_legacy_stack_limit(void) {
    const int legacy_stack_limit = 512;
    NoteStore store;
    note_store_init(&store);

    for (int i = 0; i < legacy_stack_limit + 24; ++i) {
        wchar_t text[64];
        swprintf(text, 64, L"bulk-%d", i);
        NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-24", text);
        if (event == NULL) {
            printf("FAIL dynamic add stopped at %d\n", i);
            failures++;
            break;
        }
    }

    expect_int("dynamic store count", legacy_stack_limit + 24, store.count);
    expect_int("production planning capacity", 10000, NOTE_MAX_EVENTS);
}

static void test_text_limit_rejects_overflow_without_mutating_store(void) {
    expect_int("text capacity includes terminator", 65537, NOTE_TEXT_MAX);

    NoteStore store;
    note_store_init(&store);
    const std::wstring maximum_text((size_t)NOTE_TEXT_MAX - 1, L'x');
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", maximum_text.c_str());
    expect_true("maximum text accepted", event != NULL);
    if (event == NULL) {
        return;
    }
    expect_int("maximum text preserved", NOTE_TEXT_MAX - 1, (int)wcslen(event->text));

    const int count_before = store.count;
    const int next_id_before = store.next_id;
    const std::wstring oversized_text((size_t)NOTE_TEXT_MAX, L'y');
    expect_true("oversized add rejected",
                note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", oversized_text.c_str()) == NULL);
    expect_int("oversized add preserves count", count_before, store.count);
    expect_int("oversized add preserves next id", next_id_before, store.next_id);
    expect_last_error("oversized add explains failure");

    expect_int("oversized update rejected", 0,
               note_store_update_text(&store, event->id, oversized_text.c_str()));
    expect_int("oversized update preserves text", NOTE_TEXT_MAX - 1, (int)wcslen(event->text));
    expect_last_error("oversized update explains failure");
}

static void test_long_text_and_series_roundtrip_across_persistence_formats(void) {
    QTemporaryDir directory;
    expect_true("long roundtrip temporary directory", directory.isValid());
    if (!directory.isValid()) {
        return;
    }

    NoteStore store;
    note_store_init(&store);
    const std::wstring long_text((size_t)NOTE_TEXT_MAX - 1, L'z');
    NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", long_text.c_str());
    expect_true("long roundtrip event created", event != NULL);
    if (event == NULL) {
        return;
    }
    expect_int("explicit series accepted", 1,
               note_store_set_series_id(&store, event->id, L"series-fixed-001"));

    const std::wstring sqlite_path = directory.filePath(QStringLiteral("notes.sqlite")).toStdWString();
    printf("  persistence: sqlite save\n"); fflush(stdout);
    expect_int("long sqlite save", 1, note_store_save_sqlite(&store, sqlite_path.c_str()));
    NoteStore sqlite_loaded;
    note_store_init(&sqlite_loaded);
    printf("  persistence: sqlite load\n"); fflush(stdout);
    expect_int("long sqlite load", 1, note_store_load_sqlite(&sqlite_loaded, sqlite_path.c_str()));
    expect_int("long sqlite count", 1, sqlite_loaded.count);
    if (sqlite_loaded.count == 1) {
        expect_int("long sqlite text preserved", NOTE_TEXT_MAX - 1,
                   (int)wcslen(sqlite_loaded.items[0].text));
        expect_wstr("sqlite series preserved", L"series-fixed-001", sqlite_loaded.items[0].series_id);
    }

    const std::wstring text_path = directory.filePath(QStringLiteral("notes.db.txt")).toStdWString();
    printf("  persistence: flat save\n"); fflush(stdout);
    expect_int("long legacy save", 1, note_store_save(&store, text_path.c_str()));
    NoteStore text_loaded;
    note_store_init(&text_loaded);
    printf("  persistence: flat load\n"); fflush(stdout);
    expect_int("long legacy load", 1, note_store_load(&text_loaded, text_path.c_str()));
    expect_int("long legacy count", 1, text_loaded.count);
    if (text_loaded.count == 1) {
        expect_int("long legacy text preserved", NOTE_TEXT_MAX - 1,
                   (int)wcslen(text_loaded.items[0].text));
        expect_wstr("legacy series preserved", L"series-fixed-001", text_loaded.items[0].series_id);
    }
}

static void test_legacy_sqlite_migrates_to_schema_v2_with_stable_series(void) {
    QTemporaryDir directory;
    expect_true("migration temporary directory", directory.isValid());
    if (!directory.isValid()) {
        return;
    }
    const QString path = directory.filePath(QStringLiteral("legacy.sqlite"));
    expect_true("legacy database created", execute_sql(path, {
        QStringLiteral("CREATE TABLE notes ("
                       "id INTEGER PRIMARY KEY, stage INTEGER NOT NULL, date_key TEXT NOT NULL, "
                       "text TEXT NOT NULL, completed INTEGER NOT NULL, completed_at INTEGER NOT NULL, "
                       "created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, "
                       "repeat TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("INSERT INTO notes VALUES "
                       "(7, 0, '2026-07-12', 'legacy recurring note', 0, 0, 10, 10, 'daily')"),
        QStringLiteral("PRAGMA user_version=1")
    }));

    NoteStore loaded;
    note_store_init(&loaded);
    const std::wstring wide_path = path.toStdWString();
    expect_int("legacy schema loads", 1, note_store_load_sqlite(&loaded, wide_path.c_str()));
    expect_int("legacy schema row preserved", 1, loaded.count);
    expect_int("schema migrated to v2", 2, sqlite_scalar_int(path, QStringLiteral("PRAGMA user_version")));
    expect_int("series index created", 1, sqlite_scalar_int(
        path,
        QStringLiteral("SELECT count(*) FROM sqlite_master "
                       "WHERE type='index' AND name='idx_notes_series_id'")));
    const QFileInfo migrated_database(path);
    const QStringList backups = QDir(migrated_database.absolutePath()).entryList(
        {migrated_database.fileName() + QStringLiteral(".pre-v2-*.bak")},
        QDir::Files);
    expect_int("migration creates one sidecar backup", 1, backups.size());
    if (backups.size() == 1) {
        const QString backup_path = migrated_database.absoluteDir().filePath(backups.front());
        expect_int("migration backup keeps old schema", 1,
                   sqlite_scalar_int(backup_path, QStringLiteral("PRAGMA user_version")));
        expect_int("migration backup keeps old row", 1,
                   sqlite_scalar_int(backup_path, QStringLiteral("SELECT count(*) FROM notes")));
    }
    if (loaded.count != 1) {
        return;
    }
    expect_true("migrated series assigned", loaded.items[0].series_id[0] != L'\0');
    const std::wstring migrated_series = loaded.items[0].series_id;

    NoteStore loaded_again;
    note_store_init(&loaded_again);
    expect_int("migrated schema reloads", 1, note_store_load_sqlite(&loaded_again, wide_path.c_str()));
    expect_int("migrated series row still present", 1, loaded_again.count);
    if (loaded_again.count == 1) {
        expect_wstr("migrated series remains stable", migrated_series.c_str(),
                    loaded_again.items[0].series_id);
    }
}

static void test_failed_snapshot_save_rolls_back_delete_and_reports_error(void) {
    QTemporaryDir directory;
    expect_true("rollback temporary directory", directory.isValid());
    if (!directory.isValid()) {
        return;
    }
    const QString path = directory.filePath(QStringLiteral("rollback.sqlite"));
    const std::wstring wide_path = path.toStdWString();

    NoteStore original;
    note_store_init(&original);
    note_store_add(&original, NOTE_STAGE_DAY, L"2026-07-12", L"must survive");
    expect_int("rollback fixture saved", 1, note_store_save_sqlite(&original, wide_path.c_str()));
    expect_true("delete failure trigger created", execute_sql(path, {
        QStringLiteral("CREATE TRIGGER prevent_note_delete BEFORE DELETE ON notes "
                       "BEGIN SELECT RAISE(ABORT, 'blocked delete'); END")
    }));
    expect_int("same-id update mutates without clearing table", 1,
               note_store_update_text(&original, original.items[0].id, L"updated in place"));
    expect_int("upsert succeeds while delete is blocked", 1,
               note_store_save_sqlite(&original, wide_path.c_str()));

    NoteStore replacement;
    note_store_init(&replacement);
    NoteEvent *replacement_event = note_store_add(
        &replacement, NOTE_STAGE_DAY, L"2026-07-12", L"replacement");
    replacement_event->id = 2;
    replacement.next_id = 3;
    expect_int("snapshot failure reported", 0, note_store_save_sqlite(&replacement, wide_path.c_str()));
    expect_last_error("snapshot failure has diagnostic");

    NoteStore recovered;
    note_store_init(&recovered);
    expect_int("old snapshot remains loadable", 1, note_store_load_sqlite(&recovered, wide_path.c_str()));
    expect_int("old snapshot row remains", 1, recovered.count);
    if (recovered.count == 1) {
        expect_wstr("last committed snapshot remains", L"updated in place", recovered.items[0].text);
    }
}

static void test_failed_load_preserves_existing_memory_and_connections_are_removed(void) {
    QTemporaryDir directory;
    expect_true("failed load temporary directory", directory.isValid());
    if (!directory.isValid()) {
        return;
    }
    const QString invalid_path = directory.filePath(QStringLiteral("invalid.sqlite"));
    expect_true("invalid schema fixture created", execute_sql(invalid_path, {
        QStringLiteral("CREATE TABLE notes (id INTEGER PRIMARY KEY)"),
        QStringLiteral("PRAGMA user_version=1")
    }));

    NoteStore store;
    note_store_init(&store);
    note_store_add(&store, NOTE_STAGE_DAY, L"2026-07-12", L"keep in memory");
    const std::wstring invalid_wide_path = invalid_path.toStdWString();
    expect_int("invalid load rejected", 0, note_store_load_sqlite(&store, invalid_wide_path.c_str()));
    expect_int("invalid load preserves count", 1, store.count);
    expect_wstr("invalid load preserves content", L"keep in memory", store.items[0].text);
    expect_last_error("invalid load has diagnostic");

    const int connections_before = QSqlDatabase::connectionNames().size();
    const std::wstring valid_path = directory.filePath(QStringLiteral("valid.sqlite")).toStdWString();
    expect_int("connection cleanup save", 1, note_store_save_sqlite(&store, valid_path.c_str()));
    NoteStore reloaded;
    note_store_init(&reloaded);
    expect_int("connection cleanup load", 1, note_store_load_sqlite(&reloaded, valid_path.c_str()));
    expect_int("store connections removed", connections_before, QSqlDatabase::connectionNames().size());
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    printf("RUN baseline store tests\n");
    fflush(stdout);
    test_add_filter_and_toggle();
    test_save_and_load_roundtrip();
    test_update_text_preserves_event_identity();
    test_completed_events_get_time_and_sink();
    test_new_open_events_sort_to_top();
    test_complete_all_marks_current_stage_only();
    test_toggle_all_completes_then_uncompletes();
    test_restore_deleted_event_preserves_identity();
    test_store_grows_beyond_legacy_stack_limit();
    printf("RUN text limit\n");
    fflush(stdout);
    test_text_limit_rejects_overflow_without_mutating_store();
    printf("RUN persistence roundtrip\n");
    fflush(stdout);
    test_long_text_and_series_roundtrip_across_persistence_formats();
    printf("RUN sqlite migration\n");
    fflush(stdout);
    test_legacy_sqlite_migrates_to_schema_v2_with_stable_series();
    printf("RUN sqlite rollback\n");
    fflush(stdout);
    test_failed_snapshot_save_rolls_back_delete_and_reports_error();
    printf("RUN load isolation and cleanup\n");
    fflush(stdout);
    test_failed_load_preserves_existing_memory_and_connections_are_removed();

    if (failures != 0) {
        printf("%d note_store test(s) failed\n", failures);
        return 1;
    }

    printf("note_store tests passed\n");
    return 0;
}
