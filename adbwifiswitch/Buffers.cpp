#include <cassert>
#include <cstring>

#include "Buffers.h"


// BufferBase class implementation

BufferBase::BufferBase(std::size_t reserve)
    : m_buf( reserve )
{
    
}

void BufferBase::cut()
{
    m_head = m_filled = 0;
}

void BufferBase::cut(std::size_t size, bool move)
{
    assert( size <= m_filled );
    m_head += size;
    m_filled -= size;
    if (m_filled == 0) m_head = 0;
    if (move && m_head > 0) {
        assert( m_filled > 0 );
        memmove( m_buf.data(), headPtr(), m_filled );
        m_head = 0;
    }
}

bool BufferBase::empty() const
{
    return m_filled == 0;
}

void *BufferBase::endPtr()
{
    assert( (m_head + m_filled) < m_buf.size() );
    return m_buf.data() + m_head + m_filled;
}

std::size_t BufferBase::filledSize() const
{
    return m_filled;
}

const char *BufferBase::headPtr() const
{
    assert( m_head < m_buf.size() );
    return m_buf.data() + m_head;
}

void BufferBase::reserve(std::size_t size, bool trim)
{
    if (restSize() >= size) return;
    if (trim) cut( 0, true );
    if (restSize() >= size) return;
    m_buf.resize( m_head + m_filled + size );
}

std::size_t BufferBase::restSize() const
{
    assert((m_head + m_filled) <= m_buf.size());
    return m_buf.size() - (m_head + m_filled);
}


// ReadBuffer class implementation

void ReadBuffer::addFilled(std::size_t size)
{
    m_filled += size;
    assert((m_filled + m_head) <= m_buf.size());
}


// WriteBuffer class implementation

WriteBuffer &WriteBuffer::append(const char *buf, size_t size, bool trim)
{
    reserve( size, trim );
    memcpy( endPtr(), buf, size );
    m_filled += size;
}
