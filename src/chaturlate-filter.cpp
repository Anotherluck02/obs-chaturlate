#include "chaturlate-filter.h"
#include "chaturlate-filter-data.h"
#include "plugin-support.h"
#include "model-utils/model-downloader.h"
#include "model-utils/model-find-utils.h"
#include "whisper-utils/whisper-model-utils.h"
#include "whisper-utils/whisper-processing.h"
#include "translation/translation-utils.h"
#include "translation/translation.h"
#include "transcription-filter-data.h"
#include "sherpa-tts/sherpa-tts.h"
#include "tts-utils.h"

#include <obs-module.h>
#include <util/platform.h>

#include <vector>
#include <string>
#include <sstream>

const char *chaturlate_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Chaturlate (Hold L: STT → Translation → TTS)";
}

void chaturlate_audio_samples_callback(void *data, const float *samples, int num_samples, int sample_rate)
{
	UNUSED_PARAMETER(sample_rate);
	chaturlate_filter_data *gf = (chaturlate_filter_data *)data;
	gf->audioThread->pushAudioSamples(std::vector<float>(samples, samples + num_samples));
}

// Hotkey callback
void hotkey_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	
	chaturlate_filter_data *gf = (chaturlate_filter_data *)data;
	gf->hotkey_pressed = pressed;
	
	if (pressed) {
		obs_log(LOG_INFO, "[Chaturlate] L key pressed - starting STT");
		// Clear audio buffer for new recording
		std::lock_guard<std::mutex> lock(gf->audio_mutex);
		gf->audio_buffer.clear();
		gf->processing_audio = true;
		
		// Mute the mic
		if (gf->context) {
			obs_source_t *parent = obs_filter_get_parent(gf->context);
			if (parent) {
				obs_source_set_muted(parent, true);
				gf->mic_muted_by_us = true;
			}
		}
	} else {
		obs_log(LOG_INFO, "[Chaturlate] L key released - processing audio");
		gf->processing_audio = false;
		
		// Unmute the mic
		if (gf->mic_muted_by_us && gf->context) {
			obs_source_t *parent = obs_filter_get_parent(gf->context);
			if (parent) {
				obs_source_set_muted(parent, false);
				gf->mic_muted_by_us = false;
			}
		}
		
		// Process the accumulated audio
		std::vector<float> audio_copy;
		{
			std::lock_guard<std::mutex> lock(gf->audio_mutex);
			audio_copy = gf->audio_buffer;
		}
		
		if (!audio_copy.empty() && gf->whisper_initialized) {
			obs_log(LOG_INFO, "[Chaturlate] Processing %zu audio samples", audio_copy.size());
			
			// Run Whisper STT
			std::string transcribed_text;
			try {
				// Create minimal transcription_filter_data for run_whisper_inference
				struct transcription_filter_data whisper_gf = {};
				whisper_gf.whisper_context = gf->whisper_context;
				whisper_gf.log_level = LOG_INFO;
				whisper_gf.sentence_psum_accept_thresh = 0.5f;
				whisper_gf.duration_filter_threshold = 2.25f;
				
				struct DetectionResultWithText result = run_whisper_inference(
					&whisper_gf,
					audio_copy.data(),
					audio_copy.size()
				);
				transcribed_text = result.text;
				obs_log(LOG_INFO, "[Chaturlate] Transcribed: %s", transcribed_text.c_str());
			} catch (const std::exception &e) {
				obs_log(LOG_ERROR, "[Chaturlate] Whisper error: %s", e.what());
				return;
			}
			
			if (transcribed_text.empty()) {
				obs_log(LOG_INFO, "[Chaturlate] No speech detected");
				return;
			}
			
			// Translate to English
			std::string translated_text = transcribed_text;
			if (gf->translation_enabled && gf->translation_ctx.translator) {
				try {
					std::string source_lang = "auto"; // Auto-detect source language
					translate(gf->translation_ctx, transcribed_text, source_lang, 
						gf->target_lang, translated_text);
					obs_log(LOG_INFO, "[Chaturlate] Translated: %s", translated_text.c_str());
				} catch (const std::exception &e) {
					obs_log(LOG_ERROR, "[Chaturlate] Translation error: %s", e.what());
					// Continue with original text if translation fails
				}
			}
			
			// Generate TTS
			if (!translated_text.empty()) {
				obs_log(LOG_INFO, "[Chaturlate] Generating TTS for: %s", 
					translated_text.c_str());
				generate_audio_from_text(gf->tts_context, translated_text, 
					gf->speaker_id, gf->speed);
			}
		}
	}
}

