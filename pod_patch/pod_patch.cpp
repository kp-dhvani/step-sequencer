#include "daisy_pod.h"
#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;

constexpr uint8_t NUM_STEPS = 8;
constexpr float MIN_TEMPO = 100;
constexpr float MAX_TEMPO = 400;
constexpr float BASE_FREQUENCY = 440.0f;
constexpr float MIN_FREQ = 110.0f;
constexpr float MAX_FREQ = 880.0f;

enum Mode
{
	STEP_EDIT,
	GATE_VIEW
};

struct
{
	uint8_t currentStep;
	uint8_t editStep;
	bool active[NUM_STEPS];
	float stepFrequency[NUM_STEPS];
	float phase;
	float tempo;
	Mode mode;
	bool gateState;
} sequencer;

Color stepColors[NUM_STEPS];
Oscillator osc;

void updateLeds()
{
	// edit mode: show selected step
	if (sequencer.mode == STEP_EDIT)
	{
		hw.led1.SetColor(stepColors[sequencer.editStep]);
		if (sequencer.active[sequencer.editStep])
		{
			hw.led2.Set(0, 1, 0); // Green
		}
		else
		{
			hw.led2.Set(0, 0, 0); // Off
		}
	}
	// play mode: show active step + gate state
	else
	{
		hw.led1.SetColor(stepColors[sequencer.currentStep]);
		// led2 shows gate state (green when active)
		if (sequencer.gateState)
		{
			hw.led2.Set(0, 1, 0); // Green
		}
		else
		{
			hw.led2.Set(0, 0, 0); // Off
		}
	}
	hw.UpdateLeds();
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();
	sequencer.tempo = MIN_TEMPO + hw.knob1.Process() * MAX_TEMPO;

	if (hw.encoder.RisingEdge())
	{
		sequencer.mode = sequencer.mode == STEP_EDIT ? GATE_VIEW : STEP_EDIT;
		updateLeds();
	}

	if (sequencer.mode == STEP_EDIT)
	{
		int32_t inc = hw.encoder.Increment();
		if (inc != 0)
		{
			sequencer.editStep = (sequencer.editStep + inc + NUM_STEPS) % NUM_STEPS;
			updateLeds();
		}

		// get knob value (0 to 1)
		float knobValue = hw.knob2.Process();

		// map 0-0.5 to -2 to 0 octaves
		// map 0.5-1 to 0 to +2 octaves
		float octaveOffset;
		if (knobValue < 0.5f)
		{
			// left half: map 0-0.5 to -2-0 octaves
			octaveOffset = -4.0f * (0.5f - knobValue); // gives -2 at CCW, 0 at center
		}
		else
		{
			// right half: map 0.5-1 to 0-2 octaves
			octaveOffset = 4.0f * (knobValue - 0.5f); // gives 0 at center, +2 at CW
		}

		// calculate new frequency based on octave offset
		// we need to convert the linear frequency to logarithmic pitch
		float newFrequency = BASE_FREQUENCY * powf(2.0f, octaveOffset);
		sequencer.stepFrequency[sequencer.editStep] = newFrequency;
	}

	if (hw.button2.RisingEdge() && sequencer.mode == STEP_EDIT)
	{
		sequencer.active[sequencer.editStep] = !sequencer.active[sequencer.editStep];
		updateLeds();
	}

	/**
	 * when we say tempo is 120BPM each beat represent a quarter note
	 * in most electronic music and hence sequencers steps need to align with eighth notes
	 * let's say the tempo is 120
	 * tempo/60 = 2 beats per second
	 * then 2/2 makes each beat take eight of a note
	 * without the last division /2 each step would last a full beat i.e. a quarter note
	 */
	float phaseInc = (sequencer.tempo / 60.0f / 2.0f) / hw.AudioSampleRate();

	for (size_t i = 0; i < size; i++)
	{
		sequencer.phase += phaseInc;

		if (sequencer.phase >= 1.0f)
		{
			sequencer.phase -= 1.0f;
			sequencer.currentStep = (sequencer.currentStep + 1) % NUM_STEPS;
			sequencer.gateState = sequencer.active[sequencer.currentStep];
		}
		// Update oscillator frequency when step changes
		osc.SetFreq(sequencer.stepFrequency[sequencer.currentStep]);

		// create a gate pulse that stays high for 50% of the step duration
		// this ensures a clear rising edge at the start of each active step
		float gateThreshold = 0.5f; // 50% duty cycle
		bool trigger = (sequencer.phase < gateThreshold) && sequencer.gateState;

		// generate audio signal
		float audioOut = osc.Process();
		// only output audio when step is active
		audioOut *= sequencer.active[sequencer.currentStep] ? 0.8f : 0.0f;

		// left channel: clock signal (AUDIO_OUT_L)
		// Output gate signal on both channels
		out[0][i] = trigger ? 5.0f : 0.f;

		// right channel: audio signal (AUDIO_OUT_R)
		out[1][i] = audioOut;

		// update LEDs only when step changes in view mode
		if (sequencer.mode == GATE_VIEW)
		{
			updateLeds();
		}
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(48);
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.StartAdc();

	stepColors[0].Init(Color::RED);
	stepColors[1].Init(Color::GREEN);
	stepColors[2].Init(Color::BLUE);
	stepColors[3].Init(Color::WHITE);
	stepColors[4].Init(1, 0, 1);   // Purple
	stepColors[5].Init(1, 0.8, 0); // Orange
	stepColors[6].Init(1, 1, 0);   // Yellow
	stepColors[7].Init(0, 1, 1);   // Cyan

	sequencer.mode = STEP_EDIT;
	sequencer.tempo = 120;
	sequencer.currentStep = 0;
	osc.Init(hw.AudioSampleRate());
	osc.SetWaveform(Oscillator::WAVE_SIN);
	osc.SetAmp(0.5f);

	for (int i = 0; i < NUM_STEPS; i++)
	{
		sequencer.stepFrequency[i] = BASE_FREQUENCY;
	}

	hw.led1.SetRed(1);
	hw.UpdateLeds();
	hw.StartAudio(AudioCallback);

	while (1)
	{
	}
}