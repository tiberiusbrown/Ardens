#pragma once

#include <cstring>
#include <istream>

namespace absim
{

template<bool is_output, class CharT, class Traits = std::char_traits<CharT>>
class strstream_streambuf : public std::basic_streambuf<CharT, Traits>
{
    typedef std::basic_streambuf<CharT, Traits > super_type;
    typedef strstream_streambuf<is_output, CharT, Traits> self_type;

public:

    typedef typename super_type::char_type char_type;
    typedef typename super_type::traits_type traits_type;
    typedef typename traits_type::int_type int_type;
    typedef typename traits_type::pos_type pos_type;
    typedef typename traits_type::off_type off_type;

    strstream_streambuf(CharT* s, std::streamsize n)
    {
        this->setg(s, s, s + n);
        if(is_output)
            this->setp(s, s + n);
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

    virtual std::streamsize xsputn(char_type const* s, std::streamsize n) override
    {
        if(!is_output)
            return 0;
        if(0 == n)
            return 0;
        if(this->pptr() + n >= this->epptr())
        {
            n = this->epptr() - this->pptr();
            if(0 == n && !traits_type::not_eof(this->overflow()))
                return -1;
        }
        std::memmove(this->pptr(), static_cast<void const*>(s), n);
        this->pbump(static_cast<int>(n));
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

    virtual int_type overflow(int_type c = traits_type::eof()) override
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
class basic_istrstream : public std::basic_istream<CharT, Traits>
{
public:

    basic_istrstream(CharT const* s, std::streamsize n)
        : std::basic_istream<CharT, Traits>(nullptr)
    {
        rdbuf_ = new strstream_streambuf<false, CharT, Traits>(const_cast<CharT*>(s), n);
        this->init(rdbuf_);
    }

    basic_istrstream(basic_istrstream&& other) = delete;
    basic_istrstream(basic_istrstream const& other) = delete;

    basic_istrstream& operator=(basic_istrstream&& other) = delete;
    basic_istrstream& operator=(basic_istrstream const& other) = delete;

    virtual ~basic_istrstream() override
    {
        delete rdbuf_;
    }

private:

    strstream_streambuf<false, CharT, Traits>* rdbuf_;

};

template<class CharT, class Traits = std::char_traits<CharT>>
class basic_ostrstream : public std::basic_ostream<char>
{
public:

    basic_ostrstream(CharT const* s, std::streamsize n)
        : std::basic_ostream<CharT, Traits>(nullptr)
    {
        rdbuf_ = new strstream_streambuf<true, CharT, Traits>(const_cast<CharT*>(s), n);
        this->init(rdbuf_);
    }

    basic_ostrstream(basic_ostrstream&& other) = delete;
    basic_ostrstream(basic_ostrstream const& other) = delete;

    basic_ostrstream& operator=(basic_ostrstream&& other) = delete;
    basic_ostrstream& operator=(basic_ostrstream const& other) = delete;

    virtual ~basic_ostrstream() override
    {
        delete rdbuf_;
    }

private:

    strstream_streambuf<true, CharT, Traits>* rdbuf_;

};

using istrstream = basic_istrstream<char>;
using ostrstream = basic_ostrstream<char>;

}
