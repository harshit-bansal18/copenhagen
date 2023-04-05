#include <utils.h>

int get_log2(unsigned long num) {
    int _s = 0;
    while (num != 1) {
        num = num >> 1;
        // fprintf(_debug,"%s: num=%d\n", __func__, num);
        _s++;
    }
    return _s;
}

int get_sets(int w, unsigned long c, int b_size) {
    unsigned long set_s = (unsigned long)(w*b_size);
    return (int)(c/set_s);
}

int get_tag_bits(int b_size, int i_size) {
    
    // for 64 bit address 
    return (64 - b_size - i_size);
}