#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
int main(){

    char *p = malloc(15);
	strcpy(p, "Hello world!");
    while (1){
    printf("%s\n",p);
    sleep(1);
    }

}