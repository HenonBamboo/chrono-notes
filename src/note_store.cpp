#include "note_store.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <utility>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringDecoder>
#include <QStringList>
#include <QVariant>
#include <QUuid>

#define LINE_MAX_BYTES 4096
#define FIELD_MAX_BYTES 2048

namespace {

thread_local std::wstring g_last_error;

void clear_last_error() {
    g_last_error.clear();
}

void set_last_error(const wchar_t *message) {
    g_last_error = message == NULL ? L"Unknown note store error" : message;
}

void set_last_error(const QString &message) {
    g_last_error = message.toStdWString();
}

bool wide_value_fits(const wchar_t *value, size_t capacity, bool allow_empty) {
    if (value == NULL || capacity == 0) {
        return false;
    }
    size_t length = 0;
    while (length < capacity && value[length] != L'\0') {
        ++length;
    }
    return length < capacity && (allow_empty || length > 0);
}

bool valid_stage(NoteStage stage) {
    return stage >= NOTE_STAGE_DAY && stage <= NOTE_STAGE_YEAR;
}

bool valid_repeat(const wchar_t *repeat) {
    return wide_value_fits(repeat, NOTE_REPEAT_MAX, true) &&
           (wcscmp(repeat, L"") == 0 ||
            wcscmp(repeat, L"daily") == 0 ||
            wcscmp(repeat, L"weekly") == 0 ||
            wcscmp(repeat, L"monthly") == 0 ||
            wcscmp(repeat, L"yearly") == 0);
}

bool copy_wstr_checked(wchar_t *dest, size_t dest_count, const wchar_t *src, bool allow_empty) {
    if (dest == NULL || !wide_value_fits(src, dest_count, allow_empty)) {
        return false;
    }
    const size_t length = wcslen(src);
    wmemcpy(dest, src, length + 1);
    return true;
}

bool assign_new_series_id(NoteEvent *event) {
    if (event == NULL) {
        return false;
    }
    const std::wstring series = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString();
    return copy_wstr_checked(event->series_id, NOTE_SERIES_ID_MAX, series.c_str(), false);
}

bool validate_event(const NoteEvent &event, bool require_series) {
    return event.id > 0 &&
           valid_stage(event.stage) &&
           wide_value_fits(event.date_key, NOTE_KEY_MAX, false) &&
           wide_value_fits(event.text, NOTE_TEXT_MAX, false) &&
           valid_repeat(event.repeat) &&
           (wide_value_fits(event.series_id, NOTE_SERIES_ID_MAX, false) ||
            (!require_series && event.series_id[0] == L'\0'));
}

}  // namespace

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

static QByteArray percent_encode_bytes(const QByteArray &source) {
    static const char *hex = "0123456789ABCDEF";
    QByteArray encoded;
    encoded.reserve(source.size());
    for (const char raw : source) {
        const unsigned char ch = (unsigned char)raw;
        if (ch == '%' || ch == '\t' || ch == '\r' || ch == '\n' || ch < 32) {
            encoded.append('%');
            encoded.append(hex[ch >> 4]);
            encoded.append(hex[ch & 15]);
        } else {
            encoded.append(raw);
        }
    }
    return encoded;
}

static bool percent_decode_bytes(const QByteArray &source, QByteArray *decoded) {
    if (decoded == NULL) {
        return false;
    }
    decoded->clear();
    decoded->reserve(source.size());
    for (qsizetype i = 0; i < source.size(); ++i) {
        if (source.at(i) != '%') {
            decoded->append(source.at(i));
            continue;
        }
        if (i + 2 >= source.size()) {
            return false;
        }
        const int hi = hex_value(source.at(i + 1));
        const int lo = hex_value(source.at(i + 2));
        if (hi < 0 || lo < 0) {
            return false;
        }
        decoded->append((char)((hi << 4) | lo));
        i += 2;
    }
    return true;
}

static QByteArray encode_wide_field(const wchar_t *value) {
    return percent_encode_bytes(QString::fromWCharArray(value).toUtf8());
}

