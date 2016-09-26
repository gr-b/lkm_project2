#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

unsigned long **sys_call_table;

asmlinkage long (*ref_sys_cs3013_syscall1)(void);

struct processinfo {
	long state;
	pid_t pid;
	pid_t parent_pid;
	pid_t youngest_child;
	pid_t younger_sibling;
	pid_t older_sibling;
	uid_t uid;
	long long start_time; 	// Process start time in nanosecond since boot
	long long user_time; 	// CPU time in user mode (microseconds)
	long long system_time; 	// CPU time in system mode (microseconds)
	long long cutime; 	// user time of children (microseconds)
	long long cstime; 	// system time of children (microseconds)
}; 	// struct processinfo

// Open system call
asmlinkage long (*ref_sys_open)(const char __user *filename, int flags, umode_t mode);
asmlinkage long (*ref_sys_close)(unsigned int fd);
asmlinkage long (*ref_cs3013_syscall2)(void);


// Caculates the cumulative user time of all child tasks
asmlinkage long get_children_info(struct processinfo *myinfo){
	struct task_struct *task;
	struct list_head *list;
	long long cutime = 0;
	long long cstime = 0;

	struct task_struct *youngest_child = NULL; // Error if null remains

	printk(KERN_INFO "\" Children_user_time function reached.\n");
	list_for_each(list, &current->children){
		task = list_entry(list, struct task_struct, sibling);
		cstime += cputime_to_usecs(task->stime);
		cutime += cputime_to_usecs(task->utime);
		printk(KERN_INFO "\" 'Task user time: %u. Pid: %u \n", cputime_to_usecs(task->utime), task->pid);
			
		if(youngest_child == NULL) youngest_child = task;	
	
		// Compare the time created to find the youngest one.
		// timespec_compare is -1 if first < second
		if(timespec_compare(&task->start_time, &youngest_child->start_time) == -1) 
			youngest_child = task;
	}
	
	// Now put these values into the struct
	myinfo->cutime = cutime;
	myinfo->cstime = cstime;
	myinfo->youngest_child = youngest_child->pid;
	
	return 0; 
}

asmlinkage long get_sibling_info(struct processinfo *myinfo){
	struct task_struct *asibling;
	struct task_struct *younger = NULL;
	struct task_struct *older = NULL;

	
	// Goes through every list_head in the current task's sibling list
	list_for_each_entry(asibling, &current->sibling, sibling){
		printk(KERN_INFO "\" Sibling pid: %u, current pid: %u\n", 
			asibling->pid, current->pid); 

		if(younger == NULL && 
			timespec_compare(&asibling->start_time, &current->start_time) == 1)
				younger = asibling;		

		if(older == NULL && 
			timespec_compare(&asibling->start_time, &current->start_time) == -1)
				older = asibling;		

		// If the sibling's time is lower than the current older sibling,
		// and the sibling's time is higher than the current task
		if(younger != NULL)
		if(timespec_compare(&asibling->start_time, &younger->start_time) == -1 &&
			timespec_compare(&asibling->start_time, &current->start_time) == 1)
				younger = asibling;

		// If the sibling's time is higher than the current older sibling,
		// and the sibling's time is lower than the current task
		if(older != NULL)
		if(timespec_compare(&asibling->start_time, &older->start_time) == 1 &&
			timespec_compare(&asibling->start_time, &current->start_time) == -1)
				older = asibling;
	}
	
	if(older != NULL)
		myinfo->older_sibling = older->pid;
	else myinfo->younger_sibling = 0;

	if(younger != NULL)
		myinfo->younger_sibling = younger->pid;
	else myinfo->younger_sibling = 0;	

	return 0;
}

asmlinkage long cs3013_syscall2(struct processinfo *info){
	// Create a temporary processinfo struct to house the data in kernel space
	//struct processinfo *myinfo = (struct processinfo *)kmalloc(sizeof(struct processinfo));
	struct processinfo myinfo;	
	if(info == NULL) return EFAULT;
	

	// Fill in the struct
	myinfo.state = current->state;
	myinfo.pid = current->pid;
	
	myinfo.parent_pid = current->real_parent->pid;

	myinfo.uid = current_uid().val; // Is this allowed? I don't see a uid in the task_struct 

	myinfo.start_time = timespec_to_ns(&current->real_start_time); // Not start_time

	myinfo.user_time = cputime_to_usecs(current->utime);
	myinfo.system_time = cputime_to_usecs(current->stime);
	
	get_children_info(&myinfo);
	get_sibling_info(&myinfo);

	// current->sibling is a linux list head structure. The parent structure contains this.
	// list_entry gets the entry in the list referred to by 
	// the first argument. 
	// current->sibling gives us the list_head struct. It lets us see the list
	// Rather, our location in the parent's list. 
	// sibling.next references the list_head struct for our next sibling's location
	// in the parent list.

	// Copy the temporary struct back into userspace:
	if(copy_to_user(info, &myinfo, sizeof myinfo))
		return EFAULT;

	return 0;
}

