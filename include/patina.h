/*
  ==============================================================================

    patina.h
    Created: 27 Mar 2026
    Author:  Patina Team

    Patina library aggregation header
    Simply include this file to make all DSP modules available.

    4-layer architecture:
      core/       — Foundation (compatibility layer, specs, fast math)
      constants/  — L1: Physical constants, IC specs
      parts/      — L2: Analog component primitives
      circuits/   — L3: Circuit modules
      engine/     — L4: Integrated effect engines
      config/     — Cross-cutting: configuration, presets

    Include path configuration:
      At build time: Add project root to include path
      After install: Add <prefix>/include to include path

  ==============================================================================
*/

#pragma once

// =====================================================================
//  Foundation — core/
// =====================================================================
#include "dsp/core/AudioCompat.h"
#include "dsp/core/ProcessSpec.h"
#include "dsp/core/FastMath.h"
#include "dsp/core/DenormalGuard.h"
#include "dsp/core/Oversampler.h"

// =====================================================================
//  L1: Constants — constants/
// =====================================================================
#include "dsp/constants/PartsConstants.h"

// =====================================================================
//  L2: Component primitives — parts/
// =====================================================================
#include "dsp/parts/DiodePrimitive.h"
#include "dsp/parts/RC_Element.h"
#include "dsp/parts/OTA_Primitive.h"
#include "dsp/parts/JFET_Primitive.h"
#include "dsp/parts/BJT_Primitive.h"
#include "dsp/parts/TubeTriode.h"
#include "dsp/parts/TransformerPrimitive.h"
#include "dsp/parts/PhotocellPrimitive.h"
#include "dsp/parts/TapePrimitive.h"
#include "dsp/parts/AnalogVCO.h"
#include "dsp/parts/VcaPrimitive.h"
#include "dsp/parts/OpAmpPrimitive.h"
#include "dsp/parts/InductorPrimitive.h"
#include "dsp/parts/PowerPentode.h"
#include "dsp/parts/VactrolPrimitive.h"

// =====================================================================
//  L3: Circuit modules — circuits/
// =====================================================================

// --- Drive / front end ---
#include "dsp/circuits/drive/DiodeClipper.h"
#include "dsp/circuits/drive/InputBuffer.h"
#include "dsp/circuits/drive/InputFilter.h"
#include "dsp/circuits/drive/OutputStage.h"
#include "dsp/circuits/drive/FrontEnd.h"

// --- filter ---
#include "dsp/circuits/filters/ToneFilter.h"
#include "dsp/circuits/filters/ToneShaper.h"
#include "dsp/circuits/filters/StateVariableFilter.h"
#include "dsp/circuits/filters/LadderFilter.h"
#include "dsp/circuits/filters/PhaserStage.h"
#include "dsp/circuits/filters/OtaSKFilter.h"
#include "dsp/circuits/filters/DiodeLadderFilter.h"
#include "dsp/circuits/filters/AnalogAllPass.h"
#include "dsp/circuits/filters/PassiveLCFilter.h"

// --- Saturation ---
#include "dsp/circuits/saturation/TubePreamp.h"
#include "dsp/circuits/saturation/TransformerModel.h"
#include "dsp/circuits/saturation/TapeSaturation.h"
#include "dsp/circuits/saturation/PushPullPowerStage.h"

// --- Compander / dynamics ---
#include "dsp/circuits/compander/CompanderModule.h"
#include "dsp/circuits/compander/DynamicsSuite.h"
#include "dsp/circuits/compander/EnvelopeFollower.h"
#include "dsp/circuits/compander/TremoloVCA.h"
#include "dsp/circuits/compander/NoiseGate.h"

// --- Analog compressors ---
#include "dsp/circuits/dynamics/PhotoCompressor.h"
#include "dsp/circuits/dynamics/FetCompressor.h"
#include "dsp/circuits/dynamics/VariableMuCompressor.h"
#include "dsp/circuits/dynamics/VcaCompressor.h"

// --- BBD emulation ---
#include "dsp/circuits/bbd/BbdStageEmulator.h"
#include "dsp/circuits/bbd/BbdSampler.h"
#include "dsp/circuits/bbd/BbdPipeline.h"
#include "dsp/circuits/bbd/BbdClock.h"
#include "dsp/circuits/bbd/BbdFeedback.h"
#include "dsp/circuits/bbd/BbdNoise.h"
#include "dsp/circuits/bbd/BbdTimeController.h"

// --- Delay / reverb ---
#include "dsp/circuits/delay/DelayLine.h"
#include "dsp/circuits/delay/OversamplePath.h"
#include "dsp/circuits/delay/SpringReverbModel.h"
#include "dsp/circuits/delay/PlateReverb.h"

// --- Modulation ---
#include "dsp/circuits/modulation/AnalogLfo.h"
#include "dsp/circuits/modulation/ModulationBus.h"
#include "dsp/circuits/modulation/StereoImage.h"
#include "dsp/circuits/modulation/EnvelopeGenerator.h"
#include "dsp/circuits/modulation/RingModulator.h"
#include "dsp/circuits/modulation/MidSideIron.h"
#include "dsp/circuits/modulation/MidSidePrecision.h"

// --- Mixer ---
#include "dsp/circuits/mixer/DryWetMixer.h"
#include "dsp/circuits/mixer/DuckingMixer.h"
#include "dsp/circuits/mixer/GainUtils.h"
#include "dsp/circuits/mixer/Mixer.h"

// --- Power supply / environment modeling ---
#include "dsp/circuits/power/PowerSupplySag.h"
#include "dsp/circuits/power/BatterySag.h"
#include "dsp/circuits/power/AdapterSag.h"
#include "dsp/circuits/power/CapacitorAging.h"

// =====================================================================
//  Cross-cutting: configuration / presets — config/
// =====================================================================
#include "dsp/config/ModdingConfig.h"
#include "dsp/config/Presets.h"

// =====================================================================
//  L4: Integrated engines — engine/
// =====================================================================
#include "dsp/engine/BbdDelayEngine.h"
#include "dsp/engine/DriveEngine.h"
#include "dsp/engine/ReverbEngine.h"
#include "dsp/engine/CompressorEngine.h"
#include "dsp/engine/ModulationEngine.h"
#include "dsp/engine/TapeMachineEngine.h"
#include "dsp/engine/ChannelStripEngine.h"
#include "dsp/engine/FilterEngine.h"
#include "dsp/engine/EqEngine.h"
#include "dsp/engine/LimiterEngine.h"
#include "dsp/engine/EnvelopeGeneratorEngine.h"
