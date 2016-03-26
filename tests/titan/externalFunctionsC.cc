#include <iostream>
#include <fstream>
#include <sstream>
#include "TTCN3.hh"
#include <time.h>
#include <unistd.h>
#include <vector>

namespace externalFunctions {
    using namespace std;

  string executeOnShell(const string& command);	
  string streamRead(FILE* stream);

  string executeOnShell(const string& command){
    FILE* localFile = popen(command.c_str(), "r");
    return streamRead(localFile);
  }

  string streamRead(FILE* stream){
    char tempChar = getc( stream );	
    string tempString;
    while( tempChar != EOF) {
      tempString += tempChar;
	  tempChar = getc( stream );
	}
  pclose(stream);
  return tempString;
  }

  CHARSTRING executeCommand(const CHARSTRING& command){
    string tempCommand = (const char*) command;
    string tempResult;
    TTCN_Logger::log(TTCN_DEBUG,"executeCommand - command to shell: %s",tempCommand.c_str());
    tempResult = executeOnShell(tempCommand);
    TTCN_Logger::log(TTCN_DEBUG,"executeCommand - response from shell: %s",tempResult.c_str());
    return tempResult.c_str();
  }

  INTEGER indexOfSubstring(const CHARSTRING& s1, const CHARSTRING& s2, const INTEGER& offset) {
    if(s2.lengthof()==0) return 0;
    if(s1.lengthof()==0) return -1;
    if(offset<0) return -1;
    if(s1.lengthof()<=offset) return -1;
    
    const char* str1=(const char*)s1+(int)offset;
    const char* str2=(const char*)s2;
    const char* first=strstr(str1,str2);
    if(first) return first-str1+(int)offset;
    return -1;
  }
  
}
