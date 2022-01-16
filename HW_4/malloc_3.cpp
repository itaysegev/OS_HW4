#include <cstring>
#include <unistd.h>

// max bin hold less than (128 + 1) * 1024 = 132096
#define MAX 132095
#define MIN 0
#define KB  1024


// Classes
class MallocMetaData {
public:
    size_t size;
    bool is_free;
    MallocMetaData* next;
    MallocMetaData* prev;
    // bin can be computed using size / KB (=1024)
};

class Bin {
public:
    // Data members
    MallocMetaData* head;

    // Methods
    Bin() : head(nullptr) {}
};


// Static Variables
static Bin bins[128];
static size_t num_free_blocks = 0;
static size_t num_free_bytes = 0;
static size_t num_allocated_blocks = 0;
static size_t num_allocated_bytes = 0;


// Functions
void* smalloc(size_t size) {
    if((size == MIN) || (size > MAX)) {
        return NULL;
    }
    int bin_index = size / KB;
    //Search for free block
    MallocMetaData* tmp = bins[bin_index].head; // iterator
    while (tmp != nullptr) {
        if (tmp->is_free && tmp->size >= size) { // allocate the first free block that fits
            tmp->is_free = false;
            num_free_blocks -= 1;
            num_free_bytes -= tmp->size;
            return (void*)((char*)tmp + sizeof(MallocMetaData)); //return the block after the meta data
        }
        tmp = tmp->next;
    }
    //No free blocks in desired bin
    void* result = sbrk(size+sizeof(MallocMetaData));
    if (result == (void*)(-1)) {
        return NULL;
    }
    MallocMetaData* meta_data =(MallocMetaData*) result;
    meta_data->size = size;
    meta_data->is_free = false;
    meta_data->next = nullptr;
    meta_data->prev = nullptr;
    // handles static variables
    num_allocated_blocks += 1;
    num_allocated_bytes += size;
    //if bin was empty
    if(bins[bin_index].head == nullptr) {
        bins[bin_index].head = meta_data;
    }
    else {
        //iterator
        MallocMetaData* temp = bins[bin_index].head;
        MallocMetaData* pre_temp = nullptr;

        // edge case 1: if new MetaData is new head
        if (bins[bin_index].head->size >= size) {
            meta_data->next = bins[bin_index].head;
            bins[bin_index].head->prev = meta_data;
            bins[bin_index].head = meta_data;
            return (void*)((char*)meta_data + sizeof(MallocMetaData));
        }
        // finding a place between lesser and greater values of size
        while(temp != nullptr) {
            pre_temp = temp;
            if(temp->size <= size && temp->next != NULL && temp->next->size >= size) {
                meta_data->next = temp->next;
                temp->next = meta_data;
                meta_data->prev = temp;
                meta_data->next->prev = meta_data;
                return (void*)((char*)meta_data + sizeof(MallocMetaData));
            }
            temp = temp->next;
        }
//         we reached an edge - no place in between - last MetaData in list
//        if (pre_temp->size > size) {
//            if (pre_temp == bins[bin_index].head) {
//                bins[bin_index].head = meta_data;
//                meta_data->next = pre_temp;
//                pre_temp->prev = meta_data;
//            }
//            else {
//                meta_data->prev = pre_temp->prev;
//                pre_temp->prev->next = meta_data;
//                meta_data->next = pre_temp;
//                pre_temp->prev = meta_data;
//            }
//        }

        // edge case 2: if new MetaData is new tail
        meta_data->next = pre_temp->next;
        pre_temp->next = meta_data;
        meta_data->prev = pre_temp;
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
            // todo: not sure if we need to check if memcpy was successful
            std::memcpy(allocated_block, oldp, data->size);
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