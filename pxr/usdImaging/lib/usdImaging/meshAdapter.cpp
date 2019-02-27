//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/usdImaging/usdImaging/meshAdapter.h"

#include "pxr/usdImaging/usdImaging/debugCodes.h"
#include "pxr/usdImaging/usdImaging/delegate.h"
#include "pxr/usdImaging/usdImaging/indexProxy.h"
#include "pxr/usdImaging/usdImaging/tokens.h"

#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/geomSubset.h"
#include "pxr/imaging/hd/perfLog.h"

#include "pxr/imaging/pxOsd/meshTopology.h"
#include "pxr/imaging/pxOsd/tokens.h"

#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdGeom/subset.h"
#include "pxr/usd/usdGeom/xformCache.h"

#include "pxr/base/tf/type.h"

//+NV_CHANGE FRZHANG
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/cache.h"
//-NV_CHANGE FRZHANG

PXR_NAMESPACE_OPEN_SCOPE


TF_REGISTRY_FUNCTION(TfType)
{
    typedef UsdImagingMeshAdapter Adapter;
    TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter> >();
    t.SetFactory< UsdImagingPrimAdapterFactory<Adapter> >();
}

UsdImagingMeshAdapter::~UsdImagingMeshAdapter()
{
}

bool
UsdImagingMeshAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
    return index->IsRprimTypeSupported(HdPrimTypeTokens->mesh);
}

SdfPath
UsdImagingMeshAdapter::Populate(UsdPrim const& prim,
                            UsdImagingIndexProxy* index,
                            UsdImagingInstancerContext const* instancerContext)
{
    // Check for any UsdGeomSubset children and record this adapter as
    // the delegate for their paths.
    if (UsdGeomImageable imageable = UsdGeomImageable(prim)) {
        for (const UsdGeomSubset &subset:
             UsdGeomSubset::GetAllGeomSubsets(imageable)) {
            index->AddPrimInfo(subset.GetPath(),
                               subset.GetPrim().GetParent(),
                               shared_from_this());
            // Ensure the bound material has been populated.
            if (UsdPrim materialPrim =
                prim.GetStage()->GetPrimAtPath(
                GetMaterialId(subset.GetPrim()))) {
                UsdImagingPrimAdapterSharedPtr materialAdapter =
                    index->GetMaterialAdapter(materialPrim);
                if (materialAdapter) {
                    materialAdapter->Populate(materialPrim, index, nullptr);
                }
            }
        }


		//+NV_CHANGE FRZHANG
		//Detect if mesh has skel binding to trigge skel mesh update.
		//Backup the skinningquery skeletonquery, jointindices/weights for acceleratioin
		_InitSkinningInfo(prim);
		//-NV_CHANGE FRZHANG
    }
    return _AddRprim(HdPrimTypeTokens->mesh,
                     prim, index, GetMaterialId(prim), instancerContext);
}

//+NV_CHANGE FRZHANG
UsdImagingMeshAdapter::_SkinningData*
UsdImagingMeshAdapter::_GetSkinningData(const SdfPath& cachePath) const
{
	auto it = _skinningDataCache.find(cachePath);
	return it != _skinningDataCache.end() ? it->second.get() : nullptr;
}

void
UsdImagingMeshAdapter::_InitSkinningInfo(UsdPrim const& prim)
{
	bool isSkinningMesh = prim.HasAPI<UsdSkelBindingAPI>();
	if (isSkinningMesh)
	{
		auto skinningData = std::make_shared<_SkinningData>();
		_skinningDataCache[prim.GetPath()] = skinningData;

		UsdSkelBindingAPI bindingAPI(prim);
		pxr::UsdSkelSkeleton skeleton;
		bindingAPI.GetSkeleton(&skeleton);

		UsdSkelRoot skelRoot = UsdSkelRoot::Find(prim);
		UsdSkelCache skelCache;
		skelCache.Populate(skelRoot);
		skinningData->skinningQuery = skelCache.GetSkinningQuery(prim);
		skinningData->skeletonQuery = skelCache.GetSkelQuery(skeleton);
		skinningData->skinningQuery.ComputeJointInfluences(
			&skinningData->jointIndices, &skinningData->jointWeights);
		skinningData->animTimeInterval = skinningData->skeletonQuery.GetAnimQuery().GetTimeRange();
		skinningData->lastUpdateTime = -1.0f;
		//printf("Init SkinningData for %s AnimRange %f -  %f\n", prim.GetPath().GetText(), (float)skinningData->animTimeInterval.GetMin(), (float)skinningData->animTimeInterval.GetMax());
	}
}
//_NV_CHANGE FRZHANG


