#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "./libcrc/include/checksum.h"

enum Strategy
{
    READ_SEQ,
    READ_RAND,
    MMAP_SEQ,
    MMAP_RAND
} Strategy;

void parse_cmdl_args(int argc,
                     char *argv[],
                     const char **file_path,
                     enum Strategy *strategy);

int main(int argc, char *argv[])
{
    const char *file_path = NULL;
    enum Strategy reading_strategy;

    parse_cmdl_args(argc, argv, &file_path, &reading_strategy);

    // opening file
    int file_descr = open(file_path, O_RDONLY);

    if (file_descr == -1)
    {
        printf("Provided file does not exist");
        exit(EXIT_FAILURE);
    }

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
            printf("Usage: %s [-h] [-f filepath]\n", argv[0]);
            printf("  -h           Display this help message\n");
            printf("  -f filepath  Specify a file to read\n");
            printf("  -s reading strategy  Allowed reading strategies are:\n");
            printf("\t r1 - read sequential\n\t r2 - read random \n");
            printf("\t m1 - mmap sequential \n\t m2 - mmap random\n");
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

            is_strategy_provided = 1;

            if (strcmp(optarg, "r1"))
                *strategy = READ_SEQ;
            else if (strcmp(optarg, "r2"))
                *strategy = READ_RAND;
            else if (strcmp(optarg, "m1"))
                *strategy = MMAP_SEQ;
            else if (strcmp(optarg, "m2"))
                *strategy = MMAP_RAND;
            else
            {
                printf("Unknown reading strategy: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
            printf("Unknown option: %c\n", optopt);
            exit(EXIT_FAILURE);
        case ':':
            printf("When using -f option you need to supply filepath\n");
            exit(EXIT_FAILURE);
        default:
            printf("Unknown option\n");
            exit(EXIT_FAILURE);
        }
    }

    if (!is_file_path_provided)
    {
        printf("Error: Missing required option -f\n");
        exit(EXIT_FAILURE);
    }

    if (!is_strategy_provided)
    {

        printf("Error: Missing required option -s\n");
        exit(EXIT_FAILURE);
    }
}