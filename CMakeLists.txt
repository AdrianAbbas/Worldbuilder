cmake_minimum_required(VERSION 3.22)
# macOS defaults: one universal binary that loads on Apple Silicon and Intel Macs
# back to 10.15. These must be set before project(); override on the command line.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()
project(WORLDBUILDER VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(WORLDBUILDER_BUILD_TESTS "Build the offline processor tests" ON)
set(JUCE_PATH "" CACHE PATH "Existing JUCE 8.0.16 source (otherwise fetched)")
if(JUCE_PATH)
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688 # JUCE 8.0.16
        GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
endif()
set(WORLDBUILDER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND WORLDBUILDER_FORMATS AU)
endif()
juce_add_plugin(WORLDBUILDER
    COMPANY_NAME "Veil Audio"
    BUNDLE_ID "audio.veil.worldbuilder"
    PLUGIN_MANUFACTURER_CODE Veil
    PLUGIN_CODE Wbld
    PRODUCT_NAME "WORLDBUILDER"
    DESCRIPTION "Procedural Soundscape Engine"
    FORMATS ${WORLDBUILDER_FORMATS}
    # An instrument: GarageBand and Logic only reliably host generators this way.
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    # Lets sandboxed hosts (GarageBand) load it in-process. It must still work
    # out-of-process, which the AU host harness checks.
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(WORLDBUILDER PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/WorldEngine.cpp
    Source/DSP/AirLayer.cpp Source/DSP/WaterLayer.cpp Source/DSP/FireLayer.cpp Source/DSP/EarthLayer.cpp
    Source/DSP/ThunderLayer.cpp Source/DSP/TonalBed.cpp Source/DSP/WorldSpace.cpp
    Source/Places/PlaceSlot.cpp Source/Places/Ensemble.cpp Source/Places/Castle.cpp Source/Places/Village.cpp Source/Places/Spaceport.cpp
    Source/Places/Factory.cpp Source/Places/City.cpp Source/Places/Forest.cpp Source/Places/Hive.cpp Source/Places/Temple.cpp Source/Places/Cavern.cpp
    Source/Places/Ruins.cpp Source/Places/Spaceship.cpp Source/Places/Biodome.cpp Source/Places/SeaShip.cpp Source/Places/Suburbs.cpp
    Source/Places/Mall.cpp Source/Places/Military.cpp Source/Places/Beach.cpp Source/Places/Desert.cpp Source/Places/Palace.cpp Source/Places/Tavern.cpp
    Source/Places/Station.cpp Source/Places/Megacity.cpp Source/Places/Bridge.cpp Source/Places/Mining.cpp Source/Places/AlienRuins.cpp Source/Places/Nebula.cpp
    Source/Patterns/PatternStore.cpp Source/Patterns/PatternMidi.cpp Source/Capture/Capture.cpp Source/Keys/Recorder.cpp Source/Samples/SampleBank.cpp
    Source/Locations/Travel.cpp Source/Locations/LocationBank.cpp Source/Arrangement/ArrangementStore.cpp Source/Arrangement/Templates.cpp Source/Scenes/Scenes.cpp
    Source/UI/WorldLookAndFeel.cpp Source/UI/PlaceArt.cpp Source/UI/PlaceOverlays.cpp Source/UI/OverlayArt.cpp Source/UI/PatternEditor.cpp Source/UI/KeysPanel.cpp Source/UI/CapturePanel.cpp Source/UI/LocationBar.cpp Source/UI/ArrangeView.cpp
    Source/UI/TinyPlanet.cpp Source/UI/Style.cpp Source/UI/Pixel.cpp Source/UI/PixelControls.cpp Source/UI/PixelIcons.cpp Source/UI/PixelArt.cpp Source/UI/PixelLookAndFeel.cpp)
target_include_directories(WORLDBUILDER PUBLIC Source)
# Hand-made Place artwork (optional): assets/places/<place>/{back,mid,front,lights}.png
# is bundled as <place>_<layer>.png; without any, the procedural art is used.
file(GLOB WORLDBUILDER_ART_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/assets/places/*/*.png")
if(WORLDBUILDER_ART_SOURCES)
    set(WORLDBUILDER_ART "")
    foreach(source ${WORLDBUILDER_ART_SOURCES})
        get_filename_component(layer "${source}" NAME)
        get_filename_component(folder "${source}" DIRECTORY)
        get_filename_component(place "${folder}" NAME)
        set(target "${CMAKE_BINARY_DIR}/place-art/${place}_${layer}")
        configure_file("${source}" "${target}" COPYONLY)
        list(APPEND WORLDBUILDER_ART "${target}")
    endforeach()
    juce_add_binary_data(WorldbuilderArt HEADER_NAME WorldbuilderArt.h NAMESPACE WorldbuilderArt SOURCES ${WORLDBUILDER_ART})
    target_link_libraries(WORLDBUILDER PRIVATE WorldbuilderArt)
    target_compile_definitions(WORLDBUILDER PRIVATE WB_HAS_PLACE_ART=1)
else()
    target_compile_definitions(WORLDBUILDER PRIVATE WB_HAS_PLACE_ART=0)
endif()
# Hand-drawn pixel sprites (optional, Milestone 13): assets/pixel/<name>.png replaces
# the code-drawn sprite of that name (see assets/pixel/README.md).
file(GLOB WORLDBUILDER_PIXEL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/assets/pixel/*.png")
if(WORLDBUILDER_PIXEL_SOURCES)
    juce_add_binary_data(WorldbuilderPixel HEADER_NAME WorldbuilderPixel.h NAMESPACE WorldbuilderPixel SOURCES ${WORLDBUILDER_PIXEL_SOURCES})
    target_link_libraries(WORLDBUILDER PRIVATE WorldbuilderPixel)
    target_compile_definitions(WORLDBUILDER PRIVATE WB_HAS_PIXEL_ART=1)
else()
    target_compile_definitions(WORLDBUILDER PRIVATE WB_HAS_PIXEL_ART=0)
endif()
# The developer's recordings (Milestone 15), prepared by tools/sample-prep/prepare.py from
# the private Samples repository. They are copied into every plugin bundle's Resources
# (Contents/Resources/Samples), where the sampler loads them; GarageBand's sandbox may
# read the plugin's own bundle. Without them the sampled instruments fall back to their
# synthesised models (a build for forks or without the SAMPLES_TOKEN secret).
set(WORLDBUILDER_SAMPLES_DIR "" CACHE PATH "Prepared samples (tools/sample-prep/prepare.py --out)")
if(WORLDBUILDER_SAMPLES_DIR AND EXISTS "${WORLDBUILDER_SAMPLES_DIR}/index.json")
    get_filename_component(WORLDBUILDER_SAMPLES_DIR "${WORLDBUILDER_SAMPLES_DIR}" ABSOLUTE)
    message(STATUS "WORLDBUILDER: shipping the samples in ${WORLDBUILDER_SAMPLES_DIR}")
    foreach(format ${WORLDBUILDER_FORMATS})
        set(format_target WORLDBUILDER_${format})
        add_custom_command(TARGET ${format_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:${format_target}>/../Resources/Samples"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${WORLDBUILDER_SAMPLES_DIR}" "$<TARGET_FILE_DIR:${format_target}>/../Resources/Samples"
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:${format_target}>/../Resources/Samples/report.json"
            COMMENT "Copying the samples into the ${format} bundle")
    endforeach()
else()
    message(STATUS "WORLDBUILDER: no prepared samples (WORLDBUILDER_SAMPLES_DIR); sampled instruments use their synthesised fallbacks")
endif()
target_compile_definitions(WORLDBUILDER PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(WORLDBUILDER PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
if(WORLDBUILDER_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(WorldbuilderTests PRODUCT_NAME "WORLDBUILDER Tests")
    # The linked plugin target exports the same setting using its format macro.
    get_target_property(wb_test_definitions WorldbuilderTests COMPILE_DEFINITIONS)
    list(REMOVE_ITEM wb_test_definitions JUCE_STANDALONE_APPLICATION=1)
    set_target_properties(WorldbuilderTests PROPERTIES COMPILE_DEFINITIONS "${wb_test_definitions}")
    target_sources(WorldbuilderTests PRIVATE Tests/WorldbuilderTests.cpp)
    target_link_libraries(WorldbuilderTests PRIVATE WORLDBUILDER juce::juce_audio_utils)
    target_compile_definitions(WorldbuilderTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        WB_TEST_SAMPLES_DIR="${WORLDBUILDER_SAMPLES_DIR}")
    add_test(NAME worldbuilder_tests COMMAND WorldbuilderTests)
endif()
