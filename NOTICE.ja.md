[English](NOTICE.md) / [日本語](NOTICE.ja.md)

---

# お知らせ

## [制限事項] Cubism 5 SDK for Native R5 beta2 の動作環境について (2025-10-30 更新)

Cubism 5 SDK for Native R5 beta2 について、Vulkan環境は**ビルドができません**ので、あらかじめご了承ください。
Vulkan環境は今後のアップデートにてCubism 5.3新機能も含め対応を予定しています。 
その他の環境については対応しております。

## [注意事項] Cubism 5 SDK for Native R5 beta2 Metal環境のパフォーマンスについて (2025-10-30)

Cubism 5 SDK for Native R5 beta2 Metal環境にてモデルを描画した際、iOS 26、iPadOS 26 及び macOS 26 環境においてパフォーマンスが劣化する場合がございます。
本件はコマンドエンコーダー（ `MTLRenderCommandEncoder` 及び `MTLBlitCommandEncoder` ）のオーバーヘッドによるものと考えておりますが、
今後のアップデートにてFrameworkの改修や、最新のMetalバージョンへ移行することも含めて調査し、対応を検討してまいります。

---

©Live2D
