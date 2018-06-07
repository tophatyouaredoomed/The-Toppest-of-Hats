#include <iostream>
#include <iomanip>
#include <cstdafx>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <dirent.h>
#include <vector>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <pthread>

//#include "cgicc/CgiDefs.h"
//#include "cgicc/Cgicc.h"
//#include "cgicc/HTTPHTMLHeader.h"
//#include "cgicc/HTMLClasses.h"

using namespace std;


int log = NULL;
int i = 0;
int go;

char *kill = "no";

string fileName;
string wifi;
string word ("Nothing wrong here...");
string user;
string usr (NULL);
string entry (NULL);
string System (NULL);
string object (NULL);
string Object;
string operation;
string initial_Src (NULL);
string terminal_Src (NULL);
const char* file_Name;

//Surprise! No external libraries used here aside from GNU CGI and a few Linux headers. Even that hasn't been implemented yet.
//Finally integrated that 'test.cpp' file; that's another task done.
//Enjoy. More edits may be coming soon...

//This may be used in later versions of the system, if I ever get a GUI (HTML) running...
//using namespace cgicc;
//Cgicc cgi;


bool Object_exists(const char* fileName, string object)
{
    char *object = object.c_str();
    fileName = object;
    std::ifstream infile(fileName);
    return infile.good();
}


char delete_Object( object )
{  
object = Object;
detect_File_existence();

remove ( Object );

  if( remove( Object ) != 0 )
  {
    perror( "Error deleting file" );
    Object = NULL;
    object = NULL;
  }
  else
  {
    puts( "File successfully deleted" );
  }

redo();
	
}

char copy_Object(string object)
{
object = Object;
detect_File_existence();

file_Name = Object;

  ifstream input(Object);
  stringstream file_Content;

  while(input >> file_Content);

  cout << file_Content.str() << endl;
  
redo();
  
}


char paste_Object(string object)
{
string result = object+file_Name;

detect_File_existence(result);

ofstream paste_file(result);
paste_file << file_Content;
fclose (result);

redo();

}


char move_Object(string object)
{
std::stringstream split(object);
std::string segment;
std::vector<std::string> seglist;

while(std::getline(split, segment, '|'))
{
   seglist.push_back(segment);

string initial_Src = seglist.front();
string terminal_Src = seglist.back();
}

string srcName  = initial_Src;
string destName = terminal_Src;
    
      int  ret       = rename(srcName.c_str(), destName.c_str());
    
      if   (ret == -1) { cout << "Unable to rename file: "        << srcName ; }
      else             { cout << "Successfully renamed file to: " << destName; }
    
redo();

}


char new_Object( object )
{
char *object = object.c_str();
std::stringstream split(object);
std::string segment;
std::vector<std::string> seglist;

while(std::getline(split, segment, '|'))
{
   seglist.push_back(segment);

initial_Src == seglist.front();
blank == seglist.back();
}

result = initial_Src+blank;

ofstream newFile (result);

object = result;
detect_File_existence();

int remove ( object );

  if( remove( object ) != 0 )
    perror( "Error completing directory clear" );
  else
    puts( "Directory successfully created" );

redo();

}


char rename_Object( object )
{
std::stringstream split(object);
std::string segment;
std::vector<std::string> seglist;

while(std::getline(split, segment, '|'))
{
   seglist.push_back(segment);

initial_Src == seglist.front();
terminal_Src == seglist.back();
}

char oldname[] = initial_Src;
char newname[] = terminal_Src;

check= rename ( const char * oldname, const char * newname );

if ( check == 0 )
    puts ( "File successfully renamed" );
  else
    perror( "Error renaming file" );
	
redo();

}


