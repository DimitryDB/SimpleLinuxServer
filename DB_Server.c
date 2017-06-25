#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Globals
int gLockFileDesc=-1;
int logFileDesc = -1;
int gMasterSocket=-1;
const int DB_Server_Port=30333;
// pid stored inside work folder to make testing easy 
const char *const gLockFilePath = "/home/student/work/server/Db_Server.pid";
const char *const logFilePath = "/home/student/work/server/Db_Server.log";
// my log
void myLog(char *szText) 
{
    int fd;
    char buf[16*1024];
    sprintf(buf,"%5d -- %s\n", getpid(),szText);
    write(logFileDesc,buf,strlen(buf));
    
}
void cleanUp(void)
{
	if(gLockFileDesc!=-1)
		{
		close(gLockFileDesc);
		unlink(gLockFilePath);
		gLockFileDesc=-1;
		}
    if(gLockFileDesc!=-1)
		{
		close(gLockFileDesc);
        logFileDesc;
		gLockFileDesc=-1;
		}

	if(gMasterSocket!=-1)
		{
		close(gMasterSocket);
		gMasterSocket=-1;
		}
}
// signal handlers
void FatalSigHandler(int sig)
{
    myLog("Fatall Termintation");
	cleanUp();
	_exit(0);
}
void TermHandler(int sig)
{
    myLog("Normal Termintation");
	cleanUp();
	_exit(0);
}
// demonize function
int BecomeDaemonProcess(const char *const lockFileName,int *const lockFileDesc, int *const logFileDesc,int *const thisPID) 
{
    int	curPID,stdioFD,lockResult,killResult,lockFD,i,numFiles;
    char pidBuf[17],*lfs,pidStr[7];
    FILE *lfp;
    unsigned long lockPID;
    struct flock exclusiveLock;
    chdir("/");
    // check lock
    lockFD=open(lockFileName,O_RDWR|O_CREAT|O_EXCL,0644);
    if(lockFD==-1)
        {
        lfp=fopen(lockFileName,"r");
        if(lfp==0) /* Game over. Bail out */
            {
            perror("Can't get lockfile");
            return -1;
            }
        lfs=fgets(pidBuf,16,lfp);
        if(lfs!=0)
            {
            if(pidBuf[strlen(pidBuf)-1]=='\n')
                pidBuf[strlen(pidBuf)-1]=0;
            lockPID=strtoul(pidBuf,(char**)0,10);
            killResult=kill(lockPID,0);
            if(killResult==0)
                {
                printf("\n\nERROR\n\nA lock file %s has been detected. It appears it is owned\nby the (active) process with PID %ld.\n\n",lockFileName,lockPID);
                }
            else
                {
                if(errno==ESRCH) /* non-existent process */
                    {
                    printf("\n\nERROR\n\nA lock file %s has been detected. It appears it is owned\nby the process with PID %ld, which is now defunct. Delete the lock file\nand try again.\n\n",lockFileName,lockPID);
                    }
                else
                    {
                    perror("Could not acquire exclusive lock on lock file");
                    }
                }
            }
        else
            perror("Could not read lock file");
        fclose(lfp);
        return -1;
        }
    // preform lock
    exclusiveLock.l_type=F_WRLCK;
    exclusiveLock.l_whence=SEEK_SET;
    exclusiveLock.l_len=exclusiveLock.l_start=0; 
    exclusiveLock.l_pid=0;
    lockResult=fcntl(lockFD,F_SETLK,&exclusiveLock);
    if(lockResult<0) /* can't get a lock */
        {
        close(lockFD);
        perror("Can't get lockfile");
        return -1;
        }
    // Fork
	curPID=fork();
	switch(curPID)
		{
		case 0:
		  break;
		case -1: 
		  fprintf(stderr,"Error: initial fork failed: %s\n",
					 strerror(errno));
		  return -1;
		  break;
		default: 
          printf("Starter shutdown\n");  
		  exit(0);
		  break;
		}
	if(setsid()<0)
		return -1;
    // second fork (make process session leader)
    curPID=fork();
    	switch(curPID)
		{
		case 0:
		  break;
		case -1: 
		  fprintf(stderr,"Error: secondary fork failed: %s\n",
					 strerror(errno));
		  return -1;
		  break;
		default: 
		  exit(0);
		  break;
		}
	if(ftruncate(lockFD,0)<0)
		return -1;
    // store pid in lock file
    sprintf(pidStr,"%d\n",(int)getpid());
    write(lockFD,pidStr,strlen(pidStr));
    *lockFileDesc=lockFD;
    // close all unnesesary descriptors
	numFiles = sysconf(_SC_OPEN_MAX); 
	for(i=numFiles-1;i>=0;--i)
		{
		if(i!=lockFD && i!=*logFileDesc) 
			close(i);
		}
    stdioFD=open("/dev/null",O_RDWR); 
	dup(stdioFD); 
	dup(stdioFD); 
    setpgrp();
    myLog("Daemon Started");
    return 0;
}
int ConfigureSignalHandlers(void) 
{
    struct sigaction sigtermSA;
    // ignore
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);	
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGURG, SIG_IGN);
	signal(SIGXCPU, SIG_IGN);
	signal(SIGXFSZ, SIG_IGN);
	signal(SIGVTALRM, SIG_IGN);
	signal(SIGPROF, SIG_IGN);
	signal(SIGIO, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
    // Fatall
    signal(SIGQUIT, FatalSigHandler);
	signal(SIGILL, FatalSigHandler);
	signal(SIGTRAP, FatalSigHandler);
	signal(SIGABRT, FatalSigHandler);
	signal(SIGIOT, FatalSigHandler);
	signal(SIGBUS, FatalSigHandler);
    signal(SIGFPE, FatalSigHandler);
	signal(SIGSEGV, FatalSigHandler);
	signal(SIGSTKFLT, FatalSigHandler);
	signal(SIGCONT, FatalSigHandler);
	signal(SIGPWR, FatalSigHandler);
	signal(SIGSYS, FatalSigHandler);
    //sigterm
    sigtermSA.sa_handler=TermHandler;
	sigemptyset(&sigtermSA.sa_mask);
	sigtermSA.sa_flags=0;
	sigaction(SIGTERM,&sigtermSA,NULL);
    myLog("Handlers Configured");
    return 0;
    
}
int BindPassiveSocket(const int portNum, int *const boundSocket)
{
    struct sockaddr_in sin;
	int	newsock,optval;
	size_t optlen;
    memset(&sin.sin_zero, 0, 8);
    // setup socket structure 
    sin.sin_port=htons(portNum);
	sin.sin_family=AF_INET;
	sin.sin_addr.s_addr=htonl(INADDR_ANY);
    // open socket
    if((newsock=socket(PF_INET, SOCK_STREAM, 0))<0)
		return -1;
    // bind socket
    if(bind(newsock,(struct sockaddr*)&sin,sizeof(struct sockaddr_in))<0)
		return -1;
    *boundSocket=newsock;
    myLog("Socket Bound");
    if(listen(newsock,10)<0)
		return -1;
    return 0;
}