static bool decode_wide_field(const QByteArray &encoded,
                              wchar_t *destination,
                              size_t destination_count,
                              bool allow_empty) {
    QByteArray utf8;
    if (!percent_decode_bytes(encoded, &utf8) || utf8.contains('\0')) {
        return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString decoded = decoder.decode(utf8);
    if (decoder.hasError()) {
        return false;
    }
    const std::wstring wide = decoded.toStdWString();
    return copy_wstr_checked(destination, destination_count, wide.c_str(), allow_empty);
}

static void trim_line_ending(QByteArray *line) {
    while (line != NULL && !line->isEmpty() &&
           (line->endsWith('\n') || line->endsWith('\r'))) {
        line->chop(1);
    }
}

static bool write_all(QSaveFile *file, const QByteArray &data) {
    return file != NULL && file->write(data) == data.size();
}

static bool parse_int_field(const QByteArray &field, int *value) {
    bool ok = false;
    const int parsed = field.toInt(&ok);
    if (ok && value != NULL) {
        *value = parsed;
    }
    return ok;
}

static bool parse_long_long_field(const QByteArray &field, long long *value) {
    bool ok = false;
    const qlonglong parsed = field.toLongLong(&ok);
    if (ok && value != NULL) {
        *value = (long long)parsed;
    }
    return ok;
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
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return;
    }
    store->items.clear();
    store->count = 0;
    store->next_id = 1;
}

NoteEvent *note_store_add(NoteStore *store, NoteStage stage, const wchar_t *date_key, const wchar_t *text) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return NULL;
    }
    if (!valid_stage(stage)) {
        set_last_error(L"Stage is invalid");
        return NULL;
    }
    if (!wide_value_fits(date_key, NOTE_KEY_MAX, false)) {
        set_last_error(L"Date key is empty or too long");
        return NULL;
    }
    if (!wide_value_fits(text, NOTE_TEXT_MAX, false)) {
        set_last_error(L"Note text is empty or exceeds 65536 characters");
        return NULL;
    }

    NoteEvent created{};
    NoteEvent *event = &created;
    event->id = store->next_id;
    event->stage = stage;
    if (!copy_wstr_checked(event->date_key, NOTE_KEY_MAX, date_key, false) ||
        !copy_wstr_checked(event->text, NOTE_TEXT_MAX, text, false) ||
        !assign_new_series_id(event)) {
        set_last_error(L"Failed to initialise note fields");
        return NULL;
    }
    event->completed = 0;
    event->created_at = now_seconds();
    event->updated_at = event->created_at;
    store->items.push_back(created);
    store->next_id++;
    store->count = store->items.size();
    return &store->items.back();
}

int note_store_update_text(NoteStore *store, int id, const wchar_t *text) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    if (!wide_value_fits(text, NOTE_TEXT_MAX, false)) {
        set_last_error(L"Note text is empty or exceeds 65536 characters");
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            copy_wstr_checked(store->items[i].text, NOTE_TEXT_MAX, text, false);
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    set_last_error(L"Note was not found");
    return 0;
}

int note_store_set_repeat(NoteStore *store, int id, const wchar_t *repeat) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    if (!valid_repeat(repeat)) {
        set_last_error(L"Repeat rule is invalid or too long");
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            copy_wstr_checked(store->items[i].repeat, NOTE_REPEAT_MAX, repeat, true);
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    set_last_error(L"Note was not found");
    return 0;
}

int note_store_set_series_id(NoteStore *store, int id, const wchar_t *series_id) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    if (!wide_value_fits(series_id, NOTE_SERIES_ID_MAX, false)) {
        set_last_error(L"Series id is empty or too long");
        return 0;
    }
    for (int i = 0; i < store->count; ++i) {
        if (store->items[i].id == id) {
            copy_wstr_checked(store->items[i].series_id, NOTE_SERIES_ID_MAX, series_id, false);
            store->items[i].updated_at = now_seconds();
            return 1;
        }
    }
    set_last_error(L"Note was not found");
    return 0;
}

int note_store_toggle(NoteStore *store, int id) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
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
    set_last_error(L"Note was not found");
    return 0;
}

