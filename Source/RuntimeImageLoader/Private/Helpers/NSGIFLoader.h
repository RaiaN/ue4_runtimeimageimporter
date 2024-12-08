// Copyright 2023 Petr Leontev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GIFLoader.h"

THIRD_PARTY_INCLUDES_START
extern "C" {
	#include "nsgif.h"
}
THIRD_PARTY_INCLUDES_END

#define BYTES_PER_PIXEL 4


class FNSGIFLoader : public IGIFLoader
{
#if WITH_LIBNSGIF
public:
	FNSGIFLoader() {};
	virtual ~FNSGIFLoader() {}

public: /** Gif Data Method */
	const int32 GetWidth() const override;
	const int32 GetHeight() const override;
	const int32 GetTotalFrames() const override;
    FString GetDecodeError() const override;

public: /** Get Next Frame Texture Data*/
    const FColor* GetNextFrame(int32 FrameIndex) override;
	const float GetNextFrameDelay(int32 FrameIndex) override;
	bool DecodeGIF(TArray<uint8>&& GifBytes) override;

protected: /** Bitmap Callbacks Methods */
	static void* bitmap_create(int width, int height);
	static unsigned char* bitmap_get_buffer(void* bitmap);
	static void bitmap_destroy(void* bitmap);
	
	bool DecodeInternal(nsgif_t* gif, bool first);

	void Warning(const char* context);

private: /** Gif Data*/
	nsgif_t* Gif;
	const nsgif_info_t* Info;
	TArray<FColor> TextureData;
	TArray<float> Timestamps;

	int32 Width = -1;
	int32 Height = -1;
	int32 TotalFrameCount = -1;

	nsgif_error LastError;
	FString LastContext;

private:
	const nsgif_bitmap_cb_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_get_buffer,
	};

#endif //WITH_LIBNSGIF
};