/********************************************************************
Copyright 2010-2017 K.C. Wang, <kwang@eecs.wsu.edu>
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

/********************
#define  SSIZE 1024
#define  NPROC  9
#define  FREE   0
#define  READY  1
#define  SLEEP  2
#define  BLOCK  3
#define  ZOMBIE 4
#define  printf  kprintf
 
typedef struct proc{
  struct proc *next;
  int    *ksp;         // at  4
  int    *usp;         // at  8
  int    *upc;         // at 12
  int    *cpsr;        // at 16
  int    *pgdir;       // ptable address

  int    status;
  int    priority;
  int    pid;
  int    ppid;
  struct proc *parent;
  int    event;
  int    exitCode;
  char   name[32];     // name string  
  int    kstack[SSIZE];
}PROC;
***************************/
extern PROC *kfork();
PROC proc[NPROC], *freeList, *readyQueue, *sleepList, *running;

int procsize = sizeof(PROC);

char *pname[NPROC]={"sun", "mercury", "venus", "earth", "mars", "jupiter",
                     "saturn","uranus","neptune"};

u32 *MTABLE = (u32 *)0x4000;  // P0's ptable at 16KB
int kernel_init()
{
  int i, j; 
  PROC *p; char *cp;
  int *MTABLE, *mtable;
  int paddr;

  kprintf("kernel_init()\n");
  for (i=0; i<NPROC; i++){
    p = &proc[i];
    p->pid = i;
    p->status = FREE;
    p->priority = 0;
    p->ppid = 0;

    strcpy(p->name, pname[i]);
    p->next = p + 1;
  }
  proc[NPROC-1].next = 0;
  freeList = &proc[0];
  readyQueue = 0;
  sleepList = 0;
  running = getproc();
  running->status = READY;

  printList(freeList);
}

int scheduler()
{
  char line[8];
  int pid; PROC *old=running;
  char *cp;
  kprintf("proc %d in scheduler\n", running->pid);
  if (running->status==READY)
     enqueue(&readyQueue, running);
  printQ(readyQueue);
  running = dequeue(&readyQueue);

  kprintf("next running = %d\n", running->pid);
  pid = running->pid;
  if (pid==1) color=WHITE;
  if (pid==2) color=GREEN;
  if (pid==3) color=CYAN;
  if (pid==4) color=YELLOW;
  if (pid==5) color=BLUE;
  if (pid==6) color=PURPLE;
  if (pid==7) color=RED;

  // must switch to new running's pgdir; possibly need also flush TLB
  if (running != old){
    printf("switch to proc %d pgdir at %x ", running->pid, running->pgdir);
    printf("pgdir[2048] = %x\n", running->pgdir[2048]);
    switchPgdir((u32)running->pgdir);
  }
}

/*---------------------*/
int ksleep(int event)
{
  int sr = int_off();
  printf("proc %d going to sleep on event=%d\n", running->pid, event);

  running->event = event;
  running->status = SLEEP;
  enqueue(&sleepList, running);
  printSleepList(sleepList);
  tswitch();
  int_on(sr);
}

int kwakeup(int event)
{
  PROC *temp, *p;
  temp = 0;
  int sr = int_off();

  printSleepList(sleepList);

  while (p = dequeue(&sleepList))
  {
    if (p->event == event)
    {
      printf("wakeup %d\n", p->pid);
      p->status = READY;
      enqueue(&readyQueue, p);
    }
    else
    {
      enqueue(&temp, p);
    }
  }
  sleepList = temp;
  printSleepList(sleepList);

  
  int_on(sr);
}


int kexit(int exitValue)
{

  int i;
  PROC *p;

  for (i = 2; i < NPROC; i++)
  { // Skip root proc[0] and proc[1]
    p = &proc[i];

    /// Child sent to parent (orphanage)
    if (p->status != FREE && p->ppid == running->pid)
    {
      if (p->status == ZOMBIE)
      {
        p->status = FREE;
        enqueue(&freeList, p); //Release all ZOMBIE childs
      }
      else
      {
        printf("Send child &d to proc[1] (parent)\n", p->pid);
        p->ppid = 1;
        p->parent = &proc[1];
      }
    }
  }

  running->exitCode = exitValue; // Record exitValue in proc for parent
  running->status = ZOMBIE;      // Will not free PROC
  //enqueue(&zombieList, p);

  kwakeup(&proc[1]);
  kwakeup(running->parent);

  tswitch(); //Switch process to give up CPU
}

int kwait(int *status)
{ /// proc waits for (and disposes of) a ZOMBIE child.
  int i, hasChild = 0;
  PROC *p;

  while (1)
  {
    for (i = 1; i < NPROC; i++)
    {
      p = &proc[i];

      /// Search for (any) zombie child
      if (p->status != FREE && p->ppid == running->pid)
      {
        hasChild = 1;
        if (p->status == ZOMBIE)
        {
          //dequeue(&zombieList);
          *status = p->exitCode; // Get exit code
          p->status = FREE;      // Set status to Free
          enqueue(&freeList, p); // release child PROC to freelist
          return (p->pid);       // Return pid
        }
      }
    }
    if (!hasChild)
    {
      printf("[!] - NO CHILD FOUND / P1 NO DIE\n");
      return -1;
    }
    ksleep(running);
  }
}
