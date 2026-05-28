#ifndef AI_CLIENT_H
#define AI_CLIENT_H

#include "config.h"

#include <stddef.h>
#include <wchar.h>

int ai_client_summarize(
    const AppConfig *config,
    const wchar_t *requirement,
    const wchar_t *notes_text,
    wchar_t *result,
    size_t result_count,
    wchar_t *error,
    size_t error_count
);

#endif
