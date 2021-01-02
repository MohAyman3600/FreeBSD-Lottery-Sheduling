#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/time.h>
#include <sys/umtx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

struct gift_args
{
	int tickets;
	pid_t proc_id;
	int flag;
};

/* Create system call function gift*/
int sys_gift(struct thread *td, struct gift_args *args){

	
	struct thread *td;

	struct proc *giver;
	struct proc *recver;

	giver = td->proc;
	if(giver == NULL){
		return 0;
	}

	int giver_local_tickets = 0;
	int giver_trans_tickets = 0;
	int giver_threads = 0;

	FOREACH_THREAD_IN_PROC(giver, td){
		giver_local_tickets += td->tickets.local;
		giver_trans_tickets += td->tickets.trans;
		giver_threads++;
	}

	if(args->tickets ==0 && args->proc_id == 0){
		printf("Giver process have %d transferable tickets\n",giver_trans_tickets );
	}
  
  	/* find reciver process */
	int reccver_pid = args->proc_id;
	recver = pfind(recver_pid);
	if(recver == NULL){
		return 0;
	}

	PROC_UNLOCK(recver);

	if((giver->p_ucred->cr_uid == 0) || (recver->p_ucred->cr_uid == 0)){
		return 0;
	}


	int recver_tickets = 0;
	int recver_threads = 0;

	FOREACH_THREAD_IN_PROC(recver, td){
		recver_tickets += (td->tickets.local+td->tickets.trans);
		recver_threads++;
	}

	/* Check if reciver can hold all the gift */
	if(args->tickets + recver_tickets > 100000 * recver_threads){
		printf("Reciver Can not hold all gifted tickets\n");
		return 0;
	}

	/* Check if giver have enough transferable tickets to give */
	giver_trans_tickets -= giver_threads; /* each thread should at least have one ticket */
	if(giver_trans_tickets < args->tickets){
		printf("Giver process only have %d transferable tickets\n",giver_trans_tickets );
		return 0;
	}	

	if (giver_trans_tickets ==0)
	{
		return 0;
	}

	/*take the tickets from the giver process */
	int deducted_tickets = args->ticket;
	int deducted_portion = args->tickets / giver_threads; /* distripute deduction equally among threads */
	int deduced; 
	FOREACH_THREAD_IN_PROC(giver, td){
		if(deducted_tickets > 0 ){ /* if there is more tickets to deduct */
			td->tickets.trans -= deducted_portion;
			if (td->tickets.trans < 0) /* check if the thread have tickets less than his deducted_portion */ 
			{
				deduced = (td->tickets.trans - deducted_portion) - td->tickets.trans; /* reserve the amount of deduced tickets */
				deducted_portion += -1*td->tickets.trans; /* carry on the reminded tickets to be deduced from net thread */
				td->tickets.trans = 0;
			}else{
				deducted_portion = args->tickets / giver_threads;
				deduced = deducted_portion;
			}
			deducted_tickets -= deduced; /* decrement total tickets to be deduced */
		}
	}

	/* give tickets to reciver process  (same as deduction but reversed)*/
	int rcved_tickets = args->ticket;
	int rcved_portion = args->tickets / recver_threads; /* distripute tickets equally among threads */
	int rceived; 
	FOREACH_THREAD_IN_PROC(recver, td){
		if (flag) /* flag to check if given tickets are transferable or not */
		{
			td->tickets.trans += rcved_portion;
			if(td->tickets.trans >= 100000){
				rceived = rcved_portion - (td->tickets.trans -100000);
				rcved_portion += td->tickets.trans -100000; /* carry on the reminded tickets to be deduced from net thread */
				td->tickets.trans = 100000;
			}else{
				rcved_portion = args->tickets / recver_threads;
			}
		}else{
			td->tickets.trans += rcved_portion;
			if(td->tickets.local >= 100000){
				rceived = rcved_portion - (td->tickets.local -100000);
				rcved_portion += td->tickets.local -100000; /* carry on the reminded tickets to be deduced from net thread */
				td->tickets.local = 100000;
			}else{
				rceived = rcved_portion
				rcved_portion = args->tickets / recver_threads;
			}

			rcved_tickets -= rceived;
		}
		 
	}

}