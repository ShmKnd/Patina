#pragma once
// Mixer.h — Backward compatibility wrapper
// mixer/DryWetMixer.h, DuckingMixer.h, GainUtils.h has been split into
// struct Mixer inherits and provides all functionality.

#include "GainUtils.h"

struct Mixer : GainUtils {};
