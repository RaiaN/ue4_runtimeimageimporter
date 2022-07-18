// Copyright 2022 Peter Leontev. All Rights Reserved.

#include "RuntimeImageReader.h"

#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "RenderUtils.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "PixelFormat.h"
#include "TextureResource.h"
#include "Launch/Resources/Version.h"
#include "Async/Async.h"
#include "Containers/ResourceArray.h"

#include "ImageReaders/ImageReaderFactory.h"
#include "ImageReaders/IImageReader.h"
#include "RuntimeImageUtils.h"
#include "RuntimeTextureResource.h"
#include "RuntimeTextureFactory.h"



DEFINE_LOG_CATEGORY_STATIC(LogRuntimeImageReader, Log, All);

void URuntimeImageReader::Initialize()
{
    TextureFactory = NewObject<URuntimeTextureFactory>(GetTransientPackage());

    ThreadSemaphore = FPlatformProcess::GetSynchEventFromPool(false);
    Thread = FRunnableThread::Create(this, TEXT("RuntimeImageReader"), 0, TPri_SlightlyBelowNormal);

    UE_LOG(LogRuntimeImageReader, Log, TEXT("Image reader thread started!"))
}

void URuntimeImageReader::Deinitialize()
{
    Clear();
    Stop();

    UE_LOG(LogRuntimeImageReader, Log, TEXT("Image reader thread exited!"))
}

bool URuntimeImageReader::Init()
{
    return true;
}

uint32 URuntimeImageReader::Run()
{
    while (!bStopThread)
    {
        ThreadSemaphore->Wait();
        
        BlockTillAllRequestsFinished();
    }

    return 0;
}

void URuntimeImageReader::Exit()
{
    // 
}

void URuntimeImageReader::AddRequest(const FImageReadRequest& Request)
{
    Requests.Enqueue(Request);

    bCompletedWork.AtomicSet(false);
}

bool URuntimeImageReader::GetResult(FImageReadResult& OutResult)
{
    if (Results.Num() > 0)
    {
        OutResult = Results.Pop();

        return true;
    }

    return false;
}

void URuntimeImageReader::Clear()
{
    Requests.Empty();
    Results.Empty();

    bCompletedWork = true;

    if (ImageReader.IsValid())
    {
        ImageReader->Cancel();
    }
}

void URuntimeImageReader::Stop()
{
    bStopThread = true;
    Trigger();

    if (ImageReader.IsValid())
    {
        ImageReader->Flush();
    }

    Thread->WaitForCompletion();

    FPlatformProcess::ReturnSynchEventToPool(ThreadSemaphore);
}

bool URuntimeImageReader::IsWorkCompleted() const
{
    return bCompletedWork;
}

void URuntimeImageReader::Trigger()
{
    ThreadSemaphore->Trigger();
}

void URuntimeImageReader::BlockTillAllRequestsFinished()
{
    while (!bCompletedWork && !bStopThread)
    {
        FImageReadRequest Request;
        while (Requests.Dequeue(Request) && !bStopThread)
        {
            FImageReadResult& ReadResult = Results.Emplace_GetRef();
            ReadResult.ImageFilename = Request.ImageFilename;

            ImageReader = FImageReaderFactory::CreateReader(Request.ImageFilename);

            TArray<uint8> ImageBuffer;
            if (!ImageReader->ReadImage(Request.ImageFilename, ImageBuffer))
            {
                ReadResult.OutError = FString::Printf(TEXT("Failed to read %s image. Error: %s"), *Request.ImageFilename, *ImageReader->GetLastError());
                continue;
            }

            FRuntimeImageData ImageData;
            if (!FRuntimeImageUtils::ImportBufferAsImage(ImageBuffer.GetData(), ImageBuffer.Num(), ImageData, ReadResult.OutError))
            {
                continue;
            }

            if (ReadResult.OutError.Len() > 0)
            {
                continue;
            }

            // sanity checks
            check(ImageData.RawData.Num() > 0);
            check(ImageData.TextureSourceFormat != TSF_Invalid);

            ImageData.PixelFormat = DeterminePixelFormat(ImageData.Format, Request.TransformParams);
            if (ImageData.PixelFormat == PF_Unknown)
            {
                ReadResult.OutError = FString::Printf(TEXT("Pixel format is not supported: %d"), (int32)ImageData.PixelFormat);
                continue;
            }

            ApplyTransformations(ImageData, Request.TransformParams);

            if (ImageData.TextureSourceFormat == TSF_BGRE8)
            {
                // TODO:
                TextureFactory->CreateTextureCube({ Request.ImageFilename, &ImageData });
            }
            else
            {
                ReadResult.OutTexture = TextureFactory->CreateTexture2D({ Request.ImageFilename, &ImageData });
            }

            CreateRHITexture2D(ReadResult.OutTexture, ImageData);
        }

        bCompletedWork.AtomicSet(Requests.IsEmpty());
    }
}

