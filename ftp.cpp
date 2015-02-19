// Ahmed Musazay
// Network Design
//
// Implements a ftp client program based on RFC 959 file transfer protocol.

#include "Socket.h"
#include <sys/poll.h>
#include <iostream>
#include <string.h>
#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>


using namespace std;
const int DEFAULT_PORT = 21;
const int DEFAULT_BUFF_SIZE = 5000;

int sendUserName( int &serverFd, string serverIp );
int sendPassword( int &serverFd );
void cdCommand( int &serverFd, string newDirectory);
void lsCommand( int &serverFd);
void getCommand(int &serverFd, string filename);
void putCommand( int &serverFd, string filename);
void closeCommand( int &serverFd);
void deleteCommand(int &serverFd, string filename);
void mkdirCommand(int &serverFd, string dirName);
void rmdirCommand(int &serverFd, string dirName);
void cdupCommand(int &serverFd);
int responseCode(char* buff);
string* grabIpPort(char* serverResponse);
void pollReads(int serverFd);

int main( int argc, char* argv[] ) {
  /*   ESTABLISH CONNECTION */
  Socket* sock = new Socket(DEFAULT_PORT);
  int serverFd = sock->getClientSocket( argv[1] );
  char buff[DEFAULT_BUFF_SIZE];
  bzero( buff, sizeof( buff ) );
  read ( serverFd, buff, sizeof(buff));
  int connectionResponse;
  int usernameResponse;
  int passwordResponse;

  cout << buff;
  connectionResponse = responseCode(buff);
  /* USER COMMAND */
  if (connectionResponse = 220) {
    usernameResponse = sendUserName( serverFd, argv[1] );
  } else { //server rejected connection
    cerr << "Connection could not be established" << endl;
    return -1;
  }

  /* PASSWORD COMMAND */
  if (usernameResponse == 331) {
    passwordResponse = sendPassword( serverFd );
      
  } else { //server rejected username
    cout << "USER error code: " << usernameResponse << endl ;
    return -1;
  }

  //Main commands loop (CD, LS, GET, PUT, CLOSE, QUIT)
  // and additional 4 commands
  // (delete, mkdir, rmdir, cdup)
  while ( true ) {
    string userCmd;
    cout << "ftp > ";
    cin >> userCmd;
    if (userCmd == "cd") {
      string newDirectory;
      cin >> newDirectory;
      cdCommand( serverFd, newDirectory );
    } else if (userCmd == "ls") {
      lsCommand( serverFd );
    } else if (userCmd == "get") {
      string filename;
      cin >> filename;
      getCommand( serverFd, filename );
    } else if (userCmd == "put") {
      string filename;
      cin >> filename;
      putCommand( serverFd, filename );
    } else if (userCmd == "close") {
      closeCommand( serverFd );
    } else if (userCmd == "quit") {
      close( serverFd );
      break;
    } else if (userCmd == "delete") {
      string filename;
      cin >> filename;
      deleteCommand( serverFd, filename );
    } else if (userCmd == "mkdir") {
      string dirName;
      cin >> dirName;
      mkdirCommand( serverFd, dirName );
    } else if (userCmd == "rmdir") {
      string dirName;
      cin >> dirName;
      rmdirCommand( serverFd, dirName );
    } else if (userCmd == "cdup") {
      cdupCommand( serverFd );
    } 
    else {
      cout << "Command not recognized." << endl;
    }
  }
}

// Sends username to ftp server in format USER <sp> <username> <CRLF>
// Returns response code read from server.
int sendUserName( int &serverFd, string serverIp) {
  char buff[DEFAULT_BUFF_SIZE];
  int usernameResponse;
  string username = "USER ";

  string userString( getlogin( ) );
  cout << "Name (" << serverIp << ":" << userString << "): ";
  string usernameIn;
  cin >> usernameIn;
  username += usernameIn;
  username += "\r\n";

  int nameResponse = write ( serverFd, username.c_str(), username.size() );
  bzero( buff, sizeof( buff ) );
  read ( serverFd, buff, sizeof( buff ));
  cout << buff;

  return responseCode(buff);
}

// Sends password to ftp server in format PASS <sp> <password> <CRLF>
// followed by SYST call
// Must be preceded by USER call
// returns responsce code from server.
int sendPassword( int &serverFd) {
  char buffPass[DEFAULT_BUFF_SIZE];
  string password = "PASS ";
  cout << "Password: ";
  string passIn;
  cin >> passIn;

  password += passIn;
  password += "\r\n";
  int passResponse = write ( serverFd, password.c_str(), password.size());
  bzero( buffPass, sizeof( buffPass ) );
  read ( serverFd, buffPass, sizeof( buffPass ));
  cout << buffPass;

  //Check if password was okay before sending SYST call
  int passwordResponse = responseCode(buffPass);
  if (passwordResponse == 230) {
    pollReads(serverFd);
    string systCommand = "SYST\r\n";
    int systResponse = write( serverFd, systCommand.c_str(), 
                              systCommand.size());
    bzero(buffPass, sizeof(buffPass));
    read( serverFd, buffPass, sizeof(buffPass));
    cout << buffPass;
  } 
  return passwordResponse;
}

