# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).


## [4-r.7] - 2023-05-25

### Added

* Add some function for checking consistency of MOC3.
  * Add the function of checking consistency on reviving a MOC3. (`CubismMoc::Create`)
  * Add the function of checking consistency from unrevived MOC3. (`CubismMoc::HasMocConsistencyFromUnrevivedMoc`)
* Add some functions to change Multiply and Screen colors on a per part basis.

### Changed

* Change access specifier for `CubismExpressionMotion`.
* Change to get opacity according to the current time of the motion.


## [4-r.6.2] - 2023-03-16

### Fixed

* Fix a bug that caused double and triple buffering to be disabled on DirectX systems due to multiple render textures in 4-r.6.
* Fix the condition of splitting the mask buffer according to the number of masks used to be in accordance with the specification.
* Fix some problems related to Cubism Core.
  * See `CHANGELOG.md` in Core.


## [4-r.6.1] - 2023-03-10

### Added

* Add function to validate MOC3 files.


## [4-r.6] - 2023-02-21

### Added

* Add API to allow users to configure culling.
* The number of render textures used can now be increased arbitrarily.
  * The maximum number of masks when using multiple render textures has been increased to "number of render textures * 32".

### Fixed

* Fix a bug that models with culling were not rendered correctly in Metal.
* Fix a bug that caused some masks to be rendered incorrectly when using 17 or more masks in DirectX systems.

### Removed

* Remove unnecessary variable `modelToWorldF` in renderer.


## [4-r.5.1] - 2022-09-15

### Fixed

* Fix a bug that caused a crash if an empty array existed in json.


## [4-r.5] - 2022-09-08

### Added

* Add immediate stabilization of physics.
* Add the multilingual supported documents.

### Fixed

* Fix a memory leak that occurred when defining a Vector with size 0.


## [4-r.5-beta.5] - 2022-08-04

### Fixed

* Fix a bug in which processing was interrupted when an invalid vertex was specified in the middle of a physics operation.
* Fix physics system input to be split by the physics setting time.
* Fix a tiny memory leak in Cubism physics.


## [4-r.5-beta.4.1] - 2022-07-08

### Fixed

* Fix Core API called in GetDrawableParentPartIndex function.


## [4-r.5-beta.4] - 2022-07-07

### Added

* Add a function to get the latest .moc3 Version and the .moc3 Version of the loaded model.
* Add a function to get the type of parameters of the model.
* Add a function to get the parent part of the model's Drawable.

### Changed

* Disable ARC in Metal renderer.

### Fixed

* Fix dangling pointer in `GetRenderPassDescriptor` function for Metal.


## [4-r.5-beta.3] - 2022-06-16

### Fixed

* `GetDrawableTextureIndices` function in `CubismModel` has been renamed to `GetDrawableTextureIndex` because the name was not correct.
  * `GetDrawableTextureIndices` function is marked as deprecated.
* Fix physics system behaviour when exists Physics Fps Setting in .physics3.json.
* Fix force close problem when invalid `physics3.json` is read.
* Fixed memory leak in Cocos2d-x.


## [4-r.5-beta.2] - 2022-06-02

### Fixed

* Fixed a bug that caused Multiply Color / Screen Color of different objects to be applied.
  * See `CHANGELOG.md` in Core.
  * No modifications to Samples and Framework.


## [4-r.5-beta.1] - 2022-05-19

### Added

* Add processing related to Multiply Color / Screen Color added in Cubism 4.2.
* Add a function to reset the physics states.

### Fixed

* Fix GetTextureDirectory() to return the directory name of the 0th texture path.


## [4-r.4] - 2021-12-09

### Added

* Add the rendering options on Metal:
  * `USE_RENDER_TARGET`
  * `USE_MODEL_RENDER_TARGET`

* Add anisotropic filtering to Metal.
* Add a macro to toggle the precision of floating point numbers in OpenGL fragment shaders.
* Add a function to check `.cdi3.json` exists from `.model3.json`.
* Add `CubismJsonHolder`, a common class used to instantiate and check the validity of `CubismJson`.
  * Each Json parser will now warn if an class of `CubismJson` is invalid.

### Changed

* Change each Json parser inherits a common class `CubismJsonHolder`.

### Fixed

* Fix renderer for Cocos2d-x v4.0.
   * `RenderTexture` was empty when using `USE_MODEL_RENDER_TARGET`.
* Fix motions with different fade times from switching properly.
* Fix a bug that motions currently played do not fade out when play a motion.


## [4-r.4-beta.1] - 2021-10-07

### Added

