#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <Windows.h>
#include <mmsystem.h>
#include "portaudio.h"

#pragma comment(lib, "winmm.lib")

#define SAMPLE_RATE 44100
#define MAX_NOTES 32
#define MAX_RECORD_SECONDS 300

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_SAW
} Waveform;

typedef struct {
    float frequency;
    float phase;
    int active;
} Note;

typedef struct {
    Note notes[MAX_NOTES];
    float volume;
    Waveform waveform;
    int octaveShift;
    int recording;
    float *recordBuffer;
    size_t recordPos;
    size_t recordMaxSamples;
} SynthData;

SynthData synth;

float midiNoteToFreq(int midiNote) {
    return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

void note_on(float freq) {
    for (int n = 0; n < MAX_NOTES; ++n) {
        if (!synth.notes[n].active) {
            synth.notes[n].frequency = freq;
            synth.notes[n].phase = 0.0f;
            synth.notes[n].active = 1;
            break;
        }
    }
}

void note_off(float freq) {
    for (int n = 0; n < MAX_NOTES; ++n) {
        if (synth.notes[n].active && synth.notes[n].frequency == freq) {
            synth.notes[n].active = 0;
            break;
        }
    }
}

void CALLBACK midi_callback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                            DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (wMsg == MIM_DATA) {
        DWORD msg = dwParam1;
        BYTE status = msg & 0xFF;
        BYTE data1 = (msg >> 8) & 0xFF;
        BYTE data2 = (msg >> 16) & 0xFF;
        BYTE command = status & 0xF0;
        float freq = midiNoteToFreq(data1);

        if (command == 0x90 && data2 > 0) {
            note_on(freq);
        } else if (command == 0x80 || (command == 0x90 && data2 == 0)) {
            note_off(freq);
        }
    }
}

float generateWaveSample(float frequency, float time, Waveform waveform) {
    switch (waveform) {
        case WAVE_SINE:
            return sinf(2.0f * (float)M_PI * frequency * time);
        case WAVE_SQUARE:
            return sinf(2.0f * (float)M_PI * frequency * time) >= 0 ? 1.0f : -1.0f;
        case WAVE_SAW:
            return 2.0f * (time * frequency - floorf(time * frequency + 0.5f));
        default:
            return 0.0f;
    }
}

void getNextRecordingFilename(char *filename, size_t size) {
    for (int i = 1; i < 10000; ++i) {
        snprintf(filename, size, "recording%d.wav", i);
        FILE *f = fopen(filename, "rb");
        if (!f) break;
        fclose(f);
    }
}

void writeWavFile(const char *filename, float *data, size_t numSamples) {
    FILE *file = fopen(filename, "wb");
    if (!file) return;

    int sampleRate = SAMPLE_RATE;
    short numChannels = 1;
    short bitsPerSample = 16;
    int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    int blockAlign = numChannels * bitsPerSample / 8;
    int dataSize = numSamples * sizeof(short);

    fwrite("RIFF", 1, 4, file);
    int chunkSize = 36 + dataSize;
    fwrite(&chunkSize, 4, 1, file);
    fwrite("WAVE", 1, 4, file);

    fwrite("fmt ", 1, 4, file);
    int subChunk1Size = 16;
    short audioFormat = 1;
    fwrite(&subChunk1Size, 4, 1, file);
    fwrite(&audioFormat, 2, 1, file);
    fwrite(&numChannels, 2, 1, file);
    fwrite(&sampleRate, 4, 1, file);
    fwrite(&byteRate, 4, 1, file);
    fwrite(&blockAlign, 2, 1, file);
    fwrite(&bitsPerSample, 2, 1, file);

    fwrite("data", 1, 4, file);
    fwrite(&dataSize, 4, 1, file);

    for (size_t i = 0; i < numSamples; ++i) {
        short s = (short)(data[i] * 32767.0f);
        fwrite(&s, sizeof(short), 1, file);
    }

    fclose(file);
    printf("Saved recording to %s\n", filename);
}

static int audioCallback(const void *input, void *output,
                         unsigned long frameCount,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    SynthData *data = (SynthData*)userData;
    float *out = (float*)output;
    static float t = 0.0f;
    float dt = 1.0f / SAMPLE_RATE;

    for (unsigned long i = 0; i < frameCount; ++i) {
        float sample = 0.0f;
        int activeCount = 0;

        for (int j = 0; j < MAX_NOTES; ++j) {
            if (data->notes[j].active) {
                sample += generateWaveSample(data->notes[j].frequency, data->notes[j].phase, data->waveform);
                data->notes[j].phase += dt;
                activeCount++;
            }
        }

        if (activeCount > 0) sample /= activeCount;
        sample *= data->volume;

        *out++ = sample;
        *out++ = sample;

        if (data->recording && data->recordPos < data->recordMaxSamples) {
            data->recordBuffer[data->recordPos++] = sample;
        }

        t += dt;
    }

    return paContinue;
}