// Changes working directory to given directory
void cdCommand(int &serverFd, string newDirectory) {
  char buff[DEFAULT_BUFF_SIZE];
  string directoryCommand = "CWD ";
  directoryCommand += newDirectory;
  directoryCommand += "\r\n";

  int cwdResponse = write ( serverFd, directoryCommand.c_str(), 
                            directoryCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

// Lists all files/directories in current directory
void lsCommand( int &serverFd) {
  char buff[DEFAULT_BUFF_SIZE];
  /*PASV COMMAND*/
  string pasvCommand = "PASV\r\n";
  int pasvResponse = write( serverFd, pasvCommand.c_str(),
                            pasvCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  string* ipPort = grabIpPort(buff); //buff has to be 277 .. (xx,xx,xx)
  /*establish socket with server*/
  int port = atoi(ipPort[1].c_str());
  Socket* sock = new Socket(port);
  int ftpFd = sock->getClientSocket( (char*)ipPort[0].c_str() );

  /*LIST COMMAND*/ 
  string listCommand = "LIST\r\n";
  int listResponse = write ( serverFd, listCommand.c_str(),
                             listCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
  
  bzero(buff, sizeof(buff));
  int nread;
  nread = read ( ftpFd, buff, sizeof(buff) ); //first read
  cout << buff;
  //keep reading from server until there is nothing left
  while (nread != 0) {
    bzero(buff, sizeof(buff));
    nread = read ( ftpFd, buff, sizeof(buff) );
    cout << buff;
  }
  //Read from original server connection to show transfer finished
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
  close(ftpFd);
}

//Grabs given filename from current directory
//If file does not exist, it is created in server 
void getCommand(int &serverFd, string filename) {
  char buff[DEFAULT_BUFF_SIZE];

  /*PASV COMMAND*/
  string pasvCommand = "PASV\r\n";
  int pasvResponse = write( serverFd, pasvCommand.c_str(), pasvCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  string* ipPort = grabIpPort(buff); //buff has to be 277..(xx,xx,xx,xx,xx,xx)
  /*establish socket with server for file transfer*/
  int port = atoi(ipPort[1].c_str());
  Socket* sock = new Socket(port);
  int ftpFd = sock->getClientSocket( (char*)ipPort[0].c_str() );

  //Set type to Binary
  string typeCommand = "TYPE I\r\n";
  int typeResponse = write (serverFd, typeCommand.c_str(), typeCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  /*RETR COMMAND*/
  string retrCommand = "RETR ";
  retrCommand += filename;
  retrCommand += "\r\n";

  int retrResponse = write (serverFd, retrCommand.c_str(), retrCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  //initialize file descriptor for writing to file
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int file = open( filename.c_str(), O_WRONLY | O_CREAT, mode ); 

  int nread;
  char fileBuf[DEFAULT_BUFF_SIZE];
  bzero(fileBuf, sizeof(fileBuf));
  nread = read ( ftpFd, fileBuf, sizeof(fileBuf));
  int nwrite = write( file, fileBuf, nread);

  // while there is still data to read, write it to file descriptor
  while (nread != 0) {
    bzero(fileBuf, sizeof(fileBuf));
    nread = read ( ftpFd, fileBuf, sizeof(fileBuf) );
    nwrite = write( file, fileBuf, nread);
  }
  close(file);    //close file descriptor
  close(ftpFd);   //close data connection

  //display server response message to finishing transfer
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//Transfers file from current directory to server
void putCommand( int &serverFd, string filename) {
  char buff[DEFAULT_BUFF_SIZE];
  /*PASV COMMAND*/
  string pasvCommand = "PASV\r\n";
  int pasvResponse = write( serverFd, pasvCommand.c_str(), pasvCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  string* ipPort = grabIpPort(buff); //buff has to be 277..(xx,xx,xx,xx,xx,xx)
  /*establish socket with server for file transfer*/
  int port = atoi(ipPort[1].c_str());
  Socket* sock = new Socket(port);
  int ftpFd = sock->getClientSocket( (char*)ipPort[0].c_str() );

  //Set type to Binary
  string typeCommand = "TYPE I\r\n";
  int typeResponse = write (serverFd, typeCommand.c_str(), typeCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  /*STOR COMMAND*/
  string storCommand = "STOR ";
  storCommand += filename;
  storCommand += "\r\n";
  int storResponse = write ( serverFd, storCommand.c_str(), storCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;

  int nwrite;
  char fileBuf[DEFAULT_BUFF_SIZE];
  bzero(fileBuf, sizeof(fileBuf));

  //initialize file descriptor for reading from file
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  int file = open( filename.c_str(), O_RDONLY | O_CREAT, mode );

  int nread = read ( file, fileBuf, sizeof(fileBuf) );
  nwrite = write( ftpFd, fileBuf, nread );
  while (nread != 0) { //keep reading from file until finished
    bzero(fileBuf, sizeof(fileBuf));
    nread = read (file, fileBuf, sizeof(fileBuf));
    nwrite = write(ftpFd, fileBuf, nread);
  }

  //done writing, close socket and input file
  close(ftpFd);
  close(file);

  //display server response
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//delete's given filename from current directory
void deleteCommand(int &serverFd, string filename) {
  char buff[DEFAULT_BUFF_SIZE];
  string delCommand = "DELE ";
  delCommand += filename;
  delCommand += "\r\n";
  int delResponse = write( serverFd, delCommand.c_str(), delCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//create's given directory from current directory
void mkdirCommand(int &serverFd, string dirName) {
  char buff[DEFAULT_BUFF_SIZE];
  string mkCommand = "MKD ";
  mkCommand += dirName;
  mkCommand += "\r\n";
  int delResponse = write( serverFd, mkCommand.c_str(), mkCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//delete's given directory from current directory
void rmdirCommand(int &serverFd, string dirName) {
  char buff[DEFAULT_BUFF_SIZE];
  string rmdCommand = "RMD ";
  rmdCommand += dirName;
  rmdCommand += "\r\n";
  int delResponse = write( serverFd, rmdCommand.c_str(), rmdCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//Move's current directory up one
void cdupCommand(int &serverFd) {
  char buff[DEFAULT_BUFF_SIZE];
  string cdupCommand = "CDUP\r\n";
  int cdupResponse = write( serverFd, cdupCommand.c_str(), cdupCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}


//Closes connection with ftp server
void closeCommand(int &serverFd) {
  char buff[DEFAULT_BUFF_SIZE];
  string quitCommand = "QUIT\r\n";
  int quitResponse = write( serverFd, quitCommand.c_str(), quitCommand.size());
  bzero(buff, sizeof(buff));
  read( serverFd, buff, sizeof(buff));
  cout << buff;
}

//Method for parsing IP address and port values returned by ftp server
// serverResponse should be in form "227 .. (XX,XX,XX,XX,XX,XX)"
// method returns string array of size 2 in format [ip, port]
string* grabIpPort(char* serverResponse) {
  string* ipPort = new string[2];
  int charIndex = 0;
  while (serverResponse[charIndex] != '(') charIndex++;
  charIndex++;
  //Grab IP from string
  for (int i = 0; i < 4; i++) {
    while(serverResponse[charIndex] != ',') {
      ipPort[0] += serverResponse[charIndex];
      charIndex++;
    }
    if (i != 3) ipPort[0] += "."; //avoid adding '.' to last byte
    charIndex++;
  }
  //Grap ports
  string port1str;
  string port2str;
  while(serverResponse[charIndex] != ',') {
    port1str += serverResponse[charIndex];
    charIndex++;
  }
  charIndex++;
  while(serverResponse[charIndex] != ')') {
    port2str += serverResponse[charIndex];
    charIndex++;
  }
  charIndex++;
  int port = atoi(port1str.c_str()) * 256 + atoi(port2str.c_str());
  stringstream ss;
  ss << port;
  ipPort[1] = ss.str();
  return ipPort;
}

//Method for doing poll read
//Provided by Professor Fukuda
void pollReads(int serverFd) {
  struct pollfd ufds;
  ufds.fd = serverFd;               // a socket descriptor to exmaine for read
  ufds.events = POLLIN;             // check if this sd is ready to read
  ufds.revents = 0;                 // simply zero-initialized
  int val = poll( &ufds, 1, 1000 ); // poll this socket for 1000msec (=1sec)

  if ( val > 0 ) {                  // the socket is ready to read
    char buf[20000];
    int nread = read( serverFd, &buf, sizeof(buf) );
    cout << buf;
  } else {
    cout << "Server error" << endl;
  }
}

// Returns the response code to given ftp command server reply
// Given string should be in format <response #> <sp> <message>
int responseCode(char* buff) {
  //Grab response number
  int responseCode;
  stringstream ss(buff);
  ss >> responseCode;
  return responseCode;
}



