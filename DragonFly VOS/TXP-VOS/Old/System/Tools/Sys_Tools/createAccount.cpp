#include <string>
#include <bitset>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

int register()
{

string username;
string passcode;
string path;
string folder;
int res = access(path, R_OK);

tryagain:cout << "Please enter your desired Username.";
cin >> username;

cout << "Please wait...";

string path = "\System\Users\" + username;

int res = access(path, R_OK);
if (res < 0) 
{
if (errno == ENOENT) 
    {
	cout << "Sorry; this username is already taken. Please try again, with a different one.";
	goto tryagain;
    }
else 
    {
	cout << "That works! Let's continue...";
    }
}

int mkdir(const char *path, mode_t mode);

cout << "Designing profile...";

cout << "Please enter your desired passcode.";
cin >> passcode;

ofstream psc;
psc.open ("\System\Users\"username + ".pswd");

  for (std::size_t i = 0; i < passcode.size(); ++i)
  {
      psc << bitset<8>(passcode.c_str()[i]) << endl;
  }

psc.close();

cout << "Thank you. Setting up user folder...";

string folder = "\System\Users\" + username + "\Desktop";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Documents";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Documents\Slideshows";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Documents\Tables";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Documents\Ink";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Documents\WebPages";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Multimedia";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Multimedia\Audio";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Multimedia\Visual";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Multimedia\Visual\Pictures";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Multimedia\Visual\Videos";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\Temporary_Files";
int mkdir(const char *folder, mode_t mode);

string folder = "\System\Users\" + username + "\HTDOCS";
int mkdir(const char *folder, mode_t mode);

cout << "Your account has been created. Please login at the terminal to access your system portal...";

main();
}


void logout()
{
retry:cout << "Are you sure you wish to log out?";
cin >> session;

if (session == "yes")
{
cout << "Logging out...";
		FILE * user fopen ("\System\curusr.txt","w");
		fputs ("null",user);
		fclose (user);
		system("\System\Applications\system");
		return 0;
}

if (session == "no")
{
cout << "Recalling system viewer...";
system("\System\Applications\system");
}

}


void login()
{

again:cout << "Please enter your current Username.";
cin >> username;

if (username == "cancel")
{
cout << "Good bye!";
return 0;
}

else 
{
string pswdfile = "\System\Users\"  + username + ".pswd";

inline bool exist(const std::string& pswdfile)
{
    ifstream file(pswdfile);
    if(!file)            // If the file was not found, then file is 0, i.e. !file=1 or true.
        {
		cout << "Sorry; your user was not found. Please try again.";
		goto again;
		}   // The file was not found.
    else                 // If the file was found, then file is non-0.
        {
		
		pass:string pswd;
		getline(pswdfile, pswd);
		cout << "Please enter your system passcode.";
		cin >> passcode;
		
		string psc;
		
		for (std::size_t i = 0; i < passcode.size(); ++i)
			{
				psc << bitset<8>(passcode.c_str()[i]) << endl;
			}
		
		if (psc == pswd)
		{
		cout << "Login successful. Entering system viewer...";
		FILE * user fopen ("\System\curusr.txt","w");
		fputs (username,user);
		fclose (user);
		system("\System\Applications\system");
		}
		
		else
		{
		cout << "Your system passcode was incorrect. Please try again.";
		goto pass;
		}
		
		}     // The file was found.
}

}
}


int main()
{
logsess:cout << "What would you like to do?";
cin >> logop;

if (logop == "regUser")
{
register();
}

if (logop == "logIn")
{
login();
}

if (logop == "logOutcurr")
{
logout();
}

else
{
cout << "Your entry was invalid. Please try again later";
goto logsess
}

}