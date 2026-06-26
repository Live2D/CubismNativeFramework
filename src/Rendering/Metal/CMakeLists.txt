target_sources(${LIB_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismCommandBuffer_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismCommandBuffer_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismDeviceInfo_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismDeviceInfo_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismOffscreenManager_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismOffscreenManager_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismOffscreenRenderTarget_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismOffscreenRenderTarget_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderer_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderer_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderTarget_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderTarget_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismShader_Metal.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismShader_Metal.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/MetalShaderTypes.h
)

# Disable ARC
set_target_properties(
    ${LIB_NAME}
    PROPERTIES
    MACOSX_BUNDLE YES
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC NO
)
