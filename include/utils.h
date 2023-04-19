#ifndef __UTILS_H__
#define __UTILS_H__

#define is_null(ptr) (ptr == nullptr)

int get_log2(unsigned long num);
int get_sets(int ways, unsigned long capacity, int b_size);
int get_tag_bits(int b_size, int i_size);

#define throw_error(err_msg, func)    \
({                               \
    char buf[512];                \
    sprintf(buf, err_msg, func);          \
    throw std::runtime_error(buf); \
})

#endif