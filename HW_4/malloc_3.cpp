#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

#define MAX 100000000
#define MAX_FOR_BINS 131071
#define MIN 0
#define KB  1024
#define MAX_BIN 127
#define MIN_MEM_AFTER_SPLIT 128


// Classes
class MallocMetaData {
public:
    size_t size;
    bool is_free;

    // pointers to heap sequence | mmap list
    MallocMetaData* next;
    MallocMetaData* prev;
    /* pointers to list in bins
     * bin index can be computed using size / KB (=1024)
     * */
    MallocMetaData* next_in_bin;
    MallocMetaData* prev_in_bin;
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
static MallocMetaData* mmap_head = nullptr;
static MallocMetaData* heap_head = nullptr;
static size_t num_free_blocks = 0;
static size_t num_free_bytes = 0;
static size_t num_allocated_blocks = 0;
static size_t num_allocated_bytes = 0;


// Functions
static MallocMetaData* removeFromHistogram(MallocMetaData* to_remove) {
    if (to_remove == nullptr || to_remove->is_free == false)
        return NULL;

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
    if (to_remove->next_in_bin != nullptr) {
        to_remove->next_in_bin->prev_in_bin = to_remove->prev_in_bin;
    }
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

static void removeFromMmapList(MallocMetaData* to_remove) {
    if(to_remove == nullptr) return;

    if (mmap_head == to_remove) {
        mmap_head = to_remove->next;
        if(to_remove->next != nullptr)
            to_remove->next->prev = nullptr;
    }
    else {
        to_remove->prev->next = to_remove->next;
        if (to_remove->next != nullptr) {
            to_remove->next->prev = to_remove->prev;
        }
    }
    to_remove->prev = nullptr;
    to_remove->next = nullptr;
}

static void insertToMmapList(MallocMetaData* to_insert) {
    if(to_insert == nullptr) return;

    if (mmap_head == nullptr) {
        mmap_head = to_insert;
        to_insert->prev = nullptr;
        to_insert->next = nullptr;
    }
    else {
        to_insert->next = mmap_head;
        mmap_head->prev = to_insert;
        mmap_head = to_insert;
    }
}

static void insertToHeap(MallocMetaData* to_insert) {
    //if heap was empty
    if(heap_head == nullptr) {
        heap_head = to_insert;
        if(heap_head->is_free) {
            insertToHistogram(to_insert);
        }
        return;
    }

    MallocMetaData* temp = heap_head; //iterator
    while(temp->next!= nullptr) {
        temp = temp->next;
    } // find last in the list
    temp->next = to_insert;
    to_insert->prev = temp;
    if(to_insert->is_free) {
        insertToHistogram(to_insert);
    }
}

void splitFreeBlock(MallocMetaData* block, size_t first_block_size) {
    removeFromHistogram(block);
    if(block != nullptr && block->is_free) num_free_bytes -= block->size; //first block allocated
    else num_free_blocks++; // in case we came from realloc
    num_free_bytes -= block->size; //first block allocated

    long new_addr = long(block) + long(sizeof(MallocMetaData)) + long(first_block_size);
    void* splitted_block = (void*)(new_addr);

    //update second block data
    MallocMetaData* splitted_block_metadata = (MallocMetaData*)splitted_block;
    splitted_block_metadata->is_free = true;
    size_t new_size = block->size - first_block_size - sizeof(MallocMetaData);
    splitted_block_metadata->size = new_size;

    //update first block data
    block->size = first_block_size;
    block->is_free = false;

    // update histogram
    insertToHistogram(splitted_block_metadata);

    //update list in heap
    splitted_block_metadata->next = block->next;
    splitted_block_metadata->prev = block;
    if(block->next!=nullptr) {
        block->next->prev = splitted_block_metadata;
    }
    block->next = splitted_block_metadata;

    //update Static Variables
    num_allocated_blocks++;
    num_allocated_bytes -= sizeof(MallocMetaData);
    num_free_bytes += splitted_block_metadata->size;

}

static void updateNewAllocatedMetaData(MallocMetaData* meta_data, size_t size) {
    meta_data->size = size;
    meta_data->is_free = false;
    meta_data->next = nullptr;
    meta_data->prev = nullptr;
    meta_data->next_in_bin = nullptr;
    meta_data->prev_in_bin = nullptr;
    // handles static variables
    num_allocated_blocks += 1;
    num_allocated_bytes += size;
}

static void* tryToEnlargeTopHeapChunk(MallocMetaData* tmp, size_t size) {
    if(tmp == nullptr)
        return NULL;

    while (tmp->next != nullptr) tmp = tmp->next;
    // if top chunk is free, but not big enough at the moment
    if (tmp->is_free) {
        void* result = sbrk(size - tmp->size);
        if (result == (void *) (-1)) {
            return NULL;
        }
        num_free_bytes -= tmp->size;
        num_allocated_bytes += size - tmp->size;
        removeFromHistogram(tmp);

        tmp->size = size;
        tmp->is_free = false;
        num_free_blocks--;
        return (void *) ((char *) tmp + sizeof(MallocMetaData));
    }
    return NULL;
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
            if (tmp->is_free && (tmp->size >= size)) {
                /* challenge 1 should be used in this conditional (in case tmp->size is strictly greater than size)
                 * after splitting the block we should update the histogram
                 *
                 * */
                if (tmp->size - size - sizeof(MallocMetaData) >= MIN_MEM_AFTER_SPLIT) {
                    splitFreeBlock(tmp, size);
                }
                else {
                    tmp->is_free = false;
                    num_free_blocks -= 1;
                    num_free_bytes -= tmp->size;
                }
                return (void *) ((char *) tmp + sizeof(MallocMetaData));
            }
            tmp = tmp->next;
        }
    }
    // Wilderness - find top chunk, check if free
    MallocMetaData* tmp = heap_head;
    void* enlarge_attempt = tryToEnlargeTopHeapChunk(tmp, size);
    if(enlarge_attempt != NULL) return enlarge_attempt;

