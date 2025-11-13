#ifndef CHATURLATE_FILTER_H
#define CHATURLATE_FILTER_H

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MT_ obs_module_text

const char *chaturlate_filter_name(void *unused);
void *chaturlate_filter_create(obs_data_t *settings, obs_source_t *source);
void chaturlate_filter_destroy(void *data);
void chaturlate_filter_defaults(obs_data_t *settings);
obs_properties_t *chaturlate_filter_properties(void *data);
void chaturlate_filter_update(void *data, obs_data_t *settings);
void chaturlate_filter_activate(void *data);
void chaturlate_filter_deactivate(void *data);
struct obs_audio_data *chaturlate_filter_filter_audio(void *data, struct obs_audio_data *audio);

#ifdef __cplusplus
}
#endif

#endif // CHATURLATE_FILTER_H

