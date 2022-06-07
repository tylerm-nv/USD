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

#include "pxr/pxr.h"
#include "pxr/base/tf/unicodeUtils.h"

#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>

PXR_NAMESPACE_OPEN_SCOPE

TfUnicodeUtils::TfUnicodeUtils()
{
}

TfUnicodeUtils::~TfUnicodeUtils()
{
}

TfUnicodeUtils&
TfUnicodeUtils::GetInstance()
{
    static TfUnicodeUtils utils;
    return utils;
}

std::string
TfUnicodeUtils::MakeValidUTF8Identifier(const std::string& identifier)
{
    auto append = [](uint32_t codePoint, std::string& result) {
        if (codePoint < 0x80)
        {
            // 1-byte UTF-8 encoding
            result.push_back(static_cast<char>(static_cast<unsigned char>(codePoint)));
        }
        else if (codePoint < 0x800)
        {
            // 2-byte UTF-8 encoding
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint >> 6) | 0xc0)));
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint & 0x3f) | 0x80)));
        }
        else if (codePoint < 0x10000)
        {
            // 3-byte UTF-8 encoding
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint >> 12) | 0xe0)));
            result.push_back(static_cast<char>(static_cast<unsigned char>(((codePoint >> 6) & 0x3f) | 0x80)));
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint & 0x3f) | 0x80)));
        }
        else
        {
            // 4-byte UTF-8 encoding
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint >> 18) | 0xf0)));
            result.push_back(static_cast<char>(static_cast<unsigned char>(((codePoint >> 12) & 0x3f) | 0x80)));
            result.push_back(static_cast<char>(static_cast<unsigned char>(((codePoint >> 6) & 0x3f) | 0x80)));
            result.push_back(static_cast<char>(static_cast<unsigned char>((codePoint & 0x3f) | 0x80)));
        }
    };

    std::string result;

    // empty strings are always associated with the '_' identifier
    if (identifier.empty())
    {
        result.push_back('_');
        return result;
    }

    // maximum size is always the number of bytes in the UTF-8 encoded string
    // but if a character is invalid it will be replaced by a '_' character, which
    // may compress a e.g., 4-byte UTF-8 invalid character into a single valid 1-byte UTF-8 '_' character
    result.reserve(identifier.size());
    TfUnicodeUtils::utf8_const_iterator iterator(identifier.begin(), identifier.end());
    
    // first UTF-8 character must be in XID_Start
    if (!TfUnicodeUtils::IsUTF8CharXIDStart(identifier, iterator.Wrapped()))
    {
        result.push_back('_');
    }
    else
    {
        append(*iterator, result);
    }

    for (++iterator; iterator != identifier.end(); iterator++)
    {
        if (!TfUnicodeUtils::IsUTF8CharXIDContinue(identifier, iterator.Wrapped()))
        {
            result.push_back('_');
        }
        else
        {
            append(*iterator, result);
        }
    }

    return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
