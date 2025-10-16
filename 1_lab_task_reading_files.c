#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "./libcrc/include/checksum.h"

// ####################################################
// ############## CONSTANTS DECLARATION ###############
// ####################################################

enum Strategy
{
    READ_SEQ,
    READ_RAND,
    MMAP_SEQ,
    MMAP_RAND
} Strategy;

const size_t KB_1 = 1000;
const size_t MB_1 = 1000 * KB_1;

const size_t BLOCK_SIZE = MB_1;    
const size_t READ_COUNT_SIZE = 2; 

#define ERROR -1
#define SUCCESS 0

// ####################################################
// ############ FUNCTIONS PRE-DECLARATIONS ############
// ####################################################

void parse_cmdl_args(int argc,
                     char *argv[],
                     const char **file_path,
                     enum Strategy *strategy);

void write_to_file(const unsigned char *buff, size_t buf_len, const char *filename);

int read_sequential(int file_descr, unsigned char *buff, size_t file_size);

uint64_t calc_crc64_from_stream(
    const unsigned char *buff,
    size_t buf_len,
    uint64_t crc_val
);

// ####################################################
// ################# IMPLEMENTATION ###################
// ####################################################

int main(int argc, char *argv[])
{
    const char *file_path = NULL;
    enum Strategy reading_strategy;

    parse_cmdl_args(argc, argv, &file_path, &reading_strategy);

    // opening file
    int file_descr = open(file_path, O_RDONLY);
    struct stat file_metadata;

    if (file_descr == -1)
    {
        printf("Provided file does not exist");
        exit(EXIT_FAILURE);
    }

    // getting file metadata so that we know file size
    if (fstat(file_descr, &file_metadata) == -1)
    {
        printf("fstat returned error\n");
        exit(EXIT_FAILURE);
    }

    unsigned char *buff = (unsigned char *)calloc(BLOCK_SIZE, sizeof(unsigned char));
    size_t file_size = file_metadata.st_size;
    ssize_t bytes_read = 0;
    size_t bytes_sum = 0;

    printf("###############################\n");
    printf("opened file: %s, file size: %lu\n", file_path, file_size);
    printf("###############################\n");

    // start measuring time
    switch (reading_strategy)
    {
    case READ_SEQ:
        read_sequential(file_descr, buff, file_size);
        break;
    case READ_RAND:
        bytes_read = 0;
        bytes_sum = 0;

        // to be able to use fseek we must have FILE*, so we create it from our file descriptor
        FILE *f_stream = fdopen(file_descr, "r");
        if (!f_stream)
        {
            printf("error when making FILE* from file descriptor\n");
            exit(EXIT_FAILURE);
        }

        size_t nbr_bytes_to_read = READ_COUNT_SIZE;
        bool read_front = true;
        size_t bytes_read_front = 0;
        // we start reading from front, so if file_size < READ_COUNT_SIZE we will just read all
        // data from front, so even though file_size - READ_COUNT_SIZE < 0 we will not use it
        size_t bytes_read_end = file_size > READ_COUNT_SIZE ? file_size - READ_COUNT_SIZE : file_size; // or should be file_size - 1

        do
        {
            if (read_front)
            {
                if (fseek(f_stream, bytes_read_front, SEEK_SET) == -1)
                {
                    printf("fseek front encountered error\n");
                    // TODO: add flag and break that checks if success or not
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (fseek(f_stream, bytes_read_end, SEEK_SET) == -1)
                {
                    printf("fseek end encountered error\n");
                    // TODO: add flag and break that checks if success or not
                    exit(EXIT_FAILURE);
                }
            }

            bytes_read = fread(buff + bytes_sum, sizeof(unsigned char), nbr_bytes_to_read, f_stream);

            bytes_sum += bytes_read;

            if (read_front)
            {
                printf("reading front\n");
                bytes_read_front += bytes_read;
                
            }
            else 
            {
                printf("reading end\n");
                bytes_read_end -= bytes_read;
            }
            read_front = !read_front;

            printf("bytes_read: %lu\n", bytes_read);

            if (file_size - bytes_sum < READ_COUNT_SIZE)
            {
                printf("available data: %lu is less than read count: %lu\n", (file_size - bytes_sum), READ_COUNT_SIZE);
                nbr_bytes_to_read = file_size - bytes_sum;
                read_front = true;
            }

            if (bytes_sum == file_size)
            {
                printf("bytes_sum == file_size == %lu\n", file_size);
                printf("bytes_read: %lu\n", bytes_read);
                printf("read_front: %d \n", read_front);
                break;
            }
        } while (bytes_read != 0 && bytes_read != -1);

        write_to_file(buff, bytes_sum, "./read_rand_output");
        uint64_t crc_val = crc_64_ecma(buff, bytes_sum);
        // 8897370153309143817
        // 407967422317943898
        // 6609718696175242621 <-- read_seq

        printf("CRC64 for Reading Rand: %lu\n", crc_val);
        break;

    case MMAP_SEQ:
        printf("MMAP_SEQ strategy\n");
        break;

    case MMAP_RAND:
        printf("MMAP_RAND strategy\n");
        break;
    default:
        printf("Switch got unsupported reading strategy\n");
        exit(EXIT_FAILURE);
    }

    close(file_descr);
    free(buff);
    return 0;
}

int read_sequential(int file_descr, unsigned char *buff, size_t file_size)
{
    ssize_t bytes_read = 0;
    size_t bytes_sum = 0;
    uint64_t crc_val;

    if (file_size <= BLOCK_SIZE)
    {
        // if file_size <= BLOCK_SIZE we can store whole file's data inside our
        // buffer and just use crc_64_ecma to calculate crc64
        do
        {
            bytes_read = read(file_descr, buff + bytes_sum, READ_COUNT_SIZE);
            bytes_sum += bytes_read;
        } while (bytes_read != 0 && bytes_read != -1);

        crc_val = crc_64_ecma(buff, bytes_sum);
    }
    else
    {
        // If buffer is to small we read data till we have space, calculate part
        // of crc using update_crc64, then we repeat this process
        crc_val = CRC_START_64_ECMA;
        size_t available_buff_space = BLOCK_SIZE;

        do
        {
            if (available_buff_space < READ_COUNT_SIZE)
                bytes_read = read(file_descr, buff + bytes_sum,available_buff_space);
            else
                bytes_read = read(file_descr, buff + bytes_sum, READ_COUNT_SIZE);
            
            bytes_sum += bytes_read;
            available_buff_space -= bytes_read;

            if (available_buff_space == 0)
            {
                // if we ran out of space in buffer we calculate partial crc64
                available_buff_space = BLOCK_SIZE;
                bytes_sum = 0;
                crc_val = calc_crc64_from_stream(buff, BLOCK_SIZE, crc_val);
            }
            else if (bytes_read == 0)
            {
                // If bytes_sum == 0, calc_crc64_from_stream won't do anything.
                // Both bytes_sum and bytes_read may be 0, when file size is a 
                // multiple of buff size
                crc_val = calc_crc64_from_stream(buff, bytes_sum, crc_val);
            }

        } while (bytes_read != 0 && bytes_read != -1);
    }

    if (bytes_read == -1)
    {
        printf("Encountered error while reading file using READ_SEQ\n");
        return ERROR;
    }

    printf("CRC64 for Reading Sequential: %lu\n", crc_val);

    return SUCCESS;
}

uint64_t calc_crc64_from_stream(
    const unsigned char *buff,
    size_t buf_len,
    uint64_t crc_val
)
{
    for (size_t i = 0; i < buf_len; i++)
        crc_val = update_crc_64(crc_val, buff[i]);

    printf("crc partial val: %lu\n", crc_val);
    return crc_val;
}

void parse_cmdl_args(int argc,
                     char *argv[],
                     const char **file_path,
                     enum Strategy *strategy)
{
    // parse cmd line options
    int option;
    int is_file_path_provided = 0;
    int is_strategy_provided = 0;

    // ':' at the front allows programme to distinguish between '?' and ':' case
    // ':' right after option means this option must have value supplied
    while ((option = getopt(argc, argv, ":hf:s:")) != -1)
    {
        switch (option)
        {
        case 'h':
            printf("Usage: %s [-h] [-f filepath] [-s strategy]\n", argv[0]);
            printf("  -h           Display this help message\n");
            printf("  -f filepath  Specify a file to read\n");
            printf("  -s strategy  \n\tAllowed reading strategies are:\n");
            printf("\t rs - read sequential\n\t rr - read random \n");
            printf("\t ms - mmap sequential \n\t mr - mmap random\n");
            printf("\t if -s not specified all strategies are used\n");
            exit(EXIT_SUCCESS);
        case 'f':
            if (is_file_path_provided)
            {
                printf("Option -f is allowed to be used only once\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                // printf("Opening file: %s\n", optarg);
                *file_path = optarg;
                is_file_path_provided = 1;
            }
            break;
        case 's':
            if (is_strategy_provided)
            {
                printf("Option -s is allowed to be used only once\n");
                exit(EXIT_FAILURE);
            }
            // printf("chosen strategy: %s\n", optarg);

            is_strategy_provided = 1;

            if (strcmp(optarg, "rs") == 0)
                *strategy = READ_SEQ;
            else if (strcmp(optarg, "rr") == 0)
                *strategy = READ_RAND;
            else if (strcmp(optarg, "ms") == 0)
                *strategy = MMAP_SEQ;
            else if (strcmp(optarg, "mr") == 0)
                *strategy = MMAP_RAND;
            else
            {
                printf("Unknown reading strategy\n");
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
            printf("Unknown option: %c, use -h for help\n", optopt);
            exit(EXIT_FAILURE);
        case ':':
            printf("When using -f option you need to supply filepath\n");
            exit(EXIT_FAILURE);
        default:
            printf("Unknown option, use -h for help\n");
            exit(EXIT_FAILURE);
        }
    }

    if (!is_file_path_provided)
    {
        printf("Error: Missing required option -f, use -h for help\n");
        exit(EXIT_FAILURE);
    }
}

void write_to_file(const unsigned char *buff, size_t buf_len, const char *filename)
{
    int fd = open(filename, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP);
    if (write(fd, buff, buf_len) == -1)
        printf("Write error\n");
    close(fd);
}