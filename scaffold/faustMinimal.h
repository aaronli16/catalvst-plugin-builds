/************************************************************************
 IMPORTANT NOTE: This file is the Faust architecture used to generate
 a self-contained C++ header (FaustDSP.h) via:
   faust -i --in-place -a faustMinimal.h effect.dsp -cn FaustDSP -o FaustDSP.h

 The -i flag inlines all #include'd Faust headers, making FaustDSP.h
 fully self-contained (no Faust installation needed at C++ compile time).
 ************************************************************************/

#include <cmath>
#include <cstring>

#include "faust/gui/MapUI.h"
#include "faust/gui/APIUI.h"
#include "faust/gui/meta.h"
#include "faust/dsp/dsp.h"

<<includeIntrinsic>>

<<includeclass>>