    //No free blocks in desired bin
    void* result = nullptr;
    if (size <= MAX_FOR_BINS) {
        result = sbrk(size + sizeof(MallocMetaData));
        if (result == (void *) (-1)) {
            return NULL;
        }
    }
    // in case we have allocation of 128kb or more
    else {
        result = mmap(NULL, size+sizeof(MallocMetaData), PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        if (result == (void *) (-1)) {
            return NULL;
        }
    }

    MallocMetaData *meta_data = (MallocMetaData *) result;
    updateNewAllocatedMetaData(meta_data, size);

    // or insert to heap / mmap list
    if (size <= MAX_FOR_BINS) {
        insertToHeap(meta_data);
    }
    else {
        insertToMmapList(meta_data);
    }
    return (void *) ((char *) meta_data + sizeof(MallocMetaData));
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

    //merge prev with current
    num_free_bytes -= metadata->prev->size;
    num_free_blocks--;
    num_allocated_bytes += sizeof(MallocMetaData);
    num_allocated_blocks--;

    size_t meta_data_size = metadata->size;
    MallocMetaData* metadata_next = metadata->next;
    MallocMetaData* metadata_prev = metadata->prev;
    std::memmove(
            (void*)((long)metadata_prev+(long)sizeof(MallocMetaData)),
            (void*)((long)metadata+(long)sizeof(MallocMetaData)), metadata->size);

    metadata_prev->size = meta_data_size + metadata_prev->size + sizeof(MallocMetaData);
    metadata_prev->next = metadata_next;
    metadata_prev->is_free = false;
    removeFromHistogram(metadata_prev);

    //update the next block
    if(metadata_next!=nullptr) {
        metadata_next->prev = metadata_prev;
    }

}
void merge_with_next(MallocMetaData* metadata) {

    num_free_bytes -= metadata->next->size;
    num_free_blocks--;
    num_allocated_bytes += sizeof(MallocMetaData);
    num_allocated_blocks--;

    metadata->size = metadata->size+metadata->next->size+ sizeof(MallocMetaData);
    if(metadata->next->next != nullptr) {
        metadata->next->next->prev = metadata;
    }

    //remove from histogram
    metadata->next->is_free = false;
    removeFromHistogram(metadata->next);

    metadata->next = metadata->next->next;
    insertToHistogram(metadata->prev);
}
void merge(MallocMetaData* metadata) {
    MallocMetaData* next = metadata->next;
    MallocMetaData* prev = metadata->prev;
    if(next!=nullptr && next->is_free) {
        merge_with_next(metadata); //update  static
    }
    if(prev!=nullptr && prev->is_free) {
        merge_with_prev(metadata); //update static var
    }
    
}

// Challenge 2
void mergeFreeBlocks(MallocMetaData* mid_meta_data) {
    // Merging free blocks, if possible
    MallocMetaData* low_meta_data = mid_meta_data->prev;
    if (low_meta_data == nullptr || !low_meta_data->is_free) {
        low_meta_data = nullptr;
    }
    MallocMetaData* high_meta_data = mid_meta_data->next;
    if (high_meta_data == nullptr || !high_meta_data->is_free) {
        high_meta_data= nullptr;
    }

    // merge 3 blocks
    if (low_meta_data != nullptr && high_meta_data != nullptr) {
        // Update Stats
        num_free_blocks -= 1;
        num_free_bytes += mid_meta_data->size + (sizeof(MallocMetaData)*2);
        num_allocated_bytes += (2 * sizeof(MallocMetaData));
        num_allocated_blocks -= 2;

        // remove each block from histogram
        removeFromHistogram(low_meta_data);
        removeFromHistogram(mid_meta_data);
        removeFromHistogram(high_meta_data);
        low_meta_data->size = low_meta_data->size + mid_meta_data->size + high_meta_data->size + (sizeof(MallocMetaData)*2);
        low_meta_data->next = high_meta_data->next;

        if (high_meta_data->next != nullptr)
            high_meta_data->next->prev = low_meta_data;

        // making old metadata un-reachable
        std::memset(mid_meta_data, 0, sizeof(MallocMetaData) + mid_meta_data->size);
        std::memset(high_meta_data, 0, sizeof(MallocMetaData) + high_meta_data->size);

        insertToHistogram(low_meta_data);

    }
    // merge lower and current
    else if (low_meta_data != nullptr) {
        // Update Stats
        num_free_bytes += mid_meta_data->size + sizeof(MallocMetaData);
        num_allocated_bytes += sizeof(MallocMetaData);
        num_allocated_blocks--;

        removeFromHistogram(low_meta_data);
        removeFromHistogram(mid_meta_data);
        low_meta_data->size = low_meta_data->size + mid_meta_data->size + sizeof(MallocMetaData);
        low_meta_data->next = mid_meta_data->next;

        if (mid_meta_data->next != nullptr)
            mid_meta_data->next->prev = low_meta_data;

        // making old metadata un-reachable
        std::memset(mid_meta_data, 0, sizeof(MallocMetaData) + mid_meta_data->size);

        insertToHistogram(low_meta_data);

    }
    // merge higher and current
    else if(high_meta_data != nullptr) {
        // Update Stats
        num_free_bytes += mid_meta_data->size + sizeof(MallocMetaData);
        num_allocated_bytes += sizeof(MallocMetaData);
        num_allocated_blocks--;

        removeFromHistogram(mid_meta_data);
        removeFromHistogram(high_meta_data);
        mid_meta_data->size = mid_meta_data->size + high_meta_data->size + sizeof(MallocMetaData);
        mid_meta_data->next = high_meta_data->next;

        if (high_meta_data->next != nullptr)
            high_meta_data->next->prev = mid_meta_data;

        // making old metadata un-reachable
        std::memset(high_meta_data, 0, sizeof(MallocMetaData) + high_meta_data->size);

        insertToHistogram(mid_meta_data);

    }
        // no merging possible
    else {
        num_free_blocks += 1;
        num_free_bytes += mid_meta_data->size;
    }
}

void sfree(void* p) {
    if (p == nullptr) {
        return;
    }
    MallocMetaData* mid_meta_data = (MallocMetaData*)((char*)p - sizeof(MallocMetaData));
    // if block is in histogram
    if (mid_meta_data->size <= MAX_FOR_BINS) {
        if(mid_meta_data->is_free == false){
            mid_meta_data->is_free = true;
        }
        // pointer sent was already free
        else {
            return;
        }
        mergeFreeBlocks(mid_meta_data);
    }
    // if block is from mmap list
    else {
        removeFromMmapList(mid_meta_data);
        munmap(mid_meta_data, mid_meta_data->size + sizeof(MallocMetaData));
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
    if(size > MAX_FOR_BINS) {
        if(old_metadata->size < size) {
            void* new_mapped_area = smalloc(size);
            if (new_mapped_area != NULL){
                std::memcpy(new_mapped_area, oldp, old_metadata->size);
                sfree(oldp);
            }
            return new_mapped_area;
        }
        else {
            return oldp;
        }
    }
    // option a
    if(old_metadata->size >= size) {
        if(old_metadata->size >= size + sizeof(MallocMetaData) + MIN_MEM_AFTER_SPLIT) {
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
        if(old_prev->size >=size+sizeof(MallocMetaData)+MIN_MEM_AFTER_SPLIT) {
            splitFreeBlock(old_prev, size);
        }
        return  (void*)((long)old_prev+(long)sizeof(MallocMetaData));
    }
    bool next_is_free = old_metadata->next!=nullptr && old_metadata->next->is_free;
    //try merging next option c
    if(next_is_free&& size<=old_metadata->size+old_metadata->next->size+sizeof(MallocMetaData)) {
        merge_with_next(old_metadata);
        if(old_metadata->size >= size+sizeof(MallocMetaData)+MIN_MEM_AFTER_SPLIT) {
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
        MallocMetaData* prev_metadata = old_metadata->prev;
        if(old_metadata->prev->size >=size+sizeof(MallocMetaData)+MIN_MEM_AFTER_SPLIT) {
            splitFreeBlock(old_metadata->prev, size);
        }
        return (void*)((long)prev_metadata+
                       (long)sizeof(MallocMetaData));
    }
    //wilderness challenge 3
    if(old_metadata->next==nullptr) { // last in the heap
        if(sbrk(size-old_metadata->size)==(void*)-1 ) {
            return NULL;
        }
        num_allocated_bytes += (size - old_metadata->size);
        if (old_metadata->is_free) {
            num_free_blocks--;
            num_free_bytes -= old_metadata->size;
        }
        old_metadata->is_free = false;
        old_metadata->size = size;
        return (void*)((long)old_metadata+(long)sizeof(MallocMetaData));
    }
    //smalloc option e & f 
    MallocMetaData* new_metadata = (MallocMetaData*)smalloc(size);
    if(new_metadata ==nullptr) {
        return NULL;
    }
    std::memmove(new_metadata, oldp, size);
    sfree(oldp);
    return (void*)new_metadata;
}

// Stats Functions
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

// size_t _size_meta_data() {
//     return sizeof(MallocMetaData);
// }



size_t _num_free_blocks() {
    size_t counter = 0;
    MallocMetaData* tmp = heap_head; //head of the heap list
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
    MallocMetaData* tmp = heap_head;
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
    MallocMetaData* tmp = heap_head;
    while(tmp != nullptr) {
        counter++;
        tmp = tmp->next;
    }
    MallocMetaData* mmap_tmp = mmap_head;
    while(mmap_tmp!=nullptr)
    {
        counter++;
        mmap_tmp = mmap_tmp->next;
    }
    return counter;
}

size_t _num_allocated_bytes() {
    size_t sum = 0;
    MallocMetaData* tmp = heap_head;
    while(tmp != nullptr) {
        sum+=tmp->size;
        tmp = tmp->next;
    }
    MallocMetaData* mmap_tmp = mmap_head;
    while(mmap_tmp != nullptr) {
        sum+=mmap_tmp->size;
        mmap_tmp = mmap_tmp->next;
    }
    return sum;
}

size_t _size_meta_data() {
    return sizeof(MallocMetaData);
}

size_t _num_meta_data_bytes() {
    return _size_meta_data() * _num_allocated_blocks();
}