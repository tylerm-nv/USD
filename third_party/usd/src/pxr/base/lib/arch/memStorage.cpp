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
#include "pxr/base/arch/memStorage.h"

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
	class ArchMemStorageBase : public ArchMemStorage
	{
	protected:
		friend class ArchMemStorageRegistry;
		typedef std::shared_ptr<std::string> StringRefPtr_t;
		StringRefPtr_t _pathRefPtr;

		ArchMemStorageBase(StringRefPtr_t &&pathRefPtr)
			: _pathRefPtr(std::move(pathRefPtr))
		{
		}

	public:
		virtual ~ArchMemStorageBase() override;

		virtual const std::string& GetPath() const override
		{
			return *_pathRefPtr.get();
		}
	};


	class ArchMemFixedStorage : public ArchMemStorageBase
	{
	public:
		ArchMemFixedStorage(StringRefPtr_t &&pathRefPtr, size_t length, const uint8_t* data, std::function<void(const void*, size_t)> &&func)
			: ArchMemStorageBase(std::move(pathRefPtr)), _length(length), _data(data), _func(std::move(func))
		{
		}

		virtual ~ArchMemFixedStorage() override
		{
			if (_func)
			{
				_func(_data, _length);
			}
		}

		virtual size_t GetLength() const override { return _length; }

		virtual size_t Read(uint8_t* data, size_t count, size_t offset) override
		{
			size_t offsetEnd = offset + count;
			if (offsetEnd > _length) offsetEnd = _length;

			if (offsetEnd > offset)
			{
				const size_t copyCount = (offsetEnd - offset);
				memcpy(data, _data + offset, copyCount);
				return copyCount;
			}
			else
			{
				return 0;
			}
		}
		virtual size_t Write(const uint8_t* data, size_t count, size_t offset) override
		{
			return 0;
		}

		virtual void* GetPtrForMapping(bool isReadOnly) override
		{
			return isReadOnly ? const_cast<uint8_t*>(_data) : nullptr;
		}

	private:
		size_t _length;
		const uint8_t* _data;

		std::function<void(const void*, size_t)> _func;
	};


	class ArchMemGrowingStorage : public ArchMemStorageBase
	{
	public:
		static const size_t ChunkSize = 256 * 1024; // 256 Kb (8 bits)

		ArchMemGrowingStorage(StringRefPtr_t &&pathRefPtr, size_t fixedSize)
			: ArchMemStorageBase(std::move(pathRefPtr)), _fixedSize(fixedSize), _length(fixedSize)
		{
			if (_fixedSize > 0)
			{
				_fixedChunk.reset(new uint8_t[_fixedSize]);
			}
		}

		virtual size_t GetLength() const override { return _length; }

		virtual size_t Read(uint8_t* data, size_t count, size_t offset) override
		{
			size_t offsetEnd = std::min(offset + count, _length);
			if (offsetEnd <= offset)
			{
				return 0;
			}

			auto readCopyFunc = [](uint8_t* data, const uint8_t* chunkData, size_t size) { memcpy(data, chunkData, size); };

			uint8_t* data0 = data;
			if (_CopyFixedData(data, offset, offsetEnd, readCopyFunc))
			{
				const size_t begChunkIdx = offset / ChunkSize;
				const size_t endChunkIdx = (offsetEnd + ChunkSize - 1) / ChunkSize;

				_CopyData(data, begChunkIdx, endChunkIdx, offset, offsetEnd, readCopyFunc);
			}
			return (data - data0);
		}

		virtual size_t Write(const uint8_t* data, size_t count, size_t offset) override
		{
			if (count == 0)
			{
				return 0;
			}
			size_t offsetEnd = offset + count;

			auto writeCopyFunc = [](const uint8_t* data, uint8_t* chunkData, size_t size) { memcpy(chunkData, data, size); };

			const uint8_t* data0 = data;
			if (_CopyFixedData(data, offset, offsetEnd, writeCopyFunc))
			{
				const size_t begChunkIdx = offset / ChunkSize;
				const size_t endChunkIdx = (offsetEnd + ChunkSize - 1) / ChunkSize;

				const size_t endChunkIdx0 = _chunks.size();
				if (endChunkIdx > endChunkIdx0)
				{
					//allocate new chunks
					_chunks.resize(endChunkIdx);
					for (auto idx = endChunkIdx0; idx < endChunkIdx; ++idx)
					{
						_chunks[idx].reset(new uint8_t[ChunkSize]);
					}
				}

				_CopyData(data, begChunkIdx, endChunkIdx, offset, offsetEnd, writeCopyFunc);

				_length = std::max(_length, _fixedSize + offsetEnd);
			}
			return (data - data0);
		}

		virtual void* GetPtrForMapping(bool isReadOnly) override
		{
			return (_length <= _fixedSize) ? _fixedChunk.get() : nullptr;
		}

	private:
		const size_t _fixedSize;
		size_t _length;

		typedef std::unique_ptr<uint8_t[]> Chunk_t;
		Chunk_t _fixedChunk;
		std::vector<Chunk_t> _chunks;

		template <typename Ptr, typename Func>
		bool _CopyFixedData(Ptr &data, size_t &offset, size_t &offsetEnd, Func copyFunc)
		{
			if (offset < _fixedSize)
			{
				const bool isOnlyFixed = (offsetEnd <= _fixedSize);
				const size_t copyCount = (isOnlyFixed ? offsetEnd : _fixedSize) - offset;
				copyFunc(data, _fixedChunk.get() + offset, copyCount);
				data += copyCount;
				if (isOnlyFixed)
				{
					return false;
				}
				offset = _fixedSize;
			}
			offset -= _fixedSize;
			offsetEnd -= _fixedSize;
			return true;
		}

		template <typename Ptr, typename Func>
		void _CopyData(Ptr &data, size_t begChunkIdx, size_t endChunkIdx, size_t offsetBeg, size_t offsetEnd, Func copyFunc)
		{
			if (begChunkIdx == endChunkIdx - 1)
			{
				const size_t offsetInChunk = (offsetBeg - begChunkIdx * ChunkSize);
				const size_t copyCount = offsetEnd - offsetBeg;
				copyFunc(data, _chunks[begChunkIdx].get() + offsetInChunk, offsetEnd - offsetBeg);
				data += copyCount;
			}
			else
			{
				{
					const size_t offsetInChunk = (offsetBeg - begChunkIdx * ChunkSize);
					const size_t copyCount = ChunkSize - offsetInChunk;
					copyFunc(data, _chunks[begChunkIdx].get() + offsetInChunk, copyCount);
					data += copyCount;
				}
				auto curChunkIdx = begChunkIdx + 1;
				for (; curChunkIdx < endChunkIdx - 1; ++curChunkIdx)
				{
					copyFunc(data, _chunks[curChunkIdx].get(), ChunkSize);
					data += ChunkSize;
				}
				{
					const size_t copyCount = (offsetEnd - curChunkIdx * ChunkSize);
					copyFunc(data, _chunks[curChunkIdx].get(), copyCount);
					data += copyCount;
				}
			}
		}
	};


	class ArchMemStorageRegistry
	{
		struct ComparePath
		{
			using is_transparent = void;

			bool operator()(const ArchMemStorageBase::StringRefPtr_t& path1, const ArchMemStorageBase::StringRefPtr_t& path2) const
			{
				return *path1.get() < *path2.get();
			}
			bool operator()(const ArchMemStorageBase::StringRefPtr_t& path1, const char* path2) const
			{
				return ::strcmp(path1.get()->c_str(), path2) < 0;
			}
			bool operator()(const char* path1, const ArchMemStorageBase::StringRefPtr_t& path2) const
			{
				return ::strcmp(path1, path2.get()->c_str()) < 0;
			}
		};

		std::map<ArchMemStorageBase::StringRefPtr_t, boost::weak_ptr<ArchMemStorageBase>, ComparePath> _map;

		ArchMemStorageRegistry() {}

	public:
		static ArchMemStorageRegistry* GetInst()
		{
			static ArchMemStorageRegistry inst;
			return &inst;
		}

		template <class T, typename... Args>
		boost::shared_ptr<T> Create(std::string&& path, Args&&... args)
		{
			auto pathRefPtr = std::make_shared<std::string>(std::move(path));
			auto result = _map.emplace(std::make_pair(pathRefPtr, boost::weak_ptr<ArchMemStorageBase>()));
			if (result.second)
			{
				boost::shared_ptr<T> ret = boost::make_shared<T>(std::move(pathRefPtr), std::forward<Args>(args)...);

				result.first->second = ret;
				return ret;
			}
			else
			{
				//assert(0 && "ArchMemStorage exists with the same path");
				return nullptr;
			}
		}

		void Remove(ArchMemStorageBase* ptr)
		{
			_map.erase(ptr->_pathRefPtr);
		}

		boost::shared_ptr<ArchMemStorageBase> Get(const char* path) const
		{
			auto it = _map.find(path);
			return it != _map.end() ? it->second.lock() : boost::shared_ptr<ArchMemStorageBase>();
		}
	};

	ArchMemStorageBase::~ArchMemStorageBase()
	{
		ArchMemStorageRegistry::GetInst()->Remove(this);
	}
}

