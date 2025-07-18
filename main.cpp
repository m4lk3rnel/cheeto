#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <d3d9.h>
#include <d3dx9.h>
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "minhook/include/MinHook.h"

#pragma comment(lib, "d3d9.lib")

typedef HRESULT (__stdcall* endScene)(IDirect3DDevice9* pDevice);

endScene pOriginalEndScene = nullptr; 
endScene pEndScene = nullptr;         

// using this function i can render whatever i want
HRESULT __stdcall MyEndScene(IDirect3DDevice9* pDevice) {
    static bool initialized = false;
    if (!initialized) {
        std::cout << "!!!!!!!!!!!!Hooked EndScene!" << std::endl;

        std::cout << "[*] Initializing ImGui..." << std::endl;
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     

        ImGui_ImplWin32_Init(FindWindow(NULL, "GTA: San Andreas"));
        ImGui_ImplDX9_Init(pDevice);

        std::cout << "[+] ImGUI initialized." << std::endl;

        initialized = true;
    }

    // if windowed do not render?
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("My Menu");
    ImGui::Text("Hello, world!");
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return pOriginalEndScene(pDevice);
}

void HookEndScene() {

    std::cout << "[*] Trying to read game's d3d9 device from 0xC97C28..." << std::endl;

    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)0xC97C28;
    IDirect3DDevice9* pDevice = *ppDevice;

    while (!pDevice) {
        std::cout << "[-] Device not ready yet..." << std::endl;
        Sleep(1000);
    }

    std::cout << "[*] This MIGHT be the correct address." << std::endl;
    void** vtable = *reinterpret_cast<void***>(pDevice);
    pEndScene = (endScene)vtable[42]; // 42 -> the EndScene's index in the VMT (Virtual Method Table)

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        std::cout << "[-] MinHook initialization failed! Code: " << status << std::endl;
        return;
    }

    status = MH_CreateHook((LPVOID)pEndScene, (LPVOID)&MyEndScene, reinterpret_cast<LPVOID*>(&pOriginalEndScene));
    if (status != MH_OK) {
        std::cout << "[-] Failed to CREATE hook. Code: " << status << std::endl;
        return;
    }

    status = MH_EnableHook((LPVOID)pEndScene);
    if (status != MH_OK) {
        std::cout << "[-] Failed to ENABLE hook. Code: " << status << std::endl;
        return;
    }

    std::cout << "[+] Successfully hooked EndScene using game's D3D device." << std::endl;
}

DWORD WINAPI MyThread(HMODULE hModule) {

    HookEndScene();

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls(hinstDLL);

            AllocConsole();

            FILE* stream;
            freopen_s(&stream, "CONOUT$", "w", stdout);

            std::cout << "[*] Creating thread.." << std::endl;

            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MyThread, NULL, 0, NULL);

            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;  
}