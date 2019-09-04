# Cubism Native Framework

Live2D Cubism 4 Editorで出力したモデルをアプリケーションで利用するためのフレームワークです。

モデルを表示、操作するための各種機能を提供します。
モデルをロードするにはCubism Coreライブラリと組み合わせて使用します。


## コンポーネント

### Effect

自動まばたきやリップシンクなど、モデルに対してモーション情報をエフェクト的に付加する機能を提供します。

### Id

モデルに設定されたパラメータ名・パーツ名・Drawable名を独自の型で管理する機能を提供します。

### Math

行列計算やベクトル計算など、モデルの操作や描画に必要な算術演算の機能を提供します。

### Model

モデルを取り扱うための各種機能（生成、更新、破棄）を提供します。

### Motion

モデルにモーションデータを適用するための各種機能（モーション再生、パラメータブレンド）を提供します。

### Physics

モデルに物理演算による変形操作を適用するための機能を提供します。

### Rendering

各種プラットフォームでモデルを描画するためのグラフィックス命令を実装したレンダラを提供します。

### Type

フレームワーク内で使用するC++型定義を提供します。

### Utils

JSONパーサーやログ出力などのユーティリティ機能を提供します。


## Live2D Cubism Core for Native

当リポジトリにはLive2D Cubism Core for Nativeは同梱されていません。

ダウンロードするには[こちら](https://www.live2d.com/download/cubism-sdk/download-native/)のページを参照ください。


## サンプル

標準的なアプリケーションの実装例については、下記サンプルリポジトリを参照ください。

[CubismNativeSamples](https://github.com/Live2D/CubismNativeSamples)


## マニュアル

[Cubism SDK Manual](https://docs.live2d.com/cubism-sdk-manual/top/)


## 変更履歴

当リポジトリの変更履歴については[CHANGELOG.md](/CHANGELOG.md)を参照ください。


## ライセンス

Cubism Native Framework は Live2D Open Software License で提供しています。
- Live2D Open Software License

  [日本語](https://www.live2d.com/eula/live2d-open-software-license-agreement_jp.html)
  [English](https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html)

Live2D Cubism Core for Native は Live2D Proprietary Software License で提供しています。
- Live2D Proprietary Software License

  [日本語](https://www.live2d.com/eula/live2d-proprietary-software-license-agreement_jp.html)
  [English](https://www.live2d.com/eula/live2d-proprietary-software-license-agreement_en.html)

直近会計年度の売上高が 1000 万円以上の事業者様がご利用になる場合は、SDKリリース(出版許諾)ライセンスに同意していただく必要がございます。
- [SDKリリース(出版許諾)ライセンス](https://www.live2d.com/ja/products/releaselicense)

*All business* users must obtain a Publication License. "Business" means an entity  with the annual gross revenue more than ten million (10,000,000) JPY for the most recent fiscal year.
- [SDK Release (Publication) License](https://www.live2d.com/en/products/releaselicense)
