#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/fcntl.h>
#include<signal.h>
#include<sys/ioctl.h>
#include<sys/mman.h>
#include <sys/unistd.h>

int main()
{
   char *ptr, *ptr1;
   unsigned long start, end, ctr;
   char buf[64];

   int fd = open("/dev/demo",O_RDWR);
   if(fd < 0){
       perror("open");
       exit(-1);
   }

   ptr = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0);
   if(ptr == MAP_FAILED){
        perror("mmap");
        exit(-1);
   }

   *((unsigned long *)buf) = (unsigned long)ptr;
   if(write(fd, buf, 8) < 0){
    perror("read");
    exit(-1);
   }
    
  if(read(fd, buf, 8) < 0){
    perror("read");
    exit(-1);
   }
 
  close(fd);
  munmap((void *)ptr, 4096);
  return 0;
}
