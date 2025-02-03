#include "daisy_patch_sm.h"
#include "daisysp.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM patch;

constexpr float BASE_FREQUENCY = 440.0f;

float frequencyToControlVoltage(float frequency)
{
	return log2f(frequency / BASE_FREQUENCY);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	patch.ProcessAllControls();

	for (size_t i = 0; i < size; i++)
	{
		// read input from Pod
		bool state = patch.gate_in_1.State(); // gate signal from Pod
		float audioIn = IN_L[i];			  // audio signal from Pod

		// get CV from frequency
		float cvOut = frequencyToControlVoltage(audioIn);

		dsy_gpio_write(&patch.gate_out_1, state);				// direct gate passthrough to OUT 1
		patch.WriteCvOut(CV_OUT_1, cvOut);						// CV output
		patch.WriteCvOut(CV_OUT_2, state > 0.0f ? 5.0f : 0.0f); // LED
	}
}

int main(void)
{
	patch.Init();
	patch.SetAudioBlockSize(48);
	patch.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

	patch.StartAdc();
	patch.StartAudio(AudioCallback);

	while (1)
	{
	}
}