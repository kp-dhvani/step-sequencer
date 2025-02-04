#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM patch;

constexpr float BASE_FREQUENCY = 440.0f;
float previousSample = 0.f;
int samplesSinceLastCrossing = 0;
constexpr float HYSTERESIS_THRESHOLD = 0.01f;
float currentFrequency = BASE_FREQUENCY;
float samplesInFullCycle = 0.f;

float frequencyToControlVoltage(float frequency)
{
	// Convert frequency to CV using 1V/octave formula
	float cv = log2f(frequency / BASE_FREQUENCY);

	// Scale and offset CV for 0-5V range
	// Typically we want the base frequency (440Hz) to be around 2.5V
	return (cv + 4) * 0.5f;
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	patch.ProcessAllControls();

	for (size_t i = 0; i < size; i++)
	{
		bool state = patch.gate_in_1.State(); // gate signal from Pod
		float audioIn = IN_L[i];			  // audio signal from Pod

		if (audioIn > HYSTERESIS_THRESHOLD && previousSample <= 0.f)
		{
			samplesInFullCycle = patch.AudioSampleRate() * 2.f;
			if (samplesSinceLastCrossing > 0)
			{
				currentFrequency = patch.AudioSampleRate() / (samplesInFullCycle * 2.f);
			}
			// Zero crossing detected
			samplesSinceLastCrossing = 0;
		}
		samplesSinceLastCrossing++;
		previousSample = audioIn;

		float cvOut = frequencyToControlVoltage(currentFrequency);
		cvOut = fclamp(cvOut, 0.0f, 5.0f);
		patch.WriteCvOut(CV_OUT_2, state ? 5.0f : 0.0f); // LED indicator
		dsy_gpio_write(&patch.gate_out_1, state);		 // Output to GATE_OUT_1
		patch.WriteCvOut(CV_OUT_1, cvOut);
	}
}

int main(void)
{
	// Initialize hardware
	patch.Init();
	patch.SetAudioBlockSize(48);
	patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	// Start systems
	patch.StartAdc();
	patch.StartAudio(AudioCallback);

	while (1)
	{
	}
}