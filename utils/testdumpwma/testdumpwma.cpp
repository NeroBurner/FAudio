#include <stdio.h>
#include <SDL.h>

#include <FAudio.h>

float argPlayBegin = 0.0f;
float argPlayLength = 0.0f;
float argLoopBegin = 0.0f;
float argLoopLength = 0.0f;
uint32_t argLoopCount = 0;

FAudio *faudio = NULL;
FAudioMasteringVoice *mastering_voice = NULL;
FAudioSourceVoice *source_voice = NULL;

FAudioWaveFormatExtensible wfx = {0};
FAudioBuffer buffer = {0};
FAudioBufferWMA buffer_wma = {0};

/* based on https://docs.microsoft.com/en-us/windows/desktop/xaudio2/how-to--load-audio-data-files-in-xaudio2 */
#define fourccRIFF *((uint32_t *) "RIFF")
#define fourccDATA *((uint32_t *) "data")
#define fourccFMT  *((uint32_t *) "fmt ")
#define fourccWAVE *((uint32_t *) "WAVE")
#define fourccXWMA *((uint32_t *) "XWMA")
#define fourccDPDS *((uint32_t *) "dpds")

uint32_t load_data()
{
	/* Locate the 'fmt ' chunk, and copy its contents into a WAVEFORMATEXTENSIBLE structure. */

	FAudioWaveFormatEx &fmt = wfx.Format;
	fmt.wFormatTag = FAUDIO_FORMAT_WMAUDIO2;
	//wfx.Format.wFormatTag = FAUDIO_FORMAT_EXTENSIBLE;
	fmt.nChannels = 2;
	fmt.nSamplesPerSec = 44100;
	fmt.wBitsPerSample = 32;
	fmt.nBlockAlign = fmt.nChannels * (fmt.wBitsPerSample / 8);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
	fmt.cbSize = 22;

	// Extended
	wfx.Samples.wValidBitsPerSample = 0;
	wfx.dwChannelMask = 0;
	// Extended.GUID
	wfx.SubFormat = { \
		(uint16_t) (FAUDIO_FORMAT_WMAUDIO2),
		0x0000,
		0x0010,
		{
			0x80,
			0x00,
			0x00,
			0xAA,
			0x00,
			0x38,
			0x9B,
			0x71
		}
	};
	//wfx.SubFormat.Data1 = FAUDIO_FORMAT_WMAUDIO2;
	//wfx.SubFormat.Data2 = 2;
	//wfx.SubFormat.Data3 = 3;
	//SDL_memset(wfx.SubFormat.Data4, static_cast<uint8_t>(0), 8);

	/* data */
	uint32_t dwChunkSize = 8 * fmt.nBlockAlign;
	uint8_t *pDataBuffer = static_cast<uint8_t *>(malloc(dwChunkSize));
	SDL_memset(pDataBuffer, static_cast<uint8_t>(0), dwChunkSize);

	buffer.AudioBytes = dwChunkSize;  //buffer containing audio data
	buffer.pAudioData = pDataBuffer;  //size of the audio buffer in bytes
	buffer.Flags = FAUDIO_END_OF_STREAM; // tell the source voice not to expect any data after this buffer

	/* 'dpds' chunk */
	{
		uint32_t wma_chunk_size = dwChunkSize / fmt.nBlockAlign;
		uint32_t *p_wma_buffer = static_cast<uint32_t *>(malloc(sizeof(uint32_t)*wma_chunk_size));
		SDL_memset(p_wma_buffer, static_cast<uint8_t>(0), sizeof(uint32_t)*wma_chunk_size);
		buffer_wma.pDecodedPacketCumulativeBytes = p_wma_buffer;
		buffer_wma.PacketCount = wma_chunk_size;
	}

	return 0;
}

void faudio_setup() {
	uint32_t hr = FAudioCreate(&faudio, 0, FAUDIO_DEFAULT_PROCESSOR);
	if (hr != 0) {
		return;
	}

	hr = FAudio_CreateMasteringVoice(faudio, &mastering_voice, 2, 44100, 0, 0, NULL);
	if (hr != 0) {
		return;
	}

	hr = FAudio_CreateSourceVoice(
		faudio, 
		&source_voice, 
		(FAudioWaveFormatEx *) &wfx, 
		FAUDIO_VOICE_USEFILTER, 
		FAUDIO_MAX_FREQ_RATIO, 
		NULL, NULL, NULL
	);
}

void play(void) {

	buffer.PlayBegin = argPlayBegin * wfx.Format.nSamplesPerSec;
	buffer.PlayLength = argPlayLength * wfx.Format.nSamplesPerSec;

	buffer.LoopBegin = argLoopBegin * wfx.Format.nSamplesPerSec;
	buffer.LoopLength = argLoopLength * wfx.Format.nSamplesPerSec;
	buffer.LoopCount = argLoopCount;

	if (buffer_wma.pDecodedPacketCumulativeBytes != NULL)
		FAudioSourceVoice_SubmitSourceBuffer(source_voice, &buffer, &buffer_wma);
	else
		FAudioSourceVoice_SubmitSourceBuffer(source_voice, &buffer, NULL);

	uint32_t hr = FAudioSourceVoice_Start(source_voice, 0, FAUDIO_COMMIT_NOW);

	int is_running = 1;
	while (hr == 0 && is_running) {
		FAudioVoiceState state;
		FAudioSourceVoice_GetState(source_voice, &state, 0);
		is_running = (state.BuffersQueued > 0) != 0;
		SDL_Delay(10);
	}

	FAudioVoice_DestroyVoice(source_voice);
}

int main(int argc, char *argv[]) {
	load_data();
	faudio_setup();
	play();

	return 0;
}
