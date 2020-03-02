#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/sched.h>

int init_module(void)
{
	printk(KERN_INFO "Hello kernel\n");
/*        __asm__ __volatile__( 
                              "cli;" 
                              "hlt;"
                              :::"memory");
			      */
	return 0;
}

void cleanup_module(void)
{
	printk(KERN_INFO "Goodbye kernel\n");
}
