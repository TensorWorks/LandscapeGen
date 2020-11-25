#include "GDALDataSource.h"
#include "GDALHeaders.h"
#include "GDALHelpers.h"
#include "LandscapeConstraints.h"

namespace
{
	FString GetProjectionWkt(const GDALDatasetRef& dataset)
	{
		const char* projection = dataset->GetProjectionRef();
		return ((projection != nullptr) ? FString(UTF8_TO_TCHAR(projection)) : FString() );
	}
}

void UGDALDataSource::RetrieveData(FGISDataSourceDelegate OnSuccess, FGISDataSourceDelegate OnFailure)
{
	// Attempt to perform data retrieval
	FGISData data;
	FString error = this->RetrieveDataInternal(data);
	
	//Notify the appropriate delegate of our success or failure
	if (error.IsEmpty()) {
		OnSuccess.Broadcast(error, data);
	}
	else {
		OnFailure.Broadcast(error, data);
	}
}

FString UGDALDataSource::RetrieveDataInternal(FGISData& data)
{
	//------- STEP 1: OPEN DATASETS -------
	
	// Attempt to open the heightmap dataset
	GDALDatasetRef heightmap = mergetiff::DatasetManagement::openDataset(TCHAR_TO_UTF8(*(this->HeightmapDataset)));
	if (!heightmap) {
		return TEXT("Failed to open the heightmap dataset");
	}
	
	// Attempt to open the RGB dataset
	GDALDatasetRef rgb = mergetiff::DatasetManagement::openDataset(TCHAR_TO_UTF8(*(this->RGBDataset)));
	if (!rgb) {
		return TEXT("Failed to open the RGB dataset");
	}
	
	
	//------- STEP 2: RETRIEVE DATASET METADATA -------
	
	// Attempt to retrieve the Well-Known Text (WKT) representation of the projected coordinate system used by the heightmap dataset
	FString heightmapWkt = GetProjectionWkt(heightmap);
	if (heightmapWkt.IsEmpty()) {
		return TEXT("Failed to retrieve the projected coordinate system used by the heightmap dataset");
	}
	
	// Attempt to retrieve the Well-Known Text (WKT) representation of the projected coordinate system used by the RGB dataset
	FString rgbWkt = GetProjectionWkt(rgb);
	if (rgbWkt.IsEmpty()) {
		return TEXT("Failed to retrieve the projected coordinate system used by the RGB dataset");
	}
	
	// Attempt to retrieve the projected corner coordinates for the heightmap dataset
	RasterCornerCoordinatesRef heightmapCorners = GDALHelpers::GetRasterCorners(heightmap);
	if (!heightmapCorners) {
		return TEXT("Failed to compute the projected corner coordinates of the heightmap dataset");
	}
	
	// Attempt to retrieve the projected corner coordinates for the RGB dataset
	RasterCornerCoordinatesRef rgbCorners = GDALHelpers::GetRasterCorners(rgb);
	if (!rgbCorners) {
		return TEXT("Failed to compute the projected corner coordinates of the RGB dataset");
	}
	
	
	//------- STEP 3: VERIFY METADATA VALIDITY -------
	
	// Verify that the two datasets use the same projected coordinate system
	if (heightmapWkt.Equals(rgbWkt) == false) {
		return TEXT("The heightmap dataset and RGB dataset must use the same projected coordinate system");
	}
	
	// Verify that the two datasets have the same corner coordinates
	if (heightmapCorners->UpperLeft != rgbCorners->UpperLeft || heightmapCorners->LowerRight != rgbCorners->LowerRight) {
		return TEXT("The heightmap dataset and RGB dataset must have the same geographic extents");
	}
	
	// Verify that the RGB dataset has at least 3 bands
	if (rgb->GetRasterCount() < 3) {
		return TEXT("RGB dataset must contain R, G and B channels");
	}
	
	// Verify that the heightmap dataset does not exceed the maximum supported raster size for landscape generation
	if (heightmap->GetRasterXSize() > LandscapeConstraints::MaxRasterSizeX() || heightmap->GetRasterYSize() > LandscapeConstraints::MaxRasterSizeY())
	{
		return FString::Printf(
			TEXT("Heightmap raster size of %llux%llu exceeds maximum supported size of %llux%llu"),
			heightmap->GetRasterXSize(),
			heightmap->GetRasterYSize(),
			LandscapeConstraints::MaxRasterSizeX(),
			LandscapeConstraints::MaxRasterSizeY()
		);
	}
	
	// Verify that the RGB dataset does not exceed the maximum supported raster size for landscape generation
	if (rgb->GetRasterXSize() > LandscapeConstraints::MaxRasterSizeX() || rgb->GetRasterYSize() > LandscapeConstraints::MaxRasterSizeY())
	{
		return FString::Printf(
			TEXT("RGB raster size of %llux%llu exceeds maximum supported size of %llux%llu"),
			rgb->GetRasterXSize(),
			rgb->GetRasterYSize(),
			LandscapeConstraints::MaxRasterSizeX(),
			LandscapeConstraints::MaxRasterSizeY()
		);
	}
	
	
	//------- STEP 4: EMIT WARNINGS FOR KNOWN PROBLEMATIC METADATA -------
	
	// Emit a warning if the heightmap data contains nodata values
	int hasNoDataValue = 0;
	double noDataValue = heightmap->GetRasterBand(1)->GetNoDataValue(&hasNoDataValue);
	if (hasNoDataValue) {
		UE_LOG(LogTemp, Warning, TEXT("Heightmap dataset contains nodata values, this can interfere with height scaling!"));
	}
	
	// Emit a warning if the colour interpretation metadata for the raster bands do not indicate an RGB image
	GDALColorInterp expectedInterp[] = { GCI_RedBand, GCI_GreenBand, GCI_BlueBand };
	for (int index = 1; index <= 3; ++index)
	{
		if (rgb->GetRasterBand(index)->GetColorInterpretation() != expectedInterp[index])
		{
			UE_LOG(LogTemp, Warning, TEXT("RGB dataset metadata does not indicate R,G,B ordering for raster bands!"));
			break;
		}
	}
	
	
	//------- STEP 5: RASTER DATA PREPROCESSING -------
	
	// If the heightmap data is not already in Float32 format then convert it
	if (heightmap->GetRasterBand(1)->GetRasterDataType() != GDT_Float32)
	{
		// Attempt to convert the heightmap data to Float32
		heightmap = GDALHelpers::Translate(heightmap, GDALHelpers::UniqueMemFilename(),
			GDALHelpers::ParseGDALTranslateOptions({
				TEXT("-ot"),
				TEXT("Float32"),
			})
		);
		
		// Verify that conversion succeeded
		if (!heightmap) {
			return TEXT("Failed to convert the heightmap to Float32 format");
		}
	}
	
	
	//------- STEP 6: READ HEIGHTMAP RASTER DATA -------
	
	// Retrieve the raster dimensions for the heightmap
	data.HeightBufferX = heightmap->GetRasterXSize();
	data.HeightBufferY = heightmap->GetRasterYSize();
	
	// Create a buffer to hold the heightmap data and wrap it in a RasterData object
	mergetiff::RasterData<float> heightmapData = GDALHelpers::AllocateAndWrap<float>(data.HeightBuffer, 1, data.HeightBufferY, data.HeightBufferX, 0.0f);
	
	// Attempt to read the heightmap data into our buffer
	if (mergetiff::RasterIO::readDataset(heightmap, heightmapData, {1}) == false) {
		return TEXT("Failed to read the data from the heightmap");
	}
	
	
	//------- STEP 7: READ RGB RASTER DATA -------
	
	// Retrieve the raster dimensions for the RGB data
	data.ColorBufferX = rgb->GetRasterXSize();
	data.ColorBufferY = rgb->GetRasterYSize();
	
	// Create a buffer to hold the RGBA data and wrap it in a RasterData object, filling all channels with 255 by default
	data.PixelFormat = EPixelFormat::PF_R8G8B8A8;
	mergetiff::RasterData<uint8> rgbaData = GDALHelpers::AllocateAndWrap<uint8>(data.ColorBuffer, 4, data.ColorBufferY, data.ColorBufferX, 255);
	
	// Attempt to read the RGB data into our buffer, leaving the alpha channel filled with 255
	if (mergetiff::RasterIO::readDataset(rgb, rgbaData, {1,2,3}) == false) {
		return TEXT("Failed to read the data from the RGB dataset");
	}
	
	
	//------- STEP 8: STORE REQUIRED METADATA -------
	
	// Store the corner coordinates
	data.CornerType = ECornerCoordinateType::Projected;
	data.UpperLeft = heightmapCorners->UpperLeft;
	data.LowerRight = heightmapCorners->LowerRight;
	
	// Store the projected coordinate system WKT
	data.ProjectionWKT = heightmapWkt;
	
	// If we reach this point then data retrieval succeeded
	return TEXT("");
}
