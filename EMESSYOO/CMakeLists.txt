cmake_minimum_required(VERSION 3.22)

project(MSU1PrepStudio VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE as a subdirectory (assumes JUCE is cloned/downloaded)
# You can also use FetchContent to download JUCE automatically
add_subdirectory(JUCE)

# Define the project as a GUI application
juce_add_gui_app(MSU1PrepStudio
    COMPANY_NAME "MSU1Tools"
    PRODUCT_NAME "EMESSYOO"
    BUNDLE_ID "com.msu1tools.prepstudio"
    VERSION ${PROJECT_VERSION}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/EMESSYOO.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/EMESSYOO.png"
)

juce_generate_juce_header(MSU1PrepStudio)

# Source files
target_sources(MSU1PrepStudio
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.h
        Source/MainComponent.cpp
        Source/Core/MSUProjectState.h
        Source/Core/MSUProjectState.cpp
        Source/Core/AudioFileHandler.h
        Source/Core/AudioFileHandler.cpp
        Source/Core/SNESROMReader.h
        Source/Core/SNESROMReader.cpp
        Source/Core/BackupMetadataStore.h
        Source/Core/BackupMetadataStore.cpp
        Source/Audio/AudioImporter.h
        Source/Audio/AudioImporter.cpp
        Source/Audio/AudioPlayer.h
        Source/Audio/AudioPlayer.cpp
        Source/Audio/BeforeAfterPreviewPlayer.h
        Source/Audio/BeforeAfterPreviewPlayer.cpp
        Source/Audio/PreviewPlayer.h
        Source/Audio/PreviewPlayer.cpp
        Source/Audio/NormalizationAnalyzer.h
        Source/Audio/NormalizationAnalyzer.cpp
    Source/Audio/VolumeMatchAnalyzer.h
    Source/Audio/VolumeMatchAnalyzer.cpp
        Source/Export/MSU1Exporter.h
        Source/Export/MSU1Exporter.cpp
        Source/Export/MSUManifestUpdater.h
        Source/Export/MSUManifestUpdater.cpp
        Source/Export/ManifestHandler.h
        Source/Export/ManifestHandler.cpp
        Source/UI/WaveformView.h
        Source/UI/WaveformView.cpp
        Source/UI/LoopMarker.h
        Source/UI/LoopMarker.cpp
        Source/UI/TransportControls.h
        Source/UI/TransportControls.cpp
        Source/UI/ToolbarPanel.h
        Source/UI/ToolbarPanel.cpp
        Source/UI/MSUFileBrowser.h
        Source/UI/MSUFileBrowser.cpp
        Source/UI/CustomLookAndFeel.h
        Source/UI/CustomLookAndFeel.cpp
        Source/UI/LoopEditorTab.h
        Source/UI/LoopEditorTab.cpp
        Source/UI/AudioLevelStudioComponent.h
        Source/UI/AudioLevelStudioComponent.cpp
        Source/Dialogs/NormalizationDialog.h
        Source/Dialogs/NormalizationDialog.cpp
        Source/Dialogs/ExportDialog.h
        Source/Dialogs/ExportDialog.cpp
        Source/Dialogs/BackupRestoreDialog.h
        Source/Dialogs/BackupRestoreDialog.cpp
)

# Compiler definitions
target_compile_definitions(MSU1PrepStudio
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MSU1PrepStudio,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MSU1PrepStudio,JUCE_VERSION>"
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_USE_DARK_SPLASH_SCREEN=1
)

# Link JUCE modules
target_link_libraries(MSU1PrepStudio
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_include_directories(MSU1PrepStudio
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/MSU1PrepStudio_artefacts/JuceLibraryCode
)
