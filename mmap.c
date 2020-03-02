#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <crypter.h>

int main(int argc, char *argv[])
{
  DEV_HANDLE cdev;
  char *msg = "Hello CS730! This is Vishal";
  char op_text[30];
  KEY_COMP a=30, b=17;
  uint64_t size = strlen(msg);
  strcpy(op_text, msg);
  cdev = create_handle();
	printf("userland: cdev: %d\n", cdev);
  if(cdev == ERROR) {
    printf("Unable to create handle for device\n");
    exit(0);
  }

  if(set_key(cdev, a, b) == ERROR){
    printf("Unable to set key\n");
    exit(0);
  }
	printf("set_key successfull, size of text: %lu\n", size);
//	set_config(cdev, DMA, SET);
	set_config(cdev, DMA, UNSET);
	set_config(cdev, INTERRUPT, SET);
//	set_config(cdev, INTERRUPT, UNSET);
	

  ADDR_PTR addr = map_card(cdev, size);
	printf("Original Text: %s\n", msg);
	if (!addr) {
		printf("mmapp faild\n");
		exit(1);
	}
	strncpy((char*)addr, msg, size+1);
  encrypt(cdev, addr, size, 1);
  printf("Encrypted Text: %s\n", (char *)addr);

  decrypt(cdev, addr, size, 1);
  printf("Decrypted Text: %s\n", (char *)addr);
	unmap_card(cdev, addr);
  close_handle(cdev);
  return 0;
}
