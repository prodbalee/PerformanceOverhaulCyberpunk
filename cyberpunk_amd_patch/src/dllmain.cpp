#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <windows.h>
#include <DbgHelp.h>
#include <spdlog/spdlog.h>
#include <filesystem>


#include "Image.h"
#include "Options.h"

#pragma comment( lib, "dbghelp.lib" )
#pragma comment(linker, "/DLL")

void PatchAmd(Image* apImage);
void PatchAvx(Image* apImage);
void HotPatchFix(Image* apImage);
void StringInitializerFix(Image* apImage);
void PatchSpin(Image* apImage);

void Initialize(HMODULE mod)
{
    Options options(mod);

    Image image;

    if(options.PatchSMT)
        PatchAmd(&image);

    if(options.PatchSpectre)
        HotPatchFix(&image);

    if (options.PatchAVX)
        PatchAvx(&image);

    spdlog::default_logger()->flush();
}

void PatchAmd(Image* apImage)
{
    const uint8_t payload[] = {
        0x75, 0x30, 0x33, 0xC9, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x0F, 0xA2, 0x8B, 0xC8, 0xC1, 0xF9, 0x08
    };

    auto* pMemoryItor = apImage->pTextStart;
    auto* pEnd = apImage->pTextEnd;

    while(pMemoryItor + std::size(payload) < pEnd)
    {
        if(memcmp(pMemoryItor, payload, std::size(payload)) == 0)
        {
            DWORD oldProtect = 0;
            VirtualProtect(pMemoryItor, 8, PAGE_EXECUTE_WRITECOPY, &oldProtect);
            *pMemoryItor = 0xEB;
            VirtualProtect(pMemoryItor, 8, oldProtect, nullptr);

            spdlog::info("\tAMD SMT Patch: success");

            return;
        }

        pMemoryItor++;
    }

    spdlog::warn("\tAMD SMT Patch: failed");
}

void PatchAvx(Image* apImage)
{
    const uint8_t payload[] = {
         0x55, 0x48, 0x81 , 0xec , 0xa0 , 0x00 , 0x00 , 0x00 , 0x0f , 0x29 , 0x70 , 0xe8
    };

    auto* pMemoryItor = apImage->pTextStart;
    auto* pEnd = apImage->pTextEnd;

    while (pMemoryItor + std::size(payload) < pEnd)
    {
        if (memcmp(pMemoryItor, payload, std::size(payload)) == 0)
        {
            DWORD oldProtect = 0;
            VirtualProtect(pMemoryItor, 8, PAGE_EXECUTE_WRITECOPY, &oldProtect);
            *pMemoryItor = 0xC3;
            VirtualProtect(pMemoryItor, 8, oldProtect, nullptr);

            spdlog::info("\tAVX Patch: success");

            return;
        }

        pMemoryItor++;
    }

    spdlog::warn("\tAVX Patch: failed");

}

BOOL APIENTRY DllMain(HMODULE mod, DWORD ul_reason_for_call, LPVOID) {
    switch(ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        Initialize(mod);
        break;

    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
        break;
    }

    return TRUE;
}
