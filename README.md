# heap_find_and_poke
uses process_vm_readv and process_vm_writev to patch heap memory of another process

## Terminal 1
`$gcc -o example ./example.c && ./example`

## Terminal 2
`$gcc -o poke ./heap-find-and-poke.c && ./poke example "Hello world!" "GOODBYE :)~~"`
