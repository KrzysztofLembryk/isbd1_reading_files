#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
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
    MMAP_RAND,
    ALL
} Strategy;


const size_t KB_1 = 1024;
const size_t MB_1 = 1024 * KB_1;

size_t BLOCK_SIZE = 8 * MB_1;    

#define ERROR -1
#define NS_PER_SECOND 1000000000 

// ####################################################
// ############ FUNCTIONS PRE-DECLARATIONS ############
// ####################################################

void parse_cmdl_args(int argc,
                     char *argv[],
                     const char **file_path,
                     enum Strategy *strategy,
                     size_t *block_size);

int read_sequential(const char* file_path, unsigned char *buff);

int read_rand(const char *file_path, unsigned char *buff);

int mmap_sequential(const char *file_path, unsigned char *buff);

int mmap_rand(const char *file_path, unsigned char *buff);

uint64_t calc_crc64_from_stream(
    uint64_t crc_val,
    const unsigned char *buff,
    size_t buf_len
);

void calc_delta_time(struct timespec start, 
                    struct timespec finish, 
                    struct timespec *delta);

int open_file(const char* file_path);

int get_file_metadata(int file_descr, struct stat *file_metadata);

// ####################################################
// ################# IMPLEMENTATION ###################
// ####################################################

int main(int argc, char *argv[])
{
    const char *file_path = NULL;
    enum Strategy reading_strategy;

    parse_cmdl_args(argc, argv, &file_path, &reading_strategy, &BLOCK_SIZE);

    unsigned char *buff = (unsigned char *)calloc(BLOCK_SIZE, sizeof(unsigned char));

    if (!buff)
    {
        perror("main - malloc error");
        return ERROR;
    }

    ssize_t ret_val = 0;

    switch (reading_strategy)
    {
    case ALL:
        if (read_sequential(file_path, buff) == ERROR 
            || read_rand(file_path, buff) == ERROR
            || mmap_sequential(file_path, buff) == ERROR
            || mmap_rand(file_path, buff) == ERROR)
        {
            ret_val = ERROR;
        }
        break;
    case READ_SEQ:
        ret_val = read_sequential(file_path, buff);
        break;
    case READ_RAND:
        ret_val = read_rand(file_path, buff);
        break;
    case MMAP_SEQ:
        ret_val = mmap_sequential(file_path, buff);
        break;
    case MMAP_RAND:
        ret_val = mmap_rand(file_path, buff);
        break;
    default:
        perror("Switch got unsupported reading strategy\n");
    }

    free(buff);

    if (ret_val == ERROR)
        return ERROR;

    return 0;
}

int mmap_rand(const char *file_path, unsigned char *buff)
{
    int file_descr = open_file(file_path);

    if (file_descr == ERROR)
        return ERROR;

    struct stat file_metadata;

    if (get_file_metadata(file_descr, &file_metadata) == ERROR)
    {
        close(file_descr);
        return ERROR;
    }
    
    size_t file_size = file_metadata.st_size;
    unsigned char *file_mapping = mmap(NULL, 
                                    file_size, 
                                    PROT_READ, 
                                    MAP_SHARED, 
                                    file_descr, 
                                    0);
    // When mapping is done we can safely close fd, mapping will still exist
    close(file_descr);

    if (file_mapping == MAP_FAILED)
    {
        perror("mmap_rand - mmap error");
        return ERROR;
    }

    uint64_t curr_crc;
    uint64_t final_crc = CRC_START_64_ECMA;
    // like in read_rand we calculate number of blocks
    size_t nbr_of_blocks = file_size < BLOCK_SIZE 
                        ? 1 
                        : (file_size % BLOCK_SIZE == 0 
                            ? file_size / BLOCK_SIZE
                            : file_size / BLOCK_SIZE + 1);
    
    struct timespec start, finish, delta;

    clock_gettime(CLOCK_REALTIME, &start);

    for(size_t i = 0; i < nbr_of_blocks; i++)
    {
        size_t block_idx = i % 2 == 0 ? i / 2 : nbr_of_blocks - 1 - i /2;
        size_t buff_idx = 0;

        for(size_t mapping_idx = block_idx * BLOCK_SIZE; 
            mapping_idx < file_size && buff_idx < BLOCK_SIZE; 
            mapping_idx++, buff_idx++)
        {
            buff[buff_idx] = file_mapping[mapping_idx];
        }

        curr_crc = calc_crc64_from_stream(CRC_START_64_ECMA, buff, buff_idx);
        final_crc = final_crc ^ curr_crc;
    }

    clock_gettime(CLOCK_REALTIME, &finish);
    calc_delta_time(start, finish, &delta);

    printf("CRC64 for mmap rand:\t %" PRIu64 "\ttime: %d.%.9ld\n", 
        final_crc, (int)delta.tv_sec, delta.tv_nsec);

    // we free the mapping
    if (munmap(file_mapping, file_size) < 0)
    {
        perror("mmap_rand - file unmapping error");
        return ERROR;
    }

    return 0;
}

