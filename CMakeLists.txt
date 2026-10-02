cmake_minimum_required(VERSION 3.21)

project(ProfOrganizer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Os fontes são UTF-8 (acentos e emojis nos textos). O MSVC, sem esta opção, lê os
# arquivos como Windows-1252 e estraga os textos; GCC/Clang já usam UTF-8.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt cuida de moc/uic/rcc automaticamente
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Widgets: interface · Sql: SQLite · Svg: ícones da barra lateral · (PDF usa QPdfWriter, do Qt Gui)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Svg)

# --- QXlsx (importar/exportar .xlsx) -------------------------------------------
# A biblioteca fica em third_party/QXlsx (veja o README, passo "Baixar o QXlsx").
set(QXLSX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx)
if(NOT EXISTS ${QXLSX_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "QXlsx não encontrado em third_party/QXlsx.\n"
        "Baixe https://github.com/QtExcel/QXlsx (Code > Download ZIP) e extraia de modo que exista "
        "o arquivo third_party/QXlsx/QXlsx/CMakeLists.txt. Detalhes no README.md.")
endif()
add_subdirectory(${QXLSX_DIR} ${CMAKE_BINARY_DIR}/QXlsx)
# -------------------------------------------------------------------------------

# Todos os .cpp/.h de src/ (CONFIGURE_DEPENDS: o CMake percebe arquivos novos sozinho).
#   src/core      lógica pura (média, frequência, horário, texto)
#   src/models    structs de dados
#   src/database  conexão, migrações e repositórios (único lugar com SQL)
#   src/services  .xlsx, PDF, backup, importação, desempenho
#   src/ui        widgets
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

# Ícone e imagens do app (resources/): .qrc embute os PNGs, os ícones SVG e as fontes Figtree;
# .rc dá o ícone do .exe no Windows.
set(RECURSOS ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc)
if(WIN32)
    list(APPEND RECURSOS ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc)
endif()

qt_add_executable(ProfOrganizer WIN32 MACOSX_BUNDLE ${SOURCES} ${RECURSOS})

target_include_directories(ProfOrganizer PRIVATE src)
# Número da versão: uma só fonte (a linha project(... VERSION) acima). Vai para o código como PROFORG_VERSION.
target_compile_definitions(ProfOrganizer PRIVATE PROFORG_VERSION="${PROJECT_VERSION}")
target_link_libraries(ProfOrganizer PRIVATE Qt6::Widgets Qt6::Sql Qt6::Svg QXlsx::QXlsx)

# Identificação do build (início do hash do commit), passada pelo workflow:
#   cmake ... -DPROFORG_BUILD_ID=6d9e0f0
# Aparece na barra de título e nas mensagens de erro (src/core/BuildInfo.h).
set(PROFORG_BUILD_ID "" CACHE STRING "Identificação do build (hash curto do commit)")
if(PROFORG_BUILD_ID)
    target_compile_definitions(ProfOrganizer PRIVATE PROFORG_BUILD_ID_RAW=${PROFORG_BUILD_ID})
endif()

# --- Testes (sem Qt): ctest --test-dir build ------------------------------------
enable_testing()
foreach(nome media horario frequencia contraste conta atencao lembrete csv)
    add_executable(test_${nome} tests/test_${nome}.cpp)
    target_include_directories(test_${nome} PRIVATE src)
    add_test(NAME ${nome} COMMAND test_${nome})
endforeach()
