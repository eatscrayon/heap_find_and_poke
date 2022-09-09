#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/uio.h>
#include <errno.h>

// variadic function to concat strings
// https://stackoverflow.com/questions/8465006/how-do-i-concatenate-two-strings-in-c
char *concat(int count, ...)
{
    va_list ap;
    int i;

    // Find required length to store merged string
    int len = 1; // room for NULL
    va_start(ap, count);
    for (i = 0; i < count; i++)
        len += strlen(va_arg(ap, char *));
    va_end(ap);

    // Allocate memory to concat strings
    char *merged = calloc(sizeof(char), len);
    int null_pos = 0;

    // Actually concatenate strings
    va_start(ap, count);
    for (i = 0; i < count; i++)
    {
        char *s = va_arg(ap, char *);
        strcpy(merged + null_pos, s);
        null_pos += strlen(s);
    }
    va_end(ap);

    return merged;
}

// Loop through a string and return True if the string is only made of chars 0-9
int IsNumeric(const char *CharacterList)
{
    for (; *CharacterList; CharacterList++)
        if (*CharacterList < '0' || *CharacterList > '9')
            return 0;
    return 1;
}

pid_t GetPIDbyName(const char *ProcessName)
{

    int ipid = -1;                         // Set our PID to -1 in case we can't find it
    char *StringToCompare = NULL;          // Null pointer to the contents of /proc/$pid/cmdline
    DIR *directory = NULL;                 // Null pointer to a directory struct from <dirent.h>
    struct dirent *directory_entry = NULL; // Null pointer to a dirent structure <dirent.h>

    // Init our DIR* directory struct
    directory = opendir("/proc/");
    if (directory == NULL)
    {
        perror("Couldn't open the /proc/ directory");
        return -2;
    }

    // Loop through the contents of our directory
    while ((directory_entry = readdir(directory))) // Returns a pointer to a dirent structure
    {

        if (directory_entry->d_type == DT_DIR) // if the type is a directory
        {
            if (IsNumeric(directory_entry->d_name)) // and the name is only numbers...
            {
                // The numbered folders in /proc correspond to the running processes
                // Inside each folder is a file call "cmdline" which has the name of the process
                // Build this filename and store it in CommandLinePath
                char *CommandLinePath = concat(3, "/proc/", directory_entry->d_name, "/cmdline");

                // Open the file that we constructed in the previious step
                //"rt" mean read mode + text mode
                // CmdLineFile is a pointer to a FILE struct
                FILE *CmdLineFile = fopen(CommandLinePath, "rt");

                free(CommandLinePath); // dont need this anymore so free it

                if (CmdLineFile) // CmdLineFile is NULL if the file didn't open
                {
                    // This will read CmdLineFile until a whitespace "%s" is found
                    // and store the contents in NameOfProcess
                    char NameOfProcess[4096]; // Create a char array to hold the name of the found process
                    fscanf(CmdLineFile, "%4095s", NameOfProcess);
                    fclose(CmdLineFile); // close thefile

                    // strrchr return a pointer to the last occurrence of character in str.
                    // so if we find a '/' then we jump to right of the last '/'
                    // in the string, else we store the pointer as is
                    if (strrchr(NameOfProcess, '/'))
                        StringToCompare = strrchr(NameOfProcess, '/') + 1;
                    else
                        StringToCompare = NameOfProcess;

                    // like strstr(), but ignores the case of both arguments.
                    // is our taget process a substring of the cmdline?
                    if (strcasestr(StringToCompare, ProcessName))
                    {
                        ipid = atoi(directory_entry->d_name); // converts C-string to int.
                        closedir(directory);                  // close the directory
                        return ipid;                          // Break out of the loop and return the PID
                    }
                }
            }
        }
    }
    closedir(directory); // We didn't find it, close the directory
    return ipid;         // We didn't find it, return -1
}

void read_proc_maps(pid_t pid, uintptr_t *heap_offset, uintptr_t *heap_size)
{

    char pid_string[10];
    sprintf(pid_string, "%d", pid);

    char *mapsFile = concat(3, "/proc/", pid_string, "/maps");

    FILE *file = fopen(mapsFile, "r");
    char line[4096];
    if (mapsFile)
    {
        while (fgets(line, sizeof(line), file))
        {
            if (strcasestr(line, "[heap]"))
            {
                char *address_space;
                address_space = strtok(line, " ");
                char *map_start;
                char *map_end;
                map_start = strtok(address_space, "-");
                map_end = strtok(NULL, "-");
                // printf("AS STRING: %s %s\n", map_start, map_end);
                uintptr_t int_map_start;
                uintptr_t int_map_end;
                sscanf(map_start, "%lx", &int_map_start);
                sscanf(map_end, "%lx", &int_map_end);
                // printf("AS PID_t : %lx %lx\n", int_map_start, int_map_end);
                *heap_offset = int_map_start;
                *heap_size = int_map_end - int_map_start;
                fclose(file);
                break;
            }
        }
    }
}

