#include "bigfile.h"
#include <iostream> // for cin cout cerr
#include <fstream> // for ifstream
#include <stdio.h> // for fopen fclose



Bigfile::Bigfile(std::string filename, bool is_binary)
    :m_filename(filename)
    , m_is_binary(is_binary)
    , m_size(0)
{
}

bool Bigfile::read()
{
    // Initially open file at end-of-file to determine size
    std::ifstream file;
    std::ios_base::openmode openmode = std::ifstream::in | std::ios::ate;
    if(m_is_binary)
        openmode |= std::ios::binary;
    file.open(m_filename, openmode);
    if( !file.is_open())
    {
        std::cout << "Unable to open file: '" << m_filename << "'" << std::endl;
        return false;
    }
    m_size = file.tellg();

    //allocate memory to hold file data
    try
    {
        m_data = std::make_unique<char[]>(m_size);
    }
    catch (std::bad_alloc & ba)
    {
        std::cerr << "Couldn't allocate RAM for file read: " << ba.what()
            << std::endl;
        return false;
    }

    // Read file from beginning
    file.seekg(0, std::ios::beg);
    file.read(m_data.get(), m_size);
    file.close();

    //std::cout << "Loaded '" <<m_filename << "' (" << m_size << " bytes)"
    //    << std::endl;
    return true;
}

// Return pointer to underlying data bytes
char * Bigfile::get()
{
    return m_data.get();
}

// Return file size in bytes
uint64_t Bigfile::size()
{
    return m_size;
}

std::unique_ptr<char[]> Bigfile::data()
{
    return move(m_data);
}
