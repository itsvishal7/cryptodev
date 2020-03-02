#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <crypter.h>

int main()
{
  DEV_HANDLE cdev;
  printf("1 parent and 2 children - diff key\n");
  int child1 = fork();
  int child2 = fork();
  if ( child1 == 0 && child2 > 0)  //child1
  {
        char *msg = "Hello little CS730. Child One!";
        char op_text[56];
        KEY_COMP a=40, b=13;
        uint64_t size = strlen(msg);
        strcpy(op_text, msg);
        cdev = create_handle();
        if(cdev == ERROR)
        {
                printf("[child1]: Unable to create handle for device\n");
                exit(0);
        }

        if(set_key(cdev, a, b) == ERROR){
                printf("[child1]: Unable to set key\n");
                exit(0);
        }
        printf("[child1]: Original Text: %s\n", msg);
        printf("[child1]: Entered\n");
        encrypt(cdev, op_text, size, 0);
        printf("[child1]: Encrypted Text: %s\n", op_text);
        decrypt(cdev, op_text, size, 0);
        printf("[child1]: Decrypted Text: %s\n", op_text);
        close_handle(cdev);
  }
else if(child1 > 0 && child2 == 0) //child2
  {
        char *msg = "Hello little CS730. Child Two!";
        char op_text[56];
        KEY_COMP a=50, b=23;
        uint64_t size = strlen(msg);
        strcpy(op_text, msg);
        cdev = create_handle();
        if(cdev == ERROR)
        {
                printf("[child2]: Unable to create handle for device\n");
                exit(0);
        }
        if(set_key(cdev, a, b) == ERROR){
                printf("[child2]: Unable to set key\n");
                exit(0);
        }
        printf("[child2]: Original Text: %s\n", msg);
        printf("[child2]: Entered\n");
        encrypt(cdev, op_text, size, 0);
        printf("[child2]: Encrypted Text: %s\n", op_text);
        decrypt(cdev, op_text, size, 0);
        printf("[child2]: Decrypted Text: %s\n", op_text);
        close_handle(cdev);
  }
else if(child1 == 0 && child2 == 0) //child3
{
        char *msg = "Hello little CS730. Child Three!";
        char op_text[56];
        KEY_COMP a=50, b=23;
        uint64_t size = strlen(msg);
        strcpy(op_text, msg);
        cdev = create_handle();
        if(cdev == ERROR)
        {
                printf("[child3]: Unable to create handle for device\n");
                exit(0);
        }
        if(set_key(cdev, a, b) == ERROR){
                printf("[child3]: Unable to set key\n");
                exit(0);
        }
        printf("[child3]: Original Text: %s\n", msg);
        printf("[child3]: Entered\n");
        encrypt(cdev, op_text, size, 0);
        printf("[child3]: Encrypted Text: %s\n", op_text);
        decrypt(cdev, op_text, size, 0);
        printf("[child3]: Decrypted Text: %s\n", op_text);
	close_handle(cdev);
}

else
  {
        char *msg = "Hello CS730!";
        char op_text[56];
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

        printf("[parent]: Original Text: %s\n", msg);
        encrypt(cdev, op_text, size, 0);
        printf("[parent]: Encrypted Text: %s\n", op_text);

        decrypt(cdev, op_text, size, 0);
        printf("[parent]: Encrypted Text: %s\n", op_text);

        close_handle(cdev);
  }
  return 0;
}


