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
#ifndef ARCH_MEMSTORAGE_H
#define ARCH_MEMSTORAGE_H

#include "pxr/base/arch/fileSystem.h"

#include <functional>
#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class ArchMemStorage
{
protected:
	ArchMemStorage() {}

public:
	virtual ~ArchMemStorage() {}

	virtual const std::string& GetPath() const = 0;

	virtual size_t GetLength() const = 0;

	virtual size_t Read(uint8_t* data, size_t count, size_t offset) = 0;
	virtual size_t Write(const uint8_t* data, size_t count, size_t offset) = 0;

	virtual void* GetPtrForMapping(bool isReadOnly) = 0;
};

ARCH_API boost::shared_ptr<ArchMemStorage> ArchCreateMemStorageRO(std::string &&path, const void* data, size_t size, std::function<void(const void*, size_t)> &&func = std::function<void(const void*, size_t)>());
ARCH_API boost::shared_ptr<ArchMemStorage> ArchCreateMemStorageRW(std::string &&path, size_t fixedSize = 0, ArchMutableFileMapping *outMapping = nullptr);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // ARCH_MEMSTORAGE_H
