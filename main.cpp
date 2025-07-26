#include <windows.h>
#include <iostream>
#include <d3d9.h>
#include <d3dx9.h>
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include "minhook/include/MinHook.h"
#include <cstdint>

#define NUMBER_OF_LINES 3

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d9.lib") 

typedef HRESULT(__stdcall* tEndScene)(IDirect3DDevice9* pDevice);
typedef HRESULT(__stdcall* tReset)(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters);
typedef void (__cdecl* FuncVoid)();
typedef void (__cdecl* tCreateCar)(int modelId);
typedef char (__cdecl* tShowMsg)(char*, int, int, int);
typedef void (__thiscall* tGiveWeapon)(void* pPed, unsigned int weaponType, unsigned int ammo, bool likeUnused);

HWND hWnd = NULL; 

tEndScene pOriginalEndScene = nullptr; 
tReset pOriginalReset = nullptr; 
LONG originalWndProc;

tEndScene pEndScene = nullptr;         
tReset pReset = nullptr;         

bool drawCursor = false;
bool esp = false;

FuncVoid ClearMouseHistory = (FuncVoid)0x541BD0;
FuncVoid UpdatePads = (FuncVoid)0x541DD0;

ImGuiContext* ctx = ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();

uintptr_t CPed;
uintptr_t* pCPedCVectorPosition;
float* pCPedHealth;
float* pCPedMaxHealth;

char modelId[16] = "400";

tCreateCar CreateCar;
tShowMsg ShowMsg;
tGiveWeapon GiveWeapon; 

uintptr_t* getMouseStateBytes[5];

// from AntTweakBar (https://github.com/g1mm3/Skeleton/blob/master/Skeleton/main.h)
void DrawLine(LPDIRECT3DDEVICE9 m_pDevice, int x1, int y1, int x2, int y2, DWORD Color, bool _AntiAliased = false)
{
	struct CVtx
	{
		float m_Pos[4];
		DWORD m_Color;
	};
	CVtx p[2];

	p[0].m_Pos[0] = (float)x1;
	p[0].m_Pos[1] = (float)y1;
	p[0].m_Pos[2] = 0;
	p[0].m_Pos[3] = 0;
	p[0].m_Color = Color;

	p[1].m_Pos[0] = (float)x2;
	p[1].m_Pos[1] = (float)y2;
	p[1].m_Pos[2] = 0;
	p[1].m_Pos[3] = 0;
	p[1].m_Color = Color;

	//if( m_State->m_Caps.LineCaps & D3DLINECAPS_ANTIALIAS )
	m_pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, (_AntiAliased ? TRUE : FALSE));
	m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
	m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	m_pDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, p, sizeof(CVtx));
	//if( m_State->m_Caps.LineCaps & D3DLINECAPS_ANTIALIAS )
	m_pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
}

