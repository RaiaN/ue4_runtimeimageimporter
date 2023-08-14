// Copyright 2023 Peter Leontev and Muhammad Ahmed Saleem. All Rights Reserved.

#include "RuntimeImageLoaderLibHandler.h"
#include "RuntimeImageLoaderModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

DEFINE_LOG_CATEGORY(RuntimeImageLoaderLibHandler);

 //~ Static initialization
 //--------------------------------------------------------------------
void* FLibnsgifHandler::LibnsgifHandle = nullptr;

bool FLibnsgifHandler::Initialize()
{
#if WITH_LIBNSGIF
	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("RuntimeImageLoader")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LibraryPath;
#if PLATFORM_WINDOWS
	LibraryPath = FPaths::Combine(*BaseDir, TEXT("Source/ThirdParty/RuntimeGifLibrary/x64/Release/libnsgif.dll"));
#endif // PLATFORM_WINDOWS

	LibnsgifHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

	if (LibnsgifHandle)
	{
		UE_LOG(RuntimeImageLoaderLibHandler, Display, TEXT("libnsgif.dll Loaded."));

		Fn_nsgif_create = (nsgif_create_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_create"));
		Fn_nsgif_destroy = (nsgif_destroy_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_destroy"));
		Fn_nsgif_data_scan = (nsgif_data_scan_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_data_scan"));
		Fn_nsgif_data_complete = (nsgif_data_complete_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_data_complete"));
		Fn_nsgif_frame_prepare = (nsgif_frame_prepare_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_frame_prepare"));
		Fn_nsgif_frame_decode = (nsgif_frame_decode_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_frame_decode"));
		Fn_nsgif_reset = (nsgif_reset_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_reset"));
		Fn_nsgif_get_info = (nsgif_get_info_FnPtr)FPlatformProcess::GetDllExport(LibnsgifHandle, TEXT("nsgif_get_info"));

		// Throwaway code just check weather Dll get loaded or not. ---- Start ----
		if (Fn_nsgif_create && Fn_nsgif_destroy && Fn_nsgif_data_scan && Fn_nsgif_data_complete && Fn_nsgif_frame_prepare && Fn_nsgif_frame_decode && Fn_nsgif_reset && Fn_nsgif_get_info)
			UE_LOG(RuntimeImageLoaderLibHandler, Display, TEXT("libnsgif methods Loaded."));
		// ---- End ----

		return true;
	}
	else // Throwaway code just check weather Dll get loaded or not. ---- Start ----
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("RuntimeImageLoaderLibHandler", "ThirdPartyLibraryError", "Failed to load Libnsgif library"));
		return false;
	}
	// ---- End ----

	return false;
#endif // WITH_LIBNSGIF
	return false;
}

bool FLibnsgifHandler::IsInitialized()
{
	return (LibnsgifHandle != nullptr);
}

void FLibnsgifHandler::Shutdown()
{
#if WITH_LIBNSGIF
	// Free the dll handle
	FPlatformProcess::FreeDllHandle(LibnsgifHandle);
	LibnsgifHandle = nullptr;
#endif // WITH_LIBNSGIF
}

nsgif_strerror_FnPtr FLibnsgifHandler::FunctionPointerNsgifStrError()
{
	return Fn_nsgif_strerror;
}

nsgif_create_FnPtr FLibnsgifHandler::FunctionPointerNsgifCreate()
{
	return Fn_nsgif_create;
}

nsgif_destroy_FnPtr FLibnsgifHandler::FunctionPointerNsgifDestroy()
{
	return Fn_nsgif_destroy;
}

nsgif_data_scan_FnPtr FLibnsgifHandler::FunctionPointerNsgifDataScan()
{
	return Fn_nsgif_data_scan;
}

nsgif_data_complete_FnPtr FLibnsgifHandler::FunctionPointerNsgifDataComplete()
{
	return Fn_nsgif_data_complete;
}

nsgif_frame_prepare_FnPtr FLibnsgifHandler::FunctionPointerNsgifFramePrepare()
{
	return Fn_nsgif_frame_prepare;
}

nsgif_frame_decode_FnPtr FLibnsgifHandler::FunctionPointerNsgifFrameDecode()
{
	return Fn_nsgif_frame_decode;
}

nsgif_reset_FnPtr FLibnsgifHandler::FunctionPointerNsgifReset()
{
	return Fn_nsgif_reset;
}

nsgif_get_info_FnPtr FLibnsgifHandler::FunctionPointerNsgifGetInfo()
{
	return Fn_nsgif_get_info;
}