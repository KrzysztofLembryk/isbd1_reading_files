# isbd1_reading_files
- how to use crc64: ```https://www.libcrc.org/update_crc_64/```

- To download crc lib: ```git clone https://github.com/lammertb/libcrc.git path-to/libcrc```

- How to run the project
```bash
# in libcrc folder run:
make

# in root folder run
gcc -Wall -Wextra -O2 -I./libcrc/include 1_lab_task_reading_files.c -L./libcrc/lib -lcrc -o reading_files 

# to know how to run solution
./reading_files -h
```