void
UsdImagingMeshAdapter::TrackVariability(UsdPrim const& prim,
                                        SdfPath const& cachePath,
                                        HdDirtyBits* timeVaryingBits,
                                        UsdImagingInstancerContext const* 
                                            instancerContext) const
{
    // Early return when called on behalf of a UsdGeomSubset.
    if (UsdGeomSubset(prim)) {
        return;
    }

    BaseAdapter::TrackVariability(
        prim, cachePath, timeVaryingBits, instancerContext);

    // WARNING: This method is executed from multiple threads, the value cache
    // has been carefully pre-populated to avoid mutating the underlying
    // container during update.

    // Discover time-varying points.
    _IsVarying(prim,
               UsdGeomTokens->points,
               HdChangeTracker::DirtyPoints,
               UsdImagingTokens->usdVaryingPrimvar,
               timeVaryingBits,
               /*isInherited*/false);


	//+NV_CHANGE FRZHANG
	_SkinningData* skinningData = _GetSkinningData(cachePath);
	if(skinningData != nullptr)
	{
		//printf("variability setting dirty bits for skinning mesh %s\n", prim.GetPath().GetText());
		if (UsdImagingMeshAdapter::USE_NV_GPUSKINNING)
		{
			(*timeVaryingBits) |= HdChangeTracker::NV_DirtySkelAnimXform;
		}
		else
		{
			(*timeVaryingBits) |= HdChangeTracker::DirtyPoints;
		}
	}
	//-NV_CHANGE FRZHANG
	
	// Discover time-varying primvars:normals, and if that attribute
	// doesn't exist also check for time-varying normals.
	// Only do this for polygonal meshes.
    TfToken schemeToken;
    _GetPtr(prim, UsdGeomTokens->subdivisionScheme,
            UsdTimeCode::EarliestTime(), &schemeToken);

    if (schemeToken == PxOsdOpenSubdivTokens->none) {
        bool normalsExists = false;
        _IsVarying(prim,
                UsdImagingTokens->primvarsNormals,
                HdChangeTracker::DirtyNormals,
                UsdImagingTokens->usdVaryingNormals,
                timeVaryingBits,
                /*isInherited*/false,
                &normalsExists);
        if (!normalsExists) {
            _IsVarying(prim,
                    UsdGeomTokens->normals,
                    HdChangeTracker::DirtyNormals,
                    UsdImagingTokens->usdVaryingNormals,
                    timeVaryingBits,
                    /*isInherited*/false);
        }
    }

    // Discover time-varying topology.
    if (!_IsVarying(prim,
                       UsdGeomTokens->faceVertexCounts,
                       HdChangeTracker::DirtyTopology,
                       UsdImagingTokens->usdVaryingTopology,
                       timeVaryingBits,
                       /*isInherited*/false)) {
        // Only do this check if the faceVertexCounts is not already known
        // to be varying.
        if (!_IsVarying(prim,
                           UsdGeomTokens->faceVertexIndices,
                           HdChangeTracker::DirtyTopology,
                           UsdImagingTokens->usdVaryingTopology,
                           timeVaryingBits,
                           /*isInherited*/false)) {
            // Only do this check if both faceVertexCounts and
            // faceVertexIndices are not known to be varying.
            _IsVarying(prim,
                       UsdGeomTokens->holeIndices,
                       HdChangeTracker::DirtyTopology,
                       UsdImagingTokens->usdVaryingTopology,
                       timeVaryingBits,
                       /*isInherited*/false);
        }
    }

    // Discover time-varying UsdGeomSubset children.
    if (UsdGeomImageable imageable = UsdGeomImageable(prim)) {
        for (const UsdGeomSubset &subset:
             UsdGeomSubset::GetAllGeomSubsets(imageable)) {
            _IsVarying(subset.GetPrim(),
                       UsdGeomTokens->elementType,
                       HdChangeTracker::DirtyTopology,
                       UsdImagingTokens->usdVaryingPrimvar,
                       timeVaryingBits,
                       /*isInherited*/false);
            _IsVarying(subset.GetPrim(),
                       UsdGeomTokens->indices,
                       HdChangeTracker::DirtyTopology,
                       UsdImagingTokens->usdVaryingPrimvar,
                       timeVaryingBits,
                       /*isInherited*/false);
        }
    }
}

