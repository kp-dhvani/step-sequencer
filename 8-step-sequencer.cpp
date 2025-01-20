#include "daisy_pod.h"
#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisy;
using namespace daisysp;

DaisyPod hw;

enum Mode
{
	STEP_EDIT, // for selecting and editing steps
	GATE_VIEW  // for watching the sequence play
};

uint8_t step;
bool active[8];
float tempo = 120.0f;
float phase = 0.0f; // accumulate each part of the step for a single sample
float stepTime = 0.0f;
uint8_t currentStep = 0;
bool gateOutput = false;
Mode currentMode = STEP_EDIT;

Color colors[8];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	hw.ProcessAllControls();

	/**
	 * when we say tempo is 120BPM each beat represent a quarter note
	 * in most electronic music and hence sequencers steps need to align with eighth notes
	 * let's say the tempo is 120
	 * tempo/60 = 2 beats per second
	 * then 2/2 makes each beat take half a step
	 * without the last division /2 each step would last a full beat i.e. a quarter note
	 */
	tempo = hw.knob1.Process() * 300.0f + 40.0f; // range for the tempo
	float stepFrequency = tempo / 60.0f / 2.0f;

	if (hw.encoder.RisingEdge()) // switch between edit and view mode
	{
		currentMode = currentMode == STEP_EDIT ? GATE_VIEW : STEP_EDIT;
		hw.ClearLeds();
		if (currentMode == STEP_EDIT)
		{
			if (active[step])
			{
				hw.led2.SetGreen(1);
			}
			else
			{
				hw.led2.Set(0, 0, 0);
			}
		}
		else
		{
			hw.led1.SetColor(colors[currentStep]); // show playing step
			if (gateOutput)
			{
				hw.led2.SetGreen(1);
			}
			else
			{
				hw.led2.Set(0, 0, 0);
			}
		}
		hw.UpdateLeds();
	}

	int32_t encoderChange = hw.encoder.Increment(); // step through the sequence
	if (encoderChange != 0 && currentMode == STEP_EDIT)
	{
		step = step + encoderChange;
		// wrap around to keep within 0-7 range
		step = (step % 8 + 8) % 8;
		hw.ClearLeds();
		hw.led1.SetColor(colors[step]);
		if (active[step])
		{
			hw.led2.SetGreen(1);
		}
		else
		{
			hw.led2.Set(0, 0, 0);
		}
		hw.UpdateLeds();
	}
	if (hw.button2.RisingEdge() && currentMode == STEP_EDIT) // turn a step on/off
	{
		active[step] = !active[step];

		if (active[step])
		{
			hw.led2.SetGreen(1);
		}
		else
		{
			hw.led2.Set(0, 0, 0);
		}
		hw.UpdateLeds();
	}
	for (size_t i = 0; i < size; i++)
	{
		// increment phase and check for step change
		phase += stepFrequency / hw.AudioSampleRate();
		if (phase >= 1.0f)
		{
			phase -= 1.0f;
			currentStep = (currentStep + 1) % 8;
			gateOutput = active[currentStep];

			// only update LEDs if we are in GATE_VIEW mode
			if (currentMode == GATE_VIEW)
			{
				hw.ClearLeds();
				hw.led1.SetColor(colors[currentStep]);
				if (gateOutput)
				{
					hw.led2.SetGreen(1);
				}
				else
				{
					hw.led2.Set(0, 0, 0);
				}
				hw.UpdateLeds();
			}
		}
		// create a gate pulse that stays high for 50% of the step duration
		// this ensures a clear rising edge at the start of each active step
		float gateThreshold = 0.5f; // 50% duty cycle
		bool currentGateState = (phase < gateThreshold) && gateOutput;

		// generate output with maximum possible swing
		// the sharp transition creates the rising edge my mavis needs
		float gateOut = currentGateState ? 0.95f : -0.95f;
		out[0][i] = gateOut;
		out[1][i] = gateOut;
	}
	// update LED2 for gate output in GATE_VIEW mode
	if (currentMode == GATE_VIEW)
	{
		if (gateOutput)
		{
			hw.led2.SetGreen(1);
		}
		else
		{
			hw.led2.Set(0, 0, 0);
		}
		hw.UpdateLeds();
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4);
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	hw.StartAdc();
	step = 0;
	for (int i = 0; i < 8; i++)
	{
		active[i] = false;
	}

	colors[0].Init(Color::PresetColor::RED);
	colors[1].Init(Color::PresetColor::GREEN);
	colors[2].Init(Color::PresetColor::BLUE);
	colors[3].Init(Color::PresetColor::WHITE);
	colors[4].Init(1, 0, 1);	 // purple
	colors[5].Init(1.0, 0.9, 0); // gold not working, sometimes red sometimes yellow
	colors[6].Init(1, 1, 0);	 // yellow
	colors[7].Init(0, 1, 1);	 // cyan

	hw.ClearLeds();
	hw.led1.SetColor(colors[0]);
	hw.UpdateLeds();
	hw.StartAudio(AudioCallback);
	while (1)
	{
	}
}