float getFrequencyForKey(int vkey, int octaveShift) {
    float baseFreq = 0.0f;
    switch (vkey) {
        case 'A': baseFreq = 261.63f; break;
        case 'S': baseFreq = 293.66f; break;
        case 'D': baseFreq = 329.63f; break;
        case 'F': baseFreq = 349.23f; break;
        case 'G': baseFreq = 392.00f; break;
        case 'H': baseFreq = 440.00f; break;
        case 'J': baseFreq = 493.88f; break;
        case 'K': baseFreq = 523.25f; break;
        case 'W': baseFreq = 277.18f; break;
        case 'E': baseFreq = 311.13f; break;
        case 'T': baseFreq = 369.99f; break;
        case 'Y': baseFreq = 415.30f; break;
        case 'U': baseFreq = 466.16f; break;
    }
    return baseFreq * powf(2.0f, (float)octaveShift);
}

void updateNotesFromKeyboard(SynthData *data) {
    int keys[] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K' };
    int numKeys = sizeof(keys) / sizeof(int);

    for (int i = 0; i < numKeys; ++i) {
        int key = keys[i];
        short state = GetAsyncKeyState(key);
        float freq = getFrequencyForKey(key, data->octaveShift);

        if (state & 0x8000) {
            int found = 0;
            for (int n = 0; n < MAX_NOTES; ++n) {
                if (data->notes[n].active && data->notes[n].frequency == freq) {
                    found = 1;
                    break;
                }
            }
            if (!found) note_on(freq);
        } else {
            note_off(freq);
        }
    }

    // Volume
    if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000 || GetAsyncKeyState('=') & 0x8000) {
        data->volume += 0.01f;
        if (data->volume > 1.0f) data->volume = 1.0f;
    }
    if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000 || GetAsyncKeyState('-') & 0x8000) {
        data->volume -= 0.01f;
        if (data->volume < 0.0f) data->volume = 0.0f;
    }

    // Waveform
    if (GetAsyncKeyState('Z') & 0x8000) data->waveform = WAVE_SINE;
    if (GetAsyncKeyState('X') & 0x8000) data->waveform = WAVE_SQUARE;
    if (GetAsyncKeyState('C') & 0x8000) data->waveform = WAVE_SAW;

    // Octave switching
    static int upWasPressed = 0, downWasPressed = 0;
    int up = GetAsyncKeyState(VK_UP) & 0x8000;
    int down = GetAsyncKeyState(VK_DOWN) & 0x8000;

    if (up && !upWasPressed) {
        data->octaveShift++;
        if (data->octaveShift > 3) data->octaveShift = 3;
        printf("Octave: +%d\n", data->octaveShift);
    }
    if (down && !downWasPressed) {
        data->octaveShift--;
        if (data->octaveShift < -3) data->octaveShift = -3;
        printf("Octave: %d\n", data->octaveShift);
    }

    upWasPressed = up;
    downWasPressed = down;

    // Recording toggle
    static int rWasPressed = 0;
    int r = GetAsyncKeyState('R') & 0x8000;
    if (r && !rWasPressed) {
        if (!data->recording) {
            printf("Recording started...\n");
            data->recordMaxSamples = SAMPLE_RATE * MAX_RECORD_SECONDS * 2;
            data->recordBuffer = (float*)malloc(sizeof(float) * data->recordMaxSamples);
            data->recordPos = 0;
            data->recording = 1;
        } else {
            printf("Recording stopped.\n");
            data->recording = 0;
            char filename[64];
            getNextRecordingFilename(filename, sizeof(filename));
            writeWavFile(filename, data->recordBuffer, data->recordPos);
            free(data->recordBuffer);
            data->recordBuffer = NULL;
        }
    }
    rWasPressed = r;
}

int main() {
    Pa_Initialize();

    synth.volume = 0.5f;
    synth.waveform = WAVE_SINE;
    synth.octaveShift = 0;
    synth.recording = 0;
    synth.recordBuffer = NULL;
    synth.recordPos = 0;
    synth.recordMaxSamples = 0;

    HMIDIIN hMidiDevice;
    if (midiInOpen(&hMidiDevice, 0, (DWORD_PTR)midi_callback, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
        midiInStart(hMidiDevice);
        printf("MIDI device started.\n");
    } else {
        printf("Failed to open MIDI device.\n");
    }

    PaStream *stream;
    Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, 256, audioCallback, &synth);
    Pa_StartStream(stream);

    printf("Synthesizer started.\nUse keys A to K (white keys) and W to U (black keys), or MIDI to play notes.\n");
    printf("Press Z/X/C to change waveform, +/- to change volume, UP-ARROW/DOWN-ARROW for octave.\n");
    printf("Press R to start/stop recording. Press ESC to quit.\n");

    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        updateNotesFromKeyboard(&synth);
        Sleep(10);
    }

    if (synth.recording && synth.recordBuffer) {
        char filename[64];
        getNextRecordingFilename(filename, sizeof(filename));
        writeWavFile(filename, synth.recordBuffer, synth.recordPos);
        free(synth.recordBuffer);
    }

    midiInStop(hMidiDevice);
    midiInClose(hMidiDevice);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
