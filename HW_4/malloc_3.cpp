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
    // pointers to heap sequence
    MallocMetaData* next;
    MallocMetaData* prev;

    //pointers to list
    MallocMetaData* next_in_bin;
    MallocMetaData* prev_in_bin;


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
        bins[bin_index].head = to_remove->next_in_bin;
        to_remove->next_in_bin = nullptr;
        if (bins[bin_index].head != nullptr) {
            bins[bin_index].head->prev_in_bin = nullptr;
        }
        return to_remove;
    }

    // in case is not head of bin
    to_remove->next_in_bin->prev_in_bin = to_remove->prev_in_bin;
    if(to_remove->prev_in_bin != nullptr) {
        to_remove->prev_in_bin->next_in_bin = to_remove->next_in_bin;
    }
    to_remove->next_in_bin = nullptr;
    to_remove->prev_in_bin = nullptr;
    return to_remove;
}

static MallocMetaData* insertToHistogram(MallocMetaData* to_insert) {
    int bin_index = to_insert->size / KB;
    to_insert->next_in_bin = nullptr;
    to_insert->prev_in_bin = nullptr;

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
            to_insert->next_in_bin = bins[bin_index].head;
            bins[bin_index].head->prev_in_bin = to_insert;
            bins[bin_index].head = to_insert;
            return to_insert;
        }
        // finding a place between lesser and greater values of size
        while (temp != nullptr) {
            pre_temp = temp;
            if (temp->size <= to_insert->size && temp->next_in_bin != NULL && temp->next_in_bin->size >= to_insert->size) {
                to_insert->next_in_bin = temp->next_in_bin;
                temp->next_in_bin = to_insert;
                to_insert->prev_in_bin = temp;
                to_insert->next_in_bin->prev_in_bin = to_insert;
                return to_insert;
            }
            temp = temp->next_in_bin;
        }

        // edge case 2: if new MetaData is new tail
        to_insert->next_in_bin = pre_temp->next_in_bin;
        pre_temp->next_in_bin = to_insert;
        to_insert->prev_in_bin = pre_temp;
        return to_insert;
    }
}

void splitFreeBlock(MallocMetaData* block, size_t first_block_size) {
    long new_addr = long(block)+long(first_block_size)+long(sizeof (MallocMetaData));
    void* splitted_block = (void*)(new_addr);

    //update second block data
    MallocMetaData* splitted_block_metadata = (MallocMetaData*)splitted_block;
    splitted_block_metadata->is_free = true;
    size_t new_size = block->size - first_block_size - sizeof(MallocMetaData);
    splitted_block_metadata->size = new_size;

    //update first block data
    block->size = first_block_size;
    block->is_free = false;


    insertToHistogram(splitted_block_metadata); 
    //update list
    splitted_block_metadata->next = block->next;
    splitted_block_metadata->prev = block;
    if(block->next!=nullptr) {
        block->next->prev = splitted_block_metadata;
        block->next = splitted_block_metadata;
    }
    //update Static Variables
    num_free_blocks++;
    
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

void merge_with_prev(MallocMetaData* metadata) {
    removeFromHistogram(metadata);
    //merge prev with current
    metadata->prev->size =metadata->size+ metadata->prev->size+ sizeof(MallocMetaData);
    metadata->prev->next = metadata->next;
    //update the next block
    if(metadata->next!=nullptr) {
        metadata->next->prev = metadata->prev;
    }
    //insert the new block
    insertToHistogram(metadata->prev);
    num_free_blocks--;
}
void merge_with_next(MallocMetaData* metadata) {
    //remove from histogram
    removeFromHistogram(metadata);
    metadata->size = metadata->size+metadata->next->size+ sizeof(MallocMetaData);
    if(metadata->next->next!= nullptr) {
        metadata->next->next->prev = metadata;
    }
    metadata->next = metadata->next->next;
    insertToHistogram(metadata->prev);
    num_free_blocks--;
}
void merge(MallocMetaData* metadata) {
    MallocMetaData* next = metadata->next;
    MallocMetaData* prev = metadata->prev;
    if(next!=nullptr && next->is_free) {
        merge_with_next(metadata);
    }
    if(prev!=nullptr && prev->is_free) {
        merge_with_prev(metadata);
    }

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
    if(oldp == NULL) {
        return smalloc(size);
    }
    MallocMetaData* old_metadata =(MallocMetaData*)((long)oldp-long(sizeof(MallocMetaData)));
    if(size>= 128*KB) {
        /// challenge 4
    }
    // option a
    if(old_metadata->size >= size) {
        if(old_metadata->size>= size+ sizeof(MallocMetaData)+128) {
            splitFreeBlock(old_metadata, size);
        }
        old_metadata->is_free = false;
        return oldp;
    }
    //try merging prev option b
    bool prev_is_free =old_metadata->prev!= nullptr && old_metadata->prev->is_free;
    if(prev_is_free && size<=old_metadata->size +old_metadata->prev->size+sizeof (MallocMetaData)) {
        MallocMetaData* old_prev = old_metadata->prev;
        merge_with_prev(old_metadata);
        old_metadata->prev->is_free= false;
        //Copies count characters from the object pointed to by src to the object pointed to by dest
        std::memmove(
                (void*)((long)old_metadata->prev+(long)sizeof(MallocMetaData)),
                oldp, old_metadata->size);
        if(old_prev->size >=size+sizeof(MallocMetaData)+128) {
            splitFreeBlock(old_prev, size);
        }
        return  (void*)((long)old_prev+(long)sizeof(MallocMetaData));
    }
    bool next_is_free = old_metadata->next!=nullptr && old_metadata->next->is_free;
    //try merging next option c
    if(next_is_free&& size<=old_metadata->size+old_metadata->next->size+sizeof(MallocMetaData)) {
        merge_with_next(old_metadata);
        old_metadata->is_free = false;
        if(old_metadata->size >= size+sizeof(MallocMetaData)+128) {
            splitFreeBlock(old_metadata, size);
        }
        return  (void*)((long)old_metadata+(long)sizeof(MallocMetaData));
    }
    //try merging both option d
    if(prev_is_free&& next_is_free &&
       size<= old_metadata->size+old_metadata->prev->size+
              old_metadata->next->size+ 2*sizeof (MallocMetaData))
    {
        merge_with_next(old_metadata);
        merge_with_prev(old_metadata);
        old_metadata->prev->is_free = false;
        MallocMetaData* prev_metadata = old_metadata->prev;
        std::memmove((void*)((long)(old_metadata->prev)+
                             (long)sizeof (MallocMetaData)), oldp, old_metadata->size);

        if(old_metadata->prev->size >=size+sizeof(MallocMetaData)+128) {
            splitFreeBlock(old_metadata->prev, size);
        }
        return (void*)((long)prev_metadata+
                       (long)sizeof(MallocMetaData));
    }
    //wilderness challenge 3
    if(old_metadata->next==nullptr) {
        if(sbrk(size-old_metadata->size)==(void*)-1 ) {
            return NULL;
        }
        old_metadata->is_free = false;
        old_metadata->size = size;
        return (void*)((long)old_metadata+(long)sizeof(MallocMetaData));
    }
    //smalloc
    MallocMetaData* new_metadata = (MallocMetaData*)smalloc(size);
    if(new_metadata ==nullptr) {
        return NULL;
    }
    std::memmove(new_metadata, oldp, size);
    sfree(oldp);
    return (void*)new_metadata;
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