class ArchMappingMemImpl : public ArchMappingImpl
{
	const boost::shared_ptr<ArchMemStorage> _memStorage;

	ArchMappingMemImpl(const boost::shared_ptr<ArchMemStorage>& memStorage, void* ptr)
		: ArchMappingImpl(ptr, memStorage->GetLength()), _memStorage(memStorage)
	{
	}

public:
	static ArchMappingMemImpl* Create(const boost::shared_ptr<ArchMemStorage>& memStorage, bool isReadOnly)
	{
		void* ptr = memStorage->GetPtrForMapping(isReadOnly);
		return (ptr != nullptr) ? new ArchMappingMemImpl(memStorage, ptr) : nullptr;
	}

	virtual ~ArchMappingMemImpl() override {}
};

class ArchMemFile : public ArchFile
{
	boost::shared_ptr<ArchMemStorage> _memStorage;

	ArchMemFile(const boost::shared_ptr<ArchMemStorage>& memStorage) : _memStorage(memStorage) {}

public:
	static ArchMemFile* Open(const char* fileName)
	{
		auto memStorage = ArchMemStorageRegistry::GetInst()->Get(fileName);
		return memStorage ? new ArchMemFile(memStorage) : nullptr;
	}

	virtual int64_t GetFileLength() override
	{
		return _memStorage->GetLength();
	}

