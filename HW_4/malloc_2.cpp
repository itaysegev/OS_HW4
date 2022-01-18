#include <cstring>
#include <unistd.h>
#define MAX 100000000
#define MIN 0

// Classes
class MallocMetaData {
 public:
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
};

// Static Variables
static size_t num_free_blocks = 0;
static size_t num_free_bytes = 0;
static size_t num_allocated_blocks = 0;
static size_t num_allocated_bytes = 0;

// pointer to the head of the list
static MallocMetaData* head = nullptr;

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
            // handles static variables
            num_free_blocks -= 1;
            num_free_bytes -= tmp->size;
            return (void*)((char*)tmp + sizeof(MallocMetaData)); //return the block after the meta data
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
    // handles static variables
    num_allocated_blocks += 1;
    num_allocated_bytes += size;
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
    return (void*)((char*)meta_data + sizeof(MallocMetaData));
}

void* scalloc(size_t num, size_t size) {
    if (size == MIN || size > MAX) {
        return NULL;
    }
    void* allocated_block = smalloc(num * size);
    if (allocated_block == NULL) {
        return NULL;
    }
    std::memset(allocated_block, 0, num * size);

    return allocated_block;
}

void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetaData* meta_data = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));
    if(meta_data->is_free == false) {
        meta_data->is_free = true;
        // handles static variables
        num_free_blocks += 1;
        num_free_bytes += meta_data->size;
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
            std::memcpy(allocated_block, oldp, data->size+sizeof(MallocMetaData));
            sfree(oldp);
        }
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

 size_t _size_meta_data() {
     return sizeof(MallocMetaData);
 }