#include "pch.h"
#include "MemoryBuffer.h"

MemoryBuffer::MemoryBuffer(uint32_t initialSize) : _buffer(initialSize) {

}

MemoryBuffer::MemoryBuffer(const uint8_t* data, uint32_t size) {
    Init(data, size);
}

void MemoryBuffer::Init(const uint8_t* data, uint32_t size) {
    _buffer.resize(size);
    ::memcpy(_buffer.data(), data, size);
}

uint32_t MemoryBuffer::GetData(int64_t offset, uint8_t* buffer, uint32_t count) {
    if (offset >= GetSize())
        return 0;
    if (count + offset > GetSize())
        count = uint32_t(GetSize() - offset);
    ::memcpy(buffer, _buffer.data() + offset, count);
    return count;
}

bool MemoryBuffer::Insert(int64_t offset, const uint8_t* data, uint32_t count) {
    _buffer.insert(_buffer.begin() + offset, data, data + count);
    return true;
}

bool MemoryBuffer::Delete(int64_t offset, size_t count) {
    _buffer.erase(_buffer.begin() + offset, _buffer.begin() + offset + count);
    return true;
}

bool MemoryBuffer::SetData(int64_t offset, const uint8_t* data, uint32_t count) {
    ::memcpy(_buffer.data() + offset, data, count);
    return true;
}

int64_t MemoryBuffer::GetSize() const {
    return _buffer.size();
}

uint8_t* MemoryBuffer::GetRawData(int64_t offset) {
    return _buffer.data() + offset;
}

bool MemoryBuffer::IsReadOnly() const {
    return false;
}

bool MemoryBuffer::Increase(uint32_t size) {
    _buffer.resize(_buffer.size() + size);
    return true;
}
