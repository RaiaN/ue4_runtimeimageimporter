// Copyright 2023 Petr Leontev. All Rights Reserved.

#include "WEBPGIFLoader.h"
#include "RuntimeImageLoaderLog.h"

DEFINE_LOG_CATEGORY(LibWebpGifHelper);

#if WITH_LIBWEBP
const int32 FWEBPGIFLoader::GetWidth() const
{
    return Width;
}


const int32 FWEBPGIFLoader::GetHeight() const
{
    return Height;
}


const int32 FWEBPGIFLoader::GetTotalFrames() const
{
    return TotalFrameCount;
}


FString FWEBPGIFLoader::GetDecodeError() const
{
    return FString::Printf(TEXT("WebpGifLoader: %s"), *LastError);
}


const FColor* FWEBPGIFLoader::GetNextFrame(int32 FrameIndex)
{
    if (FrameIndex > GetTotalFrames() - 1)
    {
        FrameIndex = 0;
    }

    // Calculate the starting index of the desired frame in the TextureData array
    int32 StartIndex = FrameIndex * GetWidth() * GetHeight();

    if (StartIndex >= 0 && StartIndex < TextureData.Num())
    {
        return &TextureData[StartIndex];
    }
    else
    {
        // Handling the case where the index is out of bounds
        return &FColor::Black; // return a default FColor value, like FColor::Black
    }
}

const float FWEBPGIFLoader::GetNextFrameDelay(int32 FrameIndex)
{
    return Timestamps[FrameIndex];
}


bool FWEBPGIFLoader::DecodeGIF(TArray<uint8>&& GifBytes)
{
    if (WebPGetInfo(GifBytes.GetData(), GifBytes.Num(), &Width, &Height) == 0)
    {
        SetError("Failed to validate .webp header. Please check input data is valid!");
        return false;
    }

    WebPDecoderConfig DecoderConfig;
    WebPInitDecoderConfig(&DecoderConfig);

    if (WebPGetFeatures(GifBytes.GetData(), GifBytes.Num(), &DecoderConfig.input) != VP8_STATUS_OK)
    {
        SetError("Failed to retrieve features from .webp file. Please check input data is valid!");
        return false;
    }

    if (DecoderConfig.input.has_animation)
    {
        WebPAnimDecoderOptions DecodingOptions;
        if (WebPAnimDecoderOptionsInit(&DecodingOptions) == 0)
        {
            SetError("Failed to initialize decoding options. Version mismatch. Please libwebp is valid!");
            return false;
        }
        DecodingOptions.color_mode = MODE_RGBA;

        WebPData GifData;
        {
            GifData.bytes = GifBytes.GetData();
            GifData.size = GifBytes.Num();
        }

        // Tune 'dec_options' as needed.
        WebPAnimDecoder* Decoder = WebPAnimDecoderNew(&GifData, &DecodingOptions);
        if (Decoder == nullptr)
        {
            SetError("Failed to create WebPAnimDecoder. Please check input data is valid!");
            return false;
        }

        WebPAnimInfo AnimationInfo;
        if (WebPAnimDecoderGetInfo(Decoder, &AnimationInfo) == 0)
        {
            SetError("Failed to retrieve .webp animation info. Please check input data is valid!");
            return false;
        }

        TotalFrameCount = AnimationInfo.frame_count;

        const int32 TotalPixels = TotalFrameCount * Width * Height;
        TextureData.Empty(TotalPixels);
        TextureData.AddUninitialized(TotalPixels);

        //AnimationInfo.loop_count
        for (uint32_t i = 0; i < 1; ++i) 
        {
            int32 FrameInd = 0;
            const int32 FrameBytes = Width * Height * BYTES_PER_PIXEL;

            while (WebPAnimDecoderHasMoreFrames(Decoder)) 
            {
                uint8_t* DecodedData;
                int timestamp;
                if (WebPAnimDecoderGetNext(Decoder, &DecodedData, &timestamp) == 0)
                {
                    SetError("Failed to decode .webp frame. Please check input data is valid!");
                    return false;
                }
                Timestamps.Add(timestamp / 1000.f);
               
                FPlatformMemory::Memcpy((uint8_t*)TextureData.GetData() + FrameInd * FrameBytes, DecodedData, FrameBytes);
                ++FrameInd;
            }
            WebPAnimDecoderReset(Decoder);
        }

        // const WebPDemuxer* demuxer = WebPAnimDecoderGetDemuxer(Decoder);
        // ... (Do something using 'demuxer'; e.g. get EXIF/XMP/ICC data).
        WebPAnimDecoderDelete(Decoder);

        return true;
    }
    Timestamps.Add(100.f);
    TotalFrameCount = 1;

    TextureData.SetNumZeroed(GetWidth() * GetHeight());

    uint8_t* DecodedData = WebPDecodeRGBA(GifBytes.GetData(), GifBytes.Num(), &Width, &Height);
    if (DecodedData == nullptr)
    {
        SetError("Failed to decode .webp file. Please check input data is valid!");
        return false;
    }

    FPlatformMemory::Memcpy((uint8_t*)TextureData.GetData(), DecodedData, Width * Height * BYTES_PER_PIXEL);

    WebPFree(DecodedData);

    return true;
}


bool FWEBPGIFLoader::HasValidWebpHeader(const TArray<uint8>& GifBytes)
{
    int32 WidthTemp, HeightTemp;

    return WebPGetInfo(GifBytes.GetData(), GifBytes.Num(), &WidthTemp, &HeightTemp) == 1 ? true : false;
}

void FWEBPGIFLoader::SetError(const char* error)
{
    LastError = ANSI_TO_TCHAR(error);

    UE_LOG(LibWebpGifHelper, Warning, TEXT("Decode error: %s"), *LastError);
}

#endif //WITH_LIBWEBP