void
UsdImagingMeshAdapter::MarkDirty(UsdPrim const& prim,
                                 SdfPath const& cachePath,
                                 HdDirtyBits dirty,
                                 UsdImagingIndexProxy* index)
{
    // Check if this is invoked on behalf of a UsdGeomSubset of
    // a parent mesh; if so, dirty the parent instead.
    if (cachePath.IsPrimPath() && cachePath.GetParentPath() == prim.GetPath()) {
        index->MarkRprimDirty(cachePath.GetParentPath(), dirty);
    } else {
        index->MarkRprimDirty(cachePath, dirty);
    }
}

void
UsdImagingMeshAdapter::MarkRefineLevelDirty(UsdPrim const& prim,
                                            SdfPath const& cachePath,
                                            UsdImagingIndexProxy* index)
{
    // Check if this is invoked on behalf of a UsdGeomSubset of
    // a parent mesh; if so, there's nothing to do.
    if (cachePath.IsPrimPath() && cachePath.GetParentPath() == prim.GetPath()) {
        return;
    }
    index->MarkRprimDirty(cachePath, HdChangeTracker::DirtyDisplayStyle);
}


void
UsdImagingMeshAdapter::_RemovePrim(SdfPath const& cachePath,
                                   UsdImagingIndexProxy* index)
{
    // Check if this is invoked on behalf of a UsdGeomSubset,
    // in which case there will be no rprims associated with
    // the cache path.  If so, dirty parent topology.
    if (index->HasRprim(cachePath)) {
        index->RemoveRprim(cachePath);
    } else {
        index->MarkRprimDirty(cachePath.GetParentPath(),
                              HdChangeTracker::DirtyTopology);
    }
	_skinningDataCache.erase(cachePath);
}

bool
UsdImagingMeshAdapter::_IsBuiltinPrimvar(TfToken const& primvarName) const
{
    return (primvarName == UsdImagingTokens->primvarsNormals);
}

