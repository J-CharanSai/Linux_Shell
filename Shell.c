#include "stdio.h"
#include "dirent.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sys/wait.h"
#include "fcntl.h"
#include "time.h"
#include "termios.h"
#define BUFF_SIZE 1024
#define BUFF_SIZE_S 64
#define HISTORY_SIZE 10000

char** history;
static unsigned his_c = 0;
int bg = 0;


int read_line(char* buf); 
char **pipe_split(char *buf, int *N);
char **parse(char *buf,int *n, int m);
int spawn_proc(int in, int out, char** args, int n);
int execute(char** args, int n, int in, int out);
int exe_multiWatch(char* line, int in, int out);
int fork_pipes (int n, char** commands, int m);
void loop();
void add_2_history(char* cmd);
int lcs(char* X, char* Y,int i, int j);
char** search_his(char* str, int *n);
char* substr(const char *src, int m, int n);
int comp(char* s1, char* s2);

//non - canonical mode
struct termios saved_attributes;

void
reset_input_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
}

void
set_input_mode (void)
{
  struct termios tattr;
  char *name;

  /* Make sure stdin is a terminal. */
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "Not a terminal.\n");
      exit (EXIT_FAILURE);
    }

  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_input_mode);

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &tattr);
  tattr.c_lflag &= ~(ICANON|ECHO); /* Clear ICANON and ECHO. */
  tattr.c_cc[VMIN] = 1;
  tattr.c_cc[VTIME] = 0;
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &tattr);
}

int main(int argc, char** argv)
{
	// -- CODE --
	// load the configuration 
	
	//history
	history = malloc(HISTORY_SIZE * sizeof(char*));

	//run an REPL loop
	loop();

	return 0;
}
int comp(char* s1, char* s2)
{
    int i = strlen(s1);
    int j = strlen(s2);
    if(i < j)
    {
        for(int k = 0;k<i;k++)
        {
            if(s1[k] != s2[k]) return 0;
        }
        return 1;
    }
    else
    {
        for(int k = 0;k<j;k++)
        {
            if(s1[k] != s2[k]) return 0;
        }
        return 1;
        
    }
}

char* substr(const char *src, int m, int n)
{
    int len = n - m;
 
    char *dest = (char*)malloc(sizeof(char) * (len + 1));
    for (int i = m; i < n && (*(src + i) != '\0'); i++)
    {
        *dest = *(src + i);
        dest++;
    }
    *dest = '\0';
    return dest - len;
}
void sigExitHandler(int sig)
{
	signal(SIGINT, sigExitHandler);
	return;
}
void sigtstpHandler(int sig)
{
    // Reset handler to catch SIGTSTP next time
    signal(SIGTSTP, sigtstpHandler);
    bg = 1;
}

void loop(){
	char * line;
	char ** args;
	char ** commands;
	int status = 1; // state of the command
	int n, N;

	// handling signals 
	signal(SIGINT, sigExitHandler); // ctrl+C
	signal(SIGTSTP, sigtstpHandler);//ctrl+Z

	// read commands
	// pass commands  and then exe.

	do{
		line = malloc(sizeof(char) * BUFF_SIZE);
		printf("\n>>>");
		for(int i = 0;i<BUFF_SIZE;i++) line[i] = '\0';
		status = read_line(line);
		bg = 0;
		if(line[0] == '$') {status = EXIT_SUCCESS;continue;}
		else{
			add_2_history(line);
			char * line1 = malloc(sizeof(char) * BUFF_SIZE);
			strcpy(line1,line);
			char * line2 = malloc(sizeof(char) * BUFF_SIZE);
			strcpy(line2,line);
			commands = pipe_split(line,&N);
			char * tempy = strtok(line2, " ");
			if(!strcmp(tempy,"multiWatch"))
			{
					status = exe_multiWatch(line1,0,1);
			}
			else if(N == 1)
			{
				args = parse(commands[0],&n, 0);
				if(args != NULL && n > 0)
					status = execute(args,n,0,1);
			}	
			else
			{
				status = fork_pipes(N,commands, 0);
			}
		}
		// free the memory
		free(line);
		//free(commands);
		//free(args);
		fflush(stdin);
		fflush(stdout);
	}
	while(status == EXIT_SUCCESS); 
	return;
}


