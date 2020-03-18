if (${PXR_BUILD_ALEMBIC_PLUGIN})
    add_subdirectory(usdAbc)
endif()
if (${PXR_BUILD_DRACO_PLUGIN})
    add_subdirectory(usdDraco)
endif()
if (${PXR_BUILD_MATERIALX_PLUGIN})
    add_subdirectory(usdMtlx)
endif()
add_subdirectory(sdrOsl)

# Disabled until usdMdl works with mdl_sdk 2019.327300.1956.rel
#add_subdirectory(usdMdl)
