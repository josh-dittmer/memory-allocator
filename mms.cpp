#include "mms.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

static struct mem_map_table* table_ptr = NULL;
static const char* program_name = "";

void mms_set_program_name(const char* name) {
    program_name = name;
}

void mms_create_timestamp(char buf[16], time_t timer) {
    struct tm* tm_info;
    tm_info = localtime(&timer);

    strftime(buf, 16, "%Y%m%d%H%M%S", tm_info);
}

void mms_current_timestamp(char buf[16]) {
    time_t timer;
    timer = time(NULL);

    mms_create_timestamp(buf, timer);
}

void mms_log(const char* function_name, const char* return_value, const char* params_str) {
    FILE* log_file = fopen(MMS_LOG_NAME, "a");
    if (log_file == NULL) {
        printf("[mms] Failed to open log file!\n");
        return;
    }

    char timestamp_buf[16];
    mms_current_timestamp(timestamp_buf);

    fprintf(log_file, "%s %s %d %s %s %s\n", timestamp_buf, program_name, getpid(), function_name, return_value, params_str);

    fclose(log_file);
}

void mms_log_i(const char* function_name, int return_value, const char* params_str) {
    char return_value_str[64];
    sprintf(return_value_str, "%d", return_value);
    mms_log(function_name, return_value_str, params_str);
}

void mms_log_p(const char* function_name, char* return_value, const char* params_str) {
    char return_value_str[64];
    sprintf(return_value_str, "%p", return_value);
    mms_log(function_name, return_value_str, params_str);
}

struct mem_map_table* mms_get_table_ptr() {
    key_t key = ftok(MMS_SHM_NAME, 1);
    if (key == -1) {
        if (errno == ENOENT) {
            int fd = creat(MMS_SHM_NAME, 0666);
            if (fd == -1) {
                printf("[mms] Failed to create shared memory file!\n");
                return NULL;
            }

            return mms_get_table_ptr();
        }

        printf("[mms] Failed to get shared memory file key\n");
        return NULL;
    }

    int id = shmget(key, sizeof(struct mem_map_table), 0644 | IPC_CREAT);
    if (id == -1) {
        printf("[mms] Failed to get shared memory id\n");
        return NULL;
    }

    struct mem_map_table* table_ptr = (struct mem_map_table*)shmat(id, NULL, 0);
    if (table_ptr == (void*)-1) {
        printf("[mms] Failed to attach to shared memory file\n");
        return NULL;
    }

    return table_ptr;
}

struct mem_map_table* mmc_init(int physical_size, int boundary_size) {
    if (physical_size < 0 || boundary_size < 0) {
        printf("[mms] Boundary size and physical size must be postive integeres\n");
        return NULL;
    }

    if (physical_size > DEFAULT_MEMORY_REGION_SIZE) {
        printf("[mms] Physical size cannot exceed %d bytes\n", DEFAULT_MEMORY_REGION_SIZE);
        return NULL;
    }

    if (boundary_size > physical_size) {
        printf("[mms] Boundary size cannot exceed physical size\n");
        return NULL;
    }

    if (boundary_size % 8 != 0) {
        printf("[mms] Boundary size must be divisible by 8\n");
        return NULL;
    }
    
    struct mem_map_table* new_table_ptr = mms_get_table_ptr();
    if (new_table_ptr == NULL) {
        printf("[mms] First-time init failed\n");
        return NULL;
    }
    
    new_table_ptr->mem_size = physical_size;
    new_table_ptr->curr_mem_size = 0;
    new_table_ptr->min_block_size = boundary_size;
    new_table_ptr->num_regions = 0;

    table_ptr = new_table_ptr;

    // set all memory to 0
    for (int i = 0; i < new_table_ptr->mem_size; i++) {
        table_ptr->mem_start[i] = 0x00;
    }


    printf("[mms] Successfully initialized!\n");

    return table_ptr;
}

char* mms_init() {
    table_ptr = mms_get_table_ptr();
    if (table_ptr == NULL) {
        printf("[mms] Init failed\n");
        return NULL;
    }

    return (char*)table_ptr;
}

int mms_cleanup() {
    if (shmdt(table_ptr) != 0) {
        printf("[mms] Failed to detach from shared memory!\n");
        return CLEANUP_FAILED;
    }

    return 0;
}

