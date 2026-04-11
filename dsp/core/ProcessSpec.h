#pragma once

namespace patina {

struct ProcessSpec {
    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;
};

} // namespace patina
