#pragma once
#include "AudioFileSource.h"
#include "AudioOutput.h"
class AudioGeneratorMP3 { public: bool begin(AudioFileSource *, AudioOutput *) { return false; } bool loop() { return false; } bool stop() { return true; } bool isRunning() { return false; } };
