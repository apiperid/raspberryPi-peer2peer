/*
  Real Time Embedded Systems - 2019
  Piperidis Anestis - 8689
  apiperid@ece.auth.gr
  peer2peer.c
*/

#include <unistd.h> 
#include <stdio.h> 
#include <time.h>
#include <inttypes.h>
#include <sys/socket.h> 
#include <stdlib.h>  
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <arpa/inet.h> 
#include <signal.h>
#include "myParser.h"
#include "splitAEM.h"

#define PORT 2288 
#define messages 2000
#define messageLength 256
#define packetLength 277
#define aemDigits 4
#define TIME_IN_SECONDS 60

// server variables
int server_fd, server_new_socket, server_valread; 
struct sockaddr_in server_address;
int server_opt = 1; 
int server_addrlen = sizeof(server_address); 
char server_buffer[packetLength] = {0}; 

// client variables
int client_sock, client_valread , client_valwrite; 
struct sockaddr_in client_address; 
char client_buffer[packetLength] = {0}; 

// variables
char myAem[aemDigits];
int maxSocketsQueue;
char packet[packetLength];
int headOfBuffer = 0;
char **messagesBuffer;
int targetAem = 0;
int BUFFER_AVAILABLE;
int aemCounter = 0;
uint32_t *receivers;
unsigned int times = 0,totalPackets=0,packetsForMe=0;
struct timeval timeout;

// server thread function
void* serverThread();

// functions
void client();
void packetCreate(char *sender,char *receiver);
void savePacketToBuffer();
void memoryAllocation();
void saveServerIncomingToBuffer();
void insertReceiversIntoMemory(char *list,char *my_aem);
void printMessagesBuffer();
void writeToFile(int aem,char *filename);


int main(int argc,char **argv)
{
  parseArguments(argc);

  for(int i=0;i<aemDigits;i++)
    myAem[i]=argv[2][i];
  
  checkInputAEM(myAem);

  aemCounter = parseAemList(argv[1],myAem);
  maxSocketsQueue = aemCounter+1; // my aem included
  //printf("Main :: the different aems are : %d\n",aemCounter);

  memoryAllocation();
  insertReceiversIntoMemory(argv[1],myAem);

  timeout.tv_sec  = 1;  // after 1 seconds connect() will timeout
  timeout.tv_usec = 0;
 
  // server thread - server side
  pthread_t serverThread_id; 
  pthread_create(&serverThread_id, NULL,serverThread, NULL); 

  // client side
  while(1)
  {
    times++;
    unsigned int hours,minutes,seconds; // we print the time in order to know when to stop ( 2 hours )
    hours = (TIME_IN_SECONDS*times)/(3600);
    minutes = ((TIME_IN_SECONDS*times)-(hours*3600))/60;
    seconds = ((TIME_IN_SECONDS*times)-(hours*3600)) - (minutes*60);
    sleep(TIME_IN_SECONDS);
    printf("Time Elapsed : %d h : %d m : %d s\n",hours,minutes,seconds);
    client();
  }
}

/* 
  void client() - function responsible for all job of the client side
  
  firtsly we choose the aem we will send the buffer ( server them like round robin )
  get the xx and yy part of the aem and shape the ip 10.0.xx.yy
  prepare the message for the target and save it to buffer
  try to connect with him
  if the timeout passes ( cant find the target ) move on to next ( after 1 minute )
  if connect is ok then update the Logs and send him the whole buffer

*/

