#pragma once

#include "CoreMinimal.h"

class LandscapeConstraints
{
public:
	
	// Determines the maximum supported width in pixels of raster data used to generate landscapes
	// (Returns zero if there is no limit in the current environment)
	static LANDSCAPEGENEDITOR_API uint64 MaxRasterSizeX();
	
	// Determines the maximum supported height in pixels of raster data used to generate landscapes
	// (Returns zero if there is no limit in the current environment)
	static LANDSCAPEGENEDITOR_API uint64 MaxRasterSizeY();
};
