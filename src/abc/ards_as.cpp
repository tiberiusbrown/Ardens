#include <ards_assembler.hpp>

#include <iostream>
#include <fstream>

int main(int argc, char** argv)
{

    ards::assembler_t a;

    if(argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <output> <input 1> <input 2> ..." << std::endl;
        return 1;
    }

    for(int i = 2; i < argc; ++i)
    {
        std::ifstream f(argv[i], std::ios::in);
        if(!f)
        {
            std::cerr << "Unable to open file: \"" << argv[i] << "\"" << std::endl;
            return 1;
        }
        auto e = a.assemble(f);
        if(!e.msg.empty())
        {
            std::cerr << "Assembler Error: " << e.msg << std::endl;
            return 1;
        }
    }

    {
        auto e = a.link();
        if(!e.msg.empty())
        {
            std::cerr << "Link Error: " << e.msg << std::endl;
            return 1;
        }
    }

    std::ofstream f(argv[1], std::ios::out | std::ios::binary);
    if(!f)
    {
        std::cerr << "Unable to open file: \"" << argv[1] << "\"" << std::endl;
        return 1;
    }
    f.write((char const*)a.data().data(), a.data().size());

	return 0;
}
