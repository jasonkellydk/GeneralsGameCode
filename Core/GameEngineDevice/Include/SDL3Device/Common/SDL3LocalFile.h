#pragma once

#include "Common/LocalFile.h"

// Local file object used by the SDL3 filesystem backend.
// The actual buffered IO remains in the engine's LocalFile implementation;
// this type gives the SDL3 backend its own allocation/type identity.
class SDL3LocalFile : public LocalFile
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SDL3LocalFile, "SDL3LocalFile")
public:
	SDL3LocalFile();
};
