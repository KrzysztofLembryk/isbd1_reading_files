#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
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

const size_t BLOCK_SIZE = KB_1;    

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

uint64_t read_sequential(const char* file_path, unsigned char *buff);

uint64_t read_rand(const char *file_path, unsigned char *buff);

uint64_t calc_crc64_from_stream(
    uint64_t crc_val,
    const unsigned char *buff,
    size_t buf_len
);

int open_file(const char* file_path);

int get_file_metadata(int file_descr, struct stat *file_metadata);

// ####################################################
// ################# IMPLEMENTATION ###################
// ####################################################

int main(int argc, char *argv[])
{
    const char *file_path = NULL;
    enum Strategy reading_strategy;

    parse_cmdl_args(argc, argv, &file_path, &reading_strategy);

    unsigned char *buff = (unsigned char *)calloc(BLOCK_SIZE, sizeof(unsigned char));

    // start measuring time
    switch (reading_strategy)
    {
    case READ_SEQ:
        read_sequential(file_path, buff);
        break;
    case READ_RAND:
        read_rand(file_path, buff);
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

    return 0;
}

uint64_t read_rand(const char *file_path, unsigned char *buff)
{
    // opening file
    int file_descr = open_file(file_path);
    struct stat file_metadata;

    // We want to know file size to calculate number of blocks
    // I firstly want to calculate number of blocks since in my previous 
    // attempt to do random read I didn't do that and was reading in a way
    // that not full BLOCK_SIZE of data was read in the middle of the data,
    // not at the end of it thus read sequential and read rand gave different 
    // crc values.
    get_file_metadata(file_descr, &file_metadata);

    size_t file_size = file_metadata.st_size;
    size_t nbr_of_blocks = file_size < BLOCK_SIZE 
                        ? 1 
                        : (file_size % BLOCK_SIZE == 0 
                            ? file_size / BLOCK_SIZE
                            : file_size / BLOCK_SIZE + 1);
    ssize_t bytes_read = 0;
    uint64_t final_crc_val = CRC_START_64_ECMA;

    printf("file size: %lu, block size: %lu, nbr of blocks: %lu\n", file_size, BLOCK_SIZE, nbr_of_blocks);

    for (size_t i = 0; i < nbr_of_blocks; i++)
    {
        uint64_t curr_crc_val = CRC_START_64_ECMA;
        // we go like that: i = 0 read from left, i = 1 read from right ...
        // nbr_of_blocks - 1 <-- since we count idxs from 0
        size_t block_idx = i % 2 == 0 ? i / 2 : nbr_of_blocks - 1 - i /2;
        printf("block idx: %lu, ", block_idx);

        if (lseek(file_descr, (off_t)(block_idx * BLOCK_SIZE), SEEK_SET) < 0)
        {
            printf("lseek front encountered error\n");
            exit(EXIT_FAILURE);
        }

        bytes_read = read(file_descr, buff, BLOCK_SIZE);

        if (bytes_read < 0)
        {
            printf("ERROR in read\n");
            exit(EXIT_FAILURE);
        }
        else if (bytes_read > 0)
        {
            // Unfortunately crc64, at least from library I use, is not 
            // commutative thus we calculate crc64 for each block separately, 
            // and then xor it with previous crc value stored in variable. XOR 
            // is commutative, thus order of blocks won't matter, and since 
            // blocks we read are the same but just in different order, crc64 
            // of them will be also the same so XORing such values in the end 
            // will give the same output.
            curr_crc_val = calc_crc64_from_stream(curr_crc_val, buff, bytes_read);
            final_crc_val = final_crc_val ^ curr_crc_val;
        }
    }

    printf("CRC64 for Reading Rand: %" PRIu64 "\n", final_crc_val);
    close(file_descr);

    return final_crc_val;
}

uint64_t read_sequential(const char* file_path, unsigned char *buff)
{
    // opening file
    int file_descr = open_file(file_path);
    ssize_t bytes_read = 0;
    uint64_t curr_crc_val; 
    uint64_t final_crc_val = CRC_START_64_ECMA;

    do
    {
        curr_crc_val = CRC_START_64_ECMA;
        bytes_read = read(file_descr, buff, BLOCK_SIZE);
        // Unfortunately crc64, at least from library I use, is not commutative
        // thus we calculate crc64 for each block separately, and then xor it
        // with previous crc value stored in variable. XOR is commutative, thus
        // order of blocks won't matter, and since blocks we read are the same
        // but just in different order, crc64 of them will be also the same
        // so XORing such values in the end will give the same output.
        if (bytes_read > 0)
        {
            curr_crc_val = calc_crc64_from_stream(curr_crc_val, buff, bytes_read);
            final_crc_val = final_crc_val ^ curr_crc_val;
        }
    } while (bytes_read != 0 && bytes_read != -1);

    printf("CRC64 for Reading Sequential: %" PRIu64 "\n", final_crc_val);

    close(file_descr);

    return final_crc_val;
}

uint64_t calc_crc64_from_stream(
    uint64_t crc_val,
    const unsigned char *buff,
    size_t buf_len
)
{
    for (size_t i = 0; i < buf_len; i++)
        crc_val = update_crc_64(crc_val, buff[i]);

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


int open_file(const char* file_path)
{
    
    int file_descr = open(file_path, O_RDONLY);

    if (file_descr == -1)
    {
        printf("Provided file does not exist");
        exit(EXIT_FAILURE);
    }
    return file_descr;
}

int get_file_metadata(int file_descr, struct stat *file_metadata)
{
    if (fstat(file_descr, file_metadata) == -1)
    {
        printf("fstat returned error\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}


void write_to_file(const unsigned char *buff, size_t buf_len, const char *filename)
{
    int fd = open(filename, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP);
    if (write(fd, buff, buf_len) == -1)
        printf("Write error\n");
    close(fd);
}