#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM patch;

constexpr float BASE_FREQUENCY = 440.0f; // reference frequency for CV conversion
constexpr float BASE_VOLTAGE = 0.f;		 // reference voltage (corresponds to 440Hz)

volatile float currentCv = BASE_VOLTAGE;

// internal state variables for frequency detection
float previousSample = 0.f;
bool hadPreviousCrossing = false;
uint32_t lastCrossingPosition = 0;
uint32_t totalSamplesProcessed = 0;

// add smoothing to prevent abrupt CV changes
constexpr float SLEW_RATE = 0.001f; // how fast CV can change per sample
constexpr float SMOOTHING = 0.995f;
float smoothedCv = 0.0f;
float targetCv = 0.0f; // the CV we're moving towards

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	patch.ProcessAllControls();

	for (size_t i = 0; i < size; i++)
	{
		bool state = patch.gate_in_1.State(); // gate signal from Pod

		float audioIn = IN_L[i];

		if (previousSample <= 0.0f && audioIn > 0.0f)
		{
			uint32_t currentPosition = totalSamplesProcessed + i;

			if (hadPreviousCrossing)
			{
				uint32_t periodSamples = currentPosition - lastCrossingPosition;
				float frequency = patch.AudioSampleRate() / (float)periodSamples;

				targetCv = log2f(frequency / BASE_FREQUENCY);
				targetCv = fmaxf(fminf(targetCv, 3.0f), -3.0f);
			}
			else
			{
				hadPreviousCrossing = true;
			}

			lastCrossingPosition = currentPosition;
		}
		// smoothly move current CV towards target CV
		float cvDiff = targetCv - smoothedCv;
		if (fabs(cvDiff) > SLEW_RATE)
		{
			// limit how fast CV can change
			smoothedCv += (cvDiff > 0) ? SLEW_RATE : -SLEW_RATE;
		}
		else
		{
			smoothedCv = targetCv;
		}

		previousSample = audioIn;

		patch.WriteCvOut(CV_OUT_1, smoothedCv);
		patch.WriteCvOut(CV_OUT_2, state ? 5.0f : 0.0f); // LED indicator
		dsy_gpio_write(&patch.gate_out_1, state);		 // Output to GATE_OUT_1
	}
	totalSamplesProcessed += size;
}

int main(void)
{
	patch.Init();
	patch.SetAudioBlockSize(48);
	patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	patch.StartLog();
	patch.PrintLine("init");

	patch.StartAdc();
	patch.StartAudio(AudioCallback);

	while (1)
	{
		System::Delay(1);
	}
}