void client()
{
  char inet[20] = {0};
  char buf[5] = {0};
  //printf("Client :: the head of buffer is : %d\n",headOfBuffer);
  uint32_t target = receivers[targetAem];
  uint32_t xx = getFirstHalfAEM(target);
  uint32_t yy = getSecondHalfAEM(target);
  //printf("Client :: xx = %d , yy = %d\n",(int)xx,(int)yy);
  
  targetAem = (targetAem+1)%aemCounter;

  char tar[5];
  snprintf(tar, sizeof(tar), "%d",(int)target);

  // if you want to test on raspberry - it creates the 10.0.xx.yy
  
  strcat(inet,"10.0.");
  snprintf(buf, sizeof(buf), "%hu",(unsigned short)xx);
  strcat(inet,buf);
  strcat(inet,".");
  snprintf(buf, sizeof(tar), "%hu",(unsigned short)yy);
  strcat(inet,buf);
  
  
  //strcat(inet,"127.0.0.1"); // if you test it on your pc

  //printf("inet = %s\n",inet);

  // create the packet for send
  packetCreate(myAem,tar);
  BUFFER_AVAILABLE = 0;
  savePacketToBuffer();
  totalPackets++;
  //printf("Client :: your buffer had totally %d packets\n",totalPackets);
  //printMessagesBuffer();
  BUFFER_AVAILABLE = 1;

  //printf("Client :: I am awake\n");
  client_address.sin_family = AF_INET; 
  client_address.sin_port = htons(PORT); 
  if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  { 
    perror("Socket failed"); 
    exit(EXIT_FAILURE);
  } 
  // Convert IPv4 and IPv6 addresses from text to binary form 
  client_address.sin_addr.s_addr = inet_addr(inet);// or use INADDR_ANY;   
  setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  if(inet_pton(AF_INET,inet, &client_address.sin_addr)<=0)  
  { 
    printf("Invalid address/ Address not supported , inet = %s\n",inet); 
  }
  else // if inet_pton is ok
  {
  	//write to Tried Connections.txt
  	writeToFile(target,"Tried Connections.txt");
    if (connect(client_sock, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) 
    { 
      printf("Connection with inet = %s Failed (Server Unreachable)\n",inet); 
    } 
    else
    {
      //write to logs
      writeToFile((int)target,"Logs.txt");
      for(int i=0;i<messages;i++)
        client_valwrite = write(client_sock ,messagesBuffer[i],packetLength);
    }
  }
  close(client_sock);
}

/*
  void* serverThread() - server thread of the program - server side

  bind the address for me ( 10.0.xx.yy ) where xx and yy are from my AEM
  set server to passive mode ( with listen )
  wait for a client to connect...
  when someone is connected we receive whatever he sends and check if there is duplicate
  if is not we save it to buffer
  if it is we move on the next message

*/


void* serverThread()
{
  char inet[20] = {0};
  char sender_receiver[4] ={0};
  char buf[5]; // for the xx and yy
  // Creating socket file descriptor 
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
  { 
    perror("socket failed"); 
    exit(EXIT_FAILURE); 
  }
  // Forcefully attaching socket to the port 
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&server_opt, sizeof(server_opt))) 
  { 
    perror("setsockopt"); 
    exit(EXIT_FAILURE); 
  }
  server_address.sin_family = AF_INET; 
  uint32_t xx = getFirstHalfAEM((uint32_t)atoi(myAem));
  uint32_t yy = getSecondHalfAEM((uint32_t)atoi(myAem));
  //printf("Server :: xx = %d , yy = %d\n",(int)xx,(int)yy);
  // if you want to test on raspberry - it creates the 10.0.xx.yy
  
  strcat(inet,"10.0.");
  snprintf(buf, sizeof(buf), "%hu",(unsigned short)xx);
  strcat(inet,buf);
  strcat(inet,".");
  snprintf(buf, sizeof(buf), "%hu",(unsigned short)yy);
  strcat(inet,buf);
  //printf("inet = %s\n",inet);
  
  
  //strcat(inet,"127.0.0.1"); // if you test it on your pc
  server_address.sin_addr.s_addr = inet_addr(inet);// or use INADDR_ANY; 
  server_address.sin_port = htons(PORT);
  // Forcefully attaching socket to the port 
  if (bind(server_fd, (struct sockaddr *)&server_address,sizeof(server_address))<0) 
  { 
    printf("bind failed to inet = %s\n",inet); 
    exit(EXIT_FAILURE); 
  } 
  if (listen(server_fd, maxSocketsQueue) < 0) 
  { 
    perror("listen"); 
    exit(EXIT_FAILURE); 
  }
  while(1) // endless loop - waiting for connections 
  {
    //printf("Server :: got in loop\n");
    if ((server_new_socket = accept(server_fd, (struct sockaddr *)&server_address,(socklen_t*)&server_addrlen))<0) 
    { 
      perror("Accept failed"); 
      exit(EXIT_FAILURE); 
    }  
    while((server_valread = read(server_new_socket ,server_buffer,packetLength))>0)
    {
      if(server_buffer[0]==(char)0) // if packet starts with \0 then transmission is done
        break;
      for(int digit=0;digit<aemDigits;digit++)
        sender_receiver[digit]=(char)server_buffer[digit];
      //printf("Server :: sender is : %s\n",sender_receiver);
      if(strncmp(sender_receiver,myAem,aemDigits)!=0) // receive everything if only is not sent from me
      {                                               // dont receive my own packets
        for(int digit=0;digit<aemDigits;digit++) // get the AEM of the receiver
          sender_receiver[digit]=(char)server_buffer[digit+5];
        //printf("Server :: receiver is : %s\n",sender_receiver);
        //printf("Server :: valread is : %d\n",server_valread);
        //printf("Server :: message received is : %s\n\n",server_buffer);
 
        int flag = 0;
        for(int i=0;i<messages;i++)
        {
          if((strncmp(server_buffer,messagesBuffer[i],packetLength))==0)
          {
            flag = 1;
            //printf("Duplicate Found , Flag value = %d\n",flag);
            break;
          }
        }
        if(flag==0) // flag = 0 means the message is new
        {
          if(strncmp(sender_receiver,myAem,aemDigits)==0) // check if message is for me
          {
            //printf("Server :: you have a new message\n");
            packetsForMe++;
            //printf("Server :: till now messages have as receiver you are : %d\n",packetsForMe);
          }
          saveServerIncomingToBuffer();
          totalPackets++;
          printf("Server :: your buffer had totally %d packets\n",totalPackets);
        }
      }
    }
    //printMessagesBuffer();
  } 
}