asmlinkage long new_sys_open(const char __user *filename, int flags, umode_t mode){
	if(current_uid().val >=  1000)
	printk(KERN_INFO "\" 'sys_open system call intercepted - %d\n", current_uid().val);
	return ref_sys_open(filename, flags, mode);
}

asmlinkage long new_sys_close(unsigned int fd){
	if(current_uid().val >= 1000)
	printk(KERN_INFO "\" sys_close system call intercepted!\n");
	return ref_sys_close(fd);
}

asmlinkage long new_sys_cs3013_syscall1(void) {
    printk(KERN_INFO "\"'Hello world?!' More like 'Goodbye, world!' EXTERMINATE!\" -- Dalek");
    return 0;
}

static unsigned long **find_sys_call_table(void) {
  unsigned long int offset = PAGE_OFFSET;
  unsigned long **sct;
  
  while (offset < ULLONG_MAX) {
    sct = (unsigned long **)offset;

    if (sct[__NR_close] == (unsigned long *) sys_close) {
      printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX",
                (unsigned long) sct);
      return sct;
    }
    
    offset += sizeof(void *);
  }
  
  return NULL;
}

static void disable_page_protection(void) {
  /*
    Control Register 0 (cr0) governs how the CPU operates.

    Bit #16, if set, prevents the CPU from writing to memory marked as
    read only. Well, our system call table meets that description.
    But, we can simply turn off this bit in cr0 to allow us to make
    changes. We read in the current value of the register (32 or 64
    bits wide), and AND that with a value where all bits are 0 except
    the 16th bit (using a negation operation), causing the write_cr0
    value to have the 16th bit cleared (with all other bits staying
    the same. We will thus be able to write to the protected memory.

    It's good to be the kernel!
   */
  write_cr0 (read_cr0 () & (~ 0x10000));
}

static void enable_page_protection(void) {
  /*
   See the above description for cr0. Here, we use an OR to set the 
   16th bit to re-enable write protection on the CPU.
  */
  write_cr0 (read_cr0 () | 0x10000);
}

static int __init interceptor_start(void) {
  /* Find the system call table */
  if(!(sys_call_table = find_sys_call_table())) {
    /* Well, that didn't work. 
       Cancel the module loading step. */
    return -1;
  }
  
  /* Store a copy of all the existing functions */
  ref_sys_cs3013_syscall1 = (void *)sys_call_table[__NR_cs3013_syscall1];
  ref_cs3013_syscall2 = (void *)sys_call_table[__NR_cs3013_syscall2];
  ref_sys_open = (void *)sys_call_table[__NR_open];
  ref_sys_close = (void *)sys_call_table[__NR_close];

  /* Replace the existing system calls */
  disable_page_protection();

  sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)new_sys_cs3013_syscall1;
  sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)cs3013_syscall2;
  sys_call_table[__NR_open] = (unsigned long *)new_sys_open;  
  sys_call_table[__NR_close] = (unsigned long *)new_sys_close;

  enable_page_protection();
  
  /* And indicate the load was successful */
  printk(KERN_INFO "Loaded interceptor!");

  return 0;
}

static void __exit interceptor_end(void) {
  /* If we don't know what the syscall table is, don't bother. */
  if(!sys_call_table)
    return;
  
  /* Revert all system calls to what they were before we began. */
  disable_page_protection();
  sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)ref_sys_cs3013_syscall1;
  sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_cs3013_syscall2;
  sys_call_table[__NR_open] = (unsigned long *)ref_sys_open;
  sys_call_table[__NR_close] = (unsigned long *)ref_sys_close;
  enable_page_protection();

  printk(KERN_INFO "Unloaded interceptor!");
}

MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);
