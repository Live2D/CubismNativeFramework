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

set(shader_include_files
    ${CMAKE_CURRENT_SOURCE_DIR}/MetalShaderTypes.h
)
set(blend_shader_include_files
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcColorBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcAlphaBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendShaderFuncs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/BlendShaderStructs.h
)
set(blend_shader_files
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcMaskBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcMaskInvertedBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcMaskInvertedPremultipliedAlphaBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcMaskPremultipliedAlphaBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/FragShaderSrcPremultipliedAlphaBlend.metal
)

set(normal_shader_files
    ${CMAKE_CURRENT_SOURCE_DIR}/MetalShaders.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/VertShaderSrcBlend.metal
    ${CMAKE_CURRENT_SOURCE_DIR}/VertShaderSrcMaskedBlend.metal
)

set(shader_files "${normal_shader_files};${blend_shader_files};${shader_include_files};${blend_shader_include_files}")

if(${PLATFORM_NAME} MATCHES "iphoneos")
    set(sdk_name iphoneos)
elseif(${PLATFORM_NAME} MATCHES "iphonesimulator")
    set(sdk_name iphonesimulator)
else() # Catalyst
    set(sdk_name macosx)
endif()

set(output_root_dir ${CMAKE_CURRENT_BINARY_DIR})
set(output_intermediate_dir ${output_root_dir}/Intermediates)
set(output_lib_dir ${output_root_dir}/FrameworkMetallibs)
foreach(shader ${normal_shader_files})
  get_filename_component(file_name ${shader} NAME)
  get_filename_component(full_path ${shader} ABSOLUTE)
  get_filename_component(output_file ${shader} NAME_WE)
  set(output_air_file ${output_intermediate_dir}/${output_file}.air)
  set(output_metallib_file ${output_lib_dir}/${output_file}.metallib)
  set(compiled_shaders_framework ${compiled_shaders_framework} ${output_air_file})
  set(compiled_shaders_framework ${compiled_shaders_framework} ${output_metallib_file})
  set(compiled_shaders_framework ${compiled_shaders_framework} PARENT_SCOPE)
  add_custom_command(
      OUTPUT ${output_metallib_file}
      COMMAND xcrun -sdk ${sdk_name} metal -o ${output_air_file} -c ${shader}
      COMMAND xcrun -sdk ${sdk_name} metallib -o ${output_metallib_file} ${output_air_file}
      DEPENDS "${full_path};${shader_include_files}"
  )
  set(air_files "${air_files};${output_air_file}")
  set(metallib_files "${metallib_files};${output_metallib_file}" PARENT_SCOPE)
endforeach()

foreach(shader ${blend_shader_files})
  get_filename_component(file_name ${shader} NAME)
  get_filename_component(full_path ${shader} ABSOLUTE)

  foreach(color_blend_mode ${color_blend_modes})
    foreach(alpha_blend_mode ${alpha_blend_modes})
        if((color_blend_mode MATCHES "^Normal$") AND (alpha_blend_mode MATCHES "^Over$"))
            continue()
        endif()

        get_filename_component(output_file ${shader} NAME_WE)
        set(output_file ${output_file}${color_blend_mode}${alpha_blend_mode})
        set(output_air_file ${output_intermediate_dir}/${output_file}.air)
        set(output_metallib_file ${output_lib_dir}/${output_file}.metallib)
        set(compiled_shaders_framework ${compiled_shaders_framework} ${output_air_file})
        set(compiled_shaders_framework ${compiled_shaders_framework} ${output_metallib_file})
        set(compiled_shaders_framework ${compiled_shaders_framework} PARENT_SCOPE)
        set(define_color_blend_mode "CSM_COLOR_BLEND_MODE=${Color_Blend_Mode_${color_blend_mode}}")
        set(define_alpha_blend_mode "CSM_ALPHA_BLEND_MODE=${Alpha_Blend_Mode_${alpha_blend_mode}}")
        add_custom_command(
            OUTPUT ${output_metallib_file}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_intermediate_dir}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_lib_dir}
            COMMAND xcrun -sdk ${sdk_name} metal -D ${define_alpha_blend_mode} -D ${define_color_blend_mode} -o ${output_air_file} -c ${shader}
            COMMAND xcrun -sdk ${sdk_name} metallib -o ${output_metallib_file} ${output_air_file}
            DEPENDS "${full_path};${shader_include_files};${blend_shader_include_files}"
        )
        set(air_files "${air_files};${output_air_file}")
        set(metallib_files "${metallib_files};${output_metallib_file}" PARENT_SCOPE)
    endforeach()
  endforeach()
endforeach()



source_group("shaders" FILES ${shader_files})
set(compiled_shaders_framework ${metallib_files} PARENT_SCOPE)
set(compiled_shaders_framework ${compiled_shaders_framework};${air_files} PARENT_SCOPE)
add_custom_target(
    FrameworkShaders ALL
    DEPENDS ${compiled_shaders_framework}
    SOURCES ${metallib_files}
)

set_target_properties(
    FrameworkShaders
    PROPERTIES
    MACOSX_BUNDLE YES
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC NO
)