char* mms_malloc_from_free_list(int size) {
    struct mem_map_table_entry* smallest_usable_region = NULL;

    for (int i = 0; i < table_ptr->num_regions; i++) {
        struct mem_map_table_entry* curr_region = &table_ptr->region_table[i];

        if (curr_region->pid != 0) {
            continue;
        }

        if (smallest_usable_region != NULL) {
            if (curr_region->actual_size < smallest_usable_region->actual_size && curr_region->actual_size >= size) {
                smallest_usable_region = curr_region;
            }
        } else if (curr_region->actual_size >= size) {
            smallest_usable_region = curr_region;
        }
    }

    if (smallest_usable_region == NULL) {
        return NULL;
    }

    smallest_usable_region->pid = getpid();
    smallest_usable_region->request_size = size;
    smallest_usable_region->buf_ptr = table_ptr->mem_start;

    return smallest_usable_region->buf_ptr + smallest_usable_region->offset;
}

char* mms_malloc(int size, int* error_code) {
    char params_str[128];
    sprintf(params_str, "%d %p", size, error_code);

    if (table_ptr == NULL) {
        printf("[mms] Not initialized!\n");
        *error_code = NOT_INITIALIZED;
        mms_log_i("mms_malloc", 0, params_str);
        return NULL;
    }

    if (size <= 0) {
        *error_code = BAD_SIZE;
        mms_log_i("mms_malloc", 0, params_str);
        return NULL;
    }

    int actual_size;

    if (size % table_ptr->min_block_size != 0) {
        actual_size = size + table_ptr->min_block_size - (size % table_ptr->min_block_size);
    } else {
        actual_size = size;
    }

    int new_size = table_ptr->curr_mem_size + actual_size;
    if (new_size > table_ptr->mem_size) {
        char* free_addr = mms_malloc_from_free_list(actual_size);
        if (free_addr == NULL) {
            printf("[mms] Not enough memory to allocate new %d byte block!\n", actual_size);
            *error_code = OUT_OF_MEMORY;
            
            mms_log_i("mms_malloc", 0, params_str);
            return NULL;
        }

        return free_addr;
    }

    if (table_ptr->num_regions >= DEFAULT_MAX_REGIONS) {
        printf("[mms] Cannot allocate more than %d memory regions\n", DEFAULT_MAX_REGIONS);
        
        mms_log_i("mms_malloc", 0, params_str);
        return NULL;
    }

    struct mem_map_table_entry* region_ptr = &table_ptr->region_table[table_ptr->num_regions];

    region_ptr->pid = getpid();
    region_ptr->request_size = size;
    region_ptr->actual_size = actual_size;
    region_ptr->offset = table_ptr->curr_mem_size;
    region_ptr->buf_ptr = table_ptr->mem_start;

    table_ptr->curr_mem_size += actual_size;
    table_ptr->num_regions++;

    mms_log_p("mms_malloc", region_ptr->buf_ptr + region_ptr->offset, params_str);
    return region_ptr->buf_ptr + region_ptr->offset;
}

struct mem_map_table_entry* mms_get_region(char* ptr) {
    int curr_size = 0;
    for (int i = 0; i < table_ptr->num_regions; i++) {
        struct mem_map_table_entry* curr_region = &table_ptr->region_table[i];
        curr_size += curr_region->actual_size;

        // this means the given address doesn't belong to the region
        // because the buffer base addresses differ, meaning the region
        // was likely created by a different program
        if (curr_region->buf_ptr != table_ptr->mem_start) {
            continue;
        }

        if ((long)ptr - (long)curr_region->buf_ptr < curr_size) {
            if (curr_region->pid != getpid()) {
                printf("[mms] Address %p does not belong to this process\n", ptr);
                return NULL;
            }

            return &table_ptr->region_table[i];
        }
    }

    printf("[mms] Address %p is unallocated\n", ptr);
    return NULL;
}

int mms_check_inbounds(char* ptr, struct mem_map_table_entry* region_ptr, int size) {
    long ptr_offset = (long)ptr - ((long)region_ptr->buf_ptr + region_ptr->offset);

    if (region_ptr->offset + size + ptr_offset > region_ptr->offset + region_ptr->actual_size) {
        printf("[mms] Region is too small for %d bytes starting at address %p\n", size, (region_ptr->buf_ptr + region_ptr->offset));
        return -1;
    }

    return 0;
}

int mms_memset(char* dest_ptr, char c, int size) {
    char params_str[128];
    sprintf(params_str, "%p %c %d", dest_ptr, c, size);

    if (table_ptr == NULL) {
        printf("[mms] Not initialized!\n");

        mms_log_i("mms_memset", NOT_INITIALIZED, params_str);
        return NOT_INITIALIZED;
    }

    if (size <= 0) {
        mms_log_i("mms_memset", BAD_SIZE, params_str);
        return BAD_SIZE;
    }

    struct mem_map_table_entry* region_ptr = mms_get_region(dest_ptr);
    if (region_ptr == NULL) {
        mms_log_i("mms_memset", INVALID_DEST_ADDR, params_str);
        return INVALID_DEST_ADDR;
    }

    if (mms_check_inbounds(dest_ptr, region_ptr, size) < 0) {
        mms_log_i("mms_memset", MEM_TOO_SMALL, params_str);
        return MEM_TOO_SMALL;
    }

    for (int i = 0; i < size; i++) {
        dest_ptr[i] = c;
    }

    region_ptr->last_reference_time = time(NULL);

    mms_log_i("mms_memset", 0, params_str);
    return 0;
}

