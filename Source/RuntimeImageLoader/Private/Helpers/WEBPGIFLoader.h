// Copyright 2023 Petr Leontev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GIFLoader.h"

THIRD_PARTY_INCLUDES_START
#include "webp/decode.h"
#include "webp/demux.h"
#include "webp/mux_types.h"
THIRD_PARTY_INCLUDES_END

#define BYTES_PER_PIXEL 4


class FWEBPGIFLoader : public IGIFLoader
{
#if WITH_LIBWEBP
public:
	FWEBPGIFLoader() {};
	virtual ~FWEBPGIFLoader() {}

public: /** Gif Data Method */
	const int32 GetWidth() const override;
	const int32 GetHeight() const override;
	const int32 GetTotalFrames() const override;
    FString GetDecodeError() const override;

public: /** Get Next Frame Texture Data*/
    const FColor* GetNextFrame(int32 FrameIndex) override;
	const float GetNextFrameDelay(int32 FrameIndex);
	bool DecodeGIF(TArray<uint8>&& GifBytes) override;

	static bool HasValidWebpHeader(const TArray<uint8>& GifBytes);

private:
	void SetError(const char* error);

private:
	TArray<FColor> TextureData;
	TArray<float> Timestamps;

	int32 Width = -1;
	int32 Height = -1;
	int32 TotalFrameCount = -1;

	FString LastError;

#endif //WITH_LIBWEBP
};