int note_store_complete_all(NoteStore *store, NoteStage stage, const wchar_t *date_key) {
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    if (!valid_stage(stage) || !wide_value_fits(date_key, NOTE_KEY_MAX, false)) {
        set_last_error(L"Stage or date key is invalid");
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
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    if (!valid_stage(stage) || !wide_value_fits(date_key, NOTE_KEY_MAX, false)) {
        set_last_error(L"Stage or date key is invalid");
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
    clear_last_error();
    if (store == NULL) {
        set_last_error(L"Store is null");
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == id) {
            store->items.erase(store->items.begin() + i);
            store->count = store->items.size();
            return 1;
        }
    }
    set_last_error(L"Note was not found");
    return 0;
}

int note_store_restore(NoteStore *store, const NoteEvent *event) {
    clear_last_error();
    if (store == NULL || event == NULL || event->id <= 0) {
        set_last_error(L"Store or note snapshot is invalid");
        return 0;
    }
    if (!validate_event(*event, false)) {
        set_last_error(L"Note snapshot contains invalid or oversized fields");
        return 0;
    }
    for (int i = 0; i < store->count; i++) {
        if (store->items[i].id == event->id) {
            set_last_error(L"A note with this id already exists");
            return 0;
        }
    }

    NoteEvent restored = *event;
    if (restored.series_id[0] == L'\0' && !assign_new_series_id(&restored)) {
        set_last_error(L"Failed to assign a series id");
        return 0;
    }
    store->items.push_back(restored);
    store->count = store->items.size();
    if (event->id >= store->next_id) {
        store->next_id = event->id + 1;
    }
    return 1;
}

int note_store_filter(NoteStore *store, NoteStage stage, const wchar_t *date_key, NoteEvent **out, int out_capacity) {
    clear_last_error();
    if (store == NULL || out == NULL || out_capacity <= 0) {
        set_last_error(L"Filter arguments are invalid");
        return 0;
    }
    if (!valid_stage(stage) || !wide_value_fits(date_key, NOTE_KEY_MAX, false)) {
        set_last_error(L"Stage or date key is invalid");
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
    clear_last_error();
    if (store == NULL || path == NULL || path[0] == L'\0') {
        set_last_error(L"Store or path is invalid");
        return 0;
    }

    for (const NoteEvent &event : store->items) {
        if (!validate_event(event, true)) {
            set_last_error(L"Store contains an invalid or oversized note");
            return 0;
        }
    }

    QSaveFile file(QString::fromWCharArray(path));
    if (!file.open(QIODevice::WriteOnly)) {
        set_last_error(QStringLiteral("Cannot open note file: %1").arg(file.errorString()));
        return 0;
    }

    const QByteArray header = QByteArrayLiteral("STICKY_NOTES_C_V3\t") +
                              QByteArray::number((qlonglong)store->items.size()) + '\t' +
                              QByteArray::number(store->next_id) + '\n';
    if (!write_all(&file, header)) {
        set_last_error(QStringLiteral("Cannot write note file header: %1").arg(file.errorString()));
        file.cancelWriting();
        return 0;
    }

    for (const NoteEvent &event : store->items) {
        QByteArray line;
        line.reserve(256);
        line += QByteArray::number(event.id);
        line += '\t';
        line += QByteArray::number((int)event.stage);
        line += '\t';
        line += encode_wide_field(event.date_key);
        line += '\t';
        line += QByteArray::number(event.completed ? 1 : 0);
        line += '\t';
        line += QByteArray::number((qlonglong)event.completed_at);
        line += '\t';
        line += QByteArray::number((qlonglong)event.created_at);
        line += '\t';
        line += QByteArray::number((qlonglong)event.updated_at);
        line += '\t';
        line += encode_wide_field(event.text);
        line += '\t';
        line += encode_wide_field(event.repeat);
        line += '\t';
        line += encode_wide_field(event.series_id);
        line += '\n';
        if (!write_all(&file, line)) {
            set_last_error(QStringLiteral("Cannot write note file: %1").arg(file.errorString()));
            file.cancelWriting();
            return 0;
        }
    }

    if (!file.commit()) {
        set_last_error(QStringLiteral("Cannot commit note file: %1").arg(file.errorString()));
        return 0;
    }
    return 1;
}

int note_store_load(NoteStore *store, const wchar_t *path) {
    clear_last_error();
    if (store == NULL || path == NULL || path[0] == L'\0') {
        set_last_error(L"Store or path is invalid");
        return 0;
    }

    QFile file(QString::fromWCharArray(path));
    if (!file.open(QIODevice::ReadOnly)) {
        set_last_error(QStringLiteral("Cannot open note file: %1").arg(file.errorString()));
        return 0;
    }

    QByteArray header = file.readLine();
    if (header.isEmpty() && file.atEnd()) {
        NoteStore empty;
        empty.count = 0;
        empty.next_id = 1;
        store->items = std::move(empty.items);
        store->count = 0;
        store->next_id = 1;
        return 1;
    }
    trim_line_ending(&header);
    const QList<QByteArray> header_fields = header.split('\t');
    if (header_fields.size() != 3) {
        set_last_error(L"Note file header is invalid");
        return 0;
    }

    int version = 0;
    if (header_fields.at(0) == QByteArrayLiteral("STICKY_NOTES_C_V1")) {
        version = 1;
    } else if (header_fields.at(0) == QByteArrayLiteral("STICKY_NOTES_C_V2")) {
        version = 2;
    } else if (header_fields.at(0) == QByteArrayLiteral("STICKY_NOTES_C_V3")) {
        version = 3;
    } else {
        set_last_error(L"Note file version is unsupported");
        return 0;
    }

    int expected_count = 0;
    int header_next_id = 1;
    if (!parse_int_field(header_fields.at(1), &expected_count) || expected_count < 0 ||
        !parse_int_field(header_fields.at(2), &header_next_id) || header_next_id <= 0) {
        set_last_error(L"Note file header values are invalid");
        return 0;
    }

    NoteStore loaded;
    loaded.count = 0;
    loaded.next_id = header_next_id;
    QSet<int> ids;
    int line_number = 1;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        ++line_number;
        trim_line_ending(&line);
        if (line.isEmpty()) {
            continue;
        }
        const QList<QByteArray> fields = line.split('\t');
        const bool valid_field_count =
            (version == 1 && fields.size() == 7) ||
            (version == 2 && (fields.size() == 8 || fields.size() == 9)) ||
            (version == 3 && fields.size() == 10);
        if (!valid_field_count) {
            set_last_error(QStringLiteral("Invalid field count at note file line %1").arg(line_number));
            return 0;
        }

        NoteEvent event{};
        int stage = -1;
        int completed = -1;
        if (!parse_int_field(fields.at(0), &event.id) ||
            !parse_int_field(fields.at(1), &stage) ||
            !parse_int_field(fields.at(3), &completed) ||
            (completed != 0 && completed != 1)) {
            set_last_error(QStringLiteral("Invalid numeric value at note file line %1").arg(line_number));
            return 0;
        }
        event.stage = (NoteStage)stage;
        event.completed = completed;

        const int text_index = version == 1 ? 6 : 7;
        if (!decode_wide_field(fields.at(2), event.date_key, NOTE_KEY_MAX, false) ||
            !decode_wide_field(fields.at(text_index), event.text, NOTE_TEXT_MAX, false)) {
            set_last_error(QStringLiteral("Invalid or oversized text at note file line %1").arg(line_number));
            return 0;
        }

        if (version == 1) {
            event.completed_at = 0;
            if (!parse_long_long_field(fields.at(4), &event.created_at) ||
                !parse_long_long_field(fields.at(5), &event.updated_at)) {
                set_last_error(QStringLiteral("Invalid timestamp at note file line %1").arg(line_number));
                return 0;
            }
        } else {
            if (!parse_long_long_field(fields.at(4), &event.completed_at) ||
                !parse_long_long_field(fields.at(5), &event.created_at) ||
                !parse_long_long_field(fields.at(6), &event.updated_at)) {
                set_last_error(QStringLiteral("Invalid timestamp at note file line %1").arg(line_number));
                return 0;
            }
            if (fields.size() >= 9 &&
                !decode_wide_field(fields.at(8), event.repeat, NOTE_REPEAT_MAX, true)) {
                set_last_error(QStringLiteral("Invalid repeat rule at note file line %1").arg(line_number));
                return 0;
            }
        }

        if (version == 3) {
            if (!decode_wide_field(fields.at(9), event.series_id, NOTE_SERIES_ID_MAX, false)) {
                set_last_error(QStringLiteral("Invalid series id at note file line %1").arg(line_number));
                return 0;
            }
        } else if (!assign_new_series_id(&event)) {
            set_last_error(L"Failed to assign a series id while importing notes");
            return 0;
        }

        if (!validate_event(event, true) || ids.contains(event.id)) {
            set_last_error(QStringLiteral("Invalid or duplicate note at file line %1").arg(line_number));
            return 0;
        }
        ids.insert(event.id);
        loaded.items.push_back(event);
        loaded.count = (int)loaded.items.size();
        if (event.id >= loaded.next_id) {
            loaded.next_id = event.id + 1;
        }
    }

    if (file.error() != QFileDevice::NoError) {
        set_last_error(QStringLiteral("Cannot read note file: %1").arg(file.errorString()));
        return 0;
    }
    if (loaded.count != expected_count) {
        set_last_error(L"Note file row count does not match its header");
        return 0;
    }

    store->items = std::move(loaded.items);
    store->count = loaded.count;
    store->next_id = loaded.next_id;
    return 1;
}

static QString wide_path(const wchar_t *path) {
    return path == NULL ? QString() : QString::fromWCharArray(path);
}

static QString wide_value(const wchar_t *value) {
    return value == NULL ? QString() : QString::fromWCharArray(value);
}

static bool copy_qstring_wide(wchar_t *dest,
                              size_t dest_count,
                              const QString &value,
                              bool allow_empty) {
    const std::wstring wide = value.toStdWString();
    return copy_wstr_checked(dest, dest_count, wide.c_str(), allow_empty);
}

class ScopedNotesDatabase {
public:
    explicit ScopedNotesDatabase(const wchar_t *path)
        : connection_name_(QStringLiteral("notes_%1")
                               .arg(QUuid::createUuid().toString(QUuid::Id128))),
          database_(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_)) {
        database_.setDatabaseName(wide_path(path));
    }

    ~ScopedNotesDatabase() {
        if (database_.isValid()) {
            database_.close();
        }
        database_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection_name_);
    }

    ScopedNotesDatabase(const ScopedNotesDatabase &) = delete;
    ScopedNotesDatabase &operator=(const ScopedNotesDatabase &) = delete;

    bool open() {
        if (!database_.open()) {
            set_last_error(QStringLiteral("Cannot open notes database: %1")
                               .arg(database_.lastError().text()));
            return false;
        }
        QSqlQuery busy_timeout(database_);
        if (!busy_timeout.exec(QStringLiteral("PRAGMA busy_timeout=5000"))) {
            set_last_error(QStringLiteral("Cannot configure notes database: %1")
                               .arg(busy_timeout.lastError().text()));
            return false;
        }
        return true;
    }

    QSqlDatabase &database() {
        return database_;
    }

