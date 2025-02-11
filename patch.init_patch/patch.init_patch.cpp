#include "daisy_patch_sm.h"
#include "daisysp.h"
#include <cmath>

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM patch;

// Constants for audio processing
constexpr float BASE_FREQUENCY = 440.0f;
constexpr float BASE_VOLTAGE = 0.f;

// Enhanced debugging counters
volatile uint32_t audioCallbackCount = 0;
volatile uint32_t sampleCount = 0;
volatile uint32_t zeroCrossings = 0;
volatile uint32_t lastGateState = 0;
volatile float lastAudioPeak = 0.0f;

// Audio processing state
float previousSample = 0.f;
float smoothedCv = 0.0f;
float targetCv = 0.0f;
constexpr float SLEW_RATE = 0.001f;

// Enhanced thresholds
constexpr float AUDIO_THRESHOLD = 0.01f;  // Minimum audio level to consider
constexpr float ZERO_CROSS_HYST = 0.001f; // Hysteresis for zero crossing
constexpr float MIN_PERIOD = 2.4f;		  // Minimum period (maximum frequency ~20kHz)
constexpr float MAX_PERIOD = 2400.0f;	  // Maximum period (minimum frequency ~20Hz)

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	patch.ProcessAllControls();
	audioCallbackCount++;

	// Get gate state and detect changes
	bool currentGateState = patch.gate_in_1.State();
	if (currentGateState != lastGateState)
	{
		patch.PrintLine("Gate changed: %d", currentGateState);
		lastGateState = currentGateState;
	}

	// Audio analysis for this block
	float maxLevel = 0.0f;
	float rms = 0.0f;

	for (size_t i = 0; i < size; i++)
	{
		float audioIn = IN_L[i];
		maxLevel = std::fmax(maxLevel, fabs(audioIn));
		rms += audioIn * audioIn;
	}
	rms = sqrt(rms / float(size));

	// Log audio levels periodically
	if (audioCallbackCount % 500 == 0 || maxLevel > (lastAudioPeak * 1.5f))
	{
		patch.PrintLine("Audio - Peak: %f RMS: %f", maxLevel, rms);
		lastAudioPeak = maxLevel;
	}

	// Process each sample
	for (size_t i = 0; i < size; i++)
	{
		float audioIn = IN_L[i];
		sampleCount++;

		// Enhanced zero crossing detection with hysteresis
		if (previousSample <= -ZERO_CROSS_HYST && audioIn >= ZERO_CROSS_HYST &&
			fabs(audioIn) > AUDIO_THRESHOLD)
		{

			zeroCrossings++;
			float periodSamples = sampleCount;
			sampleCount = 0; // Reset counter

			// Validate period is within reasonable range
			if (periodSamples >= MIN_PERIOD && periodSamples <= MAX_PERIOD)
			{
				float frequency = patch.AudioSampleRate() / periodSamples;

				// Calculate and validate CV
				float newTargetCv = log2f(frequency / BASE_FREQUENCY);
				newTargetCv = fmaxf(fminf(newTargetCv, 5.0f), -5.0f);

				// Only update if CV changed significantly
				if (fabs(newTargetCv - targetCv) > 0.01f)
				{
					targetCv = newTargetCv;
					patch.PrintLine("F:%f Hz CV:%f V ZC:%lu", frequency, targetCv, zeroCrossings);
				}
			}
		}

		// Smooth CV changes
		float cvDiff = targetCv - smoothedCv;
		smoothedCv += cvDiff * SLEW_RATE;

		previousSample = audioIn;

		// Write outputs with validation
		float cvOut = fmaxf(fminf(smoothedCv, 5.0f), -5.0f);
		patch.WriteCvOut(CV_OUT_1, cvOut);
		patch.WriteCvOut(CV_OUT_2, currentGateState ? 5.0f : 0.0f);
		dsy_gpio_write(&patch.gate_out_1, currentGateState);
	}

	// Periodic diagnostics
	if (audioCallbackCount % 1000 == 0)
	{
		patch.PrintLine("Stats - ZC:%lu Samples:%lu CV:%f",
						zeroCrossings, sampleCount, smoothedCv);
	}
}

int main(void)
{
	patch.Init();
	patch.StartLog(true);
	patch.PrintLine("System starting...");

	patch.SetAudioBlockSize(48);
	patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	patch.PrintLine("Audio configured: %d Hz", patch.AudioSampleRate());

	patch.StartAdc();
	patch.PrintLine("ADC started");
	patch.StartAudio(AudioCallback);
	patch.PrintLine("Audio callback started");

	// Visual indicator
	// patch.SetLed(true);

	while (1)
	{
		// patch.PrintLine("Heartbeat - System active");
		System::Delay(1000);
	}
}