#ifndef NOTE_STORE_H
#define NOTE_STORE_H

#include <stddef.h>
#include <wchar.h>

#define NOTE_MAX_EVENTS 512
#define NOTE_TEXT_MAX 512
#define NOTE_KEY_MAX 32
#define NOTE_REPEAT_MAX 16

#include <deque>

typedef enum NoteStage {
    NOTE_STAGE_DAY = 0,
    NOTE_STAGE_WEEK = 1,
    NOTE_STAGE_MONTH = 2,
    NOTE_STAGE_YEAR = 3
} NoteStage;

typedef struct NoteEvent {
    int id;
    NoteStage stage;
    wchar_t date_key[NOTE_KEY_MAX];
    wchar_t text[NOTE_TEXT_MAX];
    int completed;
    long long completed_at;
    long long created_at;
    long long updated_at;
    wchar_t repeat[NOTE_REPEAT_MAX];
} NoteEvent;

typedef struct NoteStore {
    std::deque<NoteEvent> items;
    int count;
    int next_id;
} NoteStore;

void note_store_init(NoteStore *store);
NoteEvent *note_store_add(NoteStore *store, NoteStage stage, const wchar_t *date_key, const wchar_t *text);
int note_store_update_text(NoteStore *store, int id, const wchar_t *text);
int note_store_set_repeat(NoteStore *store, int id, const wchar_t *repeat);
int note_store_toggle(NoteStore *store, int id);
int note_store_complete_all(NoteStore *store, NoteStage stage, const wchar_t *date_key);
int note_store_toggle_all(NoteStore *store, NoteStage stage, const wchar_t *date_key);
int note_store_delete(NoteStore *store, int id);
int note_store_restore(NoteStore *store, const NoteEvent *event);
int note_store_filter(NoteStore *store, NoteStage stage, const wchar_t *date_key, NoteEvent **out, int out_capacity);
int note_store_save(const NoteStore *store, const wchar_t *path);
int note_store_load(NoteStore *store, const wchar_t *path);
int note_store_save_sqlite(const NoteStore *store, const wchar_t *path);
int note_store_load_sqlite(NoteStore *store, const wchar_t *path);
const wchar_t *note_stage_label(NoteStage stage);

#endif
