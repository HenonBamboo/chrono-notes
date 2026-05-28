#include "note_store.h"

#include <stdio.h>
#include <string.h>

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
    NoteStore store;
    note_store_init(&store);

    for (int i = 0; i < NOTE_MAX_EVENTS + 24; ++i) {
        wchar_t text[64];
        swprintf(text, 64, L"bulk-%d", i);
        NoteEvent *event = note_store_add(&store, NOTE_STAGE_DAY, L"2026-05-24", text);
        if (event == NULL) {
            printf("FAIL dynamic add stopped at %d\n", i);
            failures++;
            break;
        }
    }

    expect_int("dynamic store count", NOTE_MAX_EVENTS + 24, store.count);
}

int main(void) {
    test_add_filter_and_toggle();
    test_save_and_load_roundtrip();
    test_update_text_preserves_event_identity();
    test_completed_events_get_time_and_sink();
    test_new_open_events_sort_to_top();
    test_complete_all_marks_current_stage_only();
    test_toggle_all_completes_then_uncompletes();
    test_restore_deleted_event_preserves_identity();
    test_store_grows_beyond_legacy_stack_limit();

    if (failures != 0) {
        printf("%d note_store test(s) failed\n", failures);
        return 1;
    }

    printf("note_store tests passed\n");
    return 0;
}
