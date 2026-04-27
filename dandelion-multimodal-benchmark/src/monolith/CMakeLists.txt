set(TESS_IMPORT_DIR "${CMAKE_SOURCE_DIR}/third_party/tesseract-import")
set(TESS_INSTALL_DIR "C:/Program Files/Tesseract-OCR")

add_executable(monolith_pipeline
    main.cpp
    classifier.cpp
    text_pipeline.cpp
    audio_pipeline.cpp
    image_pipeline.cpp
)

target_include_directories(monolith_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${TESS_IMPORT_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/include
)

target_link_libraries(monolith_pipeline PRIVATE
    bench_common
    whisper
    "${TESS_IMPORT_DIR}/libtesseract-5.lib"
)

# Copy libtesseract-5.dll next to the exe so it can be found at runtime
add_custom_command(TARGET monolith_pipeline POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TESS_INSTALL_DIR}/libtesseract-5.dll"
        "$<TARGET_FILE_DIR:monolith_pipeline>/libtesseract-5.dll"
    COMMENT "Copying libtesseract-5.dll"
)
