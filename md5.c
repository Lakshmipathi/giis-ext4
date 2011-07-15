#define COMMAND_LEN 20
#define DATA_SIZE 512
#include <stdio.h>
 
int main(){
    FILE *pf;
    char command[COMMAND_LEN];
    char data[DATA_SIZE];
 
    // Execute a process listing
         sprintf(command, "date"); 
          
    //          // Setup our pipe for reading and execute our command.
                  pf = popen(command,"r"); 
                   
                       if(!pf){
                        fprintf(stderr, "Could not open pipe for output.\n");
                        return;
                        }
                            
       // Grab data from process execution
           fgets(data, DATA_SIZE , pf);
                                
       // Print grabbed data to the screen.
                        fprintf(stdout, "-%s-\n",data); 
                                                  
         if (pclose(pf) != 0)
        fprintf(stderr," Error: Failed to close command stream \n");
                                                                 
      return;
  }
