#include <stdio.h>
#include <string.h>



typedef struct System {
	char Super[] = "";
	
	int Ring;
	// Can be 0, 1, or 2
	
	bool isActionable;
	bool isActive = true;
	bool isRunning = true;
	
	char Type[128];
	// Can be either Device, Resource, User, SubSystem, or IO
	char SubType[128];
	// Can be either FileSystem(FS), Memory(MemMan/MMS), Device(DeviceMan/DMS)*, RunTime(RTMan/RMS), User(UserMan/UMS), IO(IOMan/IOMS)*, or SHell
	char Subsystems[1024];
	// What Subsystems are a part of this component?
	
	char Name[128];
	char Version[64];
} System;

typedef struct SubSystem {
	char Super[] = "System";
	
	int Ring = 1;
	// Cannot be 0 - only System and Kernel can
	
	bool isActionable = true;
	bool isActive = true;
	bool isRunning;
	
	char Type[] = "SubSystem";
	char SubType[128];
	char Subsystems[1024];
	
	bool isUser;
	
	char Name[128];
	char Version[64];
} SubSystem;



typedef struct Group {
    char Super[] = "SubSystem";
    
    bool isUsrBacked;
    bool isAppBacked;
    bool isSysBacked;
    
	bool canRead;
	bool canEdit;
	bool canExec;
	bool canElevate;
    
    bool isTagRestricted;
    // Is this Group restricted to only working with appropiately-tagged items?
	
	char Tags[4096];
	// Can only be edited by Admin or higher...
    
    bool isDisabled;
    
    char Name[128];
    char Password[128];
	char Decription[4096];
} Group;

typedef struct User {
	char Super[] = "Group";
	
	//bool isGuest;
	//bool isUser;
	//bool isAdmin;
	//bool isRoot;
	
	bool isUsrBacked;
	bool isAppBacked;
	bool isSysBacked;

	bool canRead;
	bool canEdit;
	bool canExec;
	bool canElevate;
	
    bool isTagRestricted;
    // Is this User restricted to only working with appropiately-tagged items?
	
	char Tags[4096];
	// Can only be edited by Admin or higher...
    
	bool isOnline;
	bool isDisabled;
	
	char Username[128];
	char Password[128];
	char Decription[4096];
} User;



typedef struct MemoryRequest {
	char Super[] = "SubSystem";
	
	bool forBin;
	bool forTxt;
	bool forExe;
	bool forStk;
	bool forHeap;
	
	bool isReserved;
	// Will it be in use immediately, or reserved for a later time?
	
	// Variable for size of requested memory section goes here
	
	// Variable for pointer to starting memory address goes here
	// Variable for pointer to memory address at end of section goes here
	
	char Owner[128];
	// Is whoever requested the memory in the first place
	
	int SID;
	char SName[128];
} MemoryRequest;

typedef struct Section {
	char Super[] = "MemoryRequest";
	
	bool Bin;
	bool Txt;
	bool Exe;
	bool Stk;
	bool Heap;
	
	bool isReserved;
	bool isInUse;
	// Is it actively being used, or is it awaiting further activity?
	bool isDead;
	// For sections that have fulfilled their duty, and await removal from memory
	bool isOrphanned
	// For sections, whose owner(s) have left them without properly terminating them
	// Primarily used by System
	
	// Variable for size of memory section goes here
	
	// Variable for pointer to starting memory address goes here
	// Variable for pointer to memory address at end of section goes here
	
	char Owner[128];
	// Is whoever requested the memory in the first place
	
	int SID;
	char SName[128];
} Section;



typedef struct Process {
	char Super[] = "SubSystem";
	
	bool isSysInit;
	bool isAppInit;
	bool isUsrInit;
	
	char Owner[128];
	
	//bool isGuest;
	//bool isUser;
	//bool isAdmin;
	//bool isRoot;
	
	bool canRead;
	bool canEdit;
	bool canExec;
	bool canElevate;
	
	int Priority;
	// From 0 to 4 - tells how much CPU time it gets
	
	bool isInteractive;
	bool isService;
	bool isHybrid;
	
	// Only applicable for Interactive and Hybrid processes...
	bool isSelected;
	// If so, CPU time is unaltered
	bool isForeground;
	// If so, CPU time --
	bool isBackground;
	// If so, CPU time -2
	bool isMinimized
	// If so, CPU time -2
	
	// States
	bool isLoaded;
	bool isPaused;
	bool isKilled;
	
	// Variable for pointer, to beginning of the executable's image in memory, goes here
	// Variable for pointer to the next Instruction - incremented when the previous one is executed
	
	// If an error is thrown, write it down here
	char error[128];
	
	int PID;
	char PName[128];
} Process;