int mms_memcpy(char* dest_ptr, char* src_ptr, int size) {
    char params_str[128];
    sprintf(params_str, "%p %p %d", dest_ptr, src_ptr, size);

    if (table_ptr == NULL) {
        printf("[mms] Not initialized!\n");

        mms_log_i("mms_memcpy", NOT_INITIALIZED, params_str);
        return NOT_INITIALIZED;
    }

    if (size <= 0) {
        mms_log_i("mms_memcpy", BAD_SIZE, params_str);
        return BAD_SIZE;
    }

    bool using_extern_src_ptr = false;

    struct mem_map_table_entry* region_ptr_src = mms_get_region(src_ptr);
    if (region_ptr_src == NULL) {
       using_extern_src_ptr = true;
    } else {
        if (mms_check_inbounds(src_ptr, region_ptr_src, size) < 0) {
            mms_log_i("mms_memcpy", MEM_TOO_SMALL, params_str);
            return MEM_TOO_SMALL;
        }
    }

    bool using_extern_dest_ptr = false;

    struct mem_map_table_entry* region_ptr_dest = mms_get_region(dest_ptr);
    if (region_ptr_dest == NULL) {
       using_extern_dest_ptr = true;
    } else {
        if (mms_check_inbounds(dest_ptr, region_ptr_dest, size) < 0) {
            mms_log_i("mms_memcpy", MEM_TOO_SMALL, params_str);
            return MEM_TOO_SMALL;
        }
    }

    // allow internal -> internal, external -> internal, internal -> external copying, but not external -> external
    // as per the instructions given in class
    if (using_extern_src_ptr && using_extern_dest_ptr) {
        mms_log_i("mms_memcpy", INVALID_CPY_ADDR, params_str);
        return INVALID_CPY_ADDR;
    }

    /*struct mem_map_table_entry* region_ptr_dest = mms_get_region(dest_ptr);
    if (region_ptr_dest == NULL) {
        mms_log_i("mms_memcpy", INVALID_CPY_ADDR, params_str);
        return INVALID_CPY_ADDR;
    }

    if (mms_check_inbounds(dest_ptr, region_ptr_dest, size) < 0) {
        mms_log_i("mms_memcpy", MEM_TOO_SMALL, params_str);
        return MEM_TOO_SMALL;
    }*/

    for (int i = 0; i < size; i++) {
        dest_ptr[i] = src_ptr[i];
    }

    region_ptr_dest->last_reference_time = time(NULL);

    mms_log_i("mms_memcpy", 0, params_str);
    return 0;
}

int mms_print(char* src_ptr, int size) {
    char params_str[128];
    sprintf(params_str, "%p %d", src_ptr, size);

    if (table_ptr == NULL) {
        printf("[mms] Not initialized!\n");

        mms_log_i("mms_print", NOT_INITIALIZED, params_str);
        return NOT_INITIALIZED;
    }

    if (size <= 0) {
        mms_log_i("mms_print", BAD_SIZE, params_str);
        return BAD_SIZE;
    }

    bool using_extern_ptr = false;

    struct mem_map_table_entry* region_ptr = mms_get_region(src_ptr);
    if (region_ptr == NULL) {
        using_extern_ptr = true;
    } else {
        if (mms_check_inbounds(src_ptr, region_ptr, size) < 0) {
            mms_log_i("mms_print", MEM_TOO_SMALL, params_str);
            return MEM_TOO_SMALL;
        }
    }

    for (int i = 0; i < size; i++) {
        if (src_ptr[i] == '\0') {
            break;
        }

        printf("%c", src_ptr[i]);
    }

    mms_log_i("mms_print", 0, params_str);
    return 0;
}

int mms_free(char* mem_ptr) {
    char params_str[128];
    sprintf(params_str, "%p", mem_ptr);

    if (table_ptr == NULL) {
        printf("[mms] Not initialized!\n");

        mms_log_i("mms_free", NOT_INITIALIZED, params_str);
        return NOT_INITIALIZED;
    }

    struct mem_map_table_entry* region_ptr = mms_get_region(mem_ptr);
    if (region_ptr == NULL) {
        mms_log_i("mms_free", INVALID_MEM_ADDR, params_str);
        return INVALID_MEM_ADDR;
    }

    // marks memory as freed
    region_ptr->pid = 0;
    region_ptr->request_size = 0;

    mms_log_i("mms_free", 0, params_str);
    return 0;
}