* Add a function to parse the opacity from `.motion3.json`.
* Add a Renderer for Metal API in iOS.
  * There are some restrictions, see [NOTICE.md](https://github.com/Live2D/CubismNativeSamples/blob/e4144053d1546473d2e76d30779e26d84b00d9f9/NOTICE.md).

### Fixed

* Fix return correct error values for out-of-index arguments in cubismjson by [@cocor-au-lait](https://github.com/cocor-au-lait).
* Fix a warning when `SegmentType` could not be obtained when loading motion.
* Fix renderer for Cocos2d-x v4.0.
  * Rendering didn't work when using `USE_RENDER_TARGET` and high precision masking.


## [4-r.3] - 2021-06-10

## [4-r.3-beta.1] - 2021-05-13

### Added

* Add a Renderer for Cocos2d-x v4.0.
* Implement a function to get the correct value when the time axis of the Bezier handle cannot be linear.
* Add an argument to the function `SetClippingMaskBufferSize` to set the height and width of the clipping mask buffer.

### Changed

* Improve the quality of Clipping Mask on high precision masking.


## [4-r.2] - 2021-02-17

### Added

* Implement anisotropic filtering for DirectX-based Renderer.
* Implement get pixel size and `PixelsPerUnit` of the model

### Changed

* Check pointer before use to avoid crash by [@Xrysnow](https://github.com/Xrysnow)

### Fixed

* Fix Physics input reflect flag on evaluate.
* Fix renderer for OpenGL.
  * Add delete mask buffer when renderer instance is destroyed.
* Fix delay in starting fade-out for expressions.
* Fix memory bug causing segmentation fault when reallocating memory by [@adrianiainlam](https://github.com/adrianiainlam)
* Fix reference size of model matrix.
* Fix memory leaking the color buffer on destroyed `CubismOffscreenFrame_OpenGLES2`.
* Fix argument name typo at `CubismEyeBlink::SetBlinkingInterval()`.


## [4-r.1] - 2020-01-30

### Added

* Add the callback function called on finished motion playback.

### Changed

* Include header files in CMake.
* `<GL/glew>` is not included on macOS if `CSM_TARGET_COCOS` is defined.

### Fixed

* Fix rendering not working properly on Android devices with Tegra.

### Deprecated

* Use `target_include_directories` instead of using `FRAMEWORK_XXX_INCLUDE_PATH` variable in application CMake.
* Use `target_compile_definitions` instead of using `FRAMEWORK_DEFINITIOINS` variable in application CMake.
* Specify `FRAMEWORK_SOURCE` variable also in OpenGL application CMake.


## [4-beta.2] - 2019-11-14

### Added

* Add the includes to `Framework` for Linux build.

### Changed

* Refactoring `CMakeLists.txt`

### Fixed

* Fix renderer for DirectX 9 / 11.
  * Add missing implementation: Check the dynamic flags.


## [4-beta.1] - 2019-09-04

### Added

* Support new Inverted Masking features.
* Add `.editorconfig` and `.gitattributes` to manage file formats.
* Add `.gitignore`.
* Add `CHANGELOG.md`.

### Changed

* Convert all file formats according to `.editorconfig`.

### Fixed

* Fix typo of `CubismCdiJson`.
* Fix invalid expressions of `CubismCdiJson`.


[4-r.7]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.6.2...4-r.7
[4-r.6.2]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.6.1...4-r.6.2
[4-r.6.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.6...4-r.6.1
[4-r.6]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5.1...4-r.6
[4-r.5.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5...4-r.5.1
[4-r.5]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.5...4-r.5
[4-r.5-beta.5]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.4.1...4-r.5-beta.5
[4-r.5-beta.4.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.4...4-r.5-beta.4.1
[4-r.5-beta.4]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.3...4-r.5-beta.4
[4-r.5-beta.3]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.2...4-r.5-beta.3
[4-r.5-beta.2]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.5-beta.1...4-r.5-beta.2
[4-r.5-beta.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.4...4-r.5-beta.1
[4-r.4]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.4-beta.1...4-r.4
[4-r.4-beta.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.3...4-r.4-beta.1
[4-r.3]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.3-beta.1...4-r.3
[4-r.3-beta.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.2...4-r.3-beta.1
[4-r.2]: https://github.com/Live2D/CubismNativeFramework/compare/4-r.1...4-r.2
[4-r.1]: https://github.com/Live2D/CubismNativeFramework/compare/4-beta.2...4-r.1
[4-beta.2]: https://github.com/Live2D/CubismNativeFramework/compare/4-beta.1...4-beta.2
[4-beta.1]: https://github.com/Live2D/CubismNativeFramework/compare/0f5da4981cc636fe3892bb94d5c60137c9cf1eb1...4-beta.