typedef struct Thread {
	char Super[] = "Process";
	
	char Owner[128];
	// Should be the parent Process that it forked from
	
	// Permissions are inherited from the Parent as well...
	bool canRead;
	bool canEdit;
	bool canExec;
	bool canElevate;
	
	int Priority;
	// From 0 to 4 - tells how much CPU time it gets
	
	bool isInteractive;
	// Gets ++ of Parent's CPU time
	bool isService;
	// Gets -- of Parent's CPU time
	bool isHybrid;
	// Gets same CPU time as Parent
	
	// States
	bool isLoaded;
	// True when initially forked and while running
	bool isPaused;
	// If Thread is put to sleep
	bool isKilled;
	// If Thread is terminated, and execution returns to Parent
	
	// Variable for pointer, to beginning of the executable's fork point in memory, goes here
	// Variable for pointer to the next Instruction - incremented when the previous one is executed
	
	// If an error is thrown, write it down here
	char error[128];
	
	int TID;
	char TName[128];
} Thread;



typedef struct Drive {
	char Super[] = "SubSystem";
	
	bool Physical;
	bool Virtual;
	
	bool Mounted;
	char MountPoint[128];
	
	bool Readable;
	bool Writeable;
	bool Executeable;
	bool Bootable;
		
	float Size;
	bool BinaryUnits;
	bool DecimalUnits;
	char RootLoc[128];
	
	char Name[128];
	char Owner[128];
} Drive;

typedef struct Partition {
	char Super[] = "Drive";
	
	bool Physical = true;
	bool Virtual = false;
	
	char MountingPoint[384];
	int Position;
	
	char BackingDrive[128];
	
	bool Readable;
	bool Writeable;
	bool Executeable = false;
	bool Bootable;
	
	float Size;
	bool BinaryUnits = true;
	bool DecimalUnits = false;
	char RootLoc[] = "0/";
	
	char Container[256];
	char Name[128];
	char Owner[128];
} Partition;

typedef struct Volume {
	char Super[] = "Drive, Partition";
	
	bool Physical;
	bool Virtual;
	
	char MountingPoint[512];
	int Position;
	
	char Backing[512];
	
	bool Readable;
	bool Writeable;
	bool Executeable = false;
	bool Bootable;
	
	float Size;
	bool BinaryUnits;
	bool DecimalUnits;
	char RootLoc[576];
	
	char Container[256];
	char Label[128];
	char Owner[128];
} Volume;

typedef struct Container {
	char Super[] = "SubSystem, Drive, Volume, Partition";
	
	char Location[1024];
	char Backing[1024];
	
	bool Readable;
	bool Writeable;
	bool Executeable;

	bool isFile;
	bool isFolder;
	bool isLink;

	bool isHidden;
	bool isIterable;
	
	bool Text;
	bool Binary;
	char Type[100];
	
	char ParentDir[4096];
	char SubDir[4096];
	
	char target[8192];
	
	bool isEmpty;
	
	float Size;
	char Name[256];
	char Description[4096];
} Container;



typedef struct SHell {
    char Super[] = "System";
    
    char Mode[7];
    // Can be either User, Kernel, or System
    
    char currentUser[128];
    // Who's logged in here?
    
    char Command[32];
    // The action being performed
    char Object0[512];
    // The object receiving the specified action
    char Object1[512];
    // The secondary object, which may receive action as well
    
    char Input[2048];
    // The original command entered
    char Output[8192];
    // The first 8192 characters of the produced output
    
    // Output modes, for ease of readability
    bool isNotice;
    bool isWarning;
    bool isError;
} SHell;



// ToDo: create the following functions and Structures...

// Items - Error, Warning, Notice

// Functions - Create (creates new item), Add (adds to an existing item), Remove (deletes an item), Append (add to the end of an item), Edit (allow modification of an item), Read (displays Contents field of an item), SetProperty (sets a record for an item), Initiate (starts an active item), Terminate (ends an active item), LogIn (logs in a User), LogOut (logs out a User), ChangeUsr (changes the User that is logged in), 


-------
typedef struct Drive {
	char Super[] = "FS";
	
	bool Physical = false;
	bool Virtual = true;
	
	bool Mounted = false;
	char MountPoint[] = ":Test_Virtual_Drive/";
	
	bool Readable = true;
	bool Writeable = true;
	bool Executeable = false;
	bool Bootable = false;
		
	float Size = 1024;
	bool BinaryUnits = true;
	bool DecimalUnits = false;
	char RootLoc[] = "0/";
	
	char Name[] = "Test_Virtual_Drive";
} Drive;


typedef struct System {
	char Super[] = "";
	
	int Ring;
	// Can be 0, 1, or 2
	
	bool isActionable;
	bool isActive = true;
	bool isRunning = true;
	
	char Type[128];
	// Can be either Device, Resource, User, SubSystem, or IO
	char SubType[128];
	// Can be either FileSystem (FS), Memory (MemMan/MMS), Device (DeviceMan/DMS), RunTime (RTMan/RMS), User (UserMan/UMS), IO (IOMan/IOMS), or SHell
	
	char Subsystems[] = "FS, MemMan, DeviceMan, RTMan, UserMan, IOMan, SHell";
	// What Subsystems are a part of this component?
	
	char Name[128];
	char Version[64];
};
