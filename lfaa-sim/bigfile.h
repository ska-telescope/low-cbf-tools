/* This class reads a large file into RAM
 *
 * Keith Bengston. CSIRO. 21 Jan 2018
 */

#ifndef bigfile_h
#define bigfile_h
#include <string>
#include <memory> // for std::unique_ptr

class Bigfile
{
    private:
        std::string m_filename;
        bool m_is_binary;
        std::streamsize m_size;
        std::unique_ptr<char[]> m_data;

        bool exists();
    public:
        Bigfile(std::string filename, bool is_binary = false);
        bool read();
        char * get();
        uint64_t size();
        std::unique_ptr<char[]> data();
};

#endif
