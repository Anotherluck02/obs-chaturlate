#include "chaturlate-filter.h"
#include <obs-module.h>

struct obs_source_info chaturlate_filter_info = {
	.id = "chaturlate_filter_audio_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = chaturlate_filter_name,
	.create = chaturlate_filter_create,
	.destroy = chaturlate_filter_destroy,
	.get_defaults = chaturlate_filter_defaults,
	.get_properties = chaturlate_filter_properties,
	.update = chaturlate_filter_update,
	.activate = chaturlate_filter_activate,
	.deactivate = chaturlate_filter_deactivate,
	.filter_audio = chaturlate_filter_filter_audio,
};

