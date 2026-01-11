# cwave
CWave is a simple synthesizer, made in C. It lets you play and record sounds using your computer keyboard or a MIDI device. 

v1.0.0-alpha feature list:
CWave implements the following features in this version:
   1. Different waveforms (sine, square, saw…)
   2. Adding effects like volume, octave-changing
   3. Connecting a keyboard (MIDI control device)
   4. A feature to record a session.

# HOW TO RUN (Windows):

1. Download MSYS2 on your PC.
2. Open msys2.exe in the msys64 folder and run this command:
   ```pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-portaudio```

   it will ask you if you want to install, enter "Y" to install, and let it do its thing.

   test if portaudio is installed by entering this command in the shell:
    ```pacman -Qs portaudio```
    you should get a "mingw-w64-x86_64-portaudio" listed as installed.

4. TEST CODE FOR PORTAUDIO:
    ```
    #include <stdio.h>
    #include "portaudio.h"
    
    int main(void) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            printf("PortAudio error: %s\n", Pa_GetErrorText(err));
            return 1;
        }
        printf("PortAudio initialized successfully!\n");
        Pa_Terminate();
        return 0;
    }
    ```

    open mingw64.exe (in the same folder as msys2.exe) and change directory using cd to wherever you saved the above test file.
    
    run it using this command (assumes file saved to be test.c) :
   ```
    gcc test.c -o test.exe -lportaudio 
    ./test.exe
   ```
    your output should be "PortAudio Initialised successfully!"

4. open mingw64.exe and change directory to the folder where you'll be saving the "synth_stage1.c" file (use cd command to change)

use the following commands to compile the code:

gcc synth_stage1.c -o synth_stage1.exe -lportaudio
./synth_stage1.exe

# NOTE: 
1. to copy and paste, ctrl+c and ctrl+v does not work. select and right click for copy/paste instead.
2. paths must not use backslash, use forward slashes instead.