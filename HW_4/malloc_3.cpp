#include <cstring>
#include <unistd.h>

// max bin hold less than (128 + 1) * 1024 = 132096
#define MAX 132095
#define MIN 0
#define KB  1024
#define MAX_BIN 127


// Classes
class MallocMetaData {
public:
    size_t size;
    bool is_free;
    // pointers to bin list
    MallocMetaData* next;
    MallocMetaData* prev;

    //TODO: pointers to prev, and next in heap sequence


    // bin index can be computed using size / KB (=1024)
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

static MallocMetaData* removeFromHistogram(MallocMetaData* to_remove) {
    int bin_index = to_remove->size / KB;

    if (to_remove == bins[bin_index].head) {
        bins[bin_index].head = to_remove->next;
        to_remove->next = nullptr;
        if (bins[bin_index].head != nullptr) {
            bins[bin_index].head->prev = nullptr;
        }
        return to_remove;
    }

    // in case is not head of bin
    to_remove->next->prev = to_remove->prev;
    if(to_remove->prev != nullptr) {
        to_remove->prev->next = to_remove->next;
    }
    to_remove->next = nullptr;
    to_remove->prev = nullptr;
    return to_remove;
}

static MallocMetaData* insertToHistogram(MallocMetaData* to_insert) {
    int bin_index = to_insert->size / KB;
    to_insert->next = nullptr;
    to_insert->prev = nullptr;

    if (bins[bin_index].head == nullptr) {
        bins[bin_index].head = to_insert;
        return to_insert;
    }
    else {
        //iterator
        MallocMetaData *temp = bins[bin_index].head;
        MallocMetaData *pre_temp = nullptr;

        // edge case 1: if new MetaData is new head
        if (bins[bin_index].head->size >= to_insert->size) {
            to_insert->next = bins[bin_index].head;
            bins[bin_index].head->prev = to_insert;
            bins[bin_index].head = to_insert;
            return to_insert;
        }
        // finding a place between lesser and greater values of size
        while (temp != nullptr) {
            pre_temp = temp;
            if (temp->size <= to_insert->size && temp->next != NULL && temp->next->size >= to_insert->size) {
                to_insert->next = temp->next;
                temp->next = to_insert;
                to_insert->prev = temp;
                to_insert->next->prev = to_insert;
                return to_insert;
            }
            temp = temp->next;
        }

        // edge case 2: if new MetaData is new tail
        to_insert->next = pre_temp->next;
        pre_temp->next = to_insert;
        to_insert->prev = pre_temp;
        return to_insert;
    }
}
void* smalloc(size_t size) {
    if((size == MIN) || (size > MAX)) {
        return NULL;
    }

    //Search for free block
    for (int bin_index = (size / KB) ; bin_index <= MAX_BIN ; bin_index++) {
        MallocMetaData* tmp = bins[bin_index].head; // iterator
        while (tmp != nullptr) {
            // allocate the first free block that fits
            if (tmp->is_free && tmp->size >= size) {
                /* challenge 1 should be used in this conditional (in case tmp->size is strictly greater than size)
                 * after splitting the block we should update the histogram
                 *
                 * */
                tmp->is_free = false;
                num_free_blocks -= 1;
                num_free_bytes -= tmp->size;
                return (void *) ((char *) tmp + sizeof(MallocMetaData)); //return the block after the meta data
            }
            tmp = tmp->next;
        }
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
    //if no bin was found (and no merging was available?)
    insertToHistogram(meta_data);
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

// ========================== Work In Progress! ==========================
void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetaData* mid_meta_data = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));
    if(mid_meta_data->is_free == false) {
        mid_meta_data->is_free = true;
        // handles static variables
        //num_free_blocks += 1;
        //num_free_bytes += meta_data->size;
    }
    // pointer was sent but was already free
    else {
        return;
    }

    // Merging free blocks, if possible
    MallocMetaData* low_meta_data = mid_meta_data->prev;
    if (low_meta_data == nullptr || !low_meta_data->is_free) {
        low_meta_data = nullptr;
    }
    MallocMetaData* high_meta_data = mid_meta_data->next;
    if (high_meta_data == nullptr || !high_meta_data->is_free) {
        high_meta_data= nullptr;
    }

    // combine 3 blocks
    if (low_meta_data != nullptr && high_meta_data != nullptr) {
        // remove each block from histogram
        removeFromHistogram(low_meta_data);
        removeFromHistogram(mid_meta_data);
        removeFromHistogram(high_meta_data);
        low_meta_data->size = low_meta_data->size + mid_meta_data->size + high_meta_data->size + (sizeof(MetaData)*2);
        //low_meta_data->next = high_meta_data->next;
        insertToHistogram(low_meta_data);
        num_free_blocks -= 1;
        num_free_bytes += mid_meta_data->size + (sizeof(MetaData)*2);
    }
    else if (low_meta_data != nullptr) {
        removeFromHistogram(low_meta_data);
        removeFromHistogram(mid_meta_data);
        low_meta_data->size = low_meta_data->size + mid_meta_data->size + sizeof(MetaData);
        //low_meta_data->next = mid_meta_data->next;
        insertToHistogram(low_meta_data);
        num_free_bytes += mid_meta_data->size + sizeof(MetaData);
    }
    else if(high_meta_data != nullptr) {
        removeFromHistogram(mid_meta_data);
        removeFromHistogram(high_meta_data);
        mid_meta_data->size = mid_meta_data->size + high_meta_data->size + sizeof(MetaData);
        mid_meta_data->next = high_meta_data->next;
        num_free_bytes += mid_meta_data->size + sizeof(MetaData);
    }
    // no merging possible
    else {
        num_free_blocks += 1;
        num_free_bytes += mid_meta_data->size;
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