#include "note_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QUuid>

#define LINE_MAX_BYTES 4096
#define FIELD_MAX_BYTES 2048

static long long now_seconds(void) {
    return (long long)time(NULL);
}

static void copy_wstr(wchar_t *dest, size_t dest_count, const wchar_t *src) {
    if (dest_count == 0) {
        return;
    }
    if (src == NULL) {
        dest[0] = L'\0';
        return;
    }
    wcsncpy(dest, src, dest_count - 1);
    dest[dest_count - 1] = L'\0';
}

static int wide_to_utf8(const wchar_t *src, char *dest, int dest_count) {
    if (dest_count <= 0) {
        return 0;
    }
    dest[0] = '\0';
    if (src == NULL) {
        return 1;
    }
    const QByteArray bytes = QString::fromWCharArray(src).toUtf8();
    if (bytes.size() + 1 > dest_count) {
        return 0;
    }
    memcpy(dest, bytes.constData(), (size_t)bytes.size());
    dest[bytes.size()] = '\0';
    return 1;
}

static int utf8_to_wide(const char *src, wchar_t *dest, int dest_count) {
    if (dest_count <= 0) {
        return 0;
    }
    dest[0] = L'\0';
    if (src == NULL) {
        return 1;
    }
    const QString text = QString::fromUtf8(src);
    if (text.size() + 1 > dest_count) {
        return 0;
    }
    const int copied = text.toWCharArray(dest);
    dest[copied] = L'\0';
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static void percent_encode(const char *src, char *dest, size_t dest_count) {
    static const char *hex = "0123456789ABCDEF";
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dest_count; i++) {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '%' || ch == '\t' || ch == '\r' || ch == '\n' || ch < 32) {
            if (out + 3 >= dest_count) {
                break;
            }
            dest[out++] = '%';
            dest[out++] = hex[ch >> 4];
            dest[out++] = hex[ch & 15];
        } else {
            dest[out++] = (char)ch;
        }
    }
    dest[out] = '\0';
}

static void percent_decode(const char *src, char *dest, size_t dest_count) {
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dest_count; i++) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);
            dest[out++] = (char)((hi << 4) | lo);
            i += 2;
        } else {
            dest[out++] = src[i];
        }
    }
    dest[out] = '\0';
}

static long long note_sort_time(const NoteEvent *event) {
    if (event == NULL) {
        return 0;
    }
    return event->completed && event->completed_at > 0 ? event->completed_at : event->created_at;
}

static int note_should_sort_after(const NoteEvent *left, const NoteEvent *right) {
    if (left == NULL || right == NULL) {
        return 0;
    }
    if (left->completed != right->completed) {
        return left->completed > right->completed;
    }

    long long left_time = note_sort_time(left);
    long long right_time = note_sort_time(right);
    return left_time < right_time ||
           (left_time == right_time && left->id < right->id);
}

void note_store_init(NoteStore *store) {
    if (store == NULL) {
        return;
    }
    store->items.clear();
    store->count = 0;
    store->next_id = 1;
}

NoteEvent *note_store_add(NoteStore *store, NoteStage stage, const wchar_t *date_key, const wchar_t *text) {
    if (store == NULL || text == NULL || text[0] == L'\0') {
        return NULL;
    }

    NoteEvent created{};
    NoteEvent *event = &created;
    event->id = store->next_id++;
    event->stage = stage;
    copy_wstr(event->date_key, NOTE_KEY_MAX, date_key);
    copy_wstr(event->text, NOTE_TEXT_MAX, text);
    event->completed = 0;
    event->created_at = now_seconds();
    event->updated_at = event->created_at;
    store->items.push_back(created);
    store->count = store->items.size();
    return &store->items.back();
}