int mmap_sequential(const char *file_path, unsigned char *buff)
{
    // link to good mmap explanation: https://membarrier.wordpress.com/2024/08/10/memory-management-the-mmap-call/
    int file_descr = open_file(file_path);

    if (file_descr == ERROR)
        return ERROR;

    struct stat file_metadata;

    if (get_file_metadata(file_descr, &file_metadata) == ERROR)
    {
        close(file_descr);
        return ERROR;
    }
    
    size_t file_size = file_metadata.st_size;
    unsigned char *file_mapping = mmap(NULL, 
                                    file_size, 
                                    PROT_READ, 
                                    MAP_SHARED, 
                                    file_descr, 
                                    0);
    // When mapping is done we can safely close fd, mapping will still exist
    close(file_descr);

    if (file_mapping == MAP_FAILED)
    {
        perror("mmap_sequential - mmap failed");
        return ERROR;
    }

    uint64_t curr_crc = CRC_START_64_ECMA;
    uint64_t final_crc = CRC_START_64_ECMA;
    size_t buff_idx = 0;

    struct timespec start, finish, delta;

    clock_gettime(CLOCK_REALTIME, &start);

    for (size_t mapping_idx = 0; mapping_idx < file_size; mapping_idx++)
    {
        if (buff_idx == BLOCK_SIZE)
        {
            curr_crc = calc_crc64_from_stream(CRC_START_64_ECMA,
                                                    buff, BLOCK_SIZE);
            final_crc = final_crc ^ curr_crc;

            buff_idx = 0;
        }

        buff[buff_idx] = file_mapping[mapping_idx];
        buff_idx += 1;
    }

    // in loop above before we calculate crc64 for last block, we leave loop
    // so always after the loop we need to do below calculation: 
    curr_crc = calc_crc64_from_stream(CRC_START_64_ECMA, buff, buff_idx);
    final_crc = final_crc ^ curr_crc;

    clock_gettime(CLOCK_REALTIME, &finish);
    calc_delta_time(start, finish, &delta);

    printf("CRC64 for mmap seq:\t %" PRIu64 "\ttime: %d.%.9ld\n", 
        final_crc, (int)delta.tv_sec, delta.tv_nsec);

    // we free the mapping
    if (munmap(file_mapping, file_size) < 0)
    {
        perror("mmap_sequential - file unmapping error");
        return ERROR;
    }

    return 0;
}

