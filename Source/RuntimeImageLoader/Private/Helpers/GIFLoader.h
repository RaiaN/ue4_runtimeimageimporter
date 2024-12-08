// Copyright 2023 Petr Leontev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IGIFLoader
{
public:
	virtual ~IGIFLoader() = default;

	virtual bool DecodeGIF(TArray<uint8>&& GifBytes) = 0;
	virtual FString GetDecodeError() const = 0;
	virtual const int32 GetWidth() const = 0;
	virtual const int32 GetHeight() const = 0;
	virtual const int32 GetTotalFrames() const = 0;
	virtual const FColor* GetNextFrame(int32 FrameIndex) = 0;
	virtual const float GetNextFrameDelay(int32 FrameIndex) = 0;
};

class FGIFLoaderFactory
{
public:
	static TUniquePtr<IGIFLoader> CreateLoader(const FString& GifURI, const TArray<uint8>& GifData);
};
