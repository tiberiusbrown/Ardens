#pragma once

#include <cstring>
#include <istream>

namespace absim
{

template<class CharT, class Traits = std::char_traits<CharT>>
class strstream_streambuf : public std::basic_streambuf<CharT, Traits>
{
    typedef std::basic_streambuf<CharT, Traits > super_type;
    typedef strstream_streambuf<CharT, Traits> self_type;

public:

    typedef typename super_type::char_type char_type;
    typedef typename super_type::traits_type traits_type;
    typedef typename traits_type::int_type int_type;
    typedef typename traits_type::pos_type pos_type;
    typedef typename traits_type::off_type off_type;

    strstream_streambuf(CharT* s, std::streamsize n)
    {
        this->setg(s, s, s + n);
    }

    virtual std::streamsize xsgetn(char_type* s, std::streamsize n) override
    {
        if(0 == n)
            return 0;
        if(this->gptr() + n >= this->egptr())
        {
            n = this->egptr() - this->gptr();
            if(0 == n && !traits_type::not_eof(this->underflow()))
                return -1;
        }
        std::memmove(static_cast<void*>(s), this->gptr(), n);
        this->gbump(static_cast<int>(n));
        return n;
    }
    
    virtual int_type pbackfail(int_type c) override
    {
        char_type *pos = this->gptr() - 1;
        *pos = traits_type::to_char_type(c);
        this->pbump(-1);
        return 1;
    }

    virtual int_type underflow() override
    {
        return traits_type::eof();
    }

    virtual std::streamsize showmanyc() override
    {
        return static_cast<std::streamsize>(this->egptr() - this->gptr());
    }

    virtual ~strstream_streambuf() override
    {}
};

template<class CharT, class Traits = std::char_traits<CharT>>
class istrstream : public std::basic_istream<CharT, Traits>
{
public:

    istrstream(CharT const* s, std::streamsize n)
        : std::basic_istream<CharT, Traits>(nullptr)
    {
        rdbuf_ = new strstream_streambuf<CharT, Traits>(const_cast<CharT*>(s), n);
        this->init(rdbuf_);
    }

    istrstream(istrstream&& other) = delete;
    istrstream(istrstream const& other) = delete;

    istrstream& operator=(istrstream&& other) = delete;
    istrstream& operator=(istrstream const& other) = delete;

    virtual ~istrstream() override
    {
        delete rdbuf_;
    }

private:

    strstream_streambuf<CharT, Traits>* rdbuf_;

};

}
