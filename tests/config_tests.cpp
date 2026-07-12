#include "config.h"

#include <stdio.h>
#include <wchar.h>

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

static void test_defaults_are_usable_without_key(void) {
    AppConfig config;
    config_defaults(&config);

    expect_wstr("default api url", L"https://api.openai.com/v1/chat/completions", config.api_url);
    expect_wstr("default model", L"gpt-4o-mini", config.model);
    expect_wstr("default ui font family", L"Microsoft YaHei UI", config.ui_font_family);
    expect_int("default ui font size", 12, config.ui_font_size);
    expect_int("default ai disabled", 0, config_has_ai(&config));
}

static void test_save_and_load_config(void) {
    const wchar_t *path = L"config_test.ini";
    AppConfig config;
    AppConfig loaded;

    config_defaults(&config);
    wcscpy(config.api_url, L"https://example.test/v1/chat/completions");
    wcscpy(config.api_key, L"test-key");
    wcscpy(config.model, L"test-model");
    wcscpy(config.ui_font_family, L"Segoe UI");
    config.ui_font_size = 15;

    expect_int("config save succeeds", 1, config_save(&config, path));
    config_defaults(&loaded);
    expect_int("config load succeeds", 1, config_load(&loaded, path));

    expect_wstr("loaded api url", config.api_url, loaded.api_url);
    expect_wstr("loaded api key", config.api_key, loaded.api_key);
    expect_wstr("loaded model", config.model, loaded.model);
    expect_wstr("loaded ui font family", config.ui_font_family, loaded.ui_font_family);
    expect_int("loaded ui font size", config.ui_font_size, loaded.ui_font_size);
    expect_int("loaded ai enabled", 1, config_has_ai(&loaded));

    _wremove(path);
}

static void test_legacy_config_uses_font_defaults(void) {
    const wchar_t *path = L"config_legacy_test.ini";
    AppConfig loaded;
    FILE *file = _wfopen(path, L"w, ccs=UTF-8");
    fwprintf(file, L"api_url=https://legacy.test/v1/chat/completions\n");
    fwprintf(file, L"api_key=legacy-key\n");
    fwprintf(file, L"model=legacy-model\n");
    fclose(file);

    config_defaults(&loaded);
    expect_int("legacy config load succeeds", 1, config_load(&loaded, path));
    expect_wstr("legacy default ui font family", L"Microsoft YaHei UI", loaded.ui_font_family);
    expect_int("legacy default ui font size", 12, loaded.ui_font_size);

    _wremove(path);
}

static void test_invalid_font_size_falls_back_to_default(void) {
    const wchar_t *path = L"config_invalid_font_size_test.ini";
    AppConfig loaded;
    FILE *file = _wfopen(path, L"w, ccs=UTF-8");
    fwprintf(file, L"ui_font_family=Segoe UI\n");
    fwprintf(file, L"ui_font_size=99\n");
    fclose(file);

    config_defaults(&loaded);
    expect_int("invalid font size config load succeeds", 1, config_load(&loaded, path));
    expect_wstr("invalid font size keeps family", L"Segoe UI", loaded.ui_font_family);
    expect_int("invalid font size defaults", 12, loaded.ui_font_size);

    _wremove(path);
}

int main(void) {
    test_defaults_are_usable_without_key();
    test_save_and_load_config();
    test_legacy_config_uses_font_defaults();
    test_invalid_font_size_falls_back_to_default();

    if (failures != 0) {
        printf("%d config test(s) failed\n", failures);
        return 1;
    }

    printf("config tests passed\n");
    return 0;
}
