#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <crypter.h>

int main()
{
  DEV_HANDLE cdev;
  printf("1 parent and 1 child - both use different key\n");
  if ( fork() == 0)
  {
        char *msg = "Hello little CS730!";
        char op_text[16];
        KEY_COMP a=40, b=15;
        uint64_t size = strlen(msg);
        strcpy(op_text, msg);
        cdev = create_handle();
        if(cdev == ERROR)
        {
                printf("[child]: Unable to create handle for device\n");
                exit(0);
        }

        if(set_key(cdev, a, b) == ERROR){
                printf("[child]: Unable to set key\n");
                exit(0);
        }
	set_config(cdev, DMA, SET);
	set_config(cdev, INTERRUPT, SET);
        
	printf("[child]: Original Text: %s\n", msg);
        printf("[child]: Entered\n");
        encrypt(cdev, op_text, size, 0);
        printf("[child]: Encrypted Text: %s\n", op_text);
        decrypt(cdev, op_text, size, 0);
        printf("[child]: Decrypted Text: %s\n", op_text);
        close_handle(cdev);
  }
else
  {
        char *msg = "Hello CS730!";
        char op_text[16];
        KEY_COMP a=30, b=17;
        uint64_t size = strlen(msg);
        strcpy(op_text, msg);
        cdev = create_handle();

        if(cdev == ERROR)
        {
                printf("[parent]: Unable to create handle for device\n");
                exit(0);
        }

        if(set_key(cdev, a, b) == ERROR){
        printf("[parent]: Unable to set key\n");
        exit(0);
        }
	
	set_config(cdev, DMA, SET);
        set_config(cdev, INTERRUPT, UNSET);	

        printf("[parent]: Original Text: %s\n", msg);
        encrypt(cdev, op_text, size, 0);
        printf("[parent]: Encrypted Text: %s\n", op_text);

        decrypt(cdev, op_text, size, 0);
        printf("[parent]: Decrypted Text: %s\n", op_text);

        close_handle(cdev);
  }
  return 0;
}

