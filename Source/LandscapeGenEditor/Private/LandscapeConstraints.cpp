#include "LandscapeConstraints.h"

// The current implementation of the landscape generation system is limited by the maximum
// size of a single texture on the GPU, which is 16384x16384 pixels under most Unreal RHIs

uint64 LandscapeConstraints::MaxRasterSizeX() {
	return 16384;
}

uint64 LandscapeConstraints::MaxRasterSizeY() {
	return LandscapeConstraints::MaxRasterSizeX();
}
