# isbd1_reading_files
- how to use crc64: ```https://www.libcrc.org/update_crc_64/```

- To download crc lib: ```git clone https://github.com/lammertb/libcrc.git path-to/libcrc```

- To compile run 
```bash
gcc -Wall -Wextra -O2 -I./libcrc/include 1_lab_task_reading_files.c -L./libcrc/lib -lcrc -o reading_files 
```