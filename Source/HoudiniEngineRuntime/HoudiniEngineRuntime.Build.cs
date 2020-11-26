/*
 * Copyright (c) <2020> Side Effects Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Produced by:
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

using UnrealBuildTool;
using System;
using System.IO;

public class HoudiniEngineRuntime : ModuleRules
{
    public HoudiniEngineRuntime( ReadOnlyTargetRules Target ) : base( Target )
    {
        bPrecompile = true;
        PCHUsage = PCHUsageMode.NoSharedPCHs;
        PrivatePCHHeaderFile = "Private/HoudiniEngineRuntimePrivatePCH.h";

		// Check if we are compiling for unsupported platforms.
		if ( Target.Platform != UnrealTargetPlatform.Win64 &&
			Target.Platform != UnrealTargetPlatform.Mac &&
			Target.Platform != UnrealTargetPlatform.Linux &&
			Target.Platform != UnrealTargetPlatform.Switch )
		{
			System.Console.WriteLine( string.Format( "Houdini Engine Runtime: Compiling for untested target platform. Please let us know how it goes!" ) );
		}


        PublicIncludePaths.AddRange(
			new string[] {}
		);

        PrivateIncludePaths.AddRange(
            new string[] { }
        );

        // Add common dependencies.
        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"InputCore",
				"RHI",
				"Foliage",
				"Landscape"
			 }
		);

	   PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Landscape",
				"PhysicsCore"
			}
		);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"Kismet",
				}
			);
		}
	}
}
