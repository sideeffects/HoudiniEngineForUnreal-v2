/*
* Copyright (c) <2018> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#define HOUDINI_ENGINE_EDITOR

#include "HoudiniEngineRuntimePrivatePCH.h"

#include "Editor.h"

// Details panel desired sizes.
#define HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH              270
#define HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH               310
#define HAPI_UNREAL_DESIRED_SETTINGS_ROW_VALUE_WIDGET_WIDTH     350
#define HAPI_UNREAL_DESIRED_SETTINGS_ROW_FULL_WIDGET_WIDTH      400


#define HOUDINI_PARAMETER_STRING_REF_TAG						TEXT("unreal_ref")
#define HOUDINI_PARAMETER_STRING_MULTILINE_TAG					TEXT("editor")

 // URL used for bug reporting.
#define HAPI_UNREAL_BUG_REPORT_URL								TEXT("https://www.sidefx.com/bugs/submit/")
#define HAPI_UNREAL_ONLINE_DOC_URL								TEXT("https://www.sidefx.com/docs/unreal/")
#define HAPI_UNREAL_ONLINE_FORUM_URL							TEXT("https://www.sidefx.com/forum/51/")