int get_Object_info(int argc, char *argv[])
{
    struct stat sb;

   if (argc != 2) {
        fprintf(stderr, "Usage: %s <pathname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

   if (stat(argv[1], &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

   printf("File type:                ");

   switch (sb.st_mode & S_IFMT) {
    case S_IFBLK:  printf("block device\n");            break;
    case S_IFCHR:  printf("character device\n");        break;
    case S_IFDIR:  printf("directory\n");               break;
    case S_IFIFO:  printf("FIFO/pipe\n");               break;
    case S_IFLNK:  printf("symlink\n");                 break;
    case S_IFREG:  printf("regular file\n");            break;
    case S_IFSOCK: printf("socket\n");                  break;
    default:       printf("unknown?\n");                break;
    }

   printf("I-node number:            %ld\n", (long) sb.st_ino);

   printf("Mode:                     %lo (octal)\n",
            (unsigned long) sb.st_mode);

   printf("Link count:               %ld\n", (long) sb.st_nlink);
    printf("Ownership:                UID=%ld   GID=%ld\n",
            (long) sb.st_uid, (long) sb.st_gid);

   printf("Preferred I/O block size: %ld bytes\n",
            (long) sb.st_blksize);
    printf("File size:                %lld bytes\n",
            (long long) sb.st_size);
    printf("Blocks allocated:         %lld\n",
            (long long) sb.st_blocks);

   printf("Last status change:       %s", ctime(&sb.st_ctime));
    printf("Last file access:         %s", ctime(&sb.st_atime));
    printf("Last file modification:   %s", ctime(&sb.st_mtime));
    
   redo();
   return 0;
}


// Is it void? Is it char? Nobody knows...

read_sys_usage()
{

cout<<"Gathering: system and user CPU times used, integral shared memory size, integral unshared data size and stack size values, block input /output operations, and IPC messages sent and received..." endl;

int getrusage(int who, struct rusage *usage);

struct rusage
{
struct timeval ru_stime; /* system CPU time used */
struct timeval ru_utime; /* user CPU time used */
long   ru_ixrss;         /* integral shared memory size */
long   ru_idrss;         /* integral unshared data size */
long   ru_isrss;         /* integral unshared stack size */
long   ru_inblock;       /* block input operations */
long   ru_oublock;       /* block output operations */
long   ru_msgsnd;        /* IPC messages sent */
long   ru_msgrcv;        /* IPC messages received */
long   ru_nsignals;      /* signals received */
}

cout<<"Gathering: /proc/self/ CPU Usage values..." endl;

static string memory_usage()
{
ostringstream mem;
PP("hi");
ifstream proc("/proc/self/status");
string s;
while(getline(proc, s), !proc.fail())
{
if(s.substr(0, 6) == "VmSize")
{
mem << s;
return mem.str();
}
}
return mem.str();
}

redo();

}


char file_Search( object )
{
std::stringstream split(object);
std::string segment;
std::vector<std::string> seglist;

while(std::getline(split, segment, '|'))
{
   seglist.push_back(segment);

searchbar == seglist.front();
filetype == seglist.back();
}

system($ find /home searchbar filetype)

if(filetype=="within 7")
{
system(find /home searchbar -atime +7);
system(find /home searchbar -atime 7);
system(find /home searchbar -atime -7);
system(stat --format=%n | %s | %d | %p | %A | %M |  *);
}

else if(filetype=="more than 7")
{
system(find /home searchbar -mtime -7);
system(stat --format=%n | %s | %d | %p | %A | %M |  *);
}

else
{
cout << "Huh?!?" endl;
}

redo();

}

//Main feature - dirList!
//Created by TopHatProductions115, with assistance from cpp.sh and Ramith on Bytes.com

int dirList ( const char* path )
{

cout << "Please wait...";

DIR *pdir = NULL;
pdir = opendir (path);
struct dirent *pent = NULL;

if (pdir == NULL)
{

cout << "\n ERROR: variable ::pdir not initialized correctly.";
return 1;

}

while ((pent = readdir (pdir)))
{

if (pent == NULL)
{

cout << "\n ERROR: variable ::pent not initialized correctly.";
return 1;

}

cout << pent->d_name << endl;

}

closedir (pdir);

cin.get ();

redo();
}

bool directoryExistense( const char* path )
{    
    if ( path == NULL) return false;
 
    DIR *pdir = NULL;
    bool direxist = false;
    
    cout << "\n Sorry, but the given path is invalid (404)";
 
    pdir = opendir (path);
 
    if (pdir != NULL)
    {
        direxist = true;    
    
        cout << "\n Opening...";
    
        //This would be the regular program conclusion - (void) closedir (pdir);

	dirList( path );
    }
 
    return direxist;
}

int dir_List( object )
{
    string object;  
    const char* path = object.c_str();
    
    directoryExistense( path );
	redo();
}

//End main feature

//Threading manager

void checkThreadcount()
{
if (i >= 7)
{
cout << "Sorry, but you aren't allowed to create anymore threads. Please kill a process to try again." endl;
redo();
}

else if (i <= 7)
{
cout << "Good to go!" endl;
}

else
{
cerr << "What the?!?" endl;
cout << "Hang on..." endl;
system("killall -w");
main();
}

}


//Open Programs

void prgopen(object)
{
proc = pthread_create(&proc3, NULL, system, (int *) object);
proc = pthread_attr_setdetachstate (PTHREAD_CREATE_DETACHED);
proc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
i++;
redo();
}

//Threading attempt

void killThread(object)
{
if (object == "one")
{
proc = pthread_detach(pthread_t proc1);
proc = pthread_cancel(pthread_t proc1);
i--
redo();
}

else if (object == "two")
{
proc = pthread_detach(pthread_t proc2);
proc = pthread_cancel(pthread_t proc2);
i--
redo();
}

else if (object == "three")
{
proc = pthread_detach(pthread_t proc3);
proc = pthread_cancel(pthread_t proc3);
i--
redo();
}

else
{
cout << "That process does not exist. Please check your entry." endl;
redo();
}
}


void add_FunctionToPool(object)
{
proc = pthread_create(&proc1, NULL, exec_nativeFunction, (void *) object);
proc = pthread_attr_setdetachstate (PTHREAD_CREATE_DETACHED);
proc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
i++;
redo();
}


void exec_nativeFunction(object)
{
void (*call) (int);
cout << "Grabbing function: " << object << endl;
call = &object;
}


void pause_mainThread(object)
{
sleep_for( object );
redo();
}


void add_tempBinToPool(object)
{
proc = pthread_create(&proc2, NULL, exec_tempBinary, (void *) object);
proc = pthread_attr_setdetachstate (PTHREAD_CREATE_DETACHED);
proc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
i++;
redo();
}


void exec_tempBinary(object)
{
std::stringstream split(object);
std::string segment;
std::vector<std::string> seglist;

while(std::getline(split, segment, '|'))
{
   seglist.push_back(segment);

fpath == seglist.front();
fcontent == seglist.back();
}

using std::fstream;
fstream dcf;
dcf.open(fpath,std::ios::out);
dcf << fcontent;
dcf.close();

system("gcc $fpath >>.cpp -o $fpath >>.so -shared -fPIC ");
ext = ".so";
compile += fpath+ext;
dlopen(compile, RTLD_LAZY);
}


//System management

void system_shutdown()
{

if (System == "shutdown")
{
sleep(2);

cout << "Shutting down..." << endl;
continue_shutdown();
}

else
{

cout << "Are you sure that you wish to shutdown your O.S.? (enter yes or no)"<< endl;
cout << "Any unsaved work will be lost if you procede.";

cin>>(System);

if (System == "yes")
{
System = "continue";
continue_shutdown();
}

else if (System == "no")
{
cout << "Returning to main thread...";
SystemX();
}

else
{
cout << "System call error detected. Recalling system viewer..."
cout << "Returning to main thread...";
SystemX();
}

}

}


int continue_shutdown()
{
if (System == "continue")
{
System = "shutdown";
go = -1;
log = 2;

system = pthread_cancel(system);
logger = pthread_cancel(logger);
chrono = pthread_cancel(chrono);
clwncm = pthread_cancel(clwncm);

system("System\Tools\Sys_Tools\createAccount");

return(0);
exit(EXIT_SUCCESS);
}

else if (System == "go")
{
cout << "Returning to main thread..." endl;
SystemX();
}
}


int wncm ()
{
clwncm = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

wimax:cout << "Configuring Wireless Network Connection Monitor..." endl;
if (go == 0)
{
while (go == 0)
	{
	wifi = system("# iwconfig wlan0 | grep -i --color quality");
	cout << wifi;
	wifi = system("# iwconfig wlan0 | grep -i --color signal");
	cout << wifi;
	}
goto wimax;
}

else if (go == 1)
{
	wifi = "Shutting down wireless connection monitor";
	go = NULL;
	wifi = NULL;
	exit (0);
}

else if (go == 2)
{
system("/System/Tools/Sys_Tools/TXP-WNCM");
}

else
{
	wifi = "ERR:_SYS_PARAM_ERROR_DETECTED. REBOOTING.";
	cout << wifi;
	go = 0;
	clwncm = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
	goto wimax;
}
}


const string currentDateTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}


int moment()
{
	shout:cout << "Getting System Clock online..."
	chrono = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	
	if (go == 0)
	{
    cout << "currentDateTime()=" << currentDateTime() << endl;
    getchar();  // wait for keyboard input
	}
	
	else if (go == 1)
	{
    cout << "currentDateTime()=" << currentDateTime() << endl;
    getchar();  // wait for keyboard input
	}
	
	else if (go == 2)
	{
    cout << "currentDateTime()=" << currentDateTime() << endl;
    getchar();  // wait for keyboard input
	}
	
	else
	{
	word = "ERR:_SYS_PARAM_ERROR_DETECTED. REBOOTING.";
	cout << word;
	go = 0;
	chrono = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
	goto shout;
	}
}


void logg(entry)
{
user = usr;

if (log = 0)
{
logUsr();
}

else if (log = 1)
{
  ofstream enter;
  enter.open ("System\Log\log.log", ios::app);
  enter << entry;
  enter.close();
  log = 0;
  logUsr;
}

else
{
cout << "That doesn't seem right; hold on..." endl;
logUsr();
}
}


void logUsr()
{
ifstream t("\System\curusr.txt");

t.seekg(0, ios::end);   
usr.reserve(t.tellg());
t.seekg(0, ios::beg);

usr.assign((istreambuf_iterator<char>(t)),
            istreambuf_iterator<char>());

if (usr == NULL)
{
cout << "SYS_ERROR: INVALID_USER_DETECTED. SHUTTING DOWN.";
System = "shutdown";
system_shutdown();
system("\System\Tools\Sys_Tools\createAccount");
}

else
{			

if (log = NULL)
{
logger = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
cout << "User detected: " << usr << endl;
cout << "If this isn't you, logout ASAP. Your actions are logged for root view..." endl;
logg(usr);
}

else if (log = 0)
{
cout << "Returning..." endl;
logg();
}

else if (log = 1)
{
logg(entry);
}

else if (log = -1)
{
log = 1;
logg(entry);
}

else
{
logger = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
cerr << "Whoa! Something's off!" endl;
cout << "Hang on..." endl;
log = 0;
entry = "SYSTEM_ERROR_ENCOUNTERED. RELOADING.";
logg();
}

}
}


int sys_init ()
{
cout << "System Root-level Shell Interface is initiating..." endl;
cout << "System Initialization in progress. " << i << "of 7 system processes." endl;
system = pthread_create(&system, NULL, SystemX, (void *) kill);
i++;
cout << "System Initialization in progress. " << i << "of 7 system processes." endl;
logger = pthread_create(&logger, NULL, logUsr, (int *) kill);
i++;
cout << "System Initialization in progress. " << i << "of 7 system processes." endl;
chrono = pthread_create(&chrono, NULL, moment, (int *) kill);
i++;
cout << "System Initialization in progress. " << i << "of 7 system processes." endl;
clwncm = pthread_create(&clwncm, NULL, wncm, (int *) kill);
i++;

log = 0;

cout << "Main processes initiated. Performing system application system boot." endl;
cout << "Welcome to the TXP-VOS File-Seeker (FileSurfer 2.5) & EXP-Processor." endl;
cout << "Please wait, while your V.O.S. copy loads onto the system." endl;
cout << "Opening system application portal..." endl;

return 0;
}


//Command line parser

void switcher ( object, operation )
{
switch (operation)
{
case detectObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
Object_exists( object );
break;

case removeObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
copy_Object( object );
break;

case copyObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
copy_Object( object );
break;

case pasteObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
paste_Object( object );
break;

case moveObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
move_Object( object );
break;

case createObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
new_Object( object );
break;

case renameObject:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
rename_Object( object );
break;

case getInfo:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
get_Object_info();
break;

case sysUsage:
object = "system CPU";
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
read_sys_usage();
break;

case Objectsearch:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
file_Search( object );
break;

case dirList:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
dir_List( object );
break;

case prgOpen:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
prgopen( object );
break;

case killThread:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
killThread( object );
break;

case pauseThreadmain:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
pause_mainThread( object );
break;

case callNativebinarytoPool:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
add_FunctionToPool( object );
break;

case DynamicFileCompiler:
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
add_tempBinToPool( object );
break;

case Session:
System = "shutdown";
object = "system";
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
log = 1;
SystemX();
break;

case sysShutdown:
System = "shutdown";
object = "system";
entry = user + 'attempts: -' + operation + '- on: -' + object + '- .';
system_shutdown();
pthread_exit(NULL);
system("\System\Tools\Sys_Tools\createAccount");
return 0;
break;

default:
cout << "ERR: INVALID_REQUEST." endl;
SystemX();
break;
}
}


void redo()
{
cout << "Let me check something..." endl;

if (System == "go")
{
cout << "Ready to go!" endl;
System = "go";
go = 0;
SystemX();
}

else
{
cerr << "Something's terribly wrong. Sorry, but gotta go. :(" endl;
system = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
System = "shutdown";
go = -1;
system_shutdown();
system("System\Tools\Sys_Tools\createAccount");
}
}


int SystemX(int argc, char **argv)
{

if (System == "shutdown")
{
system = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, PTHREAD_CANCEL_DISABLE);
system_shutdown();
system("\System\Tools\Sys_Tools\createAccount");
}

if (System == "go")
{
System = "go";
system = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

cout << "Opening system application portal..." endl;

cout << "Please enter a valid object." endl;
obj:getline(cin, object);
cout << "Please enter a valid operation and/or command." endl;
opt:getline(cin, operation);

if (object == NULL)
{
cout << "This field cannot be NULL. Please enter a valid object." endl;
goto obj;
}

if (operation == NULL)
{
cout << "This field cannot be NULL. Please enter a valid operation and/or command." endl;
goto opt;
}

else
{
switcher( object, operation );
}

}

else
{
cout << "Something's off; hold on..." endl;
System = "go";
redo();
}

}


int main()
{

cout << "BOOT_SIGNAL_DETECTED." endl;
cout << "Starting system processes..." endl;
System = "go";
sys_init();
SystemX(int argc, char **argv);
}