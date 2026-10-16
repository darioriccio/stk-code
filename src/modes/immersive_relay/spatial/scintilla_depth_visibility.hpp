// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_DEPTH_VISIBILITY_HPP
#define HEADER_SCINTILLA_DEPTH_VISIBILITY_HPP
#include <cmath>

namespace ScintillaSpatial { namespace DepthVisibility {
// Match scintillaDepthVisible in the marker shader. Source RTTs use D24S8;
// historical copies use D24. The comparison intentionally uses float inputs
// and operations on both paths, independently of the CPU picking precision.
inline bool visible(float scene,float z,float n,float f)
{
    if(!std::isfinite(scene)||!std::isfinite(z)||!std::isfinite(n)||!std::isfinite(f))return false;
    if(!(scene>=0.0f && scene<=1.0f && n>0.0f && z>n && z<f))return false;
    // Compare 1-depth without subtracting the marker's nearly-one normalized
    // depth. This stable form retains precision at distant clip-space depths.
    const float depthStep=1.0f/16777215.0f;
    const float arithmeticGuard=8.0f/8388608.0f;
    const float markerGap=(n/(f-n))*((f-z)/z);
    // Two D24 steps cover quantization/conversion uncertainty on the scene
    // side; eight float epsilons guard the short marker arithmetic chain.
    // Both bounds move TOWARD rejection. No positive depth/metric allowance:
    // equal, overlapping or unresolved depth intervals are occluded, even for
    // a clear-depth sample in the final unresolvable slice of the far plane.
    const float sceneGapUpper=(1.0f-scene)+2.0f*depthStep;
    const float markerGapLower=markerGap*(1.0f-arithmeticGuard);
    return markerGapLower>sceneGapUpper;
}
}}
#endif
