/*
  Real Time Embedded Systems - 2019
  Piperidis Anestis - 8689
  apiperid@ece.auth.gr
  onlyCLient.c
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

// client variables
int client_sock, client_valread , client_valwrite; 
struct sockaddr_in client_address; 
char client_buffer[packetLength] = {0}; 

// variables
char myAem[aemDigits];
char packet[packetLength];
int targetAem = 0;
int aemCounter = 0;
int headOfBuffer = 0;
uint32_t *receivers;
char **messagesBuffer;
struct timeval timeout;

// functions
void client();
void savePacketToBuffer();
void packetCreate(char *sender,char *receiver);
void memoryAllocation();
void insertReceiversIntoMemory(char *list,char *my_aem);

int main(int argc,char **argv)
{
  parseArguments(argc);

  for(int i=0;i<aemDigits;i++)
    myAem[i]=argv[2][i];
  
  checkInputAEM(myAem);
  aemCounter = parseAemList(argv[1],myAem);

  memoryAllocation();
  insertReceiversIntoMemory(argv[1],myAem);

  timeout.tv_sec  = 1;  // after 1 seconds connect() will timeout
  timeout.tv_usec = 0;

  // client side
  while(1)
  {
    sleep(TIME_IN_SECONDS);
    client();
  }
}

void client()
{
  char inet[20] = {0};
  char buf[5] = {0};
  uint32_t target = receivers[targetAem];
  uint32_t xx = getFirstHalfAEM(target);
  uint32_t yy = getSecondHalfAEM(target);
  //printf("Client :: xx = %d , yy = %d\n",(int)xx,(int)yy);
  targetAem = (targetAem+1)%aemCounter;

  char tar[5];
  snprintf(tar, sizeof(tar), "%d",(int)target);

  // create the packet for send
  packetCreate(myAem,tar);
  savePacketToBuffer();
  printf("Packet created for : %s\n",tar);

  //printf("Client :: I am awake\n");
  client_address.sin_family = AF_INET; 
  client_address.sin_port = htons(PORT); 
  if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
  { 
    perror("Socket failed"); 
    exit(EXIT_FAILURE);
  } 
  // Convert IPv4 and IPv6 addresses from text to binary form 
  
  // if you want to test on raspberry - it creates the 10.0.xx.yy
  
  strcat(inet,"10.0.");
  snprintf(buf, sizeof(buf), "%hu",(unsigned short)xx);
  strcat(inet,buf);
  strcat(inet,".");
  snprintf(buf, sizeof(tar), "%hu",(unsigned short)yy);
  strcat(inet,buf);
  
  //strcat(inet,"127.0.0.1"); // if you test it on your pc

  //printf("inet = %s\n",inet);

  client_address.sin_addr.s_addr = inet_addr(inet);// or use INADDR_ANY;   
  setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  if(inet_pton(AF_INET,inet, &client_address.sin_addr)<=0)  
  { 
    printf("Invalid address/ Address not supported , inet = %s\n",inet); 
  }
  else // if inet_pton is ok
  {
    if (connect(client_sock, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) 
    { 
      printf("Connection with inet = %s Failed (Server Unreachable)\n",inet); 
    } 
    else
    {
      for(int i=0;i<messages;i++)
        client_valwrite = write(client_sock ,messagesBuffer[i],packetLength);
    }
  }
  close(client_sock);
}

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

void savePacketToBuffer()
{
  for(int i=0;i<packetLength;i++)
    messagesBuffer[headOfBuffer][i] = (char)packet[i];
  headOfBuffer = (headOfBuffer+1) % messages;
}

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
