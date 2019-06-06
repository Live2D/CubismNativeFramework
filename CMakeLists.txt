cmake_minimum_required(VERSION 3.1)

# includeパスの追加
include_directories(src)
include_directories(../Core/include)


# 各プラットフォーム共通ファイル 
set(commonfiles
    
    src/Effect/CubismBreath.cpp
    src/Effect/CubismEyeBlink.cpp
    src/Effect/CubismPose.cpp
    
    src/Id/CubismId.cpp
    src/Id/CubismIdManager.cpp
    
    src/Math/CubismMath.cpp
    src/Math/CubismMatrix44.cpp
    src/Math/CubismModelMatrix.cpp
    src/Math/CubismTargetPoint.cpp
    src/Math/CubismVector2.cpp
    src/Math/CubismViewMatrix.cpp
    
    src/Model/CubismModel.cpp
    src/Model/CubismModelUserData.cpp
    src/Model/CubismModelUserDataJson.cpp
    src/Model/CubismUserModel.cpp
    src/Model/CubismMoc.cpp
    
    src/Motion/CubismExpressionMotion.cpp
    src/Motion/CubismMotion.cpp
    src/Motion/CubismMotionJson.cpp
    src/Motion/CubismMotionManager.cpp
    src/Motion/CubismMotionQueueEntry.cpp
    src/Motion/CubismMotionQueueManager.cpp
    src/Motion/ACubismMotion.cpp
    
    src/Physics/CubismPhysicsJson.cpp
    src/Physics/CubismPhysics.cpp
    
    src/Rendering/CubismRenderer.cpp
    
    src/Type/csmRectF.cpp
    src/Type/csmString.cpp
    
    src/Utils/CubismDebug.cpp
    src/Utils/CubismJson.cpp
    src/Utils/CubismString.cpp
    
    src/CubismDefaultParameterId.cpp
    src/CubismFramework.cpp
    src/CubismModelSettingJson.cpp
    src/CubismCdiJson.cpp

)

if (${FRAMEWORK_SOURCE} MATCHES "D3D9" )

# ここにプロジェクトに追加するソースファイルを追加 DirectX9 

# アプリケーション側で設定したgl系のincludeパスをセットする
include_directories("${FRAMEWORK_DX9_INCLUDE_PATH}")

set(Framework
    
    ${commonfiles}
    
    src/Rendering/D3D9/CubismRenderer_D3D9.cpp
    src/Rendering/D3D9/CubismRenderState_D3D9.cpp
    src/Rendering/D3D9/CubismShader_D3D9.cpp
    src/Rendering/D3D9/CubismOffscreenSurface_D3D9.cpp

)

elseif (${FRAMEWORK_SOURCE} MATCHES "D3D11" )

# ここにプロジェクトに追加するソースファイルを追加 DirectX11 

# アプリケーション側で設定したgl系のincludeパスをセットする
include_directories("${FRAMEWORK_DX11_INCLUDE_PATH}")

set(Framework
    
    ${commonfiles}
    
    src/Rendering/D3D11/CubismOffscreenSurface_D3D11.cpp
    src/Rendering/D3D11/CubismRenderer_D3D11.cpp
    src/Rendering/D3D11/CubismRenderState_D3D11.cpp
    src/Rendering/D3D11/CubismShader_D3D11.cpp

)

else ()

# ここにプロジェクトに追加するソースファイルを追加 OpenGL

# アプリケーション側で設定したgl系のincludeパスをセットする
include_directories("${FRAMEWORK_GLFW_PATH}")
include_directories("${FRAMEWORK_GLEW_PATH}")

# プリプロセッサ定義の追加
add_definitions(${FRAMEWORK_DEFINITIOINS})

set(Framework

    ${commonfiles}

    src/Rendering/OpenGL/CubismRenderer_OpenGLES2.cpp
    src/Rendering/OpenGL/CubismOffscreenSurface_OpenGLES2.cpp

)

endif ()

# staticライブラリとして追加
add_library (Framework STATIC ${Framework})