int note_store_update_text(NoteStore *store, int id, const wchar_t *text) {
    if (store == NULL || text == NULL || text[0] == L'\0') {
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            copy_wstr(store->items[i].text, NOTE_TEXT_MAX, text);
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    return 0;
}

int note_store_set_repeat(NoteStore *store, int id, const wchar_t *repeat) {
    if (store == NULL || repeat == NULL) {
        return 0;
    }
    if (wcscmp(repeat, L"") != 0 &&
        wcscmp(repeat, L"daily") != 0 &&
        wcscmp(repeat, L"weekly") != 0 &&
        wcscmp(repeat, L"monthly") != 0 &&
        wcscmp(repeat, L"yearly") != 0) {
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            copy_wstr(store->items[i].repeat, NOTE_REPEAT_MAX, repeat);
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    return 0;
}

int note_store_toggle(NoteStore *store, int id) {
    if (store == NULL) {
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            store->items[i].completed = !store->items[i].completed;
            store->items[i].completed_at = store->items[i].completed ? now_seconds() : 0;
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    return 0;
}

int note_store_complete_all(NoteStore *store, NoteStage stage, const wchar_t *date_key) {
    if (store == NULL || date_key == NULL) {
        return 0;
    }

    int changed = 0;
    long long completed_time = now_seconds();
    for (int i = 0; i < store->count; i++) {
        NoteEvent *event = &store->items[i];
        if (event->stage == stage && wcscmp(event->date_key, date_key) == 0 && !event->completed) {
            event->completed = 1;
            event->completed_at = completed_time;
            event->updated_at = completed_time;
            changed++;
        }
    }
    return changed;
}

int note_store_toggle_all(NoteStore *store, NoteStage stage, const wchar_t *date_key) {
    if (store == NULL || date_key == NULL) {
        return 0;
    }

    int total = 0;
    int open_count = 0;
    for (int i = 0; i < store->count; i++) {
        NoteEvent *event = &store->items[i];
        if (event->stage == stage && wcscmp(event->date_key, date_key) == 0) {
            total++;
            if (!event->completed) {
                open_count++;
            }
        }
    }
    if (total == 0) {
        return 0;
    }

    int should_complete = open_count > 0;
    int changed = 0;
    long long timestamp = now_seconds();
    for (int i = 0; i < store->count; i++) {
        NoteEvent *event = &store->items[i];
        if (event->stage == stage && wcscmp(event->date_key, date_key) == 0 && event->completed != should_complete) {
            event->completed = should_complete;
            event->completed_at = should_complete ? timestamp : 0;
            event->updated_at = timestamp;
            changed++;
        }
    }
    return changed;
}

int note_store_delete(NoteStore *store, int id) {
    if (store == NULL) {
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            store->items.erase(store->items.begin() + i);
            store->count = store->items.size();
            return 1;
        }
    }
    return 0;
}

int note_store_restore(NoteStore *store, const NoteEvent *event) {
    if (store == NULL || event == NULL || event->id <= 0) {
        return 0;
    }
    if (event->stage < NOTE_STAGE_DAY || event->stage > NOTE_STAGE_YEAR ||
        event->date_key[0] == L'\0' || event->text[0] == L'\0') {
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == event->id) {
            return 0;
        }
    }

    store->items.push_back(*event);
    store->count = store->items.size();
    if (event->id >= store->next_id) {
        store->next_id = event->id + 1;
    }
    return 1;
}

int note_store_filter(NoteStore *store, NoteStage stage, const wchar_t *date_key, NoteEvent **out, int out_capacity) {
    if (store == NULL || out == NULL || out_capacity <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < store->count && count < out_capacity; i++) {
        NoteEvent *event = &store->items[i];
        if (event->stage == stage && wcscmp(event->date_key, date_key) == 0) {
            out[count++] = event;
        }
    }

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (note_should_sort_after(out[i], out[j])) {
                NoteEvent *tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }
    return count;
}

int note_store_save(const NoteStore *store, const wchar_t *path) {
    if (store == NULL || path == NULL) {
        return 0;
    }

    FILE *file = _wfopen(path, L"wb");
    if (file == NULL) {
        return 0;
    }

    fprintf(file, "STICKY_NOTES_C_V2\t%d\t%d\n", store->count, store->next_id);
    for (int i = 0; i < store->count; i++) {
        const NoteEvent *event = &store->items[i];
        char key_utf8[FIELD_MAX_BYTES];
        char text_utf8[FIELD_MAX_BYTES];
        char key_encoded[FIELD_MAX_BYTES * 3];
        char text_encoded[FIELD_MAX_BYTES * 3];

        if (!wide_to_utf8(event->date_key, key_utf8, (int)sizeof(key_utf8)) ||
            !wide_to_utf8(event->text, text_utf8, (int)sizeof(text_utf8))) {
            fclose(file);
            return 0;
        }

        percent_encode(key_utf8, key_encoded, sizeof(key_encoded));
        percent_encode(text_utf8, text_encoded, sizeof(text_encoded));

        char repeat_utf8[FIELD_MAX_BYTES];
        char repeat_encoded[FIELD_MAX_BYTES * 3];
        if (!wide_to_utf8(event->repeat, repeat_utf8, (int)sizeof(repeat_utf8))) {
            fclose(file);
            return 0;
        }
        percent_encode(repeat_utf8, repeat_encoded, sizeof(repeat_encoded));

        fprintf(file, "%d\t%d\t%s\t%d\t%lld\t%lld\t%lld\t%s\t%s\n",
                event->id,
                (int)event->stage,
                key_encoded,
                event->completed,
                event->completed_at,
                event->created_at,
                event->updated_at,
                text_encoded,
                repeat_encoded);
    }

    fclose(file);
    return 1;
}

int note_store_load(NoteStore *store, const wchar_t *path) {
    if (store == NULL || path == NULL) {
        return 0;
    }

    FILE *file = _wfopen(path, L"rb");
    if (file == NULL) {
        return 0;
    }

    note_store_init(store);

    char line[LINE_MAX_BYTES];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 1;
    }

    int expected_count = 0;
    int next_id = 1;
    int version = 0;
    if (sscanf(line, "STICKY_NOTES_C_V2\t%d\t%d", &expected_count, &next_id) == 2) {
        version = 2;
    } else if (sscanf(line, "STICKY_NOTES_C_V1\t%d\t%d", &expected_count, &next_id) == 2) {
        version = 1;
    } else {
        fclose(file);
        return 0;
    }
    store->next_id = next_id > 0 ? next_id : 1;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[9] = {0};
        int field_count = 0;
        char *cursor = line;

        line[strcspn(line, "\r\n")] = '\0';
        while (field_count < 9) {
            fields[field_count++] = cursor;
            char *tab = strchr(cursor, '\t');
            if (tab == NULL) {
                break;
            }
            *tab = '\0';
            cursor = tab + 1;
        }
        if ((version == 2 && field_count < 8) || (version == 1 && field_count != 7)) {
            continue;
        }

        NoteEvent parsed{};
        NoteEvent *event = &parsed;
        event->id = atoi(fields[0]);
        event->stage = (NoteStage)atoi(fields[1]);
        event->completed = atoi(fields[3]) ? 1 : 0;
        if (version == 2) {
            event->completed_at = _strtoi64(fields[4], NULL, 10);
            event->created_at = _strtoi64(fields[5], NULL, 10);
            event->updated_at = _strtoi64(fields[6], NULL, 10);
        } else {
            event->completed_at = 0;
            event->created_at = _strtoi64(fields[4], NULL, 10);
            event->updated_at = _strtoi64(fields[5], NULL, 10);
        }

        char key_utf8[FIELD_MAX_BYTES];
        char text_utf8[FIELD_MAX_BYTES];
        percent_decode(fields[2], key_utf8, sizeof(key_utf8));
        percent_decode(version == 2 ? fields[7] : fields[6], text_utf8, sizeof(text_utf8));
        utf8_to_wide(key_utf8, event->date_key, NOTE_KEY_MAX);
        utf8_to_wide(text_utf8, event->text, NOTE_TEXT_MAX);
        if (version == 2 && field_count >= 9) {
            char repeat_utf8[FIELD_MAX_BYTES];
            percent_decode(fields[8], repeat_utf8, sizeof(repeat_utf8));
            utf8_to_wide(repeat_utf8, event->repeat, NOTE_REPEAT_MAX);
        }

        if (event->id > 0 && event->stage >= NOTE_STAGE_DAY && event->stage <= NOTE_STAGE_YEAR) {
            store->items.push_back(*event);
            store->count = store->items.size();
            if (event->id >= store->next_id) {
                store->next_id = event->id + 1;
            }
        }
    }

    fclose(file);
    return 1;
}

