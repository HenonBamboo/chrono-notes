#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <wchar.h>

#define CONFIG_VALUE_MAX 512

typedef struct AppConfig {
    wchar_t api_url[CONFIG_VALUE_MAX];
    wchar_t api_key[CONFIG_VALUE_MAX];
    wchar_t model[128];
} AppConfig;

void config_defaults(AppConfig *config);
int config_load(AppConfig *config, const wchar_t *path);
int config_save(const AppConfig *config, const wchar_t *path);
int config_has_ai(const AppConfig *config);

#endif
