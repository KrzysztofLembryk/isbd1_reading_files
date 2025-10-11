#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
// #include "./libcrc/include/checksum.h"

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

// test_file_short = 8kB
// test_file_medium = 8.6MB
// test_file_big = xGB

const size_t BUFF_SIZE = 16000000;    // 16 000 000 = 16 000 kB = 16 MB
const size_t READ_COUNT_SIZE = 16000; // 16kB

// ####################################################
// ############ FUNCTIONS PRE-DECLARATIONS ############
// ####################################################

void parse_cmdl_args(int argc,
                     char *argv[],
                     const char **file_path,
                     enum Strategy *strategy);

// ####################################################
// ################# IMPLEMENTATION ###################
// ####################################################

int main(int argc, char *argv[])
{
    const char *file_path = NULL;
    enum Strategy reading_strategy;
    char *buff = (char *)calloc(BUFF_SIZE, sizeof(char));

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

    size_t file_size = file_metadata.st_size;

    printf("opened file size: %ld\n", file_size);

    // start measuring time
    switch (reading_strategy)
    {
    case READ_SEQ:
        ssize_t bytes_read = 0;
        do
        {
            bytes_read = read(file_descr, buff + bytes_read, READ_COUNT_SIZE);
            file_size += bytes_read;
        } while (bytes_read != 0 && bytes_read != -1);

        if (bytes_read == -1)
        {
            close(file_descr);
            printf("Encountered error while reading file using READ_SEQ\n");
            exit(EXIT_FAILURE);
        }


        printf("Reading done");
        break;

    case READ_RAND:
        // to be able to use fseek we must have FILE*, so we create it from our file descriptor
        FILE *f_stream = fdopen(file_descr, "r");

        bool read_front = true;
        size_t bytes_read = 0;
        size_t bytes_read_front = 0;
        // we start reading from front, so if file_size < READ_COUNT_SIZE we will just read all
        // data from front, so even though file_size - READ_COUNT_SIZE < 0 we will not use it
        size_t bytes_read_end = file_size - READ_COUNT_SIZE; // or should be file_size - 1
        size_t read_bytes_sum = 0;

        do
        {
            if (read_front)
            {
                if (fseek(f_stream, bytes_read_front, SEEK_SET) == -1)
                    ;
                {
                    printf("fseek encountered error\n");
                    // TODO: add flag and break that checks if success or not
                    exit(EXIT_FAILURE);
                }
                read_front = !read_front;
                bytes_read = fread(buff, sizeof(char), READ_COUNT_SIZE, f_stream);
                bytes_read_front += bytes_read;
            }
            else
            {
            }
            read_bytes_sum += bytes_read;
        } while (bytes_read != 0);

        if (bytes_read_front > bytes_read_end)
        {
            // this case happens when we read from front, and there is still
            // data to read but of size less then READ_COUNT, so our bytes_read_end will
            // go back too much and we will read too much data, thus we will read from front
            // only the amount that is left
        }

        printf("READ_RAND strategy\n");
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
                printf("Opening file: %s\n", optarg);
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
            printf("chosen strategy: %s\n", optarg);

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
                printf("Unknown reading strategy: %s\n", optarg);
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

        printf("Error: Missing required option -s, use -h for help\n");
        exit(EXIT_FAILURE);
    }
}