/*
  void writeToFile(int aem,char *filename)

  just write data to file
  two files exists in our program

  Tried Connections.txt = write when i tried to connect with someone else
  Logs.txt = when i found a new connection

*/

void writeToFile(int aem,char *filename)
{
	FILE *fp = fopen(filename,"a");
	if(fp==NULL)
	{
		printf("Error in writing to file : %s\n",filename);
		return;
	}
	fprintf(fp,"%d,%lu\n",aem,time(NULL));
    fclose(fp);
}

/*
  void packetCreate(char *sender,char *receiver)
  
  it creates the packet with the format :
  AEM1_AEM2_TIMESTAMP_MESSAGE
  
  The message is a char[256] array - random characters
  the packet has size of 277 characters

*/

void packetCreate(char *sender,char *receiver)
{
  char timestampBuffer[11];
  uint64_t timestamp;

  srand(time(0));
  for(int i=0;i<aemDigits;i++)
    packet[i] = (char)sender[i];
  packet[4] = (char)95; // put "_"
  for(int i=0;i<aemDigits;i++)
    packet[i+5]=receiver[i];
  packet[9] = (char)95; // put "_"

  timestamp = time(NULL);
  sprintf(timestampBuffer,"%d",(int)timestamp);
  for(int i=0;i<10;i++)
    packet[i+10] = timestampBuffer[i];
  packet[20] = (char)95; // put "_"

  for(int i=0;i<messageLength;i++)
    packet[i+21] = (char)((rand()%26)+97);
}

/*
  void savePacketToBuffer()
  
  Just save the packet we just created to out buffer

*/

void savePacketToBuffer()
{
  for(int i=0;i<packetLength;i++)
    messagesBuffer[headOfBuffer][i] = (char)packet[i];
  headOfBuffer = (headOfBuffer+1) % messages;
}

/*
  void saveServerIncomingToBuffer()
  
  Just save the server just received to buffer

*/

void saveServerIncomingToBuffer()
{
  for(int i=0;i<packetLength;i++)
    messagesBuffer[headOfBuffer][i] = (char)server_buffer[i];
  headOfBuffer = (headOfBuffer+1) % messages;
}

/*
  void insertReceiversIntoMemory(char *list,char *my_aem)
  
  We insert into an array of uint32_t the AEMs of the list 

*/

void insertReceiversIntoMemory(char *list,char *my_aem)
{
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  FILE* file = fopen(list,"r");
  if(file==NULL)
  {
    printf("Error opening the file : %s\n",list);
    exit(EXIT_FAILURE);
  }
  int i=0;
  while(i<aemCounter)
  {
    read = getline(&line, &len, file);
    if(atoi(line)!=atoi(my_aem))
    {
      receivers[i] = atoi(line);
      i++;
    }
  }
  fclose(file);
}

/*
  void memoryAllocation()
  
  allocate memory for all arrays we will need
  messagesBuffer - a 2D char array 2000x277
  receivers - a 1D uint32_t array - contains the AEMs

*/

void memoryAllocation()
{
  messagesBuffer = (char **)malloc(messages*sizeof(char *));
  if(messagesBuffer==NULL)
  {
    perror("Memory error");
    exit(EXIT_FAILURE);
  }
  for(int i=0;i<messages;i++)
  {
    messagesBuffer[i] = (char *)malloc(packetLength*sizeof(char));
    if(messagesBuffer[i]==NULL)
    {
      perror("Memory error");
      exit(EXIT_FAILURE);
    }
  }

  if(aemCounter!=0)
  {
    receivers = (uint32_t *)malloc(aemCounter*sizeof(uint32_t));
    if(receivers == NULL)
    {
      perror("Memory error");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    perror("We did not find aem with the specs we want");
    exit(EXIT_FAILURE);
  }
}

/*
  void printMessagesBuffer()
  
  prints the buffer - only the filled parts

*/

void printMessagesBuffer()
{
  printf("************* START PRINTING BUFFER *******************\n\n");
  for(int i=0;i<messages;i++)
  {
    if(messagesBuffer[i][0]==(char)0) // "\0"
      break;
    printf("Buffer :: head in position : %d\n",i);
    printf("Buffer :: message is %s\n",messagesBuffer[i]);
  }
  printf("************* END PRINTING BUFFER *********************\n\n");
}
