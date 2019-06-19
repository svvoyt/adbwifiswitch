#ifndef BUFFERS_H
#define BUFFERS_H

#include <vector>

class BufferBase
{
public:
    BufferBase(std::size_t reserve = 512);

    void cut();
    void cut(std::size_t size, bool move=false);
    bool empty() const;
    const char *headPtr() const;
    void *endPtr();
    std::size_t filledSize() const;
    void reserve( std::size_t size, bool trim = false );
    std::size_t restSize() const;
    
protected:
    std::vector<char> m_buf;
    std::size_t m_filled = 0;
    std::size_t m_head = 0;
};

class ReadBuffer : public BufferBase {
public:
    using BufferBase::BufferBase;
    
    void addFilled(std::size_t size);
    const char *head() const {return headPtr();}
    void *readPtr() {return endPtr();}
};

class WriteBuffer : public BufferBase {
public:
    using BufferBase::BufferBase;

    WriteBuffer &append(const char *buf, size_t size, bool trim = false);
    const void *ptr() const {return reinterpret_cast<const void *>( headPtr() );}
    void pushHead(std::size_t size, bool trim = false) {cut( size, trim );}
    std::size_t size() const {return m_filled;}
};

#endif // BUFFERS_H
