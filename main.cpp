#include <windows.h>
#include <iostream>
#include <d3d9.h>
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "minhook/include/MinHook.h"
#include <cstdint>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d9.lib") 

typedef HRESULT(__stdcall* tEndScene)(IDirect3DDevice9* pDevice);
typedef HRESULT(__stdcall* tReset)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters);
typedef void(__cdecl* FuncVoid)();
typedef void (__cdecl* tCreateCar)(int modelId);
typedef char(__cdecl* tShowMsg)(char*, int, int, int);

HWND hWnd = NULL; 

tEndScene pOriginalEndScene = nullptr; 
tReset pOriginalReset = nullptr; 
LONG originalWndProc;

tEndScene pEndScene = nullptr;         
tReset pReset = nullptr;         

bool drawCursor = false;

FuncVoid ClearMouseHistory = (FuncVoid)0x541BD0;
FuncVoid UpdatePads = (FuncVoid)0x541DD0;

ImGuiContext* ctx = ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();

uintptr_t CPed;
float* pCPedHealth;
float* pCPedMaxHealth;
char modelId[16] = "400";
tCreateCar CreateCar;
tShowMsg showMsg;

bool WriteMemory(void* destination, const void* data, size_t size) {
    DWORD oldProtect;
    
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "VirtualProtect failed: " << GetLastError() << std::endl;
        return false;
    }
    memcpy(destination, data, size);
    DWORD temp;
    VirtualProtect(destination, size, oldProtect, &temp);
    return true;
}

void ResetMouseControllerState() {
    *(int32_t*)0xB73424 = 0; //NewMouseControllerState.x
    *(int32_t*)0xB73428 = 0; //NewMouseControllerState.y
}

LRESULT CALLBACK MyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)    
{ 
    if(ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) return 0;
    
    return CallWindowProc((WNDPROC)originalWndProc, hwnd, uMsg, wParam, lParam); 
} 

HRESULT __stdcall MyEndScene(IDirect3DDevice9* pDevice) {
    static bool initialized = false;
    if (!initialized) {

        CPed = *(uintptr_t*)0xB6F5F0;
        pCPedHealth = (float*)(CPed + 0x540);
        pCPedMaxHealth = (float*)(CPed + 0x544);
        CreateCar = (tCreateCar)0x00407851;
        showMsg = (tShowMsg)0x588BE0;

        std::cout << "!!!!!!!!!!!!Hooked EndScene!" << std::endl;

        std::cout << "[*] Initializing ImGui..." << std::endl;
        IMGUI_CHECKVERSION();

        ImGui_ImplWin32_Init(hWnd);
        ImGui_ImplDX9_Init(pDevice);

        std::cout << "[+] ImGUI initialized." << std::endl;

        initialized = true;
    }

    if (GetAsyncKeyState(VK_INSERT) & 1) {
        drawCursor = !drawCursor;
        io.WantCaptureMouse = drawCursor;
        io.MouseDrawCursor = drawCursor;

        // https://www.blast.hk/threads/10970/page-5 - Gunborg Johansson
        if (drawCursor) {

            WriteMemory((void*)0x541DF5, "\x90\x90\x90\x90\x90", 5); // don't call CControllerConfigManager::AffectPadFromKeyBoard
            WriteMemory((void*)0x53F417, "\x90\x90\x90\x90\x90", 5); // don't call CPad__getMouseState
            WriteMemory((void*)0x53F41F, "\x33\xC0\x0F\x84", 4);     // test eax, eax -> xor eax, eax 
                                                                     // jl loc_53F526 -> jz loc_53F526

            WriteMemory((void*)0x6194A0, "\xC3\x90\x90\x90\x90", 5); // disable RsMouseSetPos (ret)
        } else {

            WriteMemory((void*)0x541DF5, "\xE8\x46\xF3\xFE\xFF", 5); // call CControllerConfigManager::AffectPadFromKeyBoard
            WriteMemory((void*)0x53F417, "\xE8\xF4\x20\xC7\x70", 5); // call CPad__getMouseState
            WriteMemory((void*)0x53F41F, "\x85\xC0\x0F\x8C", 4);     // xor eax, eax -> test eax, eax
                                                                     // jz loc_53F526 -> jl loc_53F526
            WriteMemory((void*)0x6194A0, "\xE9\x4B\xBF\x12\x00", 5); // jmp setup
        }

        ResetMouseControllerState();
        ClearMouseHistory();
        UpdatePads();
    }
   
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("cheeto"); 
    ImGui::Text("Hello, world!"); 
    ImGui::Text("Health address: 0x%p", pCPedHealth);
    ImGui::Text("MaxHealth address: 0x%p", pCPedMaxHealth);
    ImGui::SliderFloat("Health", pCPedHealth, 1.0f, (float)*pCPedMaxHealth);
    ImGui::InputText("Model id", modelId, sizeof(modelId), ImGuiInputTextFlags_CharsDecimal);

    if(ImGui::Button("Spawn vehicle")) {
        if (atoi(modelId) >= 400 && atoi(modelId) <= 611) {
            CreateCar(atoi(modelId));
            const char* message = "Vehicle spawned";
            showMsg((char*)message, 0, 0, 0);
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return pOriginalEndScene(pDevice);
}

HRESULT __stdcall MyReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters) {
    ImGui_ImplDX9_InvalidateDeviceObjects();

    HRESULT hr = pOriginalReset(pDevice, pPresentationParameters);

    if (SUCCEEDED(hr))
        ImGui_ImplDX9_CreateDeviceObjects(); 

    return hr;
}

void InitializeMinHook() {

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK) {
        std::cout << "[-] MinHook initialization failed! Code: " << status << std::endl;
        return;
    }
}

