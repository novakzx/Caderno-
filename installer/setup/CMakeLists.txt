cmake_minimum_required(VERSION 3.21)
project(CadernoSetup VERSION 1.1.0 LANGUAGES CXX RC)

# Instalador do Caderno+: um único .exe, sem Qt e sem DLLs (Win32 + GDI+). O pacote com os arquivos do programa
# é anexado depois por installer/empacotar.py. Veja docs/GUIA-PUBLICAR.md.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3)
    # Runtime estático: o instalador roda em qualquer Windows, sem o "Visual C++ Redistributable".
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

add_executable(CadernoSetup WIN32
    src/main.cpp
    src/janela.cpp
    src/instalar.cpp
    src/pacote.cpp
    src/sistema.cpp
    src/inflate.cpp
    recursos/setup.rc)

target_include_directories(CadernoSetup PRIVATE src recursos)
target_compile_definitions(CadernoSetup PRIVATE UNICODE _UNICODE)
target_link_libraries(CadernoSetup PRIVATE gdiplus ole32 shell32 shlwapi dwmapi uuid advapi32 user32 gdi32)

if(MSVC)
    # O manifesto (DPI por monitor, sem administrador) vem do setup.rc; o linker não deve gerar outro.
    target_link_options(CadernoSetup PRIVATE /MANIFEST:NO)
endif()

if(MINGW)
    target_link_options(CadernoSetup PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
    # O MinGW precisa do ponto de entrada wWinMain explícito.
    target_link_options(CadernoSetup PRIVATE -municode)
endif()
