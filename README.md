# Houdini Engine for Unreal - Version 2 - Alpha

Welcome to the Alpha repository for Version 2 of the Houdini Engine For Unreal Plugin.

This plug-in brings Houdini's powerful and flexible procedural workflow into Unreal Engine through Houdini Digital Assets. Artists can interactively adjust asset parameters inside the editor and use Unreal assets as inputs. Houdini's procedural engine will then "cook" the asset and the results will be available in the editor without the need for baking.

Version 2 is a significant rewrite of the core architecture of the existing Houdini Engine plugin, and comes  with many new features, and improvements.

# Compatibility

The current [Alpha1](https://github.com/sideeffects/HoudiniEngineForUnreal-v2/releases) release of V2 has been built for UE4.24.3 and Houdini 18.0.416.

Version 2 does not support backward compatibility with assets created by version 1 of the Houdini Engine for Unreal plugin.
To that extent, it is important that you use version 2 of the plug-in only with projects that do not contain version 1 assets.

However, Houdini Digital Assets (HDA files) that were created for version 1 of the plugin will be compatible with version 2, as it supports most of version 1 workflows.

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

