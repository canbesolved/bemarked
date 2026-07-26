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
  deps/mongoose/mongoose.c   # Mongoose 7.22 (GPLv2 or commercial)
  ${WEB_GEN_HEADERS}
)

target_include_directories(bmkd PRIVATE
  src
  deps/mongoose
  ${CMAKE_BINARY_DIR}/generated
)

# --- conversion tool: browser bookmarks HTML -> bookmarks.txt ---
add_executable(bmkd-convert tools/bmkd-convert.c src/model.c)
target_include_directories(bmkd-convert PRIVATE src)

# --- Platform libraries ---
if(WIN32)
  target_link_libraries(bmkd PRIVATE ws2_32)
elseif(APPLE)
  find_library(COCOA_LIB Cocoa)
  target_link_libraries(bmkd PRIVATE ${COCOA_LIB})
else()
  find_package(Threads REQUIRED)
  target_link_libraries(bmkd PRIVATE Threads::Threads)
endif()
