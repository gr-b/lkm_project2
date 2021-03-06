#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>


// These values MUST match the syscall_32.tbl modifications:
#define __NR_cs3013_syscall1 355
#define __NR_cs3013_syscall2 356
#define __NR_cs3013_syscall3 357

#define doprocess \
	int pid; \
	if ((pid = fork()) < 0) { \
		fprintf(stderr, "Fork Error\n"); \
		exit(1); \
	} else if(pid == 0) \
	
#define process_end } wait(0);


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


long testCall1 ( void) {
       return (long) syscall(__NR_cs3013_syscall1);
}

long getprocessinfo (struct processinfo *myinfo) {

	return (long) syscall(__NR_cs3013_syscall2, myinfo);
}
//long testCall3 ( void) {
 //       return (long) syscall(__NR_cs3013_syscall3);
//}

/* print_process_info
 * Prints out the fields of the processinfo struct.
 */
void print_process_info(struct processinfo *myinfo){
	
	// Now print out the items in the processinfo struct we should have recieved:
	printf("State: %ld\n", myinfo->state);
	printf("pid: %u\n",  myinfo->pid);
	printf("parent_pid: %u\n", myinfo->parent_pid);
	printf("youngest_child: %u\n", myinfo->youngest_child);
	printf("younger_sibling: %u.\n", myinfo->younger_sibling);
	printf("older_sibling: %u\n", myinfo->older_sibling);
	printf("uid: %u\n",  myinfo->uid);
	printf("start_time: %lld\n", myinfo->start_time);
	printf("user_time: %lld\n", myinfo->user_time);
	printf("system_time: %lld\n", myinfo->system_time);
	printf("cutime: %lld, cstime: %lld\n", myinfo->cutime, myinfo->cstime);
}


int main () {
	printf("The return values of the system calls are:\n");
	printf("cs3013_syscall1: %ld\n", testCall1());

	// Create some child processes
	char** newArgv;
	newArgv[0] = "sleep";
	newArgv[1] = "2";
	
	int i, j;
	for(i=0;i<4;i++){
		doprocess{
			printf("Child Process%d: %d\n", i,getpid());
			execvp(newArgv[0], newArgv);
		}
	}

	struct processinfo *myinfo = (struct processinfo *)malloc(sizeof(struct processinfo));
        printf("Process info: %ld\n", getprocessinfo(myinfo));
	print_process_info(myinfo);
		
        //while(wait(0) > 0);
	wait(0);

        return 0;
}

