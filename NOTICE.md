[English](NOTICE.md) / [日本語](NOTICE.ja.md)

---

# Notices

## [Restrictions] Operating Environment for Cubism 5 SDK for Native R5 beta2 (Updated 2025-10-30)

Please be aware that the Cubism 5 SDK for Native R5 beta2 does not support builds in Vulkan environments. 
Support for Vulkan, including new features of Cubism 5.3, is planned for future updates. 
Other environments are supported.

## [Cautions] Performance of Cubism 5 SDK for Native R5 beta2 in Metal Environments (2025-10-30)

When rendering models in the Metal environment of Cubism 5 SDK for Native R5 beta2, performance may degrade in iOS 26, iPadOS 26, and macOS 26 environments.
This issue is believed to be caused by overhead from the command encoders (`MTLRenderCommandEncoder` and `MTLBlitCommandEncoder`).
We will investigate and consider addressing this issue in future updates, including modifications to the Framework and migration to the latest Metal version.

---

©Live2D
