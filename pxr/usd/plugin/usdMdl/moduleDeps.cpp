/******************************************************************************
 * Copyright 2019 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
#include "pxr/pxr.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/scriptModuleLoader.h"
#include "pxr/base/tf/token.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfScriptModuleLoader) {
    // List of direct dependencies for this library.
    const std::vector<TfToken> reqs = {
        TfToken("arch"),
        TfToken("gf"),
        TfToken("ndr"),
        TfToken("sdf"),
        TfToken("sdr"),
        TfToken("tf"),
        TfToken("usd"),
        TfToken("usdGeom"),
        TfToken("usdShade"),
        TfToken("usdUI"),
        TfToken("vt")
    };
    TfScriptModuleLoader::GetInstance().
        RegisterLibrary(TfToken("usdMdl"), TfToken("pxr.UsdMdl"), reqs);
}

PXR_NAMESPACE_CLOSE_SCOPE