int peek(pid_t pid, uintptr_t offset, uintptr_t size,char buffer[])
{

    void *remotePtr = (void *)offset;
    size_t bufferLength = size;

    // Build iovec structs
    struct iovec local[1];
    local[0].iov_base = calloc(bufferLength, sizeof(char));
    local[0].iov_len = bufferLength;

    struct iovec remote[1];
    remote[0].iov_base = remotePtr;
    remote[0].iov_len = bufferLength;

    // Call process_vm_readv - handle any error codes
    ssize_t nread = process_vm_readv(pid, local, 1, remote, 1, 0);

    
    memcpy(buffer, local[0].iov_base, local[0].iov_len);

    
    return 0;
}

void poke(pid_t pid, uintptr_t offset, uintptr_t find_size,char replace[]){

    void *remotePtr = (void *)offset;
    struct iovec local[1];
    struct iovec remote[1];
    char buffer[find_size];
    strcpy(buffer, replace);

    /* Set up the iovecs. */
    local[0].iov_base = buffer;
    remote[0].iov_base = remotePtr;

    local[0].iov_len = find_size;
    remote[0].iov_len = find_size;

    /* Write 12 bytes into the child process. */
    process_vm_writev(pid, local, 1, remote, 1, 0);

}


void seek_and_poke(pid_t pid, char heap[], uintptr_t heapSize, uintptr_t heapoffset, char find[], uintptr_t find_size, char replace[],uintptr_t replace_size){

    //printf("%ld", find_size);
    

    for (int i=0;i < heapSize; i++ ){
        int match = 1;
        for (int j=0;j<find_size;j++){
            if (heap[i+j] != find[j]){
                match = 0;
                break;
            }
        }
        if (match){
            printf("Poking %d @ %lx with %ld bytes\n",pid, i+heapoffset, replace_size );
            poke(pid, i+heapoffset, replace_size,replace);
        }
        //fprint("%d",heap[i])
    }

}

void hexdump(char buffer[],int size){
    int i;
    int newline = 0;
    char hexdump[17];
    hexdump[17] = '\0';
    for (i = 0; i < size; i++)
    {
        if (newline == 0){printf ("%08x: ", i) ;};
        unsigned char c = ((char*)buffer)[i] ;
        printf ("%02x ", c) ;
        if (c >= 33 && c <= 126){
            hexdump[i%16] = c;
        }else{
            hexdump[i%16] = '.';
        }
        newline++;
        if (newline == 16){
            printf("%s",hexdump);
            printf ("\n") ;
            newline = 0;
        }
    }

}

unsigned char* hexstr_to_char(const char* hexstr)
{
    size_t len = strlen(hexstr);
    size_t final_len = len / 2;
    unsigned char* chrs = (unsigned char*)malloc(( final_len + 1) * sizeof(*chrs));
    for (size_t i=0, j=0; j<final_len; i+=2, j++)
        chrs[j] = (hexstr[i] % 32 + 9) % 25 * 16 + (hexstr[i+1] % 32 + 9) % 25;
    chrs[final_len] = '\0';
    return chrs;
}

int main(int argc, char **argv)
{
    if (argc < 2){
        printf("Usage: a.out <process name>\n");
        exit(EXIT_SUCCESS);
    }
    // Initialize our struct that will hold process information
    // Passing argv1 to the GetPIDbyName Function
    
    uintptr_t HeapOffset,HeapSize;
    pid_t ProcId = GetPIDbyName(argv[1]);

    // Print the PID of the Process
    //(Process.ProcId > 0) ? printf("Process ID : %i\n", Process.ProcId) : printf("PID Not Found\n");
    if (ProcId < 0){
        printf("PID Not Found\n");
        exit(EXIT_SUCCESS);
    }
    // Read memory maps of the process
    read_proc_maps(ProcId, &HeapOffset, &HeapSize);

    // use process_vm_readv() to read some memory
    // https://gist.github.com/FergusInLondon/fec6aebabc3c9e61e284983618f40730
    char Heap[HeapSize]; // create buffer to hold our heap
    peek(ProcId, HeapOffset, HeapSize, Heap);
    
    //hexdump( Heap,HeapSize);
    
    char Find[strlen(argv[2])];
    char Replace[strlen(argv[3])];
    size_t Find_size = sizeof(Find) / sizeof(Find[0]);
    size_t Replace_size = sizeof(Replace) / sizeof(Replace[0]);
    memcpy(Find,argv[2],Find_size);
    memcpy(Replace,argv[3],Replace_size);
    if (Find_size != Replace_size){
        printf("Warning: FIND and REPLACE are different sizes!\n");
    }
    
    seek_and_poke(ProcId,Heap,HeapSize,HeapOffset,Find,Find_size,Replace,Replace_size);

    exit(EXIT_SUCCESS);
}