void
UsdImagingMeshAdapter::UpdateForTime(UsdPrim const& prim,
                                     SdfPath const& cachePath,
                                     UsdTimeCode time,
                                     HdDirtyBits requestedBits,
                                     UsdImagingInstancerContext const*
                                         instancerContext) const
{
    TF_DEBUG(USDIMAGING_CHANGES).Msg("[UpdateForTime] Mesh path: <%s>\n",
                                     prim.GetPath().GetText());

    // Check if invoked on behalf of a UsdGeomSubset; if so, do nothing.
    if (cachePath.GetParentPath() == prim.GetPath()) {
        return;
    }

    BaseAdapter::UpdateForTime(
        prim, cachePath, time, requestedBits, instancerContext);

    UsdImagingValueCache* valueCache = _GetValueCache();
    HdPrimvarDescriptorVector& primvars = valueCache->GetPrimvars(cachePath);

    if (requestedBits & HdChangeTracker::DirtyTopology) {
        VtValue& topology = valueCache->GetTopology(cachePath);
        _GetMeshTopology(prim, &topology, time);
    }

    if (requestedBits & HdChangeTracker::DirtyPoints) {
		//+NV_CHANGE FRZHANG       
		_SkinningData* skinningData = _GetSkinningData(cachePath);
		if(skinningData)
		{
			if (UsdImagingMeshAdapter::USE_NV_GPUSKINNING)
			{
				VtValue& points = valueCache->GetPoints(cachePath);
				_GetPoints(prim, &points, time);
			}
			else
			{
				//skinningQuery's CPU skinning
				VtValue& points = valueCache->GetPoints(cachePath);
				skinningData->ComputeSkinningPoints(prim, &points, time);
			}
		}
		else
		{
		//-NV_CHANGE FRZHANG
			VtValue& points = valueCache->GetPoints(cachePath);
			_GetPoints(prim, &points, time);
		//+NV_CHANGE FRZHANG
		}
		_MergePrimvar(
			&primvars,
			HdTokens->points,
			HdInterpolationVertex,
			HdPrimvarRoleTokens->point);
		//-NV_CHANGE FRZHANG

    }

    if (requestedBits & HdChangeTracker::DirtyNormals) {
        TfToken schemeToken;
        _GetPtr(prim, UsdGeomTokens->subdivisionScheme, time, &schemeToken);
        // Only populate normals for polygonal meshes.
        if (schemeToken == PxOsdOpenSubdivTokens->none) {
            // First check for "primvars:normals"
            UsdGeomPrimvarsAPI primvarsApi(prim);
            UsdGeomPrimvar pv = primvarsApi.GetPrimvar(
                    UsdImagingTokens->primvarsNormals);
            if (pv) {
                _ComputeAndMergePrimvar(prim, cachePath, pv, time, valueCache);
            } else {
                UsdGeomMesh mesh(prim);
                VtVec3fArray normals;
                if (mesh.GetNormalsAttr().Get(&normals, time)) {
                    _MergePrimvar(&primvars,
                        UsdGeomTokens->normals,
                        _UsdToHdInterpolation(mesh.GetNormalsInterpolation()),
                        HdPrimvarRoleTokens->normal);
                    valueCache->GetNormals(cachePath) = VtValue(normals);
                }
            }
        }
    }

    // Subdiv tags are only needed if the mesh is refined.  So
    // there's no need to fetch the data if the prim isn't refined.
    if (_IsRefined(cachePath)) {
        if (requestedBits & HdChangeTracker::DirtySubdivTags) {
            SubdivTags& tags = valueCache->GetSubdivTags(cachePath);
            _GetSubdivTags(prim, &tags, time);
        }
    }


	//+NV_CHANGE FRZHANG 
	if(UsdImagingMeshAdapter::USE_NV_GPUSKINNING)
	{
		if (requestedBits & HdChangeTracker::NV_DirtySkinningBinding) {
			_SkinningData* skinningData = _GetSkinningData(cachePath);
			if (skinningData)
			{
				VtValue& points = valueCache->GetPoints(cachePath);
				GfMatrix4d& geomBindXform = valueCache->GetGeomBindXform(cachePath);
				skinningData->GetBindXform(&geomBindXform, time);
				VtValue& restPoints = valueCache->GetRestPoints(cachePath);
				restPoints = points;
				VtValue& jointIndices = valueCache->GetJointIndices(cachePath);
				VtValue& jointWeights = valueCache->GetJointWeights(cachePath);
				int& numInfluencesPerPoint = valueCache->GetNumInfluencesPerPoint(cachePath);
				bool& hasConstantInfluences = valueCache->GetHasConstantInfluences(cachePath);
				skinningData->GetBlendValues(&jointIndices, &jointWeights, &numInfluencesPerPoint, &hasConstantInfluences);
			}
		}
		if (requestedBits & HdChangeTracker::NV_DirtySkelAnimXform) {
			_SkinningData* skinningData = _GetSkinningData(cachePath);
			if (skinningData)
			{
				VtValue& skinningXforms = valueCache->GetSkinningXforms(cachePath);
				GfMatrix4d& skelLocalToWorld = valueCache->GetSkelLocalToWorld(cachePath);
				skinningData->ComputeSkelAnimValues(&skinningXforms, &skelLocalToWorld, time);
			}
		}
	}
	//-NV_CHANGE FRZHANG
}

HdDirtyBits
UsdImagingMeshAdapter::ProcessPropertyChange(UsdPrim const& prim,
                                      SdfPath const& cachePath,
                                      TfToken const& propertyName)
{
    if(propertyName == UsdGeomTokens->points)
        return HdChangeTracker::DirtyPoints;

    // Check for UsdGeomSubset changes.
    // Do the cheaper property name filtering first.
    if ((propertyName == UsdGeomTokens->elementType ||
         propertyName == UsdGeomTokens->indices) &&
         cachePath.GetPrimPath().GetParentPath() == prim.GetPath()) {
        return HdChangeTracker::DirtyTopology;
    }

    // TODO: support sparse topology and subdiv tag changes
    // (Note that a change in subdivision scheme means we need to re-track
    // the variability of the normals...)

    // Allow base class to handle change processing.
    return BaseAdapter::ProcessPropertyChange(prim, cachePath, propertyName);
}

