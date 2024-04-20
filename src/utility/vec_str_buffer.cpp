#include "utility/vec_str_buffer.h"

using namespace avant::utility;

vec_str_buffer::vec_str_buffer() : data1(10),
                                   data2(10),
                                   reader_used_len(10),
                                   writer_used_len(10),
                                   reader_idx(0),
                                   writer_idx(0),
                                   data_reader(&data1),
                                   data_writer(&data2)
{
}

void vec_str_buffer::cache_swap()
{
    // 判断是否读完了
    if (reader_idx != reader_used_len)
    {
        return;
    }
    // 没有其他数据
    if (writer_used_len == 0)
    {
        return;
    }

    reader_idx = 0;
    reader_used_len = writer_used_len;
    writer_used_len = 0;
    writer_idx = 0;
    // swap cache
    std::vector<std::string> *data_ptr = data_reader;
    data_reader = data_writer;
    data_writer = data_ptr;
    read_out_idx = 0;
}

std::string &vec_str_buffer::push()
{
    if (writer_idx == writer_used_len)
    {
        if (writer_used_len == data_writer->size()) // full
        {
            data_writer->push_back("");
        }
        writer_used_len++;
    }

    std::string &res = data_writer->at(writer_idx);
    writer_idx++;
    res.clear();
    return res;
}

const std::string &vec_str_buffer::first()
{
    if (reader_idx == reader_used_len) // empty
    {
        if (data_reader->empty())
        {
            data_reader->push_back("");
            return data_reader->at(0);
        }
        else
        {
            data_reader->at(0).clear();
        }
        return data_reader->at(0);
    }
    return data_reader->at(reader_idx);
}

void vec_str_buffer::pop()
{
    if (reader_idx <= reader_used_len)
    {
        data_reader->at(reader_idx).clear();
        data_reader->at(reader_idx).resize(0);
        reader_idx++;
        read_out_idx++;
    }
}

bool vec_str_buffer::empty()
{
    return (reader_idx >= reader_used_len);
}
