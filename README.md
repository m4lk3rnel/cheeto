### Simple cheat menu for GTA: San Andreas v1.0

#### Build
```
g++ -m32 -shared -o cheeto.asi main.cpp imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_widgets.cpp imgui/imgui_impl_dx9.cpp imgui/imgui_impl_win32.cpp imgui/imgui_tables.cpp -I"C:/MinGW/i686-w64-mingw32/include" -L"C:/MinGW/i686-w64-mingw32/lib" -ld3d9 -ld3dx9 -ldxguid -lgdi32 -luser32 -lkernel32 -ldwmapi -ldinput8 -ldxguid -I"./minhook/include" -L"./minhook/lib" -lminhook
```

- [MinHook](https://github.com/TsudaKageyu/minhook)
- [Dear ImGui](https://github.com/ocornut/imgui)