// -------------------------------------------------------------------------- //
// Private IO Helpers
// -------------------------------------------------------------------------- //

void
UsdImagingMeshAdapter::_GetMeshTopology(UsdPrim const& prim,
                                         VtValue* topo,
                                         UsdTimeCode time) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    TfToken schemeToken;
    _GetPtr(prim, UsdGeomTokens->subdivisionScheme, time, &schemeToken);

    HdMeshTopology meshTopo(
        schemeToken,
        _Get<TfToken>(prim, UsdGeomTokens->orientation, time),
        _Get<VtIntArray>(prim, UsdGeomTokens->faceVertexCounts, time),
        _Get<VtIntArray>(prim, UsdGeomTokens->faceVertexIndices, time),
        _Get<VtIntArray>(prim, UsdGeomTokens->holeIndices, time));

    // Convert UsdGeomSubsets to HdGeomSubsets.
    if (UsdGeomImageable imageable = UsdGeomImageable(prim)) {
        HdGeomSubsets geomSubsets;
        for (const UsdGeomSubset &subset:
             UsdGeomSubset::GetAllGeomSubsets(imageable)) {
             VtIntArray indices;
             TfToken elementType;
             if (subset.GetElementTypeAttr().Get(&elementType) &&
                 subset.GetIndicesAttr().Get(&indices)) {
                 if (elementType == UsdGeomTokens->face) {
                     geomSubsets.emplace_back(
                        HdGeomSubset {
                            HdGeomSubset::TypeFaceSet,
                            subset.GetPath(),
                            GetMaterialId(subset.GetPrim()),
                            indices });
                 }
             }
        }
        if (!geomSubsets.empty()) {
            meshTopo.SetGeomSubsets(geomSubsets);
        }
    }

    topo->Swap(meshTopo);
}

void
UsdImagingMeshAdapter::_GetPoints(UsdPrim const& prim,
                                   VtValue* value,
                                   UsdTimeCode time) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    if (!prim.GetAttribute(UsdGeomTokens->points).Get(value, time)) {
        *value = VtVec3fArray();
    }
}

void
UsdImagingMeshAdapter::_GetSubdivTags(UsdPrim const& prim,
                                       SubdivTags* tags,
                                       UsdTimeCode time) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if(!prim.IsA<UsdGeomMesh>())
        return;

    TfToken interpolationRule =
        _Get<TfToken>(prim, UsdGeomTokens->interpolateBoundary, time);
    if (interpolationRule.IsEmpty()) {
        interpolationRule = UsdGeomTokens->edgeAndCorner;
    }
    tags->SetVertexInterpolationRule(interpolationRule);

    TfToken faceVaryingRule = _Get<TfToken>(
        prim, UsdGeomTokens->faceVaryingLinearInterpolation, time);
    if (faceVaryingRule.IsEmpty()) {
        faceVaryingRule = UsdGeomTokens->cornersPlus1;
    }
    tags->SetFaceVaryingInterpolationRule(faceVaryingRule);

    // XXX uncomment after fixing USD schema
    // TfToken creaseMethod =
    //     _Get<TfToken>(prim, UsdGeomTokens->creaseMethod, time);
    // tags->SetCreaseMethod(creaseMethod);

    TfToken triangleRule =
        _Get<TfToken>(prim, UsdGeomTokens->triangleSubdivisionRule, time);
    if (triangleRule.IsEmpty()) {
        triangleRule = UsdGeomTokens->catmullClark;
    }
    tags->SetTriangleSubdivision(triangleRule);

    VtIntArray creaseIndices =
        _Get<VtIntArray>(prim, UsdGeomTokens->creaseIndices, time);
    tags->SetCreaseIndices(creaseIndices);

    VtIntArray creaseLengths =
        _Get<VtIntArray>(prim, UsdGeomTokens->creaseLengths, time);
    tags->SetCreaseLengths(creaseLengths);

    VtFloatArray creaseSharpnesses =
        _Get<VtFloatArray>(prim, UsdGeomTokens->creaseSharpnesses, time);
    tags->SetCreaseWeights(creaseSharpnesses);

    VtIntArray cornerIndices =
        _Get<VtIntArray>(prim, UsdGeomTokens->cornerIndices, time);
    tags->SetCornerIndices(cornerIndices);

    VtFloatArray cornerSharpnesses =
        _Get<VtFloatArray>(prim, UsdGeomTokens->cornerSharpnesses, time);
    tags->SetCornerWeights(cornerSharpnesses);
}

