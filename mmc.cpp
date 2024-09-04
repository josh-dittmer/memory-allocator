#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "mms.h"

static struct mem_map_table* table_ptr = NULL;

void dump_memory(bool write_to_file, const char* filename) {
    char output_buf[8192];

    sprintf(output_buf, "Virtual memory:\n");
    sprintf(output_buf + strlen(output_buf), "<Address>\t<Data>\n");

    int curr_address = 0;
    while(curr_address < table_ptr->mem_size) {
        sprintf(output_buf + strlen(output_buf), "%p\t", table_ptr->mem_start + curr_address);

        for (int i = 0; i < 32 && i < table_ptr->mem_size; i++) {
            sprintf(output_buf + strlen(output_buf), "%02x ", table_ptr->mem_start[curr_address + i]);
        }

        sprintf(output_buf + strlen(output_buf), "\n");

        curr_address += 32;
    }

    if (write_to_file) {
        FILE* output_file = fopen(filename, "w");
        if (output_file == NULL) {
            sprintf(output_buf + strlen(output_buf), "[mmc] Failed to open memory dump file!\n");
        } else {
            fprintf(output_file, "%s", output_buf);
            fclose(output_file);
        }
    }

    printf("%s", output_buf);
}

void display_mapping_table(bool write_to_file, const char* filename) {
    char output_buf[8192];

    sprintf(output_buf, "Memory mapping table:\n");

    int curr_region = 0;
    for (int i = 0; i < table_ptr->num_regions; i++) {
        if (table_ptr->region_table[i].pid == 0) {
            continue;
        }

        sprintf(output_buf + strlen(output_buf), "Region #%d\n", ++curr_region);
        sprintf(output_buf + strlen(output_buf), "\tPID: %d\n", table_ptr->region_table[i].pid);
        sprintf(output_buf + strlen(output_buf), "\tRequest size: %d\n", table_ptr->region_table[i].request_size);
        sprintf(output_buf + strlen(output_buf), "\tActual size: %d\n", table_ptr->region_table[i].actual_size);
        sprintf(output_buf + strlen(output_buf), "\tOffset: %d\n", table_ptr->region_table[i].offset);
        sprintf(output_buf + strlen(output_buf), "\tClient address: %p\n", &table_ptr->region_table[i].buf_ptr[table_ptr->region_table[i].offset]);

        char timestamp[16];
        mms_create_timestamp(timestamp, table_ptr->region_table[i].last_reference_time);

        sprintf(output_buf + strlen(output_buf), "\tLast Reference: %s\n\n", timestamp);
    }

    if (curr_region == 0) {
        sprintf(output_buf + strlen(output_buf), "\t(empty)\n");
    }

    if (write_to_file) {
        FILE* output_file = fopen(filename, "w");
        if (output_file == NULL) {
            sprintf(output_buf + strlen(output_buf), "[mmc] Failed to open memory dump file!\n");
        } else {
            fprintf(output_file, "%s", output_buf);
            fclose(output_file);
        }
    }

    printf("%s", output_buf);
}

int main(int argc, char* argv[]) {
    mms_set_program_name("mmc");

    if (argc < 3) {
        printf("Usage: mmc [physical size] [boundary size]\n");
        return -1;
    }

    int physical_size = atoi(argv[1]);
    int boundary_size = atoi(argv[2]);

    table_ptr = mmc_init(physical_size, boundary_size);
    if (table_ptr == NULL) {
        printf("[mmc] Failed to initialize mms!\n");
        return -1;
    }

    bool should_exit = false;
    while(!should_exit) {
        printf("[mmc] > ");

        char input_buf[128];
        fgets(input_buf, sizeof(input_buf), stdin);

        char command;
        char filename[128];
        size_t count = sscanf(input_buf, "%c %s", &command, filename);

        bool write_to_file = (count == 2);

        switch(command) {
            case 'd':
            case 'D':
                dump_memory(write_to_file, filename);
                break;
            case 'm':
            case 'M':
                display_mapping_table(write_to_file, filename);
                break;
            case 'e':
            case 'E':
                printf("[mmc] Exiting...\n");
                should_exit = true;
                break;
            default:
                printf("[mmc] Unknown command!\n");
                break;
        }
    }

    return 0;
}