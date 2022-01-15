#include <unistd.h>
#define MAX 100000000
#define MIN 0

// Functions Declarations
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void sfree(void* p);
void* srealloc(void* oldp, size_t size);

// Stats Methods Declarations
size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();

// Global variables
size_t num_free_blocks = 0;
size_t num_free_bytes = 0;
size_t num_allocated_blocks = 0;
size_t num_allocated_bytes = 0;

//static MallocMetaData* begin;
class MallocMetaData {
 public:
    size_t size;
    bool is_free;
    char* block_start;
    MallocMetaData* next;
    MallocMetaData* prev;

    //MallocMetaData()
};

// Functions
void* scalloc(size_t num, size_t size) {
    if (size == MIN || size > MAX) {
        return NULL;
    }
    // assuming smalloc updates metadata
    void* allocated_block = smalloc(num * size);
    if (allocated_block == NULL) {
        return NULL;
    }
    std::memset(allocated_block, 0, num * size);

    // in this scenario we assume smalloc returns block_start or equivalent
    return allocated_block;
}

void* srealloc(void* oldp, size_t size) {
    if (size == MIN || size > MAX) {
        return NULL;
    }

    if (oldp == NULL) {
        return smalloc(size);
    }

    MallocMetaData* data = (MallocMetaData*)((char*)oldp - sizeof(MallocMetaData));
    if (data->size < size) {
        void* allocated_block = smalloc(size);
        if (allocated_block != NULL) {
            // todo: not sure if we need to check if memcpy was successful
            std::memcpy(allocated_block, oldp, data->size);
            sfree(oldp);
        }
        // in this scenario we assume smalloc returns block_start or equivalent
        return allocated_block;
    }

    // reaches here if block can be reused
    return oldp;
}

// Stats Functions
size_t _num_free_blocks() {
    return num_free_blocks;
}

size_t _num_free_bytes() {
    return num_free_bytes;
}

size_t _num_allocated_blocks() {
    return num_allocated_blocks;
}

size_t _num_allocated_bytes() {
    return num_allocated_bytes;
}

size_t _num_meta_data_bytes() {
    return sizeof(MallocMetaData) * num_allocated_blocks;
}

size_t _size_meta_data( {
    return sizeof(MallocMetaData);
}