//+NV_CHANGE FRZHANG
void
UsdImagingMeshAdapter::_SkinningData::ComputeSkinningPoints(UsdPrim const& prim,
	VtValue* value,
	UsdTimeCode time)
{
	HD_TRACE_FUNCTION();
	HF_MALLOC_TAG_FUNCTION();

	VtMatrix4dArray xforms;

#define TIME_RANGE_OPTIMIZATION 0
#if TIME_RANGE_OPTIMIZATION
	//Time range optimization
	double desiredUpdateTime = time.GetValue();
	if (!animTimeInterval.Contains(desiredUpdateTime))
	{
		if (desiredUpdateTime < animTimeInterval.GetMin())desiredUpdateTime = animTimeInterval.GetMin();
		if (desiredUpdateTime > animTimeInterval.GetMax())desiredUpdateTime = animTimeInterval.GetMax();
	}
	bool bShouldUpdate = false;
	bShouldUpdate |= lastUpdateTime < 0;
	bShouldUpdate |= desiredUpdateTime != lastUpdateTime;
	lastUpdateTime = desiredUpdateTime;
	if(!bShouldUpdate)
	{
		//printf("skin update for %s at time %f, original data count %d\n", prim.GetPath().GetText(), time.GetValue(), value->GetArraySize());
		return;
	}
#endif

	if (skeletonQuery.ComputeSkinningTransforms(&xforms, time))
	{

		VtVec3fArray skinningPoints;
		prim.GetAttribute(UsdGeomTokens->points).Get(&skinningPoints, time);
		if (skinningQuery.ComputeSkinnedPoints(xforms, jointIndices, jointWeights, &skinningPoints, time))
		{
			*value = skinningPoints;
			return;
		}
	}

	*value = VtVec3fArray();
}


bool
UsdImagingMeshAdapter::_SkinningData::GetBlendValues(VtValue* jointIndices, VtValue* jointWeights, int* numInfluencesPerPoint, bool* hasConstantInfluences, UsdTimeCode time)
{
	VtIntArray ji;
	VtFloatArray jw;
	if (skinningQuery.ComputeJointInfluences(&ji, &jw, time))
	{
		*jointIndices = ji;
		*jointWeights = jw;
		*numInfluencesPerPoint = skinningQuery.GetNumInfluencesPerComponent();
		//hasConstantInfluences = skinningQuery.IsRigidlyDeformed();
		*hasConstantInfluences = true;
		return true;
	}
	*jointIndices = VtIntArray();
	*jointWeights = VtFloatArray();
	*numInfluencesPerPoint = 0;
	*hasConstantInfluences = true;
	return false;
}


bool
UsdImagingMeshAdapter::_SkinningData::GetBindXform(GfMatrix4d* geomBindXform, UsdTimeCode time)
{
	*geomBindXform = skinningQuery.GetGeomBindTransform(time);
	return true;
}


bool
UsdImagingMeshAdapter::_SkinningData::ComputeSkelAnimValues(VtValue* skinningXform, GfMatrix4d* skelLocalToWorld, UsdTimeCode time)
{
	VtMatrix4dArray xforms;
	if (skeletonQuery.ComputeSkinningTransforms(&xforms, time))
	{
		*skinningXform = xforms;
		UsdGeomXformCache xformCache(time);
		UsdPrim const& skelPrim = skeletonQuery.GetPrim();
		*skelLocalToWorld =
			xformCache.GetLocalToWorldTransform(skelPrim);
		return true;
	}
	*skinningXform = VtMatrix4dArray();
	*skelLocalToWorld = GfMatrix4d(1);
	return false;
}
//-NV_CHANGE FRZHANG


PXR_NAMESPACE_CLOSE_SCOPE

