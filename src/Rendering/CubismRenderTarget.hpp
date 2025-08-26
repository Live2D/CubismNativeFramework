/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include "CubismFramework.hpp"
#include "Type/csmVector.hpp"
#include "Type/csmRectF.hpp"
#include "Type/csmMap.hpp"

//------------ LIVE2D NAMESPACE ------------
namespace Live2D { namespace Cubism { namespace Framework { namespace Rendering {

/**
 * @brief   モデル描画を処理するレンダラ<br>
 *           サブクラスに環境依存の描画命令を記述する
 */
template<class T>
class CubismRenderTarget
{
public:
    /**
     * @brief   オフスクリーンのIndexを設定
     */
    void SetOffscreenIndex(csmInt32 offscreenIndex);

    /**
     * @brief   オフスクリーンのIndexを取得
     */
    csmInt32 GetOffscreenIndex() const;

    /**
     * @brief 以前のオフスクリーンレンダリングターゲットを設定
     *
     * @param oldOffscreen 以前のオフスクリーンレンダリングターゲット
     */
    void SetOldOffscreen(T* oldOffscreen);

    /**
     * @brief 以前のオフスクリーンレンダリングターゲットを取得
     *
     * @return 以前のオフスクリーンレンダリングターゲット
     */
    T* GetOldOffscreen() const;

    /**
     * @brief 親のオフスクリーンレンダリングターゲットを設定
     *
     * @param parentRenderTarget 親のオフスクリーンレンダリングターゲット
     */
    void SetParentPartOffscreen(T* parentRenderTarget);

    /**
     * @brief 親のオフスクリーンレンダリングターゲットを取得
     *
     * @return 親のオフスクリーンレンダリングターゲット
     */
    T* GetParentPartOffscreen() const;

protected:
    /**
     * @brief   コンストラクタ
     */
    CubismRenderTarget();

    /**
     * @brief   デストラクタ
     */
    virtual ~CubismRenderTarget();

private:
    csmInt32 _offscreenIndex;   ///< _offscreensのIndex
    T* _parentRenderTarget;     ///< 親のオフスクリーンレンダリングターゲット
    T* _oldOffscreen;           ///< 以前のオフスクリーンレンダリングターゲット
};

template<class T>
void CubismRenderTarget<T>::SetOffscreenIndex(csmInt32 const offscreenIndex)
{
    _offscreenIndex = offscreenIndex;
}

template<class T>
csmInt32 CubismRenderTarget<T>::GetOffscreenIndex() const
{
    return _offscreenIndex;
}

template<class T>
void CubismRenderTarget<T>::SetOldOffscreen(T* oldOffscreen)
{
    _oldOffscreen = oldOffscreen;
}

template<class T>
T* CubismRenderTarget<T>::GetOldOffscreen() const
{
    return _oldOffscreen;
}

template<class T>
void CubismRenderTarget<T>::SetParentPartOffscreen(T* parentRenderTarget)
{
    _parentRenderTarget = parentRenderTarget;
}

template<class T>
T* CubismRenderTarget<T>::GetParentPartOffscreen() const
{
    return _parentRenderTarget;
}

template<class T>
CubismRenderTarget<T>::CubismRenderTarget()
{
}

template<class T>
CubismRenderTarget<T>::~CubismRenderTarget()
{
}
}}}}

//------------ LIVE2D NAMESPACE ------------