bool WorldToScreen(D3DXVECTOR3 pos, D3DXVECTOR3& out, IDirect3DDevice9* pDevice)
{
    D3DVIEWPORT9 viewport;
    D3DXMATRIX projection, view, world;

    pDevice->GetViewport(&viewport);
    pDevice->GetTransform(D3DTS_PROJECTION, &projection);
    pDevice->GetTransform(D3DTS_VIEW, &view);
    D3DXMatrixIdentity(&world); // World = identity (no transformations)

    D3DXVec3Project(&out, &pos, &viewport, &projection, &view, &world);

    // Check if the result is visible (you can add more checks if needed)
    return out.z < 1.0f;
}

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
        std::cout << "!!!!!!!!!!!!Hooked EndScene!" << std::endl;
        memcpy(getMouseStateBytes, (const void*)0x53F417, 5);

        pDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

        CPed = *(uintptr_t*)0xB6F5F0;
        pCPedHealth = (float*)(CPed + 0x540);
        pCPedMaxHealth = (float*)(CPed + 0x544);
        pCPedCVectorPosition = (uintptr_t*)(CPed + 0x14);

        //functions
        CreateCar = (tCreateCar)0x00407851;
        ShowMsg = (tShowMsg)0x588BE0;
        GiveWeapon = (tGiveWeapon)0x5E6080;

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

        if(drawCursor) {
            // https://www.blast.hk/threads/10970/page-5 - Gunborg Johansson
            WriteMemory((void*)0x541DF5, "\x90\x90\x90\x90\x90", 5); // don't call CControllerConfigManager::AffectPadFromKeyBoard
            WriteMemory((void*)0x53F417, "\x90\x90\x90\x90\x90", 5); // don't call CPad__getMouseState
            WriteMemory((void*)0x53F41F, "\x33\xC0\x0F\x84", 4);     // test eax, eax -> xor eax, eax 
                                                                    // jl loc_53F526 -> jz loc_53F526

            WriteMemory((void*)0x6194A0, "\xC3\x90\x90\x90\x90", 5); // disable RsMouseSetPos (ret)
        } else {
            WriteMemory((void*)0x541DF5, "\xE8\x46\xF3\xFE\xFF", 5); // call CControllerConfigManager::AffectPadFromKeyBoard
            WriteMemory((void*)0x53F417, getMouseStateBytes, 5);     // call CPad__getMouseState

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
    ImGui::SliderFloat("Health", pCPedHealth, 1.0f, (float)*pCPedMaxHealth);
    ImGui::InputText("Model id", modelId, sizeof(modelId), ImGuiInputTextFlags_CharsDecimal);

    if(ImGui::Button("Spawn vehicle")) {
        if (atoi(modelId) >= 400 && atoi(modelId) <= 611) {
            CreateCar(atoi(modelId));
            const char* message = "Vehicle spawned";
            ShowMsg((char*)message, 0, 0, 0);
        }
    }

    if(ImGui::Button("Minigun")) {

        GiveWeapon((void*)CPed, 0x26, 999999, true);

        *(BYTE*)(CPed + 0x718) = 8;   

        const char* message = "Minigun :>";
        ShowMsg((char*)message, 0, 0, 0);
    }

    ImGui::Checkbox("ESP", &esp);

    if(esp) {
        uintptr_t pedPoolInfo = *(uintptr_t*)0xB74490;
        uintptr_t pedPoolBeginning = *(uintptr_t*)(pedPoolInfo);        
        uintptr_t pedPoolByteMap   = *(uintptr_t*)(pedPoolInfo + 4);

        for (int i = 1; i < 140; ++i) {
            BYTE status = *(BYTE*)(pedPoolByteMap + i);
            if (status > 0 && status < 128) {
                uintptr_t ped = pedPoolBeginning + i * 0x7C4;

                uintptr_t matrixPtr = *(uintptr_t*)(ped + 0x14);
                if (matrixPtr) {
                    float x = *(float*)(matrixPtr + 0x30);
                    float y = *(float*)(matrixPtr + 0x34);
                    float z = *(float*)(matrixPtr + 0x38);

                    float health = *(float*)(ped + 0x540);

                    D3DXVECTOR3 pos(x,y,z);
                    D3DXVECTOR3 screenPos;
                    if(WorldToScreen(pos, screenPos, pDevice)) {
                        //1680x1050
                        if (screenPos.x >= 0 && screenPos.x <= 1680 && screenPos.y >= 0 && screenPos.y <= 1050) {

                            DrawLine(pDevice, 840, 1050, screenPos.x, screenPos.y, 0xFFFF00FF, true);
                        }
                    }
                }
            }
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
    
    // hWnd = FindWindowW(NULL, L"GTA:SA:MP");
    std::cout << "[*] Hooking WndProc..." << std::endl;
    originalWndProc = SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)MyWndProc);
}

void HookEndScene() {

    std::cout << "[*] Trying to read game's d3d9 device from 0xC97C28..." << std::endl;

    IDirect3DDevice9** ppDevice = (IDirect3DDevice9**)0xC97C28;
    IDirect3DDevice9* pDevice = *ppDevice;

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
    Sleep(35000);
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