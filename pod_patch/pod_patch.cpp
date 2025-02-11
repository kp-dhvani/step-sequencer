#include "daisy_pod.h"
#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;

constexpr bool DEBUG_ENABLED = true;

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

constexpr uint8_t NUM_INTERVALS = 7;

/**
 * use the knob2 to choose the pitch of the oscillator
 * going counter clockwise will go below the root/octave (440Hz)
 * going clockwise will go up the root/octave
 */
float sevenNoteScale[7] = {
	1.0f,		 // Root
	9.0f / 8.0f, // Major Second (1.125)
	5.0f / 4.0f, // Major Third (1.25)
	4.0f / 3.0f, // Perfect Fourth (1.333)
	3.0f / 2.0f, // Perfect Fifth (1.5)
	5.0f / 3.0f, // Major Sixth (1.666)
	15.0f / 8.0f // Major Seventh (1.875)
};

struct
{
	uint8_t currentStep;
	uint8_t editStep; // current editing step
	bool active[NUM_STEPS];
	float stepFrequency[NUM_STEPS];
	float phase; // use phase to calculate the duration of each step for one block of the sample
	float tempo;
	Mode mode;
	bool gateState;
	int8_t interval[NUM_STEPS]; // store the pitch interval for each step with the base as 440Hz
} sequencer;

Color stepColors[NUM_STEPS];
Oscillator osc;

void DebugPrint(const char *format, ...)
{
	if (!DEBUG_ENABLED)
		return;

	char buffer[256];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	hw.seed.PrintLine(buffer);
}

float getIntervalFrequency(int interval)
{
	if (interval == 0)
	{
		return BASE_FREQUENCY;
	}
	bool isGoingUp = interval > 0;
	uint8_t position = abs(interval);
	float ratio = sevenNoteScale[position - 1];
	if (!isGoingUp)
	{
		ratio = (1.0f / ratio) / 2.0f; // invert the ratio when going below the octave
	}

	return BASE_FREQUENCY * ratio;
}

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

	if (hw.encoder.RisingEdge())
	{
		sequencer.mode = sequencer.mode == STEP_EDIT ? GATE_VIEW : STEP_EDIT;
		updateLeds();
	}

	if (sequencer.mode == STEP_EDIT)
	{
		float valueForTempo = hw.knob1.Process();
		sequencer.tempo = MIN_TEMPO + valueForTempo * (MAX_TEMPO - MIN_TEMPO);
		int32_t inc = hw.encoder.Increment();
		if (inc != 0)
		{
			sequencer.editStep = (sequencer.editStep + inc + NUM_STEPS) % NUM_STEPS;
			updateLeds();
		}

		// use the knob2 to set the interval for each step in the sequence
		float valueForInterval = hw.knob2.Process();

		int calculatedInterval = (int)roundf((valueForInterval - 0.5f) * (2 * NUM_INTERVALS));
		sequencer.interval[sequencer.editStep] = calculatedInterval;
		// with the given interval get the frequency value
		sequencer.stepFrequency[sequencer.editStep] = getIntervalFrequency(sequencer.interval[sequencer.editStep]);
	}

	if (hw.button2.RisingEdge() && sequencer.mode == STEP_EDIT)
	{
		sequencer.active[sequencer.editStep] = !sequencer.active[sequencer.editStep];
		updateLeds();
	}

	/**
	 * when we say tempo is 120BPM each beat represent a quarter note
	 * this is an 8 step sequencer so the each step has to eighth of a note
	 * let's say the tempo is 120
	 * tempo/60 = 2 beats per second and each beat is a quarter not
	 * then 2/2 makes each beat take eight of a note
	 * without the last division /2 each step would last a quarter note
	 */
	float phaseInc = ((sequencer.tempo / 60.0f) / 2.0f) / hw.AudioSampleRate();

	for (size_t i = 0; i < size; i++)
	{
		sequencer.phase += phaseInc;
		if (sequencer.phase >= 1.0f)
		{
			sequencer.phase -= 1.0f;
			sequencer.currentStep = (sequencer.currentStep + 1) % NUM_STEPS;
			sequencer.gateState = sequencer.active[sequencer.currentStep];
		}
		// update oscillator frequency when step changes
		osc.SetFreq(sequencer.stepFrequency[sequencer.currentStep]);

		// create a gate pulse that stays high for 50% of the step duration
		// this ensures a clear rising edge at the start of each active step
		float gateThreshold = 0.5f; // 50% duty cycle
		bool trigger = (sequencer.phase < gateThreshold) && sequencer.gateState;

		// generate audio signal
		float audioOut = osc.Process();
		// only output audio when step is active
		audioOut *= sequencer.active[sequencer.currentStep] ? 0.9f : 0.0f;

		// left channel: gate signal (AUDIO_OUT_L)
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
	hw.knob1.SetSampleRate(48000);

	hw.seed.StartLog(false);

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
	sequencer.editStep = 0;
	osc.Init(hw.AudioSampleRate());
	osc.SetWaveform(Oscillator::WAVE_SIN);
	osc.SetAmp(0.95f);

	for (int i = 0; i < NUM_STEPS; i++)
	{
		sequencer.stepFrequency[i] = BASE_FREQUENCY;
		sequencer.active[i] = false;
	}

	hw.led1.SetRed(1);
	hw.UpdateLeds();
	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}