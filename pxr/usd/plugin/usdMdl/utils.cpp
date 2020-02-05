/******************************************************************************
 * Copyright 2019 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
#include "pxr/pxr.h"
#include "pxr/usd/plugin/usdMdl/utils.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/usd/plugin/usdMdl/neuray.h"

using std::string;
using mi::neuraylib::Mdl;
using mi::neuraylib::IMaterial_definition;
using mi::neuraylib::IFunction_definition;
using mi::neuraylib::IFunction_call;
using mi::neuraylib::IExpression;
using mi::neuraylib::IType;
using mi::neuraylib::IType_alias;
using mi::neuraylib::IType_compound;
using mi::neuraylib::IType_array;
using mi::neuraylib::IType_struct;
using mi::neuraylib::IScene_element;
using mi::neuraylib::IValue_float;
using mi::neuraylib::IValue_int;
using mi::neuraylib::IValue_double;
using mi::neuraylib::IValue_bool;
using mi::neuraylib::IValue_vector;
using mi::neuraylib::IValue_atomic;

PXR_NAMESPACE_OPEN_SCOPE

void MdlUtils::DumpTokens(const TfTokenVector & vec)
{
    TfTokenVector::const_iterator it;
    for (auto & item : vec)
    {
        Mdl::LogInfo("[DumpTokens] " + item.GetString());
    }
}

std::string MdlUtils::Uppercase(const std::string& in)
{
    std::string out(in);
#if defined(ARCH_OS_WINDOWS)
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
#endif
    return out;
}

std::string MdlUtils::ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

std::string MdlUtils::GetModuleNameFromModuleFilename(const std::string & filename)
{
    mi::base::Handle<mi::neuraylib::IMdl_compiler> compiler(
        mi::neuraylib::Mdl::Get().GetNeuray()->get_api_component<mi::neuraylib::IMdl_compiler>()
    );
    return GetModuleNameFromModuleFilename(filename, compiler.get());
}

std::string MdlUtils::GetModuleNameFromModuleFilename(const std::string & filename, mi::neuraylib::IMdl_compiler *compiler)
{
    std::string fn(filename);
    fn = ReplaceAll(fn, "\\", "/"); // Use consistent separator
    fn = ReplaceAll(fn, ".mdl", ""); // Remove extension
    if (TfIsRelativePath(filename))
    {
        // The filename might be relative, in that case just transform to MDL name
        fn = ReplaceAll(fn, "/", "::");
        if (fn.find("::") != 0)
        {
            // Must start with leading "::"
            fn = "::" + fn;
        }
        return fn;
    }
    assert(compiler);
    std::vector<string> modulePaths;
    for (mi::Size i = 0; i < compiler->get_module_paths_length(); i++)
    {
        std::string path(compiler->get_module_path(i)->get_c_str());
        path = ReplaceAll(path, "\\", "/"); // Use consistent separator
        modulePaths.emplace_back(path);
        size_t index(Uppercase(fn).find(Uppercase(path)));
        if (index == 0)
        {
            fn = fn.substr(path.size());
            fn = ReplaceAll(fn, "/", "::");
            if (fn.find("::") != 0)
            {
                // Must start with leading "::"
                fn = "::" + fn;
            }
            return fn;
        }
    }

    mi::neuraylib::Mdl::LogError("[moduleFilenameToModuleName] Can not find module for: " + filename);
    if (modulePaths.empty())
    {
        mi::neuraylib::Mdl::LogError("[moduleFilenameToModuleName] No MDL module paths set");

    }
    for (auto & p : modulePaths)
    {
        mi::neuraylib::Mdl::LogError("[moduleFilenameToModuleName] Tried MDL module path: " + p);
    }
    return "";
}

std::string MdlUtils::GetDBName(const std::string & name)
{
    std::string DBName(name);
    if (DBName.find("mdl::") == 0)
    {
        return DBName;
    }
    if (DBName.find("::") == 0)
    {
        return "mdl" + DBName;
    }
    return "mdl::" + DBName;
}

std::string MdlUtils::GetUniqueName(mi::neuraylib::ITransaction* transaction, const std::string& prefixIn)
{
    static mi::Uint32 s_uniqueID = 0;
    std::string prefix(prefixIn);
    if (prefix.empty())
    {
        prefix = "elt";
    }
    mi::base::Handle<const mi::base::IInterface> interf(transaction->access<mi::base::IInterface>(prefix.c_str()));
    if (!interf)
    {
        return prefix;
    }
    std::stringstream oss;
    while(true)
    {
        oss.str("");
        oss << prefix << "_" << s_uniqueID++;
        mi::base::Handle<const mi::base::IInterface> interf(transaction->access<mi::base::IInterface>(oss.str().c_str()));
        if (! interf)
        {
            return oss.str();
        }
    }
    return "";
}

bool MdlUtils::GetUsdToMdlSaveToModule()
{
    // Return true for testing the USD->MDL conversion
    // return true;
    return false;
}

TfToken MdlToUsd::ConvertComplexType(const IType * type) const
{
    if (type->get_kind() == mi::neuraylib::IType::TK_ALIAS)
    {
        mi::base::Handle<const IType_alias> type_alias(type->get_interface<IType_alias>());
        mi::base::Handle<const IType> aliased_type(type_alias->get_aliased_type());
        return ConvertType(aliased_type.get());
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_VECTOR)
    {
        mi::base::Handle<const IType_compound> type_compound(type->get_interface<IType_compound>());
        mi::base::Handle<const IType> component_type(type_compound->get_component_type(0));
        mi::Size size(type_compound->get_size());
        switch (component_type.get()->get_kind())
        {
        case mi::neuraylib::IType::TK_BOOL:
            switch (size)
            {
            case 2:
                return SdfValueTypeNames->Int2.GetAsToken();
                break;
            case 3:
                return SdfValueTypeNames->Int3.GetAsToken();
                break;
            case 4:
                return SdfValueTypeNames->Int4.GetAsToken();
                break;
            default:
                break;
            }
            break;
        case mi::neuraylib::IType::TK_INT:
            switch (size)
            {
            case 2:
                return SdfValueTypeNames->Int2.GetAsToken();
                break;
            case 3:
                return SdfValueTypeNames->Int3.GetAsToken();
                break;
            case 4:
                return SdfValueTypeNames->Int4.GetAsToken();
                break;
            default:
                break;
            }
            break;
        case mi::neuraylib::IType::TK_FLOAT:
            switch (size)
            {
            case 2:
                return SdfValueTypeNames->Float2.GetAsToken();
                break;
            case 3:
                return SdfValueTypeNames->Float3.GetAsToken();
                break;
            case 4:
                return SdfValueTypeNames->Float4.GetAsToken();
                break;
            default:
                break;
            }
            break;
        case mi::neuraylib::IType::TK_DOUBLE:
            switch (size)
            {
            case 2:
                return SdfValueTypeNames->Double2.GetAsToken();
                break;
            case 3:
                return SdfValueTypeNames->Double3.GetAsToken();
                break;
            case 4:
                return SdfValueTypeNames->Double4.GetAsToken();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        return TfToken();
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_MATRIX)
    {
        mi::base::Handle<const IType_compound> type_compound(type->get_interface<IType_compound>());
        mi::base::Handle<const IType> component_type(type_compound->get_component_type(0));
        mi::Size size(type_compound->get_size());
        if (component_type->get_kind() == mi::neuraylib::IType::TK_VECTOR)
        {
            mi::base::Handle<const IType_compound> type_compound(component_type->get_interface<IType_compound>());
            mi::base::Handle<const IType> component_type(type_compound->get_component_type(0));
            mi::Size size(type_compound->get_size());
            switch (component_type.get()->get_kind())
            {
            case mi::neuraylib::IType::TK_INT:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Matrix2d.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Matrix3d.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Matrix4d.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            case mi::neuraylib::IType::TK_FLOAT:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Matrix2d.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Matrix3d.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Matrix4d.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            case mi::neuraylib::IType::TK_DOUBLE:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Matrix2d.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Matrix3d.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Matrix4d.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
            return TfToken();

        }
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_ARRAY)
    {
        mi::base::Handle<const IType_array> type_array(type->get_interface<IType_array>());
        mi::base::Handle<const IType> element_type(type_array->get_element_type());
        const mi::neuraylib::IType::Kind elementKind(element_type->get_kind());
        
        // Type name
        TYPENAMEMAP::const_iterator itname(m_TypeNameMap.find(elementKind));
        std::string itnamestr = (itname != m_TypeNameMap.end()) ? itname->second : "Unknown";

        bool immediate_sized(type_array->is_immediate_sized());
        mi::Size size(type_array->get_size());
        if (elementKind == mi::neuraylib::IType::TK_VECTOR)
        {
            mi::base::Handle<const IType_compound> type_compound(element_type->get_interface<IType_compound>());
            mi::base::Handle<const IType> component_type(type_compound->get_component_type(0));
            mi::Size size(type_compound->get_size());
            switch (component_type.get()->get_kind())
            {
            case mi::neuraylib::IType::TK_BOOL:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Int2Array.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Int3Array.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Int4Array.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            case mi::neuraylib::IType::TK_INT:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Int2Array.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Int3Array.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Int4Array.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            case mi::neuraylib::IType::TK_FLOAT:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Float2Array.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Float3Array.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Float4Array.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            case mi::neuraylib::IType::TK_DOUBLE:
                switch (size)
                {
                case 2:
                    return SdfValueTypeNames->Double2Array.GetAsToken();
                    break;
                case 3:
                    return SdfValueTypeNames->Double3Array.GetAsToken();
                    break;
                case 4:
                    return SdfValueTypeNames->Double4Array.GetAsToken();
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
            return TfToken();
        }
        switch (elementKind)
        {
        case mi::neuraylib::IType::TK_BOOL:
            return SdfValueTypeNames->BoolArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_INT:
            return SdfValueTypeNames->IntArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_ENUM:
            return SdfValueTypeNames->IntArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_FLOAT:
            return SdfValueTypeNames->FloatArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_DOUBLE:
            return SdfValueTypeNames->DoubleArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_STRING:
            return SdfValueTypeNames->StringArray.GetAsToken();
            break;
        // mi::neuraylib::IType::TK_VECTOR
        // mi::neuraylib::IType::TK_MATRIX
        case mi::neuraylib::IType::TK_COLOR:
            return SdfValueTypeNames->Color3fArray.GetAsToken();
            break;
        // mi::neuraylib::IType::TK_ARRAY 
        case mi::neuraylib::IType::TK_STRUCT:
            return SdfValueTypeNames->TokenArray.GetAsToken();
            break;
        case mi::neuraylib::IType::TK_TEXTURE:
            return SdfValueTypeNames->AssetArray.GetAsToken();
            break;
        // mi::neuraylib::IType::TK_LIGHT_PROFILE
        // mi::neuraylib::IType::TK_BSDF_MEASUREMENT
        // mi::neuraylib::IType::TK_BSDF
        // mi::neuraylib::IType::TK_EDF
        // mi::neuraylib::IType::TK_VDF
        default:
            break;
        }
        mi::neuraylib::Mdl::LogWarning("[ConvertComplexType] Unsupported TK_ARRAY of type: " + itnamestr);
    }
    return TfToken();
}

TfToken MdlToUsd::ConvertType(const IType * type) const
{
    mi::neuraylib::IType::Kind kind(type->get_kind());
    TYPENAMEMAP::const_iterator itname(m_TypeNameMap.find(kind));
    std::string itnamestr("Unknown");
    if (itname != m_TypeNameMap.end())
    {
        itnamestr = itname->second;
    }
    TYPEMAP::const_iterator it(m_TypeMap.find(kind));
    if (it != m_TypeMap.end())
    {
        //mi::neuraylib::Mdl::LogVerbose("[MdlToUsd] Kind converted as token: " + itnamestr);
        return it->second.GetAsToken();
    }

    // Try other types
    TfToken tk(ConvertComplexType(type));
    if (!tk.IsEmpty())
    {
        return tk;
    }

    if (itname != m_TypeNameMap.end())
    {
        mi::neuraylib::Mdl::LogWarning("[MdlToUsd] Unsupported type: " + itname->second);
    }
    else
    {
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Unknown type");
    }
    return TfToken();
}

template<class neurayType, class gfType> VtValue GetValueFromArrayOfVector(const mi::neuraylib::IValue_array * valuearray, mi::Size numberOfElements, mi::Size vectorElementSize)
{
    VtArray<gfType> output;
    for (mi::Size i = 0; i < numberOfElements; i++)
    {
        mi::base::Handle<const IValue_vector> vec(valuearray->get_value<IValue_vector>(i));

        gfType item;
        for (mi::Size j = 0; j < vectorElementSize; j++)
        {
            mi::base::Handle<const neurayType> ifv(vec->get_value<neurayType>(j));
            if (ifv)
            {
                item[j] = ifv->get_value();
            }
        }
        output.push_back(item);
    }
    return VtValue(output);
}

const char * GetStringAnnotation(const mi::neuraylib::IAnnotation_block * block, const std::string & name)
{
    mi::Size sz(block->get_size());
    for (mi::Size i = 0; i < sz; i++)
    {
        mi::base::Handle<const mi::neuraylib::IAnnotation> anno(block->get_annotation(i));
        if (anno)
        {
            if (name == anno->get_name())
            {
                mi::base::Handle<const mi::neuraylib::IExpression_list> args(anno->get_arguments());
                if (args)
                {
                    mi::base::Handle<const mi::neuraylib::IExpression> expr(args->get_expression(mi::Size(0)));
                    if (expr)
                    {
                        mi::base::Handle<const IType> type(expr->get_type());
                        if (type)
                        {
                            if (type->get_kind() == mi::neuraylib::IType::TK_STRING)
                            {
                                mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(expr->get_interface<mi::neuraylib::IExpression_constant>());
                                if (constant)
                                {
                                    mi::base::Handle<const mi::neuraylib::IValue_string> value(constant->get_value<mi::neuraylib::IValue_string>());
                                    if (value)
                                    {
                                        return value->get_value();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

VtValue MdlToUsd::GetValue(const IType * type, const IExpression * value, TfToken & typeOut, ExtraInfo & extra) const
{
    if (value->get_kind() == mi::neuraylib::IExpression::EK_CALL)
    {
        mi::base::Handle<const mi::neuraylib::IExpression_call> callexpr(value->get_interface<mi::neuraylib::IExpression_call>());
        const char* call(callexpr->get_call());
        typeOut = SdfValueTypeNames->Token.GetAsToken();
        extra.m_valueType = ExtraInfo::VTT_CALL;
        return VtValue(std::string(call));
    }
    else if (value->get_kind() == mi::neuraylib::IExpression::EK_PARAMETER)
    {
        mi::base::Handle<const mi::neuraylib::IExpression_parameter> parameter(value->get_interface<mi::neuraylib::IExpression_parameter>());
        /// Returns the index of the referenced parameter.
        mi::Size index(parameter->get_index());
        extra.m_valueType = ExtraInfo::VTT_PARM;
        return VtValue(index);
    }

    if (type->get_kind() == mi::neuraylib::IType::TK_ALIAS)
    {
        mi::base::Handle<const IType_alias> type_alias(type->get_interface<IType_alias>());
        mi::base::Handle<const IType> aliased_type(type_alias->get_aliased_type());
        return GetValue(aliased_type.get(), value, typeOut, extra);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_BOOL)
    {
        return ConvertValue<mi::neuraylib::IValue_bool>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_INT)
    {
        return ConvertValue<mi::neuraylib::IValue_int>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_ENUM)
    {
        if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
        {
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                mi::base::Handle<const mi::neuraylib::IValue_enum> enumval(constant->get_value<mi::neuraylib::IValue_enum>());
                if (enumval)
                {
                    const char* enumname(enumval->get_name());
                    mi::base::Handle<const mi::neuraylib::IType_enum> enumtype(enumval->get_type());
                    if (enumtype)
                    {
                        const char * symbol(enumtype->get_symbol());
                        if (symbol)
                        {
                            extra.m_valueType = ExtraInfo::VTT_ENUM;
                            extra.m_enumSymbol = symbol;
                            extra.m_enumValueName = enumname;
                            mi::base::Handle<const mi::neuraylib::IAnnotation_block> annoblock(enumtype->get_value_annotations(enumval->get_index()));
                            if (annoblock)
                            {
                                const char * anno(GetStringAnnotation(annoblock.get(), "::anno::description(string)"));
                                if (anno)
                                {
                                    extra.m_enumValueDescription = anno;
                                }
                            }
                        }
                    }
                }
            }
        }
        return ConvertValue<mi::neuraylib::IValue_enum>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_FLOAT)
    {
        return ConvertValue<mi::neuraylib::IValue_float>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_DOUBLE)
    {
        return ConvertValue<mi::neuraylib::IValue_double>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_STRING)
    {
        return ConvertValue<mi::neuraylib::IValue_string>(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_VECTOR)
    {
        if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
        {
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                VtValue vectorValue(GetValueFromVector(constant.get()));
                if (vectorValue.IsEmpty())
                {
                    mi::neuraylib::Mdl::LogError("[MdlToUsd] Vector value not supported");
                }
                return vectorValue;
            }
        }
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_MATRIX)
    {
        return GetValueFromMatrix(value);
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_COLOR)
    {
        if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
        {
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                mi::base::Handle<const mi::neuraylib::IValue_color> v(constant->get_value<mi::neuraylib::IValue_color>());
                if (v)
                {
                    mi::base::Handle<const mi::neuraylib::IValue_float> r(v->get_value(0));
                    mi::base::Handle<const mi::neuraylib::IValue_float> g(v->get_value(1));
                    mi::base::Handle<const mi::neuraylib::IValue_float> b(v->get_value(2));
                    float rgb[] = { r->get_value(), g->get_value(), b->get_value() };
                    typeOut = SdfValueTypeNames->Color3f.GetAsToken();
                    return VtValue(GfVec3f(rgb[0], rgb[1], rgb[2]));
                }
            }
        }
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_TEXTURE)
    {
        if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
        {
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                mi::base::Handle<const mi::neuraylib::IValue_texture> texture(constant->get_value<mi::neuraylib::IValue_texture>());
                if (texture)
                {
                    const char* file_path(texture->get_file_path());
                    if (file_path)
                    {
                        return VtValue(SdfAssetPath(file_path));
                    }
                }
            }
        }
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_ARRAY)
    {
        mi::base::Handle<const IType_array> type_array(type->get_interface<IType_array>());
        mi::base::Handle<const IType> element_type(type_array->get_element_type());
        const mi::neuraylib::IType::Kind elementKind(element_type->get_kind());
        if (elementKind == mi::neuraylib::IType::TK_BOOL)
        {
            return ConvertArrayValue<mi::neuraylib::IValue_bool, bool>(value);
        }
        else if (elementKind == mi::neuraylib::IType::TK_INT)
        {
            return ConvertArrayValue<mi::neuraylib::IValue_int, int>(value);
        }
        else if (elementKind == mi::neuraylib::IType::TK_FLOAT)
        {
            return ConvertArrayValue<mi::neuraylib::IValue_float, float>(value);
        }
        else if (elementKind == mi::neuraylib::IType::TK_DOUBLE)
        {
            return ConvertArrayValue<mi::neuraylib::IValue_double, double>(value);
        }
        else if (elementKind == mi::neuraylib::IType::TK_STRING)
        {
            return ConvertArrayValue<mi::neuraylib::IValue_string, string>(value);
        }
        else if (elementKind == mi::neuraylib::IType::TK_VECTOR)
        {
            if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
            {
                mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
                if (constant)
                {
                    mi::base::Handle<const mi::neuraylib::IValue_array> valuearray(constant->get_value<mi::neuraylib::IValue_array>());
                    if (valuearray)
                    {
                        mi::neuraylib::IType::Kind vectorElementTypeKind;
                        mi::Size vectorElementSize(0);
                        mi::Size numberOfElements(valuearray->get_size());

                        if (numberOfElements > 0)
                        {
                            mi::base::Handle<const IValue_vector> vec(valuearray->get_value<IValue_vector>(0));
                            mi::base::Handle<const IValue_atomic> atom(vec->get_value(0));
                            mi::base::Handle<const IType> type(atom->get_type());                            
                            vectorElementTypeKind = type->get_kind();
                            vectorElementSize = vec->get_size();
                        }

                        switch(vectorElementSize)
                        { 
                        case 2:
                            switch (vectorElementTypeKind)
                            {
                            case(mi::neuraylib::IType::TK_BOOL):
                                return GetValueFromArrayOfVector<IValue_bool, GfVec2i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_INT):
                                return GetValueFromArrayOfVector<IValue_int, GfVec2i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_FLOAT):
                                return GetValueFromArrayOfVector<IValue_float, GfVec2f>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_DOUBLE):
                                return GetValueFromArrayOfVector<IValue_double, GfVec2d>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            default:
                                break;
                            }
                            break;
                        case 3:
                            switch (vectorElementTypeKind)
                            {
                            case(mi::neuraylib::IType::TK_BOOL):
                                return GetValueFromArrayOfVector<IValue_bool, GfVec3i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_INT):
                                return GetValueFromArrayOfVector<IValue_int, GfVec3i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_FLOAT):
                                return GetValueFromArrayOfVector<IValue_float, GfVec3f>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_DOUBLE):
                                return GetValueFromArrayOfVector<IValue_double, GfVec3d>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            default:
                                break;
                            }
                            break;
                        case 4:
                            switch (vectorElementTypeKind)
                            {
                            case(mi::neuraylib::IType::TK_BOOL):
                                return GetValueFromArrayOfVector<IValue_bool, GfVec4i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_INT):
                                return GetValueFromArrayOfVector<IValue_int, GfVec4i>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_FLOAT):
                                return GetValueFromArrayOfVector<IValue_float, GfVec4f>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            case(mi::neuraylib::IType::TK_DOUBLE):
                                return GetValueFromArrayOfVector<IValue_double, GfVec4d>(valuearray.get(), numberOfElements, vectorElementSize);
                                break;
                            default:
                                break;
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
        else if (elementKind == mi::neuraylib::IType::TK_COLOR)
        {
            if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
            {
                mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
                if (constant)
                {
                    mi::base::Handle<const mi::neuraylib::IValue_array> valuearray(constant->get_value<mi::neuraylib::IValue_array>());
                    if (valuearray)
                    {
                        VtArray<GfVec3f> output;

                        mi::Size sz(valuearray->get_size());
                        for (mi::Size i = 0; i<sz; i++)
                        {
                            mi::base::Handle<const mi::neuraylib::IValue_color> cvalue(valuearray->get_value<mi::neuraylib::IValue_color>(i));
                            if (value)
                            {
                                mi::base::Handle<const mi::neuraylib::IValue_float> r(cvalue->get_value(0));
                                mi::base::Handle<const mi::neuraylib::IValue_float> g(cvalue->get_value(1));
                                mi::base::Handle<const mi::neuraylib::IValue_float> b(cvalue->get_value(2));
                                float rgb[] = {r->get_value(), g->get_value(), b->get_value()};
                                output.push_back(GfVec3f(rgb[0], rgb[1], rgb[2]));
                            }
                        }
                        VtValue outputvaluecolor(output);
                        return outputvaluecolor;
                    }
                }
            }
        }
        else if (elementKind == mi::neuraylib::IType::TK_STRUCT)
        {
            VtArray<TfToken> arrayOut;
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                mi::base::Handle<const mi::neuraylib::IValue_array> arr(constant->get_value<mi::neuraylib::IValue_array>());

                if (arr)
                {
                    mi::Size len(arr->get_size());
                    for (mi::Size i = 0; i < len; i++)
                    {
                        mi::base::Handle<const mi::neuraylib::IValue> val(arr->get_value(i));
                        if (val)
                        {

                        }
                    }
                }
            }
        }
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Array not supported");
    }
    else if (type->get_kind() == mi::neuraylib::IType::TK_STRUCT)
    {
        if (value->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
        {
            mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(value->get_interface<mi::neuraylib::IExpression_constant>());
            if (constant)
            {
                mi::base::Handle<const mi::neuraylib::IValue_struct> structure(constant->get_value<mi::neuraylib::IValue_struct>());
                if (structure)
                {
                    mi::Size sz(structure->get_size());
                    for (mi::Size i = 0; i < sz; i++)
                    {
                        mi::base::Handle<const mi::neuraylib::IValue> val(structure->get_value(i));

                    }

                    mi::base::Handle<const mi::neuraylib::IType_struct> castval(type->get_interface<mi::neuraylib::IType_struct>());

                    const char* symbol(castval->get_symbol());
                    if (symbol)
                    {
                        //typeOut = SdrPropertyTypes->Vstruct;          WRONG

                        extra.m_valueType = ExtraInfo::VTT_STRUCT;
                        return VtValue();
                        return VtValue(std::string(symbol));
                    }
                }
            }
        }
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Structure not supported");
    }
    else
    {
        TYPENAMEMAP::const_iterator itname(m_TypeNameMap.find(type->get_kind()));
        if (itname != m_TypeNameMap.end())
        {
            mi::neuraylib::Mdl::LogError("[MdlToUsd] Can not set default value for: " + itname->second);
        }
    }
    return VtValue();
}

template<class neurayType, class gfType> VtValue MdlToUsd::GetValueFromVector(const mi::neuraylib::IValue_vector * vec) const
{
    gfType rtnval;
    for (mi::Size i = 0; i < vec->get_size(); i++)
    {
        mi::base::Handle<const neurayType> ifv(vec->get_value<neurayType>(i));
        if (ifv)
        {
            rtnval[i] = ifv->get_value();
        }
        else
        {
            mi::neuraylib::Mdl::LogError("[GetValueFromVector] Bad type");
            return VtValue();
        }
    }
    return VtValue(rtnval);
}

VtValue MdlToUsd::GetValueFromVector(const mi::neuraylib::IExpression_constant * constant) const
{
    if (constant)
    {
        mi::base::Handle<const mi::neuraylib::IValue_vector> v(constant->get_value<mi::neuraylib::IValue_vector>());
        if (v)
        {
            mi::Size size(v->get_size());

            mi::base::Handle<const mi::neuraylib::IType_vector> t(v->get_type());

            if (t)
            {
                mi::base::Handle<const mi::neuraylib::IType_atomic> ta(t->get_element_type());
                if (ta)
                {
                    switch (ta->get_kind())
                    {
                    case mi::neuraylib::IType::TK_FLOAT:
                        switch (size)
                        {
                        case 2:
                            return GetValueFromVector<IValue_float, GfVec2f>(v.get());
                        case 3:
                            return GetValueFromVector<IValue_float, GfVec3f>(v.get());
                        case 4:
                            return GetValueFromVector<IValue_float, GfVec4f>(v.get());
                        }
                        break;
                    case mi::neuraylib::IType::TK_INT:
                        switch (size)
                        {
                        case 2:
                            return GetValueFromVector<IValue_int, GfVec2i>(v.get());
                        case 3:
                            return GetValueFromVector<IValue_int, GfVec3i>(v.get());
                        case 4:
                            return GetValueFromVector<IValue_int, GfVec4i>(v.get());
                        }
                        break;
                    case mi::neuraylib::IType::TK_BOOL:
                        switch (size)
                        {
                            case 2:
                                return GetValueFromVector<IValue_bool, GfVec2i>(v.get());
                            case 3:
                                return GetValueFromVector<IValue_bool, GfVec3i>(v.get());
                            case 4:
                                return GetValueFromVector<IValue_bool, GfVec4i>(v.get());
                        }
                        break;
                    case mi::neuraylib::IType::TK_DOUBLE:
                        switch (size)
                        {
                        case 2:
                            return GetValueFromVector<IValue_double, GfVec2d>(v.get());
                        case 3:
                            return GetValueFromVector<IValue_double, GfVec3d>(v.get());
                        case 4:
                            return GetValueFromVector<IValue_double, GfVec4d>(v.get());
                        }
                        break;
                    }
                }
            }
        }
    }
    return VtValue();
}

template<class T> VtValue MdlToUsd::ConvertValue(const IExpression * expression) const
{
    if (expression->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
    {
        mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(expression->get_interface<mi::neuraylib::IExpression_constant>());
        if (constant)
        {
            mi::base::Handle<const T> value(constant->get_value<T>());
            if (value)
            {
                return VtValue(value->get_value());
            }
        }
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_CALL)
    {
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Function call not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_PARAMETER)
    {
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Parameter reference expression not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_DIRECT_CALL)
    {
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Direct call expression not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_TEMPORARY)
    {
        mi::neuraylib::Mdl::LogError("[MdlToUsd] Temporary reference expression not supported");
    }
    return VtValue();
}

template<class MdlElementType, class USDElementType> VtValue MdlToUsd::ConvertArrayValue(const IExpression * expression) const
{
    if (expression->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
    {
        mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(expression->get_interface<mi::neuraylib::IExpression_constant>());
        if (constant)
        {
            mi::base::Handle<const mi::neuraylib::IValue_array> valuearray(constant->get_value<mi::neuraylib::IValue_array>());
            if (valuearray)
            {
                VtArray<USDElementType> output;

                mi::Size sz(valuearray->get_size());
                for (mi::Size i = 0; i<sz; i++)
                {
                    mi::base::Handle<const MdlElementType> value(valuearray->get_value<MdlElementType>(i));
                    if (value)
                    {
                        output.push_back(value->get_value());
                    }
                }
                return VtValue(output);
            }
        }
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_CALL)
    {
        mi::neuraylib::Mdl::LogError("[ConvertArrayValue] Function call not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_PARAMETER)
    {
        mi::neuraylib::Mdl::LogError("[ConvertArrayValue] Parameter reference expression not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_DIRECT_CALL)
    {
        mi::neuraylib::Mdl::LogError("[ConvertArrayValue] Direct call expression not supported");
    }
    else if (expression->get_kind() == mi::neuraylib::IExpression::EK_TEMPORARY)
    {
        mi::neuraylib::Mdl::LogError("[ConvertArrayValue] Temporary reference expression not supported");
    }
    return VtValue();
}

template<class MdlType, class OutType> VtValue GetValueFromMatrixInternal(const mi::neuraylib::IValue_matrix * matrix, mi::Size matsize)
{
    OutType output;

    mi::base::Handle<const mi::neuraylib::IType_matrix> matrixtype(matrix->get_type());
    mi::base::Handle<const mi::neuraylib::IType_vector> vectype(matrixtype->get_element_type());
    const mi::Size vesize(vectype->get_size());

    // column-major order
    for (mi::Size column = 0; column < matsize; column++)
    {
        mi::base::Handle<const mi::neuraylib::IValue_vector> v(matrix->get_value(column));
        if (v)
        {
            for (mi::Size row = 0; row < vesize; row++)
            {
                mi::base::Handle<const MdlType> a(v->get_value<MdlType>(row));
                output.GetArray()[column * matsize + row] = a->get_value();
            }
        }
    }
    return VtValue(output);
}

template<class MdlType> VtValue GetValueFromMatrixFromMdlType(const mi::neuraylib::IValue_matrix * matrix)
{
    const mi::Size matsize(matrix->get_size());
    switch (matsize)
    {
    case 2:
    return GetValueFromMatrixInternal<MdlType, GfMatrix2d>(matrix, matsize);
    case 3:
    return GetValueFromMatrixInternal<MdlType, GfMatrix3d>(matrix, matsize);
    case 4:
    return GetValueFromMatrixInternal<MdlType, GfMatrix4d>(matrix, matsize);
    default:
    break;
    }
    return VtValue();
}

VtValue MdlToUsd::GetValueFromMatrix(const IExpression * expression) const
{
    if (expression->get_kind() == mi::neuraylib::IExpression::EK_CONSTANT)
    {
        mi::base::Handle<const mi::neuraylib::IExpression_constant> constant(expression->get_interface<mi::neuraylib::IExpression_constant>());
        if (constant)
        {
            mi::base::Handle<const mi::neuraylib::IValue_matrix> matrix(constant->get_value<mi::neuraylib::IValue_matrix>());
            if (matrix)
            {
                mi::base::Handle<const mi::neuraylib::IType_matrix> matrixtype(matrix->get_type());
                if (matrixtype)
                {
                    mi::base::Handle<const mi::neuraylib::IType_vector> vectype(matrixtype->get_element_type());

                    if (vectype)
                    {
                        mi::base::Handle<const mi::neuraylib::IType_atomic> elttype(vectype->get_element_type());

                        if (elttype)
                        {
                            const mi::Size vesize(vectype->get_size());
                            const mi::Size matsize(matrixtype->get_size());
                            if (vesize == matsize)
                            {
                                if (mi::neuraylib::IType::TK_FLOAT == elttype->get_kind())
                                {
                                    return GetValueFromMatrixFromMdlType<mi::neuraylib::IValue_float>(matrix.get());
                                }
                                else if (mi::neuraylib::IType::TK_DOUBLE == elttype->get_kind())
                                {
                                    return GetValueFromMatrixFromMdlType<mi::neuraylib::IValue_double>(matrix.get());
                                }
                            }
                            else
                            {
                                const mi::neuraylib::IType::Kind eltkind(elttype->get_kind());
                                TYPENAMEMAP::const_iterator itname(m_TypeNameMap.find(eltkind));
                                std::stringstream str;
                                str << "[MdlToUsd] Matrix " << matsize << " x " << vesize << " of " << itname->second << " not supported";
                                mi::neuraylib::Mdl::LogError(str.str());
                            }
                        }
                    }
                }
            }
        }
    }
    return VtValue();
}

/// class UsdToMdl
void MdlUtils::DumpAttribute(UsdAttribute & attribute)
{
    Mdl::LogInfo("[DumpAttribute] attribute -------> " + std::string(attribute.GetName()));

    SdfValueTypeName tn(attribute.GetTypeName());
    Mdl::LogInfo("[DumpAttribute] type: " + std::string(tn.GetAsToken().GetText()));

    SdfVariability v(attribute.GetVariability());
        //SdfVariabilityVarying,
        //SdfVariabilityUniform,
        //SdfVariabilityConfig,

    Mdl::LogInfo("[DumpAttribute] is custom: " + (attribute.IsCustom() ? std::string("yes") : std::string("no")));
    Mdl::LogInfo("[DumpAttribute] is defined: " + (attribute.IsDefined() ? std::string("yes") : std::string("no")));
    Mdl::LogInfo("[DumpAttribute] is authored: " + (attribute.IsAuthored() ? std::string("yes") : std::string("no")));
    Mdl::LogInfo("[DumpAttribute] role name: " + std::string(attribute.GetRoleName().GetText()));
    Mdl::LogInfo("[DumpAttribute] has value: " + (attribute.HasValue()? std::string("yes"): std::string("no")));
    Mdl::LogInfo("[DumpAttribute] has authored value opinion: " + (attribute.HasAuthoredValueOpinion() ? std::string("yes") : std::string("no")));
    //CRASH Mdl::LogInfo("[DumpAttribute] has fallback value: " + (attribute.HasFallbackValue() ? std::string("yes") : std::string("no")));

    //bool Get(VtValue* value, UsdTimeCode time = UsdTimeCode::Default()) const;
    SdfPathVector sources;
    Mdl::LogInfo("[DumpAttribute] has connections: " + (attribute.GetConnections(&sources) && !sources.empty() ? std::string("yes") : std::string("no")));
    for (auto & p : sources)
    {    
        Mdl::LogInfo("[DumpAttribute] \tconnections: " + (p.GetString()));
    }
}

void MdlUtils::DumpInput(UsdShadeInput & input)
{
    Mdl::LogInfo("[DumpInput] Input fullname: " + std::string(input.GetFullName().GetString()));
    Mdl::LogInfo("[DumpInput] Input basename: " + std::string(input.GetBaseName().GetString()));

    SdfValueTypeName tn(input.GetTypeName());
    Mdl::LogInfo("[DumpInput] type: " + std::string(tn.GetAsToken().GetText()));

    NdrTokenMap metadata(input.GetSdrMetadata());
    for (auto & d : metadata)
    {
        Mdl::LogInfo("[DumpInput] metadata: " + d.first.GetString() + " " + d.second);
    }
    Mdl::LogInfo("[DumpInput] is defined: " + (input.IsDefined() ? std::string("yes") : std::string("no")));
}

// Utility function to dump the arguments of a material instance or function call.
template <class T>
void dump_instance(
    mi::neuraylib::IExpression_factory* expression_factory, const T* material, std::ostream& s)
{
    mi::Size count = material->get_parameter_count();
    mi::base::Handle<const mi::neuraylib::IExpression_list> arguments(material->get_arguments());

    for (mi::Size index = 0; index < count; index++) {

        mi::base::Handle<const mi::neuraylib::IExpression> argument(
            arguments->get_expression(index));
        std::string name = material->get_parameter_name(index);
        mi::base::Handle<const mi::IString> argument_text(
            expression_factory->dump(argument.get(), name.c_str(), 1));
        s << argument_text->get_c_str() << std::endl;

    }
    s << std::endl;
}

PXR_NAMESPACE_CLOSE_SCOPE