static QString wide_path(const wchar_t *path) {
    return path == NULL ? QString() : QString::fromWCharArray(path);
}

static QString wide_value(const wchar_t *value) {
    return value == NULL ? QString() : QString::fromWCharArray(value);
}

static void copy_qstring_wide(wchar_t *dest, size_t dest_count, const QString &value) {
    const std::wstring wide = value.toStdWString();
    copy_wstr(dest, dest_count, wide.c_str());
}

static QSqlDatabase open_notes_database(const wchar_t *path) {
    const QString connection = QStringLiteral("notes_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(wide_path(path));
    if (!db.open()) {
        return db;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    return db;
}

int note_store_save_sqlite(const NoteStore *store, const wchar_t *path) {
    if (store == NULL || path == NULL) {
        return 0;
    }
    QSqlDatabase db = open_notes_database(path);
    if (!db.isOpen()) {
        return 0;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS notes ("
            "id INTEGER PRIMARY KEY,"
            "stage INTEGER NOT NULL,"
            "date_key TEXT NOT NULL,"
            "text TEXT NOT NULL,"
            "completed INTEGER NOT NULL,"
            "completed_at INTEGER NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL,"
            "repeat TEXT NOT NULL DEFAULT '')"))) {
        db.close();
        return 0;
    }
    query.exec(QStringLiteral("DELETE FROM notes"));
    query.prepare(QStringLiteral(
        "INSERT INTO notes (id, stage, date_key, text, completed, completed_at, created_at, updated_at, repeat) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    db.transaction();
    for (int i = 0; i < store->count; ++i) {
        const NoteEvent *event = &store->items[i];
        query.bindValue(0, event->id);
        query.bindValue(1, (int)event->stage);
        query.bindValue(2, wide_value(event->date_key));
        query.bindValue(3, wide_value(event->text));
        query.bindValue(4, event->completed);
        query.bindValue(5, QVariant::fromValue<qlonglong>(event->completed_at));
        query.bindValue(6, QVariant::fromValue<qlonglong>(event->created_at));
        query.bindValue(7, QVariant::fromValue<qlonglong>(event->updated_at));
        query.bindValue(8, wide_value(event->repeat));
        if (!query.exec()) {
            db.rollback();
            db.close();
            return 0;
        }
    }
    db.commit();
    db.close();
    return 1;
}

int note_store_load_sqlite(NoteStore *store, const wchar_t *path) {
    if (store == NULL || path == NULL) {
        return 0;
    }
    QSqlDatabase db = open_notes_database(path);
    if (!db.isOpen()) {
        return 0;
    }
    QSqlQuery create(db);
    if (!create.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS notes ("
            "id INTEGER PRIMARY KEY,"
            "stage INTEGER NOT NULL,"
            "date_key TEXT NOT NULL,"
            "text TEXT NOT NULL,"
            "completed INTEGER NOT NULL,"
            "completed_at INTEGER NOT NULL,"
            "created_at INTEGER NOT NULL,"
            "updated_at INTEGER NOT NULL,"
            "repeat TEXT NOT NULL DEFAULT '')"))) {
        db.close();
        return 0;
    }

    note_store_init(store);
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT id, stage, date_key, text, completed, completed_at, created_at, updated_at, repeat FROM notes ORDER BY id"))) {
        db.close();
        return 0;
    }
    while (query.next()) {
        NoteEvent event{};
        event.id = query.value(0).toInt();
        event.stage = (NoteStage)query.value(1).toInt();
        copy_qstring_wide(event.date_key, NOTE_KEY_MAX, query.value(2).toString());
        copy_qstring_wide(event.text, NOTE_TEXT_MAX, query.value(3).toString());
        event.completed = query.value(4).toInt() ? 1 : 0;
        event.completed_at = query.value(5).toLongLong();
        event.created_at = query.value(6).toLongLong();
        event.updated_at = query.value(7).toLongLong();
        copy_qstring_wide(event.repeat, NOTE_REPEAT_MAX, query.value(8).toString());
        if (event.id > 0 && event.stage >= NOTE_STAGE_DAY && event.stage <= NOTE_STAGE_YEAR && event.text[0] != L'\0') {
            store->items.push_back(event);
            store->count = store->items.size();
            if (event.id >= store->next_id) {
                store->next_id = event.id + 1;
            }
        }
    }
    db.close();
    return 1;
}

const wchar_t *note_stage_label(NoteStage stage) {
    switch (stage) {
        case NOTE_STAGE_DAY:
            return L"每天";
        case NOTE_STAGE_WEEK:
            return L"每周";
        case NOTE_STAGE_MONTH:
            return L"每月";
        case NOTE_STAGE_YEAR:
            return L"每年";
        default:
            return L"未知";
    }
}
