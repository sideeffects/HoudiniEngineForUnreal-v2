# Houdini Engine for Unreal - Version 2 - Alpha

Welcome to the Alpha repository for Version 2 of the Houdini Engine For Unreal Plugin.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside the editor and use Unreal assets as inputs. Houdini's procedural engine will then "cook" the asset and the results will be available in the editor without the need for baking.

Version 2 is a significant rewrite of the core architecture of the existing Houdini Engine plugin, and comes  with many new features, and improvements.

Here are some of the new features and improvements already available in Alpha 1:

- New and redesigned core architecture, more modular and lightweight.
  All the Houdini Engine / HAPI logic is now contained into a new Editor-only module
  This removes the need to bake HDAs to native actors and components before packaging a game.
- Static Mesh creation time has been optimized and now uses Mesh Descriptions.
  Alternatively, you can also decide to use an even faster Proxy Mesh generation while editing the HDA.
  Those can then be automatically refined to Static Meshes, either on a timer, or when saving/playing the level.
- Bgeo files can be imported natively in the content browser.
- World composition support (early stage):
  Tiled heightfields can now be imported as multiple landscapes in separate levels and added to World Composition.
  World composition support will be fleshed out further alongside the PDG support.
- HDA parameters and inputs editing now support multi-selection.
- Inputs have received a variety of improvements, among those:
  - Colliders on a Static Mesh can now be imported as group geometry.
  - World inputs can now read data from brushes.
  - Instancers and Foliage are now imported as packed primitives.
  - World inputs have an improved bound selector mode, that lets them send all the actors and objects contained in the bounds of the selected object.
  - World inputs can now import data from all supported input objects (landscape, houdini asset actors..)
  - World inputs can now import data from actors placed in a different level than the Houdini Asset Actors's.
  - A single curve input can now create and import any number of curves.
  - You can alt-click on curve inputs or editable curves to create new points.
- Parameter UI/UX has been improved:
  - Folder UI (tabs, collapsible) has been improved
  - Ramps UI has been improved, and it is easy to turn off auto-update while editing them.
  - When an asset is dropped on a string parameter, it automatically set its value to the asset ref.
  - Support for File parameters has been improved (custom extension, directory, new file...)

As the plugin now relies on native UProperties for serialization, operations like cut and paste, move between level, duplication etc..

For more details on the new features and improvements available on this alpha, please visit the [Wiki](https://github.com/sideeffects/HoudiniEngineForUnreal-v2/wiki/What's-new-%3F).

The main focus of this Alpha is to ensure proper compatibility with version 1 workflows and tools.
Therefore, some of the new features planned for the full release of version 2 are not available in the Alpha yet.
Noticeably, Alpha1 does not have full PDG and Blueprint support.

# Feedback

Please use this repository's "Issues" to report any bugs, problems, or simply to give us feedback on your experience with version2.

# Compatibility

Currently, the [Alpha1](https://github.com/sideeffects/HoudiniEngineForUnreal-v2/releases) release of V2 has binaries that have been built for UE4.24.3 and Houdini 18.0.416.

Source code for the plugin is available on this repository for UE4.24 and the master branch of Unreal (4.26). Binaries and source code for UE4.25 will be added soon to this repo.

It is important to note that Version 2 is not backward compatible with version 1 of the Houdini Engine for Unreal plugin.

To that extent, it is important that you use version 2 of the plug-in only with projects that do not contain version 1 assets.

However, the Houdini Digital Assets themselves (the HDA files), that were created for version 1 of the plugin are fully compatible with version 2, as it supports most of version 1 workflows.

# Installing the plugin

01. Download the pre-built binaries of the plugin in the "Releases" section of this repository. 
    	
01. Extract the "HoudiniEngine" folder in the release to the "Plugins/Runtime" folder of Unreal.
    You can install the plugin either directly in the engine folder (in "Engine/Plugins/Runtime/HoudiniEngine") or in your project folder (in "Plugins/Runtime/HoudiniEngine").
01. Start Unreal Engine, open the Plug-ins menu and make sure to enable the `HoudiniEngine v2` plug-in (in the `Rendering` section). Restart UE4 if you had to enable it.
01. To confirm that the plug-in has been succesfully installed and enabled, you can check that the editor main menu bar now has a new "Houdini Engine" menu, between "Edit" and "Window".
01. You should now be able to import Houdini Digital Assets (HDA) `.otl` or `.hda` files or drag and drop them into the `Content Browser`.
01. Once you have an HDA in the `Content Browser` you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters and inputs in the `Details` panel. Modifying any of the parameters will force the asset to recook and possibly update its geometry.


# Building from source

01. Get the UE4 source code from: https://github.com/EpicGames/UnrealEngine/releases
01. Within the UE4 source, navigate to `Engine/Plugins/Runtime`, and clone this repo into a folder named `HoudiniEngine`.
01. Download and install the correct build of 64-bit Houdini. To get the build number, look at the header of `Source/HoudiniEngine/HoudiniEngine.Build.cs`, under `Houdini Version`.
01. Generate the UE4 Project Files (by running `GenerateProjectFiles`) and build Unreal, either in x64 `Debug Editor` or x64 `Development Editor`.
01. When starting the Unreal Engine editor, go to Plug-ins menu and make sure to enable the `HoudiniEngine v2` plug-in (in the `Rendering` section). Restart UE4 if you had to enable it.
01. To confirm that the plug-in has been succesfully installed and enabled, you can check that the editor main menu bar now has a new "Houdini Engine" menu, between "Edit" and "Window".
01. You should now be able to import Houdini Digital Assets (HDA) `.otl` or `.hda` files or drag and drop them into the `Content Browser`.
01. Once you have an HDA in the `Content Browser` you should be able to drag it into Editor viewport. This will spawn a new Houdini Asset Actor. Geometry cooking will be done in a separate thread and geometry will be displayed once the cooking is complete. At this point you will be able to see asset parameters in the `Details` pane. Modifying any of the parameters will force the asset to recook and possibly update its geometry.


# License

Please remember this the Alpha is currently private, and is therefore subject to our Alpha/Beta NDA and [License](https://github.com/sideeffects/HoudiniEngineForUnreal-v2/blob/4.24/LICENSE.md) agreement.
