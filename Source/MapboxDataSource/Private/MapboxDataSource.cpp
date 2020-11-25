#include "MapboxDataSource.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "GDALHelpers.h"
#include "LandscapeConstraints.h"

namespace
{
	float long2tilexf(double lon, int z)
	{
		return (lon + 180.0) / 360.0 * (1 << z);
	}
	
	float lat2tileyf(double lat, int z)
	{
		double latrad = lat * PI/180.0;
		return (1.0 - asinh(tan(latrad)) / PI) / 2.0 * (1 << z);
	}
	
	int long2tilex(double lon, int z)
	{
		return (int)(floor(long2tilexf(lon, z)));
	}
	
	int lat2tiley(double lat, int z)
	{
		return (int)(floor(lat2tileyf(lat, z)));
	}
	
	double tilex2long(int x, int z)
	{
		return x / (double)(1 << z) * 360.0 - 180;
	}
	
	double tiley2lat(int y, int z)
	{
		double n = PI - 2.0 * PI * y / (double)(1 << z);
		return 180.0 / PI * atan(0.5 * (exp(n) - exp(-n)));
	}
}

const FString UMapboxDataSource::ProjectionWKT = GDALHelpers::WktFromEPSG(3857);

void UMapboxDataSource::RetrieveData(FGISDataSourceDelegate InOnSuccess, FGISDataSourceDelegate InOnFailure)
{
	this->OnSuccess = InOnSuccess;
	this->OnFailure = InOnFailure;
	this->RequestSectionRGBHeight(this->reqUpperLat, this->reqLeftLon, this->reqLowerLat, this->reqRightLon, this->reqZoom);
}

bool UMapboxDataSource::ValidateRequest(FString& OutError)
{
	const int minZoom = 0;
	const int maxZoom = 15;
	
	const float minLat = -85.0511f;
	const float maxLat = 85.0511f;
	const float minLon = -180.0f;
	const float maxLon = 180.0f;
	
	if (this->reqZoom < minZoom || this->reqZoom > maxZoom)
	{
		OutError = FString::Printf(TEXT("Zoom out of valid Range (%d-%d)"), minZoom, maxZoom);
		return false;
	}
	
	if (this->reqUpperLat < this->reqLowerLat)
	{
		OutError = FString(TEXT("Upper Latitude value is lower than Lower Latitude value"));
		return false;
	}
	
	if (this->reqRightLon < this->reqLeftLon)
	{
		OutError = FString(TEXT("Right Longitude value is lower than Left Longitude value"));
		return false;
	}
	
	if (this->reqUpperLat > maxLat || this->reqLowerLat < minLat)
	{
		OutError = FString::Printf(TEXT("Upper or Lower Latitude value out of range (%f-%f)"), minLat, maxLat);
		return false;
	}
	
	if (this->reqRightLon > maxLon || this->reqLeftLon < minLon)
	{
		OutError = FString::Printf(TEXT("Upper or Lower Latitude value out of range (%f-%f)"), minLon, maxLon);
		return false;
	}
	
	return true;
}