	virtual ArchConstFileMapping MapFileReadOnly(std::string *errMsg) override
	{
		return ArchConstFileMapping(ArchMappingMemImpl::Create(_memStorage, true));
	}
	virtual ArchMutableFileMapping MapFileReadWrite(std::string *errMsg) override
	{
		return ArchMutableFileMapping(ArchMappingMemImpl::Create(_memStorage, false));
	}

	virtual int64_t PRead(void *buffer, size_t count, int64_t offset) override
	{
		return _memStorage->Read(static_cast<uint8_t*>(buffer), count, offset);
	}
	virtual int64_t PWrite(void const *bytes, size_t count, int64_t offset) override
	{
		return _memStorage->Write(static_cast<const uint8_t*>(bytes), count, offset);
	}

	virtual void FileAdvise(int64_t offset, size_t count, ArchFileAdvice adv) override
	{
		//do nothing
	}

};

boost::shared_ptr<ArchMemStorage> ArchCreateMemStorageRO(std::string &&path, const void* data, size_t size, std::function<void(const void*, size_t)> &&func)
{
	return ArchMemStorageRegistry::GetInst()->Create<ArchMemFixedStorage>(std::move(path), size, static_cast<const uint8_t*>(data), std::move(func));
}

boost::shared_ptr<ArchMemStorage> ArchCreateMemStorageRW(std::string &&path, size_t fixedSize, ArchMutableFileMapping *outMapping)
{
	auto memStorage = ArchMemStorageRegistry::GetInst()->Create<ArchMemGrowingStorage>(std::move(path), fixedSize);
	if (memStorage && outMapping)
	{
		*outMapping = ArchMutableFileMapping(ArchMappingMemImpl::Create(memStorage, false));
	}
	return memStorage;
}

ArchFile* _ArchOpenMemFile(char const* fileName, char const* mode)
{
	return ArchMemFile::Open(fileName);
}


PXR_NAMESPACE_CLOSE_SCOPE
