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

    expect_int("config save succeeds", 1, config_save(&config, path));
    config_defaults(&loaded);
    expect_int("config load succeeds", 1, config_load(&loaded, path));

    expect_wstr("loaded api url", config.api_url, loaded.api_url);
    expect_wstr("loaded api key", config.api_key, loaded.api_key);
    expect_wstr("loaded model", config.model, loaded.model);
    expect_int("loaded ai enabled", 1, config_has_ai(&loaded));

    _wremove(path);
}

int main(void) {
    test_defaults_are_usable_without_key();
    test_save_and_load_config();

    if (failures != 0) {
        printf("%d config test(s) failed\n", failures);
        return 1;
    }

    printf("config tests passed\n");
    return 0;
}
