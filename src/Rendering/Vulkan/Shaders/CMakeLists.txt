set(normal_shader_files
   ${CMAKE_CURRENT_SOURCE_DIR}/src/common.glsl
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrc.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcCopy.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMask.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskInverted.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskInvertedPremultipliedAlpha.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskPremultipliedAlpha.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcPremultipliedAlpha.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcSetupMask.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrc.vert
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrcCopy.vert
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrcMasked.vert
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrcSetupMask.vert
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrcBlend.vert
   ${CMAKE_CURRENT_SOURCE_DIR}/src/VertShaderSrcMaskedBlend.vert
)

set(blend_shader_files
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcColorBlend.glsl
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcAlphaBlend.glsl
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcBlend.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskBlend.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskInvertedBlend.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskInvertedPremultipliedAlphaBlend.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcMaskPremultipliedAlphaBlend.frag
   ${CMAKE_CURRENT_SOURCE_DIR}/src/FragShaderSrcPremultipliedAlphaBlend.frag
)
set (shader_files "${normal_shader_files};${blend_shader_files}")

set(color_blend_modes
    Normal
    Add
    AddGlow
    Darken
    Multiply
    ColorBurn
    LinearBurn
    Lighten
    Screen
    ColorDodge
    Overlay
    SoftLight
    HardLight
    LinearLight
    Hue
    Color
)

set(Color_Blend_Mode_Normal 0)
set(Color_Blend_Mode_Add 1)
set(Color_Blend_Mode_AddGlow 2)
set(Color_Blend_Mode_Darken 3)
set(Color_Blend_Mode_Multiply 4)
set(Color_Blend_Mode_ColorBurn 5)
set(Color_Blend_Mode_LinearBurn 6)
set(Color_Blend_Mode_Lighten 7)
set(Color_Blend_Mode_Screen 8)
set(Color_Blend_Mode_ColorDodge 9)
set(Color_Blend_Mode_Overlay 10)
set(Color_Blend_Mode_SoftLight 11)
set(Color_Blend_Mode_HardLight 12)
set(Color_Blend_Mode_LinearLight 13)
set(Color_Blend_Mode_Hue 14)
set(Color_Blend_Mode_Color 15)

set(alpha_blend_modes
    Over
    Atop
    Out
    ConjointOver
    DisjointOver
)

set(Alpha_Blend_Mode_Over 0)
set(Alpha_Blend_Mode_Atop 1)
set(Alpha_Blend_Mode_Out 2)
set(Alpha_Blend_Mode_ConjointOver 3)
set(Alpha_Blend_Mode_DisjointOver 4)

# Shader compilation
foreach(shader ${normal_shader_files})
  get_filename_component(file_name ${shader} NAME)
  if(file_name MATCHES "common.glsl")
    continue()
  endif()
  get_filename_component(full_path ${shader} ABSOLUTE)
  set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/compiledShaders)
  string(REGEX REPLACE \\.frag|\\.vert "" output_file ${file_name})
  set(output_file ${output_dir}/${output_file}.spv)
  set(compiled_shaders_framework ${compiled_shaders_framework} ${output_file})
  set(compiled_shaders_framework ${compiled_shaders_framework} PARENT_SCOPE)
  set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)

  if(WIN32)
    add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
            COMMAND $ENV{VK_SDK_PATH}/Bin/glslc.exe ${full_path} -o ${output_file}
            DEPENDS ${full_path}
        )
  endif()
  if(UNIX AND NOT APPLE)
    add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
            COMMAND glslc ${full_path} -o ${output_file}
            DEPENDS ${full_path}
        )
  endif()
endforeach()

foreach(shader ${blend_shader_files})
  get_filename_component(file_name ${shader} NAME)
  if(file_name MATCHES "\.glsl")
    continue()
  endif()
  get_filename_component(full_path ${shader} ABSOLUTE)
  set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/compiledShaders)
  get_filename_component(output_file ${shader} NAME_WE)
  foreach(color_blend_mode ${color_blend_modes})
    foreach(alpha_blend_mode ${alpha_blend_modes})
      if((color_blend_mode MATCHES "^Normal$") AND (alpha_blend_mode MATCHES "^Over$"))
        continue()
      endif()
      get_filename_component(output_file ${shader} NAME_WE)
      set(output_file ${output_dir}/${output_file}${color_blend_mode}${alpha_blend_mode}.spv)
      set(compiled_shaders_framework ${compiled_shaders_framework} ${output_file})
      set(compiled_shaders_framework ${compiled_shaders_framework} PARENT_SCOPE)
      set_source_files_properties(${shader} PROPERTIES HEADER_FILE_ONLY TRUE)
      set(define_color_blend_mode "CSM_COLOR_BLEND_MODE=${Color_Blend_Mode_${color_blend_mode}}")
      set(define_alpha_blend_mode "CSM_ALPHA_BLEND_MODE=${Alpha_Blend_Mode_${alpha_blend_mode}}")
      if(WIN32)
        add_custom_command(
                OUTPUT ${output_file}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
                COMMAND $ENV{VK_SDK_PATH}/Bin/glslc.exe -D${define_color_blend_mode} -D${define_alpha_blend_mode} -o ${output_file} ${full_path}
                DEPENDS ${full_path}
            )
      endif()
      if(UNIX AND NOT APPLE)
        add_custom_command(
                OUTPUT ${output_file}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
                COMMAND glslc -D${define_color_blend_mode} -D${define_alpha_blend_mode} -o ${output_file} ${full_path}
                DEPENDS ${full_path}
            )
      endif()
    endforeach()
  endforeach()
endforeach()


source_group("shaders" FILES ${shader_files})
add_custom_target(
    FrameworkShaders ALL
    DEPENDS ${compiled_shaders_framework}
    SOURCES ${shader_files}
)