void UMapboxDataSource::RequestSectionRGBHeight(float upperLat, float leftLon, float lowerLat, float rightLon, int zoom)
{
	this->hasReqFailed = false;
	
	// Check that request values are valid
	FString ValidationError;
	if (!this->ValidateRequest(ValidationError))
	{
		this->OnFailure.Broadcast(ValidationError, FGISData());
		return;
	}
	
	const int dimx = 256;
	const int dimy = 256;
	
	// Find tile indice bounds for given coordinates and zoom
	float minxf = long2tilexf(leftLon, zoom);
	float minyf = lat2tileyf(upperLat, zoom);
	
	float maxxf = long2tilexf(rightLon, zoom);
	float maxyf = lat2tileyf(lowerLat, zoom);
	
	////////// Make requests produce square results for now /////////////
	
	minxf = (int)(floor(minxf));
	minyf = (int)(floor(minyf));
	
	maxxf = (int)(floor(maxxf));
	maxyf = (int)(floor(maxyf));
	
	// Expand either x or y dimension to produce square result
	bool isXLarger = (maxxf - minxf) >= (maxyf - minyf);
	float largerSize = isXLarger ? (maxxf - minxf) : (maxyf - minyf);
	float& smallerMin = isXLarger ? minyf : minxf;
	float& smallerMax = isXLarger ? maxyf : maxxf;
	
	while (largerSize > (smallerMax - smallerMin))
	{
		if (((int)(smallerMax - smallerMin)) % 2) {
			smallerMin--;
		} else {
			smallerMax++;
		}
	}
	
	////////// Make requests produce square results for now /////////////
	
	int minx = (int)(floor(minxf));
	int miny = (int)(floor(minyf));
	
	int maxx = (int)(floor(maxxf));
	int maxy = (int)(floor(maxyf));
	
	// /v4/{tileset_id}/{zoom}/{x}/{y}{@2x}.{format}
	// https://api.mapbox.com/v4/mapbox.terrain-rgb/{z}/{x}/{y}.pngraw?access_token=YOUR_MAPBOX_ACCESS_TOKEN
	// https://api.mapbox.com/v4/mapbox.satellite/1/0/0@2x.jpg90?access_token=YOUR_MAPBOX_ACCESS_TOKEN
	// Set up parts of RGB and Height requests
	FString BaseURL = TEXT("https://api.mapbox.com/v4/");
	
	FString TextureTilesetId = TEXT("mapbox.satellite");
	FString TextureFormat = TEXT(".jpg90");
	
	FString HeightTilesetId = TEXT("mapbox.terrain-rgb");
	FString HeightFormat = TEXT(".pngraw");
	
	FString ApiKey = this->reqAPIKey;
	
	// Set some useful variables for use by individual requests
	UMapboxDataSource* RequestTask = this;
	RequestTask->OffsetX = minx;
	RequestTask->OffsetY = miny;
	RequestTask->MaxX = maxx - minx + 1;
	RequestTask->MaxY = maxy - miny + 1;
	RequestTask->DimX = dimx * RequestTask->MaxX;
	RequestTask->DimY = dimy * RequestTask->MaxY;
	RequestTask->TileDimX = dimx;
	RequestTask->TileDimY = dimy;
	RequestTask->Zoom = zoom;
	
	// For properly cropped results:
	// RequestTask->MinU = (minxf - ((float)minx)) / ((float)RequestTask->MaxX);
	// RequestTask->MaxU = (maxxf - ((float)maxx)) / ((float)RequestTask->MaxX);
	// RequestTask->MinV = (minyf - ((float)miny)) / ((float)RequestTask->MaxY);
	// RequestTask->MaxV = (maxyf - ((float)maxy)) / ((float)RequestTask->MaxY);
	// (We are currently producing square results)
	
	// Used for cropping, we aren't cropping the results at the moment so just set UV bounds to (0-1)
	RequestTask->MinU = 0.0f;
	RequestTask->MaxU = 1.0f;
	RequestTask->MinV = 0.0f;
	RequestTask->MaxV = 1.0f;
	
	// RequestTask->NumXHeightPixels = abs(maxxf - minxf) * dimx;
	// RequestTask->NumYHeightPixels = abs(maxyf - minyf) * dimy;
	
	// Set size of output arrays for RGB and height data
	RequestTask->NumXHeightPixels = RequestTask->DimX;
	RequestTask->NumYHeightPixels = RequestTask->DimY;

	// check that number of tiles is smaller than max texture size
	if (RequestTask->NumXHeightPixels > LandscapeConstraints::MaxRasterSizeX() || RequestTask->NumYHeightPixels > LandscapeConstraints::MaxRasterSizeY())
	{
		FString ErrString = FString::Printf(TEXT("%d x %d tile dims too large please reduce to 64 x 64 by lowering zoom level or reducing area"), (maxx - minx), (maxy - miny));
		this->OnFailure.Broadcast(ErrString, FGISData());
		return;
	}

	RequestTask->RGBData = TArray<FColor>();
	RequestTask->RGBData.AddDefaulted(RequestTask->NumXHeightPixels * RequestTask->NumYHeightPixels);
	
	RequestTask->HeightData = TArray<float>();
	RequestTask->HeightData.AddDefaulted(RequestTask->NumXHeightPixels * RequestTask->NumYHeightPixels);
	
	// Iterate over range of tile indices and create RGB and height data requests for each
	for (int x = minx; x <= maxx; x++)
	{
		for (int y = miny; y <= maxy; y++)
		{
			FString TextureRequestURL = FString::Printf(TEXT("%s%s/%d/%d/%d%s?access_token=%s"), *BaseURL, *TextureTilesetId, zoom, x, y, *TextureFormat, *ApiKey);
			FMapboxRequestData TextureReqData = { EMapboxRequestDataType::RGB, x, y };
			RequestTask->Start(TextureRequestURL, TextureReqData);
			
			FString HeightRequestURL = FString::Printf(TEXT("%s%s/%d/%d/%d%s?access_token=%s"), *BaseURL, *HeightTilesetId, zoom, x, y, *HeightFormat, *ApiKey);
			FMapboxRequestData HeightReqData = { EMapboxRequestDataType::HEIGHT, x, y };
			RequestTask->Start(HeightRequestURL, HeightReqData);
		}
	}
}

