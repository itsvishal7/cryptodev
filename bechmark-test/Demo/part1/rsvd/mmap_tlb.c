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
   int *ptr;
   unsigned long ctr;
   char buf[64];
   int fd = open("/dev/demo",O_RDWR);
   if(fd < 0){
       perror("open");
       exit(-1);
   }

   ptr = (int*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_POPULATE, 0, 0);
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

  *(int *)ptr = 100;
  
  printf("%d\n",*ptr);


/*
  for(ctr=0; ctr<10; ++ctr){
      memset(ptr, 0, 4096);
      printf("Accessing #%ldth time\n", ctr+1);
  } 
  */
  close(fd);    //Should be before munmap
  munmap((void *)ptr, 4096);
  return 0;
}
