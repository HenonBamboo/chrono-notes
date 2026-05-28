#include "config.h"

#include <stdio.h>
#include <string.h>

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

static void trim_newline(wchar_t *text) {
    if (text == NULL) {
        return;
    }
    size_t len = wcslen(text);
    while (len > 0 && (text[len - 1] == L'\n' || text[len - 1] == L'\r')) {
        text[--len] = L'\0';
    }
}

void config_defaults(AppConfig *config) {
    if (config == NULL) {
        return;
    }
    copy_wstr(config->api_url, CONFIG_VALUE_MAX, L"https://api.openai.com/v1/chat/completions");
    copy_wstr(config->api_key, CONFIG_VALUE_MAX, L"");
    copy_wstr(config->model, 128, L"gpt-4o-mini");
}

int config_load(AppConfig *config, const wchar_t *path) {
    if (config == NULL || path == NULL) {
        return 0;
    }

    config_defaults(config);

    FILE *file = _wfopen(path, L"r, ccs=UTF-8");
    if (file == NULL) {
        return 0;
    }

    wchar_t line[1024];
    while (fgetws(line, 1024, file) != NULL) {
        trim_newline(line);
        wchar_t *eq = wcschr(line, L'=');
        if (eq == NULL) {
            continue;
        }
        *eq = L'\0';
        const wchar_t *key = line;
        const wchar_t *value = eq + 1;

        if (wcscmp(key, L"api_url") == 0) {
            copy_wstr(config->api_url, CONFIG_VALUE_MAX, value);
        } else if (wcscmp(key, L"api_key") == 0) {
            copy_wstr(config->api_key, CONFIG_VALUE_MAX, value);
        } else if (wcscmp(key, L"model") == 0) {
            copy_wstr(config->model, 128, value);
        }
    }

    fclose(file);
    return 1;
}

int config_save(const AppConfig *config, const wchar_t *path) {
    if (config == NULL || path == NULL) {
        return 0;
    }

    FILE *file = _wfopen(path, L"w, ccs=UTF-8");
    if (file == NULL) {
        return 0;
    }

    fwprintf(file, L"api_url=%ls\n", config->api_url);
    fwprintf(file, L"api_key=%ls\n", config->api_key);
    fwprintf(file, L"model=%ls\n", config->model);
    fclose(file);
    return 1;
}

int config_has_ai(const AppConfig *config) {
    return config != NULL &&
           config->api_url[0] != L'\0' &&
           config->api_key[0] != L'\0' &&
           config->model[0] != L'\0';
}