void HookWndProc() {
    hWnd = FindWindowW(NULL, L"GTA: San Andreas");
    std::cout << "[*] Hooking WndProc..." << std::endl;
    originalWndProc = SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)MyWndProc);
}

void HookEndScene() {

    Sleep(35000); 
    std::cout << "[*] Trying to read game's d3d9 device from 0xC97C28..." << std::endl;

    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)0xC97C28;
    IDirect3DDevice9* pDevice = *ppDevice;

    std::cout << "[*] This MIGHT be the correct address." << std::endl;

    void** vtable = *reinterpret_cast<void***>(pDevice);
    pEndScene = (tEndScene)vtable[42]; // 42 -> the EndScene's index in the VMT (Virtual Method Table)

    std::cout << "[*] Hooking EndScene..." << std::endl;
    MH_STATUS status = MH_CreateHook((LPVOID)pEndScene, (LPVOID)&MyEndScene, reinterpret_cast<LPVOID*>(&pOriginalEndScene));

    if (status != MH_OK) {
        std::cout << "[-] Failed to CREATE hook (EndScene). Code: " << status << std::endl;
        return;
    }

    std::cout << "[*] Created hook for EndScene..." << std::endl;

    status = MH_EnableHook((LPVOID)pEndScene);
    if (status != MH_OK) {
        std::cout << "[-] Failed to ENABLE hook (EndScene). Code: " << status << std::endl;
        return;
    }

    std::cout << "[+] Successfully hooked EndScene using game's D3D device." << std::endl;
}

void HookReset() {

    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)0xC97C28;
    IDirect3DDevice9* pDevice = *ppDevice;

    void** vtable = *reinterpret_cast<void***>(pDevice);
    pReset = (tReset)vtable[16]; // 16 -> the Reset's index in the VMT (Virtual Method Table)
    std::cout << "[*] Hooking Reset..." << std::endl;

    MH_STATUS status = MH_CreateHook((LPVOID)pReset, (LPVOID)&MyReset, reinterpret_cast<LPVOID*>(&pOriginalReset));
    if (status != MH_OK) {
        std::cout << "[-] Failed to CREATE hook (Reset). Code: " << status << std::endl;
        return;
    }

    std::cout << "[*] Created hook for Reset..." << std::endl;

    status = MH_EnableHook((LPVOID)pReset);
    if (status != MH_OK) {
        std::cout << "[-] Failed to ENABLE hook (Reset). Code: " << status << std::endl;
        return;
    }

    std::cout << "[+] Successfully hooked Reset!" << std::endl;
}


void Hook() {
    InitializeMinHook();
    HookWndProc();
    HookEndScene();
    HookReset();
}

DWORD WINAPI MainThread(HMODULE hModule) {

    AllocConsole();

    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);

    std::cout << "[*] Creating thread.." << std::endl;

    Hook();

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:

        DisableThreadLibraryCalls(hinstDLL);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, NULL, 0, NULL);
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