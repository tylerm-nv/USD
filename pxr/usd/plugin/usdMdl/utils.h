/******************************************************************************
 * Copyright 2019 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
#pragma once
#include <mi/mdl_sdk.h>
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usdShade/material.h"
#include <map>
#include <string>
#include <stack>

PXR_NAMESPACE_OPEN_SCOPE

/// Helper utility
namespace MdlUtils
{
    void DumpTokens(const TfTokenVector & vec);
    void DumpAttribute(UsdAttribute & attribute);
    void DumpInput(UsdShadeInput & input);

    /// Return the qualified name from a module filename.
    /// Based on the current MDL search paths.
    /// e.g:
    ///     C:\ProgramData\NVIDIA Corporation\mdl\nvidia\core_definitions.mdl
    /// will transform to
    ///     ::nvidia::core_definitions
    /// If C:\ProgramData\NVIDIA Corporation\mdl is in the MDL search paths
    std::string GetModuleNameFromModuleFilename(const std::string & filename);
    std::string GetModuleNameFromModuleFilename(const std::string & filename, mi::neuraylib::IMdl_compiler *compiler);

    std::string Uppercase(const std::string& in);

    /// Replace all occurences of 'from' string to 'to' in the input 'str'
    std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);

    /// Just ensure the name starts with "mdl::"
    std::string GetDBName(const std::string & name);

    /// Return an MDL element name which does not exist yet
    std::string GetUniqueName(mi::neuraylib::ITransaction* transaction, const std::string & prefix = std::string());

    /// Testing only: Save MDL converted material to module
    bool GetUsdToMdlSaveToModule();

} // namespace MdlUtils

/// Helper class to convert from MDL to USD
class MdlToUsd
{
public:
    struct ExtraInfo
    {
        enum VALUE_TYPE
        {
            VTT_NOT_SET = -1
            , VTT_PARM    //VtValue contain integer corresponding to the parameter index
            , VTT_CALL    //VtValue contains a string corresponding to a call
            , VTT_STRUCT  //VtValue contains a string corresponding to a structure
            , VTT_ENUM    //VtValue contains an int corresponding to an enum,
                          // enum symbol is stored in m_enumSymbol, enum value name is stored in m_enumValueName
        };
        VALUE_TYPE m_valueType = VTT_NOT_SET;
        std::string m_enumSymbol;
        std::string m_enumValueName;
        std::string m_enumValueDescription;
    };

public:
    /// Get USD value of an MDL value
    VtValue GetValue(const mi::neuraylib::IType * type, const mi::neuraylib::IExpression * value, TfToken & typeOut, ExtraInfo & extra) const;

    /// Get USD type for a given MDL type
    TfToken ConvertType(const mi::neuraylib::IType * type) const;

private:
    /// A map of MDL types to type names, usefull for logging
    typedef std::map<mi::neuraylib::IType::Kind, std::string> TYPENAMEMAP;
    TYPENAMEMAP m_TypeNameMap =
    {
        { mi::neuraylib::IType::TK_ALIAS,"TK_ALIAS" }
        ,{ mi::neuraylib::IType::TK_BOOL,"TK_BOOL" }
        ,{ mi::neuraylib::IType::TK_INT,"TK_INT" }
        ,{ mi::neuraylib::IType::TK_ENUM,"TK_ENUM" }
        ,{ mi::neuraylib::IType::TK_FLOAT,"TK_FLOAT" }
        ,{ mi::neuraylib::IType::TK_DOUBLE,"TK_DOUBLE" }
        ,{ mi::neuraylib::IType::TK_STRING,"TK_STRING" }
        ,{ mi::neuraylib::IType::TK_VECTOR,"TK_VECTOR" }
        ,{ mi::neuraylib::IType::TK_MATRIX,"TK_MATRIX" }
        ,{ mi::neuraylib::IType::TK_COLOR,"TK_COLOR" }
        ,{ mi::neuraylib::IType::TK_ARRAY,"TK_ARRAY" }
        ,{ mi::neuraylib::IType::TK_STRUCT,"TK_STRUCT" }
        ,{ mi::neuraylib::IType::TK_TEXTURE,"TK_TEXTURE" }
        ,{ mi::neuraylib::IType::TK_LIGHT_PROFILE,"TK_LIGHT_PROFILE" }
        ,{ mi::neuraylib::IType::TK_BSDF_MEASUREMENT,"TK_BSDF_MEASUREMENT" }
        ,{ mi::neuraylib::IType::TK_BSDF,"TK_BSDF" }
        ,{ mi::neuraylib::IType::TK_EDF,"TK_EDF" }
        ,{ mi::neuraylib::IType::TK_VDF,"TK_VDF" }
    };

    /// A map of MDL types to USD types for the MDL types which mapping is easy
    typedef std::map<mi::neuraylib::IType::Kind, SdfValueTypeName> TYPEMAP;
    const TYPEMAP m_TypeMap =
    {
        // mi::neuraylib::IType::TK_ALIAS : See ConvertComplexType()
        { mi::neuraylib::IType::TK_BOOL, SdfValueTypeNames->Bool }
        ,{ mi::neuraylib::IType::TK_INT, SdfValueTypeNames->Int }
        ,{ mi::neuraylib::IType::TK_ENUM, SdfValueTypeNames->Int }
        ,{ mi::neuraylib::IType::TK_FLOAT, SdfValueTypeNames->Float }
        ,{ mi::neuraylib::IType::TK_DOUBLE, SdfValueTypeNames->Double }
        ,{ mi::neuraylib::IType::TK_STRING, SdfValueTypeNames->String }
        // mi::neuraylib::IType::TK_VECTOR : See ConvertComplexType()
        // mi::neuraylib::IType::TK_MATRIX : See ConvertComplexType()
        ,{ mi::neuraylib::IType::TK_COLOR, SdfValueTypeNames->Color3f }
        // mi::neuraylib::IType::TK_ARRAY : See ConvertComplexType()
        ,{ mi::neuraylib::IType::TK_STRUCT, SdfValueTypeNames->Token }
        ,{ mi::neuraylib::IType::TK_TEXTURE, SdfValueTypeNames->Asset }
        // mi::neuraylib::IType::TK_LIGHT_PROFILE
        // mi::neuraylib::IType::TK_BSDF_MEASUREMENT
        // mi::neuraylib::IType::TK_BSDF
        // mi::neuraylib::IType::TK_EDF
        // mi::neuraylib::IType::TK_VDF
    };

private:
    template<class neurayType, class gfType> VtValue GetValueFromVector(const mi::neuraylib::IValue_vector * vec) const;
    VtValue GetValueFromVector(const mi::neuraylib::IExpression_constant * constant) const;
    VtValue GetValueFromMatrix(const mi::neuraylib::IExpression * expression) const;
    template<class T> VtValue ConvertValue(const mi::neuraylib::IExpression * expression) const;
    TfToken ConvertComplexType(const mi::neuraylib::IType * type) const;
    template<class MdlElementType, class USDElementType> VtValue ConvertArrayValue(const mi::neuraylib::IExpression * expression) const;
};

PXR_NAMESPACE_CLOSE_SCOPE

