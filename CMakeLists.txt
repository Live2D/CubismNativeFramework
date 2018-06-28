cmake_minimum_required(VERSION 3.1)

# includeパスの追加
include_directories(src)
include_directories(../Core/include)

# アプリケーション側で設定したgl系のincludeパスをセットする
include_directories("${FRAMEWORK_GLFW_PATH}")
include_directories("${FRAMEWORK_GLEW_PATH}")

# プリプロセッサ定義の追加
add_definitions(${FRAMEWORK_DEFINITIOINS})

# ここにプロジェクトに追加するソースファイルを追加
set(Framework
    
    src/CubismDefaultParameterId.cpp
    src/CubismFramework.cpp
    src/CubismModelSettingJson.cpp
    
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
    
    src/Model/CubismMoc.cpp
    src/Model/CubismModel.cpp
    src/Model/CubismModelUserData.cpp
    src/Model/CubismModelUserDataJson.cpp
    src/Model/CubismUserModel.cpp
    
    src/Motion/ACubismMotion.cpp
    src/Motion/CubismExpressionMotion.cpp
    src/Motion/CubismMotion.cpp
    src/Motion/CubismMotionJson.cpp
    src/Motion/CubismMotionManager.cpp
    src/Motion/CubismMotionQueueEntry.cpp
    src/Motion/CubismMotionQueueManager.cpp
    
    src/Physics/CubismPhysics.cpp
    src/Physics/CubismPhysicsJson.cpp
    
    src/Rendering/CubismRenderer.cpp
    src/Rendering/OpenGL/CubismRenderer_OpenGLES2.cpp
    
    src/Type/csmRectF.cpp
    src/Type/csmString.cpp
    
    src/Utils/CubismDebug.cpp
    src/Utils/CubismJson.cpp
    src/Utils/CubismString.cpp
    
    src/CubismDefaultParameterId.cpp
    src/CubismFramework.cpp
    src/CubismModelSettingJson.cpp
)

# staticライブラリとして追加
add_library (Framework STATIC ${Framework})
