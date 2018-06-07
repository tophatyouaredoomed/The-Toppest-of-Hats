// TXP-WNCM - Wireless Network Connection Manager
// With inspiration from 
// http://xmodulo.com/manage-wifi-connection-command-line.html
// Proper conversion to real C++ coming soon...

#include <iostream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>


using namespace std;


main()
{
string test = NULL;
string go = 0;
string default;
string key;
string ssid;
string command;
string suffix;
string cl;
string interface;
string option;
int reset;

again: cout: "Would you like to edit wireless connection settings (1 for yes, 0 for no)?"
cin go;

if (go = 0)
{
exit;
}

if (go = 1)
{
interface_new();
}

else
{
cout: "English, please."
goto again;
}
}


interface_new()
{

cout: "Please give the name of your wireless interface. If you do not know, it is most likely the standard wlan0.";
cin: interface;

string command = "$ sudo ip link set ";
string suffix = " up";
string cl = command + interface + suffix;
system | cl;

if(err)
{
cout: Please try a different interface
recall(reset=1)
}

else
{
cout: Interface is currently online. Scanning for local wireless networks...
cout: Here are the visible, available Wireless networks: n/

string command = "$ sudo iw dev ";
string suffix = " scan | less";
string cl = command + interface + suffix;

system | cl;

cout: "Would you like to connect to a different wireless network?";
cin option;

if(option="yes")
{
connection_new();
}

if(option="no")
{
reacall(reset=0)
}

else
{
cout: "exiting Wireless Network Manager";
exit;
}

}

}


connection_new()
{
cout: "Please give the network SSID.";
cin ssid;

cout: "Please tell whether the network is encrypted or not (yes or no - if yes, give type instead).";
cin option;

if(option="wpa2")
{
cout: "Sorry, but this system does not have the utilites to support that type of encryption at this time";
}

if(option="wpa")
{
connect_wpa(ssid);
}

if(option="wep")
{
connect_wep(ssid);
}

if(option="no")
{
connect_nonencr(ssid);
}

}


connect_wep(ssid)
{
cout: "Please enter a valid WEP key.";
cin key;

string command = "$ sudo iw dev wlan0 connect ";
string default = " key 0:";
string suffix = key;
string cl = command + ssid + default + suffix;

system | cl;
}


connect_wpa(ssid)
{
cout: "Do you have wpasupplicant installed (yes, no, or idk)?"
cin option;

switch (option)
{
case yes:
goto start;

case no:
goto install;

case idk:
goto install;
}

install: system | ifconfig -a;
system | aptitude install wpasupplicant;
system | vim System/etc/wpa_supplicant.conf;

start: cout: "Please enter a valid WEP key.";
cin key;

std::ofstream wpa;

wpa.open("System/etc/wpa_supplicant/wpa_supplicant.conf", std::ios_base::app);
string default = "network={    ssid="" + ssid + "    psk="" + key + "    priority=1 }; ";
wpa << default;

system | $ sudo wpa_supplicant -i wlan0 -c System/etc/wpa_supplicant/wpa_supplicant.conf;
system | $ sudo dhcpcd wlan0;
system | $ System/Tools/Wireless/iwconfig;
}


connect_nonencr(ssid)
{
string command = "$ sudo iw dev wlan0 connect ";
string suffix = ssid;
string cl = command + suffix;

system | cl;
}

//http://xmodulo.com/manage-wifi-connection-command-line.html
//http://www.howtoforge.com/how-to-connect-to-a-wpa-wifi-using-command-lines-on-debian
//http://xmodulo.com/category/web
//http://xmodulo.com/category/utilities
//http://xmodulo.com/category/filesystem
//http://xmodulo.com/category/desktop