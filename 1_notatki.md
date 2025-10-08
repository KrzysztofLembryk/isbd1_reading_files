
Jak robimy np. select to tak naprawdę program jakoś szacuje gdzie mniej więcej
jest ta informacja i wybiera tylko ten kawałek do czytania.

- fseek - do skakania po pliku
- fstat - trzeba podać mmapowi wielkość pliku

- najpierw do megabajta plik - ogólnie więcej niż ten bloczek co wczytujemy w 
read

# w zadaniu parametry
- pierwszy plik < blocksize (to co wczytujemy) (cold run i hot run)
- drugi plik >> blocksize (to co wczytujemy)
- trzeci plik > pamięć ram

# mmap
```c
mmap(void *addr, // tutaj warto podac nulla, zeby system wybral adres
    size_t dlugosc_pamieci_ktora_mapujemy_w_bajtach, // tu rozmiar pliku z fstat
    int prot, // tu z reguly READ
    int flags, // tu share
    int fd, // tu plik
    off_t offset) //
```