void *chaturlate_filter_create(obs_data_t *settings, obs_source_t *source)
{
	obs_log(LOG_INFO, "[Chaturlate] Creating filter");
	
	void *data = bmalloc(sizeof(chaturlate_filter_data));
	chaturlate_filter_data *gf = new (data) chaturlate_filter_data();
	
	gf->context = source;
	
	// Create audio thread for output
	gf->audioThread = std::make_unique<AudioThread>(source);
	gf->audioThread->start();
	
	// Register hotkey for "L" key
	gf->hotkey_id = obs_hotkey_register_source(
		source, "chaturlate_hold_key",
		"Hold to Translate (L)", hotkey_callback, gf);
	
	chaturlate_filter_update(data, settings);
	
	obs_log(LOG_INFO, "[Chaturlate] Filter created successfully");
	return data;
}

void chaturlate_filter_destroy(void *data)
{
	chaturlate_filter_data *gf = (chaturlate_filter_data *)data;
	
	obs_log(LOG_INFO, "[Chaturlate] Destroying filter");
	
	// Stop audio thread
	if (gf->audioThread) {
		gf->audioThread->stop();
	}
	
	// Clean up Whisper
	if (gf->whisper_context) {
		whisper_free(gf->whisper_context);
	}
	
	// Clean up TTS
	destroy_sherpa_tts_context(gf->tts_context);
	
	// Clean up translator (translation_ctx uses unique_ptr, auto-cleans)
	
	// Clean up resampler
	if (gf->resampler) {
		audio_resampler_destroy(gf->resampler);
	}
	
	// Unregister hotkey
	if (gf->hotkey_id != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(gf->hotkey_id);
	}
	
	gf->~chaturlate_filter_data();
	bfree(data);
}

void chaturlate_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "whisper_model", "ggml-base.en.bin");
	obs_data_set_default_string(settings, "whisper_language", "en");
	obs_data_set_default_bool(settings, "vad_enabled", true);
	obs_data_set_default_bool(settings, "translation_enabled", true);
	obs_data_set_default_string(settings, "translation_model", "opus-mt-en-en");
	obs_data_set_default_string(settings, "target_lang", "en");
	obs_data_set_default_string(settings, "tts_model", "vits-coqui-en-vctk");
	obs_data_set_default_int(settings, "speaker_id", 0);
	obs_data_set_default_double(settings, "speed", 1.0);
}