int read_rand(const char *file_path, unsigned char *buff)
{
    // opening file
    int file_descr = open_file(file_path);

    if (file_descr == ERROR)
        return ERROR;

    struct stat file_metadata;

    // We want to know file size to calculate number of blocks
    // I firstly want to calculate number of blocks since in my previous 
    // attempt to do random read I didn't do that and was reading in a way
    // that not full BLOCK_SIZE of data was read in the middle of the data,
    // not at the end of it thus read sequential and read rand gave different 
    // crc values.

    if (get_file_metadata(file_descr, &file_metadata) == ERROR)
    {
        close(file_descr);
        return ERROR;
    }

    size_t file_size = file_metadata.st_size;
    size_t nbr_of_blocks = file_size < BLOCK_SIZE 
                        ? 1 
                        : (file_size % BLOCK_SIZE == 0 
                            ? file_size / BLOCK_SIZE
                            : file_size / BLOCK_SIZE + 1);
    ssize_t bytes_read = 0;
    uint64_t final_crc_val = CRC_START_64_ECMA;

    struct timespec start, finish, delta;

    clock_gettime(CLOCK_REALTIME, &start);

    for (size_t i = 0; i < nbr_of_blocks; i++)
    {
        uint64_t curr_crc_val = CRC_START_64_ECMA;

        // we go like that: i = 0 read from left, i = 1 read from right ...
        // nbr_of_blocks - 1 <-- since we count idxs from 0
        size_t block_idx = i % 2 == 0 ? i / 2 : nbr_of_blocks - 1 - i /2;

        if (lseek(file_descr, (off_t)(block_idx * BLOCK_SIZE), SEEK_SET) < 0)
        {
            perror("read_rand - lseek encountered error");
            close(file_descr);
            return ERROR;
        }

        bytes_read = read(file_descr, buff, BLOCK_SIZE);

        if (bytes_read < 0)
        {
            perror("read_rand - ERROR in read\n");
            close(file_descr);
            return ERROR;
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

    clock_gettime(CLOCK_REALTIME, &finish);
    calc_delta_time(start, finish, &delta);

    printf("CRC64 for read rand:\t %" PRIu64 "\ttime: %d.%.9ld\n", 
        final_crc_val, (int)delta.tv_sec, delta.tv_nsec);

    close(file_descr);

    return 0;
}

int read_sequential(const char* file_path, unsigned char *buff)
{
    // opening file
    int file_descr = open_file(file_path);

    if (file_descr == ERROR)
        return ERROR;

    ssize_t bytes_read = 0;
    uint64_t curr_crc_val; 
    uint64_t final_crc_val = CRC_START_64_ECMA;

    struct timespec start, finish, delta;

    clock_gettime(CLOCK_REALTIME, &start);

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
    } while (bytes_read != 0 && bytes_read != ERROR);

    clock_gettime(CLOCK_REALTIME, &finish);
    calc_delta_time(start, finish, &delta);

    if (bytes_read < 0)
    {
        perror("read_sequential - error in read");
        close(file_descr);
        return ERROR;
    }

    printf("CRC64 for read seq:\t %" PRIu64 "\ttime: %d.%.9ld\n", 
        final_crc_val, (int)delta.tv_sec, delta.tv_nsec);

    close(file_descr);

    return 0;
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
                     enum Strategy *strategy,
                     size_t *block_size)
{
    // parse cmd line options
    int option;
    bool is_file_path_provided = false;
    bool is_strategy_provided = false;
    bool are_bytes_provided = false;

    // ':' at the front allows programme to distinguish between '?' and ':' case
    // ':' right after option means this option must have value supplied
    while ((option = getopt(argc, argv, ":hf:s:b:")) != ERROR)
    {
        switch (option)
        {
        case 'h':
            printf("Usage: %s [-h] [-f filepath] [-s strategy]\n", argv[0]);
            printf("  -h            Display this help message\n");
            printf("  -f filepath   Specify a file to read (mandatory)\n");
            printf("  -b block size Size of block we read in bytes\n");
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
                *file_path = optarg;
                is_file_path_provided = true;
            }
            break;
        case 'b':
            if (are_bytes_provided)
            {
                printf("Option -b is allowed to be used only once\n");
                exit(EXIT_FAILURE);
            }

            are_bytes_provided = true;

            *block_size = (size_t)strtol(optarg, NULL, 10);
            break;
        case 's':
            if (is_strategy_provided)
            {
                printf("Option -s is allowed to be used only once\n");
                exit(EXIT_FAILURE);
            }

            is_strategy_provided = true;

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

    if (!is_strategy_provided)
    {
        *strategy = ALL;
    }

    if (!are_bytes_provided)
    {
        *block_size = 8 * MB_1;
    }

    printf("opening file: '%s', BLOCK SIZE: %lu\n", *file_path, *block_size);
}

int open_file(const char* file_path)
{
    
    int file_descr = open(file_path, O_RDONLY);

    if (file_descr == ERROR)
    {
        perror("open_file");
        return ERROR;
    }
    return file_descr;
}

int get_file_metadata(int file_descr, struct stat *file_metadata)
{
    if (fstat(file_descr, file_metadata) == ERROR)
    {
        perror("get_file_metadata - fstat returned error\n");
        return ERROR;
    }
    return 0;
}

void calc_delta_time(struct timespec start, 
                    struct timespec finish, 
                    struct timespec *delta)
{
    delta->tv_nsec = finish.tv_nsec - start.tv_nsec;
    delta->tv_sec  = finish.tv_sec - start.tv_sec;
    if (delta->tv_sec > 0 && delta->tv_nsec < 0)
    {
        delta->tv_nsec += NS_PER_SECOND;
        delta->tv_sec--;
    }
    else if (delta->tv_sec < 0 && delta->tv_nsec > 0)
    {
        delta->tv_nsec -= NS_PER_SECOND;
        delta->tv_sec++;
    }
}