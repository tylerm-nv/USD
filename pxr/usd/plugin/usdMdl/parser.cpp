/******************************************************************************
 * Copyright 2019 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/
#include "pxr/pxr.h"
#include "pxr/usd/ndr/debugCodes.h"
#include "pxr/usd/ndr/node.h"
#include "pxr/usd/ndr/nodeDiscoveryResult.h"
#include "pxr/usd/ndr/parserPlugin.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/plugin/usdMdl/neuray.h"
#include "pxr/usd/plugin/usdMdl/utils.h"
#include <mi/neuraylib/imdl_compiler.h>
#include <iostream>

using std::string;
using mi::neuraylib::Mdl;
using mi::base::Handle;
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

PXR_NAMESPACE_OPEN_SCOPE

namespace {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((discoveryType, "mdl"))
    ((sourceType, "mdl"))

    // The name to use for unnamed outputs.
    ((defaultOutputName, "result"))
);

class ShaderBuilder
{
private:
    const NdrNodeDiscoveryResult & m_discoveryResult;
    std::string m_MDLElementQualifiedName;
    mi::base::Handle<mi::neuraylib::INeuray> m_neuray;
    mi::base::Handle<mi::neuraylib::ITransaction> m_transaction;
    mi::base::Handle<mi::neuraylib::IMdl_compiler> m_mdl_compiler;
    mi::base::Handle<mi::neuraylib::IMdl_factory> m_mdl_factory;
private:
    bool ConvertParm(const IType * type, const IExpression * defaultValue, TfToken & typeOut, VtValue & defaultValueOut)
    {
        MdlToUsd converter;
        typeOut = converter.ConvertType(type);
        if (typeOut.IsEmpty())
        {
            return false;
        }
        if (defaultValue)
        {
            MdlToUsd::ExtraInfo extra;
            defaultValueOut = converter.GetValue(type, defaultValue, typeOut, extra);
        }
        return true;
    }

    template<class T> bool traverseFunctionOrMaterial(const T * scene_element, NdrPropertyUniquePtrVec & properties)
    {
        mi::Size count = scene_element->get_parameter_count();
        mi::base::Handle<const mi::neuraylib::IType_list> types(scene_element->get_parameter_types());
        mi::base::Handle<const mi::neuraylib::IExpression_list> defaults(scene_element->get_defaults());

        mi::base::Handle<const IFunction_definition> function(scene_element->template get_interface<IFunction_definition>());
        if (function)
        {
            mi::base::Handle<const mi::neuraylib::IType> rtn_type(function->get_return_type());

            // TODO
        }

        for (mi::Size index = 0; index < count; index++)
        {
            mi::base::Handle<const IType> parm_type(types->get_type(index));
            std::string parm_name = scene_element->get_parameter_name(index);

            mi::base::Handle<const mi::neuraylib::IExpression> defaultexp(defaults->get_expression(parm_name.c_str()));
            if (!defaultexp.is_valid_interface())
            {
                continue;
            }

            TfToken type;
            VtValue defaultValue;
            if (!ConvertParm(parm_type.get(), defaultexp.get(), type, defaultValue))
            {
                mi::neuraylib::Mdl::LogError("[ShaderBuilder] Failed to convert " + std::string(scene_element->get_mdl_name()) + " " + parm_name);
                continue;
            }

            TfToken name(parm_name);
            bool isOutput(false);
            NdrTokenMap metadata;
            NdrTokenMap hints;
            NdrOptionVec options;

            // Add the property.
            properties.push_back(
                SdrShaderPropertyUniquePtr(
                    new SdrShaderProperty(name,
                                            type,
                                            defaultValue,
                                            isOutput,
                                            0,
                                            metadata,
                                            hints,
                                            options)));
        }
        return true;
    }
    
public:
    ShaderBuilder(const NdrNodeDiscoveryResult& discoveryResult)
        : m_discoveryResult(discoveryResult)
    {
        SetMDLElementQualifiedName(m_discoveryResult.identifier.GetString());
    }

    ~ShaderBuilder()
    {
    }

    void SetMDLElementQualifiedName(const std::string & element)
    {
        m_MDLElementQualifiedName = element;
    }
    
    NdrNodeUniquePtr Build()
    {
        m_neuray = mi::base::make_handle_dup< mi::neuraylib::INeuray>(mi::neuraylib::Mdl::Get().GetNeuray());
        // Access the database and create a transaction.
        mi::base::Handle<mi::neuraylib::IDatabase> database(
            m_neuray->get_api_component<mi::neuraylib::IDatabase>());
        mi::base::Handle<mi::neuraylib::IScope> scope(database->get_global_scope());
        m_transaction = scope->create_transaction();
        NdrNodeUniquePtr rtn;
        if (!m_transaction)
        {
            Mdl::LogError("[ShaderBuilder::Build()] Unable to create transaction");
            return rtn;
        }
        m_mdl_compiler = m_neuray->get_api_component<mi::neuraylib::IMdl_compiler>();
        m_mdl_factory = m_neuray->get_api_component<mi::neuraylib::IMdl_factory>();

        NdrPropertyUniquePtrVec properties;

        std::string id(m_MDLElementQualifiedName);
        id = MdlUtils::GetDBName(id);
        {
            mi::base::Handle<const IMaterial_definition> material(m_transaction->access<IMaterial_definition>(id.c_str()));
            if (material)
            {
                bool rtn(traverseFunctionOrMaterial<IMaterial_definition>(material.get(), properties));
            }
            else
            {
                mi::base::Handle<const IFunction_definition> function(m_transaction->access<IFunction_definition>(id.c_str()));
                if (function)
                {
                    bool rtn(traverseFunctionOrMaterial<IFunction_definition>(function.get(), properties));
                }
                else
                {
                    mi::base::Handle<const IFunction_call> fcall(m_transaction->access<IFunction_call>(id.c_str()));
                    if (fcall)
                    {
                        std::cout << "fct call" << std::endl;
                    }
                }
            }

            TfToken context; // ???

            rtn = NdrNodeUniquePtr(
                new SdrShaderNode(m_discoveryResult.identifier,
                                  m_discoveryResult.version,
                                  m_discoveryResult.name,
                                  m_discoveryResult.family,
                                  context,
                                  m_discoveryResult.sourceType,
                                  m_discoveryResult.uri,
                                  m_discoveryResult.resolvedUri,
                                  std::move(properties),
                                  std::move(m_discoveryResult.metadata)));
        }
        m_transaction->commit();

        return rtn;
    }
};
} // anonymous namespace

/// Parses nodes in MDL files.
class UsdMdlParserPlugin : public NdrParserPlugin {
public:
    UsdMdlParserPlugin() = default;
    ~UsdMdlParserPlugin() override = default;

    NdrNodeUniquePtr Parse(
        const NdrNodeDiscoveryResult& discoveryResult) override;
    const NdrTokenVec& GetDiscoveryTypes() const override;
    const TfToken& GetSourceType() const override;
};

NdrNodeUniquePtr
UsdMdlParserPlugin::Parse(
    const NdrNodeDiscoveryResult& discoveryResultIn)
{
    NdrNodeDiscoveryResult discoveryResult(discoveryResultIn);
    for (auto & m : discoveryResult.metadata)
    {
        if (m.first == "info:mdl:sourceAsset:subIdentifier")
        {
#if 0
            //discoveryResult.identifier = NdrIdentifier(m.second);
            //discoveryResult.name = NdrIdentifier(m.second);
            NdrNodeConstPtr nodeConst = SdrRegistry::GetInstance().GetNodeByIdentifier(NdrIdentifier(m.second));
            NdrNodePtr node((NdrNodePtr)nodeConst);

            if (node)
            {
                Mdl::LogVerbose("[UsdMdlParserPlugin] node identifier: " + node->GetIdentifier().GetString());
                Mdl::LogVerbose("[UsdMdlParserPlugin] node name: " + node->GetName());
                Mdl::LogVerbose("[UsdMdlParserPlugin] node version: " + string(node->GetVersion().GetString()));
                Mdl::LogVerbose("[UsdMdlParserPlugin] node family: " + string(node->GetFamily()));
                Mdl::LogVerbose("[UsdMdlParserPlugin] node sourceType: " + string(node->GetSourceType()));

                Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryResult identifier: " + discoveryResult.identifier.GetString());
                Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryResult name: " + discoveryResult.name);
                Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryResult version: " + string(discoveryResult.version.GetString()));
                Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryResult family: " + string(discoveryResult.family));
                Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryResult sourceType: " + string(discoveryResult.sourceType));

                return NdrNodeUniquePtr(node);
            }
#endif
        }
    }

    Mdl::LogVerbose("[UsdMdlParserPlugin] MDL discovery result");
    Mdl::LogVerbose("[UsdMdlParserPlugin] identifier: " + discoveryResult.identifier.GetString());
    Mdl::LogVerbose("[UsdMdlParserPlugin] version: " + string(discoveryResult.version.GetString()));
    Mdl::LogVerbose("[UsdMdlParserPlugin] name: " + discoveryResult.name);
    Mdl::LogVerbose("[UsdMdlParserPlugin] family: " + string(discoveryResult.family));
    Mdl::LogVerbose("[UsdMdlParserPlugin] discoveryType: " + string(discoveryResult.discoveryType));
    Mdl::LogVerbose("[UsdMdlParserPlugin] sourceType: " + string(discoveryResult.sourceType));
    Mdl::LogVerbose("[UsdMdlParserPlugin] uri: " + discoveryResult.uri);
    Mdl::LogVerbose("[UsdMdlParserPlugin] resolvedUri: " + discoveryResult.resolvedUri);
    Mdl::LogVerbose("[UsdMdlParserPlugin] sourceCode: " + discoveryResult.sourceCode);
    for( auto & m: discoveryResult.metadata)
    {
        Mdl::LogVerbose("[UsdMdlParserPlugin] metadata: " + string(m.first) + " / " + string(m.second));
    }
    Mdl::LogVerbose("[UsdMdlParserPlugin] blindData: " + discoveryResult.blindData);

    ShaderBuilder builder(discoveryResult);

    for (auto & m : discoveryResult.metadata)
    {
        if (m.first == "info:mdl:sourceAsset:subIdentifier")
        {
            std::string moduleName(MdlUtils::GetModuleNameFromModuleFilename(discoveryResult.uri));

            // Comment out the following, the MDL Element Qualified Name is set in the ctor of ShaderBuilder
            // // Build MDL qualifed name from module name and element name
            // std::string elementName(m.second);
            // std::string qualifiedName = moduleName + "::" + elementName;
            // builder.SetMDLElementQualifiedName(qualifiedName);

            // Load module
            mi::neuraylib::ModuleLoader loader;
            loader.SetModuleName(moduleName);
            loader.LoadModule();
        }
    }

    return builder.Build();
}

const NdrTokenVec&
UsdMdlParserPlugin::GetDiscoveryTypes() const
{
    static const NdrTokenVec discoveryTypes = {
        _tokens->discoveryType
    };
    return discoveryTypes;
}

const TfToken&
UsdMdlParserPlugin::GetSourceType() const
{
    return _tokens->sourceType;
}

NDR_REGISTER_PARSER_PLUGIN(UsdMdlParserPlugin)

PXR_NAMESPACE_CLOSE_SCOPE