obs_properties_t *chaturlate_filter_properties(void *data)
{
	UNUSED_PARAMETER(data);
	
	obs_properties_t *ppts = obs_properties_create();
	
	// Whisper STT section
	obs_properties_t *whisper_group = obs_properties_create();
	obs_properties_add_group(ppts, "whisper_settings", "Speech-to-Text (Whisper)", 
		OBS_GROUP_NORMAL, whisper_group);
	
	obs_property_t *whisper_models = obs_properties_add_list(
		whisper_group, "whisper_model", "Whisper Model",
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(whisper_models, "Base English", "ggml-base.en.bin");
	obs_property_list_add_string(whisper_models, "Small English", "ggml-small.en.bin");
	obs_property_list_add_string(whisper_models, "Medium English", "ggml-medium.en.bin");
	
	obs_properties_add_bool(whisper_group, "vad_enabled", "Enable Voice Activity Detection");
	
	// Translation section
	obs_properties_t *translation_group = obs_properties_create();
	obs_properties_add_group(ppts, "translation_settings", "Translation",
		OBS_GROUP_NORMAL, translation_group);
	
	obs_properties_add_bool(translation_group, "translation_enabled", "Enable Translation");
	obs_properties_add_text(translation_group, "target_lang", "Target Language (e.g., 'en')", 
		OBS_TEXT_DEFAULT);
	
	// TTS section
	obs_properties_t *tts_group = obs_properties_create();
	obs_properties_add_group(ppts, "tts_settings", "Text-to-Speech (TTS)", 
		OBS_GROUP_NORMAL, tts_group);
	
	obs_property_t *tts_model = obs_properties_add_list(
		tts_group, "tts_model", "TTS Model",
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	
	// Add TTS models
	for (auto& model_info : model_infos) {
		obs_property_list_add_string(tts_model, model_info.friendly_name.c_str(),
			model_info.local_folder_name.c_str());
	}
	
	obs_properties_add_int(tts_group, "speaker_id", "Speaker ID", 0, 1000, 1);
	obs_properties_add_float_slider(tts_group, "speed", "Speed", 0.1, 2.5, 0.1);
	
	// Info text
	obs_properties_add_text(ppts, "info", 
		"Hold the 'L' key to record, translate, and speak. Configure hotkey in OBS Settings > Hotkeys.",
		OBS_TEXT_INFO);
	
	return ppts;
}

void chaturlate_filter_update(void *data, obs_data_t *settings)
{
	chaturlate_filter_data *gf = (chaturlate_filter_data *)data;
	
	obs_log(LOG_DEBUG, "[Chaturlate] Updating filter settings");
	
	// Update Whisper model
	std::string whisper_model = obs_data_get_string(settings, "whisper_model");
	if (whisper_model != gf->whisper_model_path) {
		gf->whisper_model_path = whisper_model;
		
		// Reinitialize Whisper
		if (gf->whisper_context) {
			whisper_free(gf->whisper_context);
			gf->whisper_context = nullptr;
		}
		
		// TODO: Load Whisper model from correct path
		// For now, skip initialization - will be done when model downloading is implemented
		obs_log(LOG_INFO, "[Chaturlate] Whisper model set to: %s", whisper_model.c_str());
	}
	
	// Update VAD
	gf->vad_enabled = obs_data_get_bool(settings, "vad_enabled");
	
	// Update translation
	gf->translation_enabled = obs_data_get_bool(settings, "translation_enabled");
	gf->target_lang = obs_data_get_string(settings, "target_lang");
	
	// Update TTS
	std::string new_tts_model = obs_data_get_string(settings, "tts_model");
	if (new_tts_model != gf->tts_context.model_name) {
		destroy_sherpa_tts_context(gf->tts_context);
		gf->tts_context.model_name = new_tts_model;
		gf->tts_context.callback_data = gf;
		init_sherpa_tts_context(gf->tts_context, chaturlate_audio_samples_callback, gf);
	}
	
	gf->speaker_id = (uint32_t)obs_data_get_int(settings, "speaker_id");
	gf->speed = (float)obs_data_get_double(settings, "speed");
}

void chaturlate_filter_activate(void *data)
{
	UNUSED_PARAMETER(data);
	obs_log(LOG_DEBUG, "[Chaturlate] Filter activated");
}

void chaturlate_filter_deactivate(void *data)
{
	UNUSED_PARAMETER(data);
	obs_log(LOG_DEBUG, "[Chaturlate] Filter deactivated");
}

struct obs_audio_data *chaturlate_filter_filter_audio(void *data, struct obs_audio_data *audio)
{
	chaturlate_filter_data *gf = (chaturlate_filter_data *)data;
	
	// If hotkey is pressed, accumulate audio for STT
	if (gf->hotkey_pressed && gf->processing_audio) {
		// Copy audio data to buffer
		size_t frames = audio->frames;
		float *channel_data = (float *)audio->data[0];
		
		std::lock_guard<std::mutex> lock(gf->audio_mutex);
		gf->audio_buffer.insert(gf->audio_buffer.end(), 
			channel_data, channel_data + frames);
	}
	
	// Pass through the audio (or return nullptr to mute)
	return audio;
}