private:
    QString connection_name_;
    QSqlDatabase database_;
};

static void rollback_with_error(QSqlDatabase &database, const QString &primary_error) {
    if (!database.rollback()) {
        set_last_error(QStringLiteral("%1; rollback also failed: %2")
                           .arg(primary_error, database.lastError().text()));
        return;
    }
    set_last_error(primary_error);
}

static bool query_user_version(QSqlDatabase &database, int *version) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        set_last_error(QStringLiteral("Cannot read database schema version: %1")
                           .arg(query.lastError().text()));
        return false;
    }
    if (version != NULL) {
        *version = query.value(0).toInt();
    }
    return true;
}

static bool create_migration_backup(QSqlDatabase &database) {
    const QString database_path = database.databaseName();
    if (database_path.isEmpty() || database_path == QStringLiteral(":memory:")) {
        return true;
    }
    const QString suffix = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz")) +
                           QLatin1Char('-') +
                           QUuid::createUuid().toString(QUuid::Id128).left(8);
    const QString backup_path = database_path + QStringLiteral(".pre-v2-") + suffix +
                                QStringLiteral(".bak");
    QString escaped_path = QDir::toNativeSeparators(backup_path);
    escaped_path.replace(QLatin1Char('\''), QStringLiteral("''"));

    QSqlQuery backup(database);
    if (!backup.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped_path))) {
        set_last_error(QStringLiteral("Cannot create schema migration backup: %1")
                           .arg(backup.lastError().text()));
        return false;
    }
    return true;
}