EPixelFormat URuntimeImageReader::DeterminePixelFormat(ERawImageFormat::Type ImageFormat, const FTransformImageParams& Params) const
{
    EPixelFormat PixelFormat;
    
    // determine pixel format
    switch (ImageFormat)
    {
        case ERawImageFormat::G8:            PixelFormat = (Params.bForUI) ? PF_B8G8R8A8 : PF_G8; break;
        case ERawImageFormat::G16:           PixelFormat = PF_G16; break;
        case ERawImageFormat::BGRA8:         PixelFormat = PF_B8G8R8A8; break;
        case ERawImageFormat::BGRE8:         PixelFormat = PF_B8G8R8A8; break;
        case ERawImageFormat::RGBA16:        PixelFormat = (Params.bForUI) ? PF_B8G8R8A8 : PF_R16G16B16A16_SINT; break;
        case ERawImageFormat::RGBA16F:       PixelFormat = PF_FloatRGBA; break;
        default:                             PixelFormat = PF_Unknown; break;
    }

    return PixelFormat;
}

struct FTextureDataResource : public FResourceBulkDataInterface
{
public:
    FTextureDataResource(void* InMipData, int32 InDataSize)
        : MipData(InMipData), DataSize(InDataSize)
    {}

    const void* GetResourceBulkData() const override { return MipData; }
    uint32 GetResourceBulkDataSize() const override { return DataSize; }
    void Discard() override {}

private:
    void* MipData;
    int32 DataSize;
};


void URuntimeImageReader::CreateRHITexture2D(UTexture2D* NewTexture, const FRuntimeImageData& ImageData)
{
    FTexture2DRHIRef RHITexture2D = nullptr;

#if PLATFORM_WINDOWS
    RHITexture2D = CreateRHITexture2D_Windows(NewTexture, ImageData);
#elif (PLATFORM_ANDROID || PLATFORM_ANDROID_VULKAN)
    RHITexture2D = CreateRHITexture2D_Mobile(NewTexture, ImageData);
#else
    RHITexture2D = CreateRHITexture2D_Other(NewTexture, ImageData);
#endif

    FinalizeRHITexture2D(NewTexture, RHITexture2D);
}

FTexture2DRHIRef URuntimeImageReader::CreateRHITexture2D_Windows(UTexture2D* NewTexture, const FRuntimeImageData& ImageData)
{
    uint32 NumMips = 1;
    uint32 NumSamples = 1;
    void* Mip0Data = (void*)ImageData.RawData.GetData();

    ETextureCreateFlags TextureFlags = TexCreate_ShaderResource;
    if (ImageData.SRGB)
    {
        TextureFlags |= TexCreate_SRGB;
    }

    FTexture2DRHIRef RHITexture2D = nullptr;

    ensureMsgf(ImageData.SizeX > 0, TEXT("ImageData.SizeX must be > 0"));
    ensureMsgf(ImageData.SizeY > 0, TEXT("ImageData.SizeY must be > 0"));

    if (GRHISupportsAsyncTextureCreation)
    {
        RHITexture2D = RHIAsyncCreateTexture2D(
            ImageData.SizeX, ImageData.SizeY,
            ImageData.PixelFormat,
            ImageData.NumMips,
            TextureFlags,
            &Mip0Data,
            1
        );
    }
    else
    {
        FTextureDataResource TextureData(Mip0Data, ImageData.RawData.Num());

        FRHIResourceCreateInfo CreateInfo(TEXT("RuntimeImageReaderTextureData"));
        CreateInfo.BulkData = &TextureData;

        FGraphEventRef CreateTextureTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
            [&RHITexture2D, &CreateInfo, &ImageData, &TextureFlags]()
            {
                RHITexture2D = RHICreateTexture2D(
                    ImageData.SizeX, ImageData.SizeY,
                    ImageData.PixelFormat,
                    ImageData.NumMips,
                    1,
                    TextureFlags,
                    CreateInfo
                );
            }, TStatId(), nullptr, ENamedThreads::ActualRenderingThread
        );
        CreateTextureTask->Wait();
    }

    return RHITexture2D;
}


