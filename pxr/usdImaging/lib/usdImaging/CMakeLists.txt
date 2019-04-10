set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE usdImaging)

# Note: both python include directives are included for compatibility.
pxr_library(usdImaging
    LIBRARIES
        gf
        tf
        plug
        trace
        vt
        work
        hd
        pxOsd
        sdf
        usd
        usdGeom
		usdSkel
        usdLux
        usdShade
        usdVol
        ar
        ${Boost_PYTHON_LIBRARY}
        ${PYTHON_LIBRARY}
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${PYTHON_INCLUDE_PATH}
        ${PYTHON_INCLUDE_DIRS}
        ${TBB_INCLUDE_DIRS}

    PUBLIC_CLASSES
        adapterRegistry
        collectionCache
        debugCodes
        delegate
        indexProxy
        inheritedCache
        instancerContext
        tokens
        valueCache
        basisCurvesAdapter
        capsuleAdapter
        coneAdapter
        cubeAdapter
        cylinderAdapter
        cylinderLightAdapter
        diskLightAdapter
        distantLightAdapter
        domeLightAdapter
        fieldAdapter
        gprimAdapter
        instanceAdapter
        lightAdapter
        materialAdapter
        meshAdapter
        nurbsPatchAdapter
        pointsAdapter
        pointInstancerAdapter
        primAdapter
        rectLightAdapter
        sphereAdapter
        sphereLightAdapter
        textureUtils
        volumeAdapter

    PUBLIC_HEADERS
        api.h
        unitTestHelper.h
        version.h

    RESOURCE_FILES
        plugInfo.json
)

# #nv begin #fast-updates
pxr_build_test(testUsdImagingFastUpdates
    LIBRARIES
        trace
        sdf
        hd
        usd
        usdImaging
    CPPFILES
        testenv/testUsdImagingFastUpdates.cpp
)

pxr_register_test(testUsdImagingFastUpdates
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdImagingFastUpdates"
)
# nv end