int read_line( char* buf)
{
    int bufsize = BUFF_SIZE;
    int pos = 0;
    int c;

    if(!buf){
        fprintf(stderr,"allocaiton error\n");
        exit(EXIT_FAILURE);
    }
    set_input_mode();
    int i=0,j =0,s = 0;
    while(1)
    {
        c = getchar();// stored it in an int not char
        if (c == EOF || c == '\n' || c==3)//EOF is an int
        {
            reset_input_mode();
            putchar('\n');
            if(pos == 0)
                buf[pos] = '$';
            else if(c == 3) buf[0] = '$';
            else 
                buf[pos] = '\0';
            return EXIT_SUCCESS; 
        }
        else if(c == 18)
        {
            reset_input_mode();
            buf[0] = '$';
            fprintf(stdout, "\nEnter the serach term:");
            char* search_term = malloc(BUFF_SIZE * sizeof(char));
            read_line(search_term);
            int num;
            char ** results = search_his(search_term,&num );
            if(num == 0)
            {
                fprintf(stdout, "No match for search term in history\n");
                return EXIT_SUCCESS;
            }
            else if(num == 1)
            {
                strcpy(buf,results[0]);
                fprintf(stdout, "%s\n",results[0]);
                    printf(">>>%s\n",buf);
                return EXIT_SUCCESS;
            }
            else
            {
                for(int i =0 ;i<num;i++)
                {
                    fprintf(stdout,"%d . %s\n",i+1,results[i]);
                }
                fprintf(stdout, "Enter Number :");
                int x;
                scanf("%d",&x);
                if(x >0 && x <= num)
                {
                    strcpy(buf,results[x-1]);
                    printf("\n>>>%s\n",buf);
                    return EXIT_SUCCESS;
                } 
                else 
                    {fprintf(stderr, "Incorrect number\n" );
                return EXIT_SUCCESS;}

            }
        }
        else if(c == '\t' && pos > 0)
        {
            j = 0;
            DIR *dr;
            struct dirent *en;
            char** files = malloc(BUFF_SIZE * sizeof(char*));
            char* cwd = malloc(BUFF_SIZE * sizeof(char));
            strcpy(cwd,".");
            if(s){
                strcat(cwd,"/");
                buf[pos] = '\0';
                strcat(cwd,substr(buf,i,s));
            }
            int k;
            dr = opendir(cwd); //open all or present directory
            if (dr) {
                while ((en = readdir(dr)) != NULL) {
                if(s) k = s;
                else k = i;
                if(comp(en->d_name,buf+k)){
                    files[j] = malloc(sizeof(char) * BUFF_SIZE);
                    strcpy(files[j],en->d_name);
                    j++;}
                }
                closedir(dr); //close all directory
          }
          if(j == 0)
          {
            continue;
          }
          else if(j == 1)
          {
            fprintf(stderr,"%s",files[0]+strlen(buf)-k);
            strcpy(buf+k,files[0]);
            pos = strlen(files[0]) + k ;
            continue;
          }
          else{
               for(int k1 = 0;k1<j;k1++)
               {
                fprintf(stderr, "\n%d. %s",k1+1, files[k1]);
               }
               char* N = malloc(sizeof(char)*BUFF_SIZE_S);
               int n1 = 0;
               while((c = getchar()) != ' ')
               {    if(c == '\n') break;
                    N[n1] = c;
                    n1++;
               }
               N[n1] = '\0';
               n1 = strtol(N,NULL,10);
               n1 -= 1;
               fprintf(stderr," %d %s ",n1,N);
               if(n1 >= 0 && n1 <= j){
                    buf[pos] = '\0';
                    fprintf(stderr,"\n>>>%s%s",buf,files[n1]+strlen(buf)-k);
                    strcpy(buf+k,files[n1]);
                    pos = strlen(files[n1]) + k ;

               }
               else
               {
                fprintf(stderr, "Invalid number \n");
                reset_input_mode();
                buf[0] = '$';
                return EXIT_SUCCESS;
               }
               continue;
            }
        }
        else
        {
        		if(c == 8) continue;
            if(c == '\t') continue;
            if(c == ' ') i = pos+1;
            if(c == '/') s = pos+1;
            buf[pos] = c;
            putchar(c);
        }
        pos++;

        if(pos >= bufsize)
        {
            int temp = bufsize;
            bufsize += BUFF_SIZE;
            buf = realloc(buf, bufsize * sizeof(char));
            if(!buf)
            {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
            for(int i = temp;i<bufsize;i++) buf[i] = '\0';
        }
    }
}

char **pipe_split(char *buf, int *N)
{
	int bufsize = BUFF_SIZE_S;
	int pos = 0;
	char **commands = malloc(bufsize * sizeof(char*));
	if(!commands)
	{
		fprintf(stderr,"allocation Error\n");
		exit(EXIT_FAILURE);
	}
	for(int i = 0;i<bufsize;i++)
	{
		commands[i] = malloc(bufsize * sizeof(char));
		if(commands[i] == NULL)
		{
			perror("allocation error\n");
			exit(EXIT_FAILURE);
		}
	}
	char* cmd = strtok(buf,"|");

	while(cmd != NULL)
	{
		commands[pos] = cmd;
		pos++;
		if(pos >= bufsize){
			bufsize += BUFF_SIZE_S;
			commands = realloc(commands, bufsize * sizeof(char*));
			if(!commands)
			{
				fprintf(stderr,"allocation Error\n");
				exit(EXIT_FAILURE);
			}
			for(int i = bufsize-BUFF_SIZE_S;i<bufsize;i++)
			{
				commands[i] = realloc(commands[i],bufsize * sizeof(char));
				if(commands[i] == NULL)
				{
					perror("allocation error\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		cmd = strtok(NULL, "|");
	}
	commands[pos] = NULL;
	*N = pos;
	return commands;
}

//parse the line
char** parse(char *buf,int *n, int m)
{
	int bufsize = BUFF_SIZE;
	int pos = 0;
	int l = 0,r = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	int buflen = strlen(buf);

	if(!tokens)
	{
		fprintf(stderr, "allocation error\n");
		exit(EXIT_FAILURE);
	}

	for(int i = 0;i<bufsize;i++)
	{
		tokens[i] = (char*)malloc(buflen * sizeof(char));
		if(tokens[i] == NULL)
		{
			perror("allocation error\n");
			exit(EXIT_FAILURE);
		}
	}

	int j,bufind = 0;

	for(int i = 0;i<buflen; )
	{
		if(buf[i] == ' '||buf[i] =='\t'||buf[i] == '\r'||buf[i] == '\a'||buf[i] == '\n')
		{
			
			if(strlen(tokens[(pos)]) > 0)
			{
				tokens[pos][bufind] = '\0';
				(pos)++;
			}
			bufind = 0;
			i++;
			
		}
		if(m == 1 && (buf[i] == '['||buf[i] ==','||buf[i] == ']'))
		{
			if(buf[i] == '[') l += 1;
			if(buf[i] == ']') r += 1;
			if( l==0 && r==1 || (l > 1 || r > 1) )
			{
				fprintf(stderr, "Error : Invalid command \n");
				exit(EXIT_SUCCESS);
			}
			if(strlen(tokens[(pos)]) > 0)
			{
				tokens[pos][bufind] = '\0';
				(pos)++;
			}
			bufind = 0;
			i++;
			
		}
		else if(buf[i] == '\"')
		{
			bufind = 0;
			j = i +1;
			while(j < buflen && buf[j] != '\"')
			{
				tokens[pos][bufind] = buf[j];
				bufind++;
				j++;
			}
			if(strlen(tokens[pos]) > 0){
			tokens[pos][bufind] = '\0';
			pos++;}
			bufind = 0;
			i = j + 1;
		}
		else if(buf[i] == '\'')
		{
			bufind = 0;
			j = i +1;
			while(j < buflen && buf[j] != '\'')
			{
				tokens[pos][bufind] = buf[j];
				bufind++;
				j++;
			}
			if(strlen(tokens[pos]) > 0){
			tokens[pos][bufind] = '\0';
			pos++;}
			bufind = 0;
			i = j + 1;
		}
		else
		{
			tokens[pos][bufind] = buf[i];
			bufind++;
			if(i == buflen-1)
			{
				if(strlen(tokens[pos]) > 0){
				tokens[pos][bufind] = '\0';
				pos++;}
			}
			i++;
		}


		if((pos)>=bufsize)
		{
			bufsize += BUFF_SIZE;
			tokens = (char**)realloc(tokens, sizeof(char*) * bufsize);
			if(!tokens)
			{
				fprintf(stderr, "allocation error\n");
				exit(EXIT_FAILURE);
			}

		}
	}
	*n = pos;
	return tokens;
}

int spawn_proc(int in, int out, char** args, int n)
{
  pid_t pid;
  char * arg;
  int status;
  if ((pid = fork ()) < 0)
  {
  	fprintf(stderr, "Error : error in forking \n" );
  	exit(EXIT_FAILURE);
  }
  else if(pid == 0)
    {
    	kill(pid, SIGINT);
      if (in != 0)
        {
          dup2 (in, 0);
          close (in);
        }

      if (out != 1)
        {
          dup2 (out, 1);
          close (out);
        }
      int sym_found = 0,j = 0;
      for(int i = 0;i < n;i++)
      {
      	arg = args[i];
      	if(!sym_found)
      	{
      		if(strcmp(arg,"&")==0||strcmp(arg,">")==0||strcmp(arg,"<")==0)
      		{
      			j = i;
      			sym_found = 1;
      		}
      	}
      	if(!strcmp(arg,">"))
      	{
      		int red_out_fd = open(args[i+1], O_CREAT | O_TRUNC | O_WRONLY, 0644);
      		dup2(red_out_fd, STDOUT_FILENO );
      	}
      	if(!strcmp(arg,"<"))
      	{
      		int red_in_fd = open(args[i+1], O_RDONLY);
      		dup2(red_in_fd, STDIN_FILENO );
      	}
      }
      if(!sym_found) j = n;
      char ** args2  = malloc(sizeof(char*) * (j+1));
      if(args2 == NULL)
      {
      	fprintf(stderr, "Error : allocation error\n" );
      	exit(EXIT_FAILURE);
      }
      int i = 0;
      for(;i<j;i++)
      {
      	args2[i] = args[i];
      }

      args2[i] = NULL;
      //execute the command by allocating child process address space & pid 
        //to new process by using execvp
  	  if(execvp(args2[0],args2) == -1 )
  	  {
  	  	fprintf(stderr,"Error : execution failed\n");
  	  	exit(EXIT_FAILURE);
  	  }

    }
    else// parent process
    {
    	pid_t wpid;
    	if(strcmp(args[n-1],"&"))
    	{
    		do{
    			wpid = waitpid(pid, &status, WUNTRACED);
    			//signal by ctrl+Z then break;
    			if(bg)
    			{
    				bg = 0;
    				break;
    			}
    		}while(!WIFEXITED(status) && !WIFSIGNALED(status));
    	}
    }

  return EXIT_SUCCESS;
}

int execute(char** args, int n, int in, int out)
{
	if(args[0] == NULL) return EXIT_SUCCESS;
	else
	{
		if(!strcmp(args[0],"exit"))
		{
			printf("Exiting from shell \n");
			return 1;
		}
		else if(!strcmp(args[0],"cd"))
		{
			if(n > 2) printf("ERROR : too many arguments are passed");
			else if(args[1] == NULL) printf("ERROR : No arguemt for cd\n");
			else if(chdir(args[1])) printf("ERROR : directory not found\n");
			return EXIT_SUCCESS;
		}
		else if(!strcmp(args[0],"help"))
		{
			printf("++++++++++++++++++++++++++++++++HELP PAGE++++++++++++++++++++++++++++++++ \n");
			printf("built-in commands: \n");
			printf("\tcd\t-\tchange directory\n");
			printf("\thelp\t-\topens helps page \n");
			printf("\texit\t-\tExits shell\n");
			printf("\tman\t-\ttype man <command> to know about the command\n");
			return EXIT_SUCCESS;
		}
		else return spawn_proc(in,out,args, n);
	}
}


int exe_multiWatch(char* line, int in, int out)
{
	if(line[strlen(line)-1] != ']')
	{
		fprintf(stderr, "Error : Invalid Commnad\n");
		return EXIT_SUCCESS;
	}
	int n;
	char** args = parse(line, &n, 1);
	
	int stas;
	for(int i=1;i<n;i++) // loop will run n times (n=5)
    {
        if(fork() == 0)
        {	
        	// usleep(100000 * i);
        	kill(getpid(),SIGINT);
     //    	time_t now = time(0);
					// char * tm_str = ctime(&now);
					// tm_str[strlen(tm_str)-1] = '\0';
					// char* temp = strtok(tm_str," ");
					// for(int i = 0;i<3;i++)
					// temp = strtok(NULL," ");
			  //   printf("\"%s\" , %s : \n", args[i],temp);
					// printf("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<\n");
					int N, stas = 1;
					
					char** commands = pipe_split(args[i],&N);
					stas = fork_pipes(N,commands, 1);
					// printf("->->->->->->->->->->->->->->->->->->->->->->->->->->\n\n");
          exit(0);
        }
    }
    for(int i=0;i<5;i++) // loop will run n times (n=5)
    wait(NULL);
	return EXIT_SUCCESS;
}


int fork_pipes (int n, char** commands, int m)
{
  int i,nargs,status;
  pid_t pid;
  int err = 0;
  int in, fd [2];
  char ** args;

  in = 0;

  for (i = 0; i < n - 1; ++i)
    {
      if(pipe (fd) == -1) 
      {
      	err = 1;
      	fprintf(stderr,"Error : error in piping\n");
      	break;
      }
      else{
      	args = parse(commands[i],&nargs, 0);
      	if(nargs > 0) status = execute(args,nargs,in,fd[1]);
      	else
      	{
      		err = 1;
      		fprintf(stderr, "Error : syntax error\n");
      		break;
      	}
      	close(fd[1]);
      	in = fd[0];
      }
    }
    if(!err)
    {
    	args = parse(commands[i],&nargs, 0);
      	if(nargs > 0){
      		if(m){
      		time_t now = time(0);
					char * tm_str = ctime(&now);
					tm_str[strlen(tm_str)-1] = '\0';
					char* temp = strtok(tm_str," ");
					for(int i = 0;i<3;i++)
					temp = strtok(NULL," ");
					printf("\"");
				  for(int i = 0;i<n-1;i++)
				  {
				  	printf("%s | ",commands[i]);
				  }
				  printf("%s\" : %s\n",commands[n-1],temp);
					printf("<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<-<\n");}
					status = execute(args,nargs,in,fd[1]);
					if(m)
					printf("->->->->->->->->->->->->->->->->->->->->->->->->->->-\n\n");
				}
      	else
      	{
      		err = 1;
      		fprintf(stderr, "Error : syntax error\n");
      	}
    }
    fflush(stdin);
    fflush(stdout);
    return status;
}
 
void add_2_history(char* cmd)
{
	if(his_c < HISTORY_SIZE)
	{
		history[his_c] = malloc(BUFF_SIZE * sizeof(char));
		strcpy(history[his_c],cmd);
		his_c++;
	}
	else
	{
		for(int i = 0;i<HISTORY_SIZE-1;i++)
		{
			strcpy(history[i],history[i+1]);
		}
		strcpy(history[HISTORY_SIZE-1],cmd);
	}
}

int lcs(char* X, char* Y,int m, int n)
{
	
	int LCS[m][n];
	int c = 0;
	for(int i = 0;i<=m;i++)
	{
		for(int j = 0;j<=n;j++)
		{
			if(i == 0 || j == 0)
			{
				LCS[i][j] = 0;
			}
			else if(X[i-1]==Y[j-1])
			{
				LCS[i][j] = LCS[i-1][j-1] + 1;
				c = c > LCS[i][j] ? c : LCS[i][j];
			}
			else LCS[i][j] = 0;
		}
	}
	return c;
}

char** search_his(char* str,int *n)
{
	char** cmds = malloc(HISTORY_SIZE * sizeof(char*));
	char** cmd = malloc( sizeof(char*));
	int j = 0;
	int len = strlen(str);
	for(int i = his_c-1;i>=0;i--)
	{
		if(!strcmp(str,history[i]))
		{
			cmd[0] = malloc(BUFF_SIZE* sizeof(char)); 
			strcpy(cmd[0],history[i]);
			*n = 1;
			return cmd;
		}
		else{
			if(lcs(history[i],str,strlen(history[i]),len) > 2)
			{
				cmds[j] = malloc(BUFF_SIZE * sizeof(char) );
				strcpy(cmds[j],history[i]);
				j++;
			}
		}
		
	}
	*n = j;
	return cmds;
}

