#=============================================================================
# Copyright 2019 NVIDIA Corporation. All rights reserved.
#
# Find MDL SDK includes and libraries
#=============================================================================

set(MDL_SDK_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "MDL SDK root directory")

find_path(MDL_SDK_INCLUDE_DIRS 
    mi/mdl_sdk.h
    HINTS
        "${MDL_SDK_ROOT}"
        "$ENV{MDL_SDK_ROOT}"        
    PATH_SUFFIXES
        include
    DOC
        "MDL SDK Header Path"
)

if (WIN32)
    set(MDL_SDK_LIB_NAME libmdl_sdk.dll)
	set(MDL_SDK_LIB_FOLDER nt-x86-64)
else()
    set(MDL_SDK_LIB_NAME libmdl_sdk.so)
	set(MDL_SDK_LIB_FOLDER linux-x86-64)
endif()

find_path(MDL_SDK_LIB_DIRS 
    "${MDL_SDK_LIB_NAME}"
    HINTS
        "${MDL_SDK_ROOT}"
        "$ENV{MDL_SDK_ROOT}"
    PATH_SUFFIXES
        "${MDL_SDK_LIB_FOLDER}/lib"
    DOC
        "MDL SDK Library Path"
)

