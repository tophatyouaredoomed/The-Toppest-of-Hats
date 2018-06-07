int hour;
int minute;
int second;

int year;
int month;
int day;

Date currentDate;
Time currentTime;

class DTFormat{
	
	DTFormat(int hour, int minute, int second){
		String specTime
	}
	
	DTFormat(int hour, int minute){
		String rltvTime
	}
	
	DTFormat(int year, int month, int day){
		String currDate
	}
	
}

class TXP-FS{
	
	readDisk(){
		// Start at sector 0 of the drive
		// Traverse the current sector
		// While you traverse the sector:
			// If you find non-zero data (a fs object header):
				// Is it a file?
					// If so, are there any raw attributes available?
					// How big is it?
					// What is its name?
					// What is its type?
					// What is its permission level? Is it executable?
					// Is it a text file, or binary file?
					// When was it created or last modified?
					// Is is a symbollic link?
					// Is it an invalid file pointer?
					// Who owns it?
					// Was it hidden?
					// Call TXP-FS constructor to make a TXP-FS Object out of the data collected
					// Create TXP-FS Object pointer and add it to TXP-FS Object list (FS table)
					
				// Is it a folder or directory?
					// What is its name?
					// How big is it?
					// Who owns it?
					// Was it hidden?
					// Does it have subdirectories?
					// What is its permission level?
					// Is it an invalid pointer?
					// Call TXP-FS constructor to make a TXP-FS Object out of the data collected
					// Create TXP-FS Object pointer and add it to TXP-FS Object list (FS table)
				
				// Is it a partition?
					// What is its name?
					// How big is it?
					// What is its format?
					// Call TXP-FS constructor to make a TXP-FS Object out of the data collected
					// Create TXP-FS Object pointer and add it to TXP-FS Object list (FS table)
			
			// else: 
				// Report unsupported FS object
		// return FS table
	}
	
}