int main(int argc,char *argv[]) {
    int result, accCon;
	pid_t daemonPID, curPID;
    struct sockaddr_in	client;
    socklen_t clilen;
    clilen=sizeof(client);
    
    logFileDesc = open (logFilePath, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if((result=BecomeDaemonProcess(gLockFilePath,&gLockFileDesc,&logFileDesc,&daemonPID))<0)
		{
		printf("Failed to become daemon process\n");
		exit(result);
		}
    if((result=ConfigureSignalHandlers())<0)
		{
		myLog("ConfigureSignalHandlers failed!");
        cleanUp();
		exit(result);
		}
    if((result=BindPassiveSocket(DB_Server_Port,&gMasterSocket))<0)
		{
		myLog("BindPassiveSocket failed");
		cleanUp();
		exit(result);
		}
    for(;;) 
    {
        accCon = accept(gMasterSocket,(struct sockaddr *)&client,&clilen);
        if(accCon<0)
		{
			myLog("Accept Connections failed");
			cleanUp();
			exit(result);
		}
        else if (accCon>0)
        {
           curPID=fork();
           if(curPID==0) {
                int curCon = accCon;
                char buf[16*1024];
                myLog("Connection Accepted");
                read(curCon,buf, sizeof(buf));
                myLog(buf);
                write(curCon, "OK",2);
                close(curCon);
                myLog("Connection Closed");
                return 0;
           }
           else
              continue;
        }
        else 
            continue;
    }
}
