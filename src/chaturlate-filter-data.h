#ifndef CHATURLATE_FILTER_DATA_H
#define CHATURLATE_FILTER_DATA_H

#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <obs.h>
#include <media-io/audio-resampler.h>
#include <whisper.h>

#include "sherpa-tts/sherpa-tts.h"
#include "audio-thread.h"
#include "translation/translation.h"
#include "whisper-utils/whisper-processing.h"
#include "whisper-utils/silero-vad-onnx.h"

struct chaturlate_filter_data {
	obs_source_t *context;
	obs_hotkey_id hotkey_id;
	
	// Audio handling
	std::unique_ptr<AudioThread> audioThread;
	audio_resampler_t *resampler;
	std::vector<float> audio_buffer;
	std::mutex audio_mutex;
	
	// Whisper STT
	struct whisper_context *whisper_context;
	struct whisper_context_params whisper_ctx_params;
	std::string whisper_model_path;
	bool whisper_initialized;
	
	// VAD
	std::unique_ptr<VadIterator> vad;
	bool vad_enabled;
	
	// Translation
	struct ctranslate2::TranslatorPool *translator_pool;
	std::string translation_model_path;
	std::string target_lang;
	bool translation_enabled;
	
	// TTS
	sherpa_tts_context tts_context;
	uint32_t speaker_id;
	float speed;
	
	// State
	bool hotkey_pressed;
	bool mic_muted_by_us;
	bool processing_audio;
	
	chaturlate_filter_data() {
		context = nullptr;
		hotkey_id = OBS_INVALID_HOTKEY_ID;
		resampler = nullptr;
		whisper_context = nullptr;
		whisper_initialized = false;
		translator_pool = nullptr;
		translation_enabled = false;
		hotkey_pressed = false;
		mic_muted_by_us = false;
		processing_audio = false;
		vad_enabled = true;
		speaker_id = 0;
		speed = 1.0f;
		target_lang = "en";
	}
};

#endif // CHATURLATE_FILTER_DATA_H

