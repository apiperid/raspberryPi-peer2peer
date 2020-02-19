/*
  void parseArguments(int argc)

  check if only the arguments are 3

*/
void parseArguments(int argc)
{
  if(argc!=3)
  {
    printf("************************************************\n");
    printf("*           Arguments must be 3                *\n");
    printf("* First argument : excecutable file            *\n");
    printf("* Second argument : list of aems ( .txt file ) *\n");
    printf("* Third argument : your aem                    *\n");
    printf("************************************************\n");
    exit(1);
  }  
}

/*
  int parseAemList(char *list,char *my_aem)

  parse the .txt file with AEMs given
  and check if is everything ok with these AEMs
  and count them

*/

int parseAemList(char *list,char *my_aem)
{
  int AemCounter = 0;
  uint32_t tmpAEM;

  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  FILE* file = fopen(list,"r");
  if(file==NULL)
  {
    printf("Error opening the file : %s\n",list);
    exit(1);
  }
  while((read = getline(&line, &len, file)) != -1)
  {
    tmpAEM = atoi(line);
    //printf("%" PRIu32 "\n",tmpAEM);
    if(tmpAEM<1000||tmpAEM>9999)
    {
      printf("An aem in the list is not in range [1000,9999] or the file contains characters too\n");
      exit(EXIT_FAILURE);
    }
    if(tmpAEM!=atoi(my_aem))
      AemCounter++;
  }
  fclose(file);
  //printf("%d\n",AemCounter);
  return AemCounter;
}

/*
  void checkInputAEM(char *my_aem)

  check if only my AEM is in range [1000,9999] = 4 digits

*/

void checkInputAEM(char *my_aem)
{
  if(atoi(my_aem)<1000 || atoi(my_aem)>9999)
  {
    printf("The input aem is out of range [1000,9999]\n");
    exit(EXIT_FAILURE);
  }
}