FTexture2DRHIRef URuntimeImageReader::CreateRHITexture2D_Mobile(UTexture2D* NewTexture, const FRuntimeImageData& ImageData)
{
    uint32 NumMips = 1;
    uint32 NumSamples = 1;
    void* Mip0Data = (void*)ImageData.RawData.GetData();

    ETextureCreateFlags TextureFlags = TexCreate_ShaderResource;
    if (ImageData.SRGB)
    {
        TextureFlags |= TexCreate_SRGB;
    }

    FTexture2DRHIRef RHITexture2D = nullptr;

    ensureMsgf(ImageData.SizeX > 0, TEXT("ImageData.SizeX must be > 0"));
    ensureMsgf(ImageData.SizeY > 0, TEXT("ImageData.SizeY must be > 0"));

    FGraphEventRef CreateTextureTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
        [&RHITexture2D, &ImageData, &TextureFlags]()
        {
            FRHIResourceCreateInfo DummyCreateInfo(TEXT("DummyCreateInfo"));
            RHITexture2D = RHICreateTexture2D(
                ImageData.SizeX, ImageData.SizeY,
                ImageData.PixelFormat,
                ImageData.NumMips,
                1,
                TextureFlags,
                DummyCreateInfo
            );

            FUpdateTextureRegion2D TextureRegion2D;
            {
                TextureRegion2D.DestX = 0;
                TextureRegion2D.DestY = 0;
                TextureRegion2D.SrcX = 0;
                TextureRegion2D.SrcY = 0;
                TextureRegion2D.Width = ImageData.SizeX;
                TextureRegion2D.Height = ImageData.SizeY;
            }

            RHIUpdateTexture2D(
                RHITexture2D, 0, TextureRegion2D,
                TextureRegion2D.Width * GPixelFormats[ImageData.PixelFormat].BlockBytes,
                ImageData.RawData.GetData()
            );
        }, TStatId(), nullptr, ENamedThreads::ActualRenderingThread
    );
    CreateTextureTask->Wait();

    return RHITexture2D;
}

FTexture2DRHIRef URuntimeImageReader::CreateRHITexture2D_Other(UTexture2D* NewTexture, const FRuntimeImageData& ImageData)
{
    // TODO: Figure out the best way to support other graphics APIs and platfornms
    return CreateRHITexture2D_Windows(NewTexture, ImageData);
}

void URuntimeImageReader::FinalizeRHITexture2D(UTexture2D* NewTexture, FTexture2DRHIRef RHITexture2D)
{
    // Create proper texture resource so UMG can display runtime texture
    FRuntimeTextureResource* NewTextureResource = new FRuntimeTextureResource(NewTexture, RHITexture2D);
    NewTexture->SetResource(NewTextureResource);

    FGraphEventRef UpdateResourceTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
        [&NewTextureResource, &NewTexture, &RHITexture2D]()
        {
            NewTextureResource->InitResource();
            RHIUpdateTextureReference(NewTexture->TextureReference.TextureReferenceRHI, RHITexture2D);
            NewTextureResource->SetTextureReference(NewTexture->TextureReference.TextureReferenceRHI);

        }, TStatId(), nullptr, ENamedThreads::ActualRenderingThread
    );
    UpdateResourceTask->Wait();
}

void URuntimeImageReader::ApplyTransformations(FRuntimeImageData& ImageData, FTransformImageParams TransformParams)
{
    if (!TransformParams.IsPercentSizeValid())
    {
        UE_LOG(LogRuntimeImageReader, Verbose, TEXT("Supplied transform params are not valid! PercentSizeX, PercentSizeX: (%d, %d)"), TransformParams.PercentSizeX, TransformParams.PercentSizeY);
    }
    
    if (TransformParams.IsPercentSizeValid())
    {
        const int32 TransformedSizeX = FMath::Floor(ImageData.SizeX * TransformParams.PercentSizeX * 0.01f);
        const int32 TransformedSizeY = FMath::Floor(ImageData.SizeY * TransformParams.PercentSizeY * 0.01f);
        
        FImage TransformedImage;
        TransformedImage.Init(TransformedSizeX, TransformedSizeY, ImageData.Format);

        ImageData.ResizeTo(TransformedImage, TransformedImage.SizeX, TransformedImage.SizeY, ImageData.Format, ImageData.GammaSpace);

        ImageData.RawData = MoveTemp(TransformedImage.RawData);
        ImageData.SizeX = TransformedImage.SizeX;
        ImageData.SizeY = TransformedImage.SizeY;
    }

    if (TransformParams.bForUI)
    {
        // no need to convert float RGBA
        if (ImageData.TextureSourceFormat != TSF_RGBA16F)
        {
            FImage BGRAImage;
            BGRAImage.Init(ImageData.SizeX, ImageData.SizeY, ERawImageFormat::BGRA8);
            ImageData.CopyTo(BGRAImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);

            ImageData.RawData = MoveTemp(BGRAImage.RawData);
            ImageData.SRGB = true;
            ImageData.GammaSpace = EGammaSpace::sRGB;
        }
    }
}
