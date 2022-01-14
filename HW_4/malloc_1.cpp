#include <unistd.h>
#define MAX 100000000
#define MIN 0

void* smalloc(size_t size) {
    if((size == MIN) || (size > MAX)) {
        return NULL;
    }
    void* result = sbrk(size);
    if(result == (void*)(-1)) {
        return NULL; 
    }
    return result;
}