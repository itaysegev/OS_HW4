#include <cstring>
#include <unistd.h>
#define MAX 100000000
#define MIN 0

// Functions Declarations
// void* smalloc(size_t size);
// void* scalloc(size_t num, size_t size);
// void sfree(void* p);
// void* srealloc(void* oldp, size_t size);

// Stats Methods Declarations
// size_t _num_free_blocks();
// size_t _num_free_bytes();
// size_t _num_allocated_blocks();
// size_t _num_allocated_bytes();
// size_t _num_meta_data_bytes();
// size_t _size_meta_data();

// // Global variables
// size_t num_free_blocks = 0;
// size_t num_free_bytes = 0;
// size_t num_allocated_blocks = 0;
// size_t num_allocated_bytes = 0;

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

// Global pointer to the head of the list
MallocMetaData* head = nullptr;

// Functions

void* smalloc(size_t size) {
    if((size == MIN) || (size > MAX)) {
        return NULL;
    }
    //Search for free block
    MallocMetaData* tmp = head; // iterator
    while (tmp != nullptr) {
        if (tmp->is_free && tmp->size >= size) { // allocate the first free block that fits
            tmp->is_free = false;
            return (void*)((long)tmp+(long)sizeof(MallocMetaData)); //return the block after the meta data
        }
        tmp = tmp->next;
    }
    //No free blocks
    void* result = sbrk(size+sizeof(MallocMetaData));
    if (result == (void*)(-1)) {
        return NULL;
    }
    MallocMetaData* meta_data =(MallocMetaData*) result;
    meta_data->size = size;
    meta_data->is_free = false;
    meta_data->next = nullptr;
    //if list was empty
    if(head == nullptr) { 
        head = meta_data;
        head->prev = nullptr;
    }
    else {
        MallocMetaData* temp = head; //iterator
        while(temp->next!= nullptr) {
         temp = temp->next;
        } // find last in the list
        temp->next = meta_data;
        meta_data->prev = temp;
    }
    return (void*)((long)meta_data+(long)sizeof(MallocMetaData));
}

void* scalloc(size_t num, size_t size) {
    if (size == MIN || size > MAX) {
        return NULL;
    }
    // assuming smalloc updates meta_data
    void* allocated_block = smalloc(num * size);
    if (allocated_block == NULL) {
        return NULL;
    }
    std::memset(allocated_block, 0, num * size);

    // in this scenario we assume smalloc returns block_start or equivalent
    return allocated_block;
}

void sfree(void* p) {
    MallocMetaData* meta_data;
    if(p!=nullptr) {
        meta_data =(MallocMetaData*)((long)p-(long)sizeof(MallocMetaData)); //remove the meta data from given pointer to get to the real block
        meta_data->is_free = true;  
    }

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
    size_t counter = 0;
    MallocMetaData* tmp = head;
    while(tmp != nullptr) {
        if(tmp->is_free) {
            counter++;
        }
        tmp = tmp->next;
    }
    return counter;
}

size_t _num_free_bytes() {
    size_t sum = 0;
    MallocMetaData* tmp = head;
    while(tmp != nullptr) {
        if(tmp->is_free) {
            sum+=tmp->size;
        }
        tmp = tmp->next;
    }
    return sum;
}

size_t _num_allocated_blocks() {
    size_t counter = 0;
    MallocMetaData* tmp = head;
    while(tmp != nullptr) {
        counter++;
        tmp = tmp->next;
    }
    return counter;
}

size_t _num_allocated_bytes() {
    size_t sum = 0;
    MallocMetaData* tmp = head;
    while(tmp != nullptr) {
        sum+=tmp->size;
        tmp = tmp->next;
    }
    return sum;
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}

size_t _num_meta_data_bytes() {
    return _size_meta_data() * _num_allocated_blocks();
}




// size_t _num_free_blocks() {
//     return num_free_blocks;
// }

// size_t _num_free_bytes() {
//     return num_free_bytes;
// }

// size_t _num_allocated_blocks() {
//     return num_allocated_blocks;
// }

// size_t _num_allocated_bytes() {
//     return num_allocated_bytes;
// }

// size_t _num_meta_data_bytes() {
//     return sizeof(MallocMetaData) * num_allocated_blocks;
// }

// size_t _size_meta_data( {
//     return sizeof(MallocMetaData);
// }