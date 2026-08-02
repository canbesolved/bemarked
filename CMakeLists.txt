# SPDX-License-Identifier: GPL-2.0-only
cmake_minimum_required(VERSION 3.16)
project(bmkd C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Embed web UI assets: web/<file> -> generated/<symbol>.h ---
set(WEB_ASSETS
  "index.html:web_index_html"
  "style.css:web_style_css"
  "app.js:web_app_js"
  "assets/folder_icon.svg:folder_icon_svg"
  "assets/arrow_icon.svg:arrow_icon_svg"
  "assets/arrow_down_icon.svg:arrow_down_icon_svg"
  "assets/add_icon.svg:add_icon_svg"
  "assets/app_icon.svg:app_icon_svg"
  "assets/favicon.ico:favicon_ico"
  "assets/apple-touch-icon.png:apple_touch_icon_png"
)
set(WEB_GEN_HEADERS "")
foreach(entry ${WEB_ASSETS})
  string(REPLACE ":" ";" parts "${entry}")
  list(GET parts 0 fname)
  list(GET parts 1 sym)
  set(src ${CMAKE_SOURCE_DIR}/web/${fname})
  set(gen ${CMAKE_BINARY_DIR}/generated/${sym}.h)
  add_custom_command(
    OUTPUT  ${gen}
    COMMAND ${CMAKE_COMMAND}
            -DINPUT=${src} -DOUTPUT=${gen} -DSYMBOL=${sym}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_asset.cmake
    DEPENDS ${src} ${CMAKE_SOURCE_DIR}/cmake/embed_asset.cmake
    COMMENT "Embedding ${fname} -> ${sym}.h"
    VERBATIM
  )
  list(APPEND WEB_GEN_HEADERS ${gen})
endforeach()

add_executable(bmkd
  src/main.c
  src/config.c
  src/model.c
  src/storage.c
  src/server.c
  src/platform.c
  src/compat.c            # POSIX/Windows portability shims
  src/import.c            # browser bookmarks HTML -> bookmarks (CLI + /import)
  deps/mongoose/mongoose.c   # Mongoose 7.22 (GPLv2 or commercial)
  ${WEB_GEN_HEADERS}
)

target_include_directories(bmkd PRIVATE
  src
  deps/mongoose
  ${CMAKE_BINARY_DIR}/generated
)

# allow multi-MB request bodies (bookmark imports) — Mongoose default is 3 MB
target_compile_definitions(bmkd PRIVATE MG_MAX_RECV_SIZE=33554432)

# --- Platform libraries ---
# Threads are needed everywhere (on Windows this pulls in winpthreads when
# building with mingw-w64's posix thread model).
find_package(Threads REQUIRED)
target_link_libraries(bmkd PRIVATE Threads::Threads)
if(WIN32)
  target_link_libraries(bmkd PRIVATE ws2_32)   # Mongoose sockets
endif()
