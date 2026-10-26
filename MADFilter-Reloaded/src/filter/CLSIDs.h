// CLSIDs.h — COM class identifiers for MADFilter Reloaded
//
// These are entirely new GUIDs, distinct from the original MADFilter.
// The original CLSID {D1A36F81-56AE-11d6-8C1F-00104B49AA66} is NOT used
// by default to avoid hijacking existing installations.

#pragma once
#include <initguid.h>

// {4FAC742E-1B8B-4DE8-AD7B-D0158F7C2448}
DEFINE_GUID(CLSID_MADFilterReloaded,
    0x4fac742e, 0x1b8b, 0x4de8, 0xad, 0x7b, 0xd0, 0x15, 0x8f, 0x7c, 0x24, 0x48);

// Filter name shown in GraphEdit / DirectShow property sheets
#define MADFILTER_RELOADED_NAME  L"MADFilter Reloaded"
