// mms api

#include <time.h> 

// properties
#define MMS_SHM_NAME "./mms_file"
#define MMS_LOG_NAME "./mms.log"
#define DEFAULT_MEMORY_REGION_SIZE 8192
#define DEFAULT_MAX_REGIONS 256
#define DEFAULT_MAX_PAGES (DEFAULT_MEMORY_REGION_SIZE / 8)

// errors
#define OUT_OF_MEMORY 100
#define MEM_TOO_SMALL 101
#define INVALID_DEST_ADDR 102
#define INVALID_CPY_ADDR 103
#define INVALID_MEM_ADDR 104
#define NOT_INITIALIZED 105
#define CLEANUP_FAILED 106
#define BAD_SIZE 107

struct mem_map_table_entry {
    int pid;

    int request_size;
    int actual_size;

    int offset;
    char* buf_ptr;

    time_t last_reference_time;
};

struct mem_map_table {
    int mem_size;
    int curr_mem_size;
    int min_block_size;

    int num_regions;
    struct mem_map_table_entry region_table[DEFAULT_MAX_REGIONS];

    char mem_start[DEFAULT_MEMORY_REGION_SIZE]; // actual memory region
};

extern void mms_create_timestamp(char buf[16], time_t timer);

extern void mms_set_program_name(const char* name);

extern struct mem_map_table* mmc_init(int physical_size, int boundary_size);

extern char* mms_init();

extern int mms_cleanup();

extern char* mms_malloc(int size, int* error_code);

extern int mms_memset(char* dest_ptr, char c, int size);

extern int mms_memcpy(char* dest_pr, char* src_ptr, int size);

extern int mms_print(char* src_ptr, int size);

extern int mms_free(char* mem_ptr);