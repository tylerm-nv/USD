/******************************************************************************
 * Copyright 2019 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
#pragma once

#include "pxr/pxr.h"
#include "pxr/usd/sdf/fileFormat.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

#define USDMDL_FILE_FORMAT_TOKENS    \
    ((Id,      "mdl"))                         \
    ((Version, "1.0"))                          \
    ((Target,  "usd"))

TF_DECLARE_PUBLIC_TOKENS(UsdMdlFileFormatTokens, USDMDL_FILE_FORMAT_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(UsdMdlFileFormat);

/// \class UsdMdlFileFormat
///
class UsdMdlFileFormat : public SdfFileFormat {
public:
    // SdfFileFormat overrides
    SdfAbstractDataRefPtr InitData(const FileFormatArguments&) const override;
    bool CanRead(const std::string &file) const override;
    bool Read(SdfLayer* layer,
        const std::string& resolvedPath,
        bool metadataOnly) const override;
    bool WriteToFile(const SdfLayer& layer,
                     const std::string& filePath,
                     const std::string& comment = std::string(),
                     const FileFormatArguments& args = 
                         FileFormatArguments()) const override;
	bool WriteToString(const SdfLayer& layer,
					std::string* str,
                    const std::string& comment=std::string()) const override;
    bool WriteToStream(const SdfSpecHandle &spec,
                       std::ostream& out,
                       size_t indent) const override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

    UsdMdlFileFormat();
    ~UsdMdlFileFormat() override;

#if (PXR_MINOR_VERSION==19 && PXR_PATCH_VERSION<7)
private:
    // SdfFileFormat overrides
    bool _IsStreamingLayer(const SdfLayer& layer) const override
    {
        return false;
    }
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE
