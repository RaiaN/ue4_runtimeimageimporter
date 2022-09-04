﻿// Copyright 2022 Peter Leontev. All Rights Reserved.

#include "CoreMinimal.h"
#include "ImageCore.h"


extern void GenerateBaseCubeMipFromLongitudeLatitude2D(FImage* OutMip, const FImage& SrcImage, const uint32 MaxCubemapTextureResolution, uint8 SourceEncodingOverride);