static bool ensure_schema_v2(QSqlDatabase &database) {
    int version = 0;
    if (!query_user_version(database, &version)) {
        return false;
    }
    if (version > 2) {
        set_last_error(QStringLiteral("Notes database schema %1 is newer than supported schema 2")
                           .arg(version));
        return false;
    }

    QSqlQuery table_query(database);
    if (!table_query.exec(QStringLiteral(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='notes'")) ||
        !table_query.next()) {
        set_last_error(QStringLiteral("Cannot inspect notes schema: %1")
                           .arg(table_query.lastError().text()));
        return false;
    }
    const bool table_exists = table_query.value(0).toInt() > 0;
    table_query.finish();

    QSet<QString> columns;
    if (table_exists) {
        QSqlQuery columns_query(database);
        if (!columns_query.exec(QStringLiteral("PRAGMA table_info(notes)"))) {
            set_last_error(QStringLiteral("Cannot inspect notes columns: %1")
                               .arg(columns_query.lastError().text()));
            return false;
        }
        while (columns_query.next()) {
            columns.insert(columns_query.value(1).toString());
        }
        columns_query.finish();
        const QStringList required_v1_columns = {
            QStringLiteral("id"),
            QStringLiteral("stage"),
            QStringLiteral("date_key"),
            QStringLiteral("text"),
            QStringLiteral("completed"),
            QStringLiteral("completed_at"),
            QStringLiteral("created_at"),
            QStringLiteral("updated_at"),
            QStringLiteral("repeat")
        };
        for (const QString &required : required_v1_columns) {
            if (!columns.contains(required)) {
                set_last_error(QStringLiteral("Notes schema is missing required column '%1'")
                                   .arg(required));
                return false;
            }
        }
        if ((version < 2 || !columns.contains(QStringLiteral("series_id"))) &&
            !create_migration_backup(database)) {
            return false;
        }
    }

    if (!database.transaction()) {
        set_last_error(QStringLiteral("Cannot start schema transaction: %1")
                           .arg(database.lastError().text()));
        return false;
    }

    QSqlQuery schema_query(database);
    if (!table_exists) {
        if (!schema_query.exec(QStringLiteral(
                "CREATE TABLE notes ("
                "id INTEGER PRIMARY KEY,"
                "stage INTEGER NOT NULL,"
                "date_key TEXT NOT NULL,"
                "text TEXT NOT NULL,"
                "completed INTEGER NOT NULL,"
                "completed_at INTEGER NOT NULL,"
                "created_at INTEGER NOT NULL,"
                "updated_at INTEGER NOT NULL,"
                "repeat TEXT NOT NULL DEFAULT '',"
                "series_id TEXT NOT NULL DEFAULT '')"))) {
            rollback_with_error(database,
                                QStringLiteral("Cannot create notes schema: %1")
                                    .arg(schema_query.lastError().text()));
            return false;
        }
    } else {
        if (!columns.contains(QStringLiteral("series_id")) &&
            !schema_query.exec(QStringLiteral(
                "ALTER TABLE notes ADD COLUMN series_id TEXT NOT NULL DEFAULT ''"))) {
            rollback_with_error(database,
                                QStringLiteral("Cannot add series id column: %1")
                                    .arg(schema_query.lastError().text()));
            return false;
        }
    }

    const QStringList migration_statements = {
        QStringLiteral("UPDATE notes SET series_id=lower(hex(randomblob(16))) "
                       "WHERE series_id IS NULL OR series_id=''"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notes_stage_date_completed "
                       "ON notes(stage, date_key, completed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notes_updated_at ON notes(updated_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notes_series_id ON notes(series_id)"),
        QStringLiteral("PRAGMA user_version=2")
    };
    for (const QString &statement : migration_statements) {
        if (!schema_query.exec(statement)) {
            rollback_with_error(database,
                                QStringLiteral("Cannot migrate notes schema: %1")
                                    .arg(schema_query.lastError().text()));
            return false;
        }
    }

    if (!database.commit()) {
        const QString error = QStringLiteral("Cannot commit notes schema: %1")
                                  .arg(database.lastError().text());
        rollback_with_error(database, error);
        return false;
    }
    return true;
}

int note_store_prepare_sqlite(const wchar_t *path) {
    clear_last_error();
    if (path == NULL || path[0] == L'\0') {
        set_last_error(L"Database path is invalid");
        return 0;
    }

    ScopedNotesDatabase connection(path);
    if (!connection.open()) {
        return 0;
    }
    return ensure_schema_v2(connection.database()) ? 1 : 0;
}

int note_store_save_sqlite(const NoteStore *store, const wchar_t *path) {
    clear_last_error();
    if (store == NULL || path == NULL || path[0] == L'\0') {
        set_last_error(L"Store or database path is invalid");
        return 0;
    }
    for (const NoteEvent &event : store->items) {
        if (!validate_event(event, true)) {
            set_last_error(L"Store contains an invalid or oversized note");
            return 0;
        }
    }

    ScopedNotesDatabase connection(path);
    if (!connection.open()) {
        return 0;
    }
    QSqlDatabase &database = connection.database();
    if (!ensure_schema_v2(database)) {
        return 0;
    }
    if (!database.transaction()) {
        set_last_error(QStringLiteral("Cannot start snapshot transaction: %1")
                           .arg(database.lastError().text()));
        return 0;
    }

    QSqlQuery temp_ids(database);
    if (!temp_ids.exec(QStringLiteral(
            "CREATE TEMP TABLE IF NOT EXISTS incoming_note_ids "
            "(id INTEGER PRIMARY KEY)")) ||
        !temp_ids.exec(QStringLiteral("DELETE FROM incoming_note_ids"))) {
        rollback_with_error(database,
                            QStringLiteral("Cannot prepare snapshot identity set: %1")
                                .arg(temp_ids.lastError().text()));
        return 0;
    }

    QSqlQuery upsert(database);
    if (!upsert.prepare(QStringLiteral(
            "INSERT INTO notes "
            "(id, stage, date_key, text, completed, completed_at, created_at, updated_at, repeat, series_id) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET "
            "stage=excluded.stage, date_key=excluded.date_key, text=excluded.text, "
            "completed=excluded.completed, completed_at=excluded.completed_at, "
            "created_at=excluded.created_at, updated_at=excluded.updated_at, "
            "repeat=excluded.repeat, series_id=excluded.series_id"))) {
        rollback_with_error(database,
                            QStringLiteral("Cannot prepare notes snapshot: %1")
                                .arg(upsert.lastError().text()));
        return 0;
    }
    QSqlQuery remember_id(database);
    if (!remember_id.prepare(QStringLiteral(
            "INSERT INTO incoming_note_ids(id) VALUES (?)"))) {
        rollback_with_error(database,
                            QStringLiteral("Cannot prepare snapshot identity: %1")
                                .arg(remember_id.lastError().text()));
        return 0;
    }

    for (const NoteEvent &event : store->items) {
        upsert.bindValue(0, event.id);
        upsert.bindValue(1, (int)event.stage);
        upsert.bindValue(2, wide_value(event.date_key));
        upsert.bindValue(3, wide_value(event.text));
        upsert.bindValue(4, event.completed);
        upsert.bindValue(5, QVariant::fromValue<qlonglong>(event.completed_at));
        upsert.bindValue(6, QVariant::fromValue<qlonglong>(event.created_at));
        upsert.bindValue(7, QVariant::fromValue<qlonglong>(event.updated_at));
        upsert.bindValue(8, wide_value(event.repeat));
        upsert.bindValue(9, wide_value(event.series_id));
        if (!upsert.exec()) {
            rollback_with_error(database,
                                QStringLiteral("Cannot write notes snapshot: %1")
                                    .arg(upsert.lastError().text()));
            return 0;
        }
        remember_id.bindValue(0, event.id);
        if (!remember_id.exec()) {
            rollback_with_error(database,
                                QStringLiteral("Cannot record snapshot identity: %1")
                                    .arg(remember_id.lastError().text()));
            return 0;
        }
    }

    QSqlQuery delete_missing(database);
    if (!delete_missing.exec(QStringLiteral(
            "DELETE FROM notes WHERE NOT EXISTS "
            "(SELECT 1 FROM incoming_note_ids WHERE incoming_note_ids.id=notes.id)"))) {
        rollback_with_error(database,
                            QStringLiteral("Cannot remove notes missing from snapshot: %1")
                                .arg(delete_missing.lastError().text()));
        return 0;
    }

    if (!database.commit()) {
        const QString error = QStringLiteral("Cannot commit notes snapshot: %1")
                                  .arg(database.lastError().text());
        rollback_with_error(database, error);
        return 0;
    }
    return 1;
}

int note_store_load_sqlite(NoteStore *store, const wchar_t *path) {
    clear_last_error();
    if (store == NULL || path == NULL || path[0] == L'\0') {
        set_last_error(L"Store or database path is invalid");
        return 0;
    }

    ScopedNotesDatabase connection(path);
    if (!connection.open()) {
        return 0;
    }
    QSqlDatabase &database = connection.database();
    if (!ensure_schema_v2(database)) {
        return 0;
    }

    QSqlQuery query(database);
    if (!query.exec(QStringLiteral(
            "SELECT id, stage, date_key, text, completed, completed_at, created_at, updated_at, repeat, series_id "
            "FROM notes ORDER BY id"))) {
        set_last_error(QStringLiteral("Cannot read notes snapshot: %1")
                           .arg(query.lastError().text()));
        return 0;
    }

    NoteStore loaded;
    loaded.count = 0;
    loaded.next_id = 1;
    while (query.next()) {
        NoteEvent event{};
        event.id = query.value(0).toInt();
        event.stage = (NoteStage)query.value(1).toInt();
        const int completed = query.value(4).toInt();
        event.completed = completed;
        event.completed_at = query.value(5).toLongLong();
        event.created_at = query.value(6).toLongLong();
        event.updated_at = query.value(7).toLongLong();
        if (!copy_qstring_wide(event.date_key, NOTE_KEY_MAX, query.value(2).toString(), false) ||
            !copy_qstring_wide(event.text, NOTE_TEXT_MAX, query.value(3).toString(), false) ||
            !copy_qstring_wide(event.repeat, NOTE_REPEAT_MAX, query.value(8).toString(), true) ||
            !copy_qstring_wide(event.series_id, NOTE_SERIES_ID_MAX, query.value(9).toString(), false) ||
            (completed != 0 && completed != 1) || !validate_event(event, true)) {
            set_last_error(QStringLiteral("Database contains invalid or oversized note id %1")
                               .arg(event.id));
            return 0;
        }
        loaded.items.push_back(event);
        loaded.count = (int)loaded.items.size();
        if (event.id >= loaded.next_id) {
            loaded.next_id = event.id + 1;
        }
    }
    if (query.lastError().isValid()) {
        set_last_error(QStringLiteral("Cannot finish reading notes snapshot: %1")
                           .arg(query.lastError().text()));
        return 0;
    }

    store->items = std::move(loaded.items);
    store->count = loaded.count;
    store->next_id = loaded.next_id;
    return 1;
}

const wchar_t *note_store_last_error(void) {
    return g_last_error.c_str();
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