void UMapboxDataSource::Start(FString URL, FMapboxRequestData data)
{
	// Create the Http request and add to pending request list
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	
	UE_LOG(LogTemp, Log, TEXT("Sent Request: %s"), *URL);
	
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UMapboxDataSource::HandleMapboxRequest, data);
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
	
	this->TotalRequests++;
}

void UMapboxDataSource::HandleMapboxRequest(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FMapboxRequestData data)
{
	RemoveFromRoot();
	
	// Check if the HTTP requests succeeded, enter failure state if any request fails
	if ( bSucceeded && HttpResponse.IsValid() && HttpResponse->GetContentLength() > 0 )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrappers[3] =
		{
			ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG),
			ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG),
			ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP),
		};
		
		// Iterate of image wrappers until one is valid for the received data
		for ( auto ImageWrapper : ImageWrappers )
		{
			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed(HttpResponse->GetContent().GetData(), HttpResponse->GetContentLength()) )
			{
				// Determine which tile index we are dealing with
				int TileXIdx = data.RelX - this->OffsetX;
				int TileYIdx = data.RelY - this->OffsetY;
				
				switch (data.DataType)
				{
					case EMapboxRequestDataType::RGB:
					{
						// Get image data
						TArray64<uint8> RawData;
						const ERGBFormat InFormat = ERGBFormat::BGRA;
						ImageWrapper->GetRaw(InFormat, 8, RawData);
						
						// Set up offsets used to calculate correct source and destination indices for cropping
						int YHeightOffsetMin = this->DimY * this->MinV;
						int XHeightOffsetMin = this->DimX * this->MinU;
						int YHeightOffsetMax = ((int)(this->DimY * this->MaxV)) % this->TileDimY;
						int XHeightOffsetMax = ((int)(this->DimX * this->MaxU)) % this->TileDimX;
						
						int TilePixelOffsetX = -XHeightOffsetMin + TileXIdx * this->TileDimX;
						int TilePixelOffsetY = -YHeightOffsetMin + TileYIdx * this->TileDimY;
						
						// Iterate over y indices of source image that we want to read from (ignoring cropped indices)
						for (int y = TileYIdx ? 0 : YHeightOffsetMin; y < ((TileYIdx + 1 == this->MaxY && YHeightOffsetMax) ? YHeightOffsetMax : this->TileDimY); y++)
						{
							
							// Set y location to start writing to in destination array
							auto DestPtrIdx = ((int64)(y + TilePixelOffsetY)) * this->NumXHeightPixels + TilePixelOffsetX;
							FColor* DestPtr = &((FColor*)this->RGBData.GetData())[DestPtrIdx];
							
							// Set y location to start reading from in source image
							auto SrcPtrIdx = ((int64)(y)) * this->TileDimX + (TileXIdx ? 0 : XHeightOffsetMin);
							const FColor* SrcPtr = &((FColor*)(RawData.GetData()))[SrcPtrIdx];
							
							// Check that we don't aren't reading out of bounds data from source data
							if (SrcPtrIdx >= (256 * 256)) {
								UE_LOG(LogTemp, Error, TEXT("Accessing Height data out of bounds"))
							}
							
							// Check that we don't aren't writing to out of bounds indices in the destination array
							if (DestPtrIdx >= (this->NumXHeightPixels * this->NumYHeightPixels)) {
								UE_LOG(LogTemp, Error, TEXT("Writing Height data to out of bounds address"))
							}
							
							// Iterate over x indices of source image that we want to read from (ignoring cropped indices)
							for (int x = TileXIdx ? 0 : XHeightOffsetMin; x < ((TileXIdx + 1 == this->MaxX && XHeightOffsetMax) ? XHeightOffsetMax : this->TileDimX); x++)
							{
								*DestPtr++ = FColor(SrcPtr->R, SrcPtr->G, SrcPtr->B, SrcPtr->A);
								SrcPtr++;
							}
						}
						
						break;
					}
					case EMapboxRequestDataType::HEIGHT:
					{
						// Get image data
						TArray64<uint8> RawData;
						const ERGBFormat InFormat = ERGBFormat::BGRA;
						ImageWrapper->GetRaw(InFormat, 8, RawData);
						
						// Set up offsets used to calculate correct source and destination indices for cropping
						int YHeightOffsetMin = this->DimY * this->MinV;
						int XHeightOffsetMin = this->DimX * this->MinU;
						int YHeightOffsetMax = ((int)(this->DimY * this->MaxV)) % this->TileDimY;
						int XHeightOffsetMax = ((int)(this->DimX * this->MaxU)) % this->TileDimX;
						
						int TilePixelOffsetX = -XHeightOffsetMin + TileXIdx * this->TileDimX;
						int TilePixelOffsetY = -YHeightOffsetMin + TileYIdx * this->TileDimY;
						
						// Iterate over y indices of source image that we want to read from (ignoring cropped indices)
						for (int y = TileYIdx ? 0 : YHeightOffsetMin; y < ((TileYIdx + 1 == this->MaxY && YHeightOffsetMax) ? YHeightOffsetMax : this->TileDimY); y++)
						{
							// Set y location to start writing to in destination array
							auto DestPtrIdx = ((int64)(y + TilePixelOffsetY)) * this->NumXHeightPixels + TilePixelOffsetX;
							float* DestPtr = &((float*)this->HeightData.GetData())[DestPtrIdx];
							
							// Set y location to start reading from in source image
							auto SrcPtrIdx = ((int64)(y)) * this->TileDimX + (TileXIdx ? 0 : XHeightOffsetMin);
							const FColor* SrcPtr = &((FColor*)(RawData.GetData()))[SrcPtrIdx];
							
							// Check that we are not reading out of bounds data from src data
							if (SrcPtrIdx >= (256 * 256)) {
								UE_LOG(LogTemp, Error, TEXT("Accessing Height data out of bounds"))
							}
							
							// Check that we are not writing to out of bounds indices in the destination array
							if (DestPtrIdx >= (this->NumXHeightPixels * this->NumYHeightPixels)) {
								UE_LOG(LogTemp, Error, TEXT("Writing Height data to out of bounds address"))
							}
							
							// Iterate over x indices of source image that we want to read from (ignoring cropped indices)
							for (int x = TileXIdx ? 0 : XHeightOffsetMin; x < ((TileXIdx + 1 == this->MaxX && XHeightOffsetMax) ? XHeightOffsetMax : this->TileDimX); x++)
							{
								*DestPtr++ = -10000.0f + ((SrcPtr->R * 256 * 256 + SrcPtr->G * 256 + SrcPtr->B) * 0.1f);
								SrcPtr++;
							}
						}
						break;
					}
					default:
					{
						break;
					}
				}
				
				this->CompletedRequests++;
				UE_LOG(LogTemp, Log, TEXT("Completed Request, total resolved: %d"), this->CompletedRequests);
				
				// All requests complete, collate results and broadcast to success delegate
				if (this->CompletedRequests >= this->TotalRequests || this->bIgnoreMissingTiles)
				{
					UE_LOG(LogTemp, Log, TEXT("MAPBOX REQUEST COMPLETE"));
					
					FGISData OutData;
					OutData.HeightBuffer = this->HeightData;
					OutData.HeightBufferX = this->NumXHeightPixels;
					OutData.HeightBufferY = this->NumYHeightPixels;
					OutData.ColorBuffer = TArray<uint8>((uint8*)this->RGBData.GetData(), this->RGBData.Num() * sizeof(FColor));
					OutData.ColorBufferX = this->NumXHeightPixels;
					OutData.ColorBufferY = this->NumYHeightPixels;
					OutData.ProjectionWKT = UMapboxDataSource::ProjectionWKT;
					OutData.CornerType = ECornerCoordinateType::LatLon;
					OutData.UpperLeft = FVector2D(tiley2lat(this->OffsetY, this->Zoom), tilex2long(this->OffsetX, this->Zoom));
					OutData.LowerRight = FVector2D(tiley2lat(this->OffsetY+this->MaxY, this->Zoom), tilex2long(this->OffsetX+this->MaxX, this->Zoom));
					OutData.PixelFormat = EPixelFormat::PF_B8G8R8A8;
					
					this->OnSuccess.Broadcast(FString(), OutData);
				}
				return;
			}
		}
	}
	
	if (!this->hasReqFailed)
	{
		this->hasReqFailed = true;
		FString FailString = bSucceeded 
			? FString(TEXT("One or more requests failed: Unable to process image"))
			: FString(TEXT("One or more requests failed: Web request failed"));
		this->OnFailure.Broadcast(FailString, FGISData());
	}
}
