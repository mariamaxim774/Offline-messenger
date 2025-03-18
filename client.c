
/* cliTCPIt.c */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include<signal.h>
#include<pthread.h>

#define PORT 3458
#define IP "127.0.0.1"
#define BUFFER_SIZE 1023
/* codul de eroare returnat de anumite apeluri */
extern int errno;


pthread_t thread_id;
int ok=1,logged=0;
int port=PORT;
int yes=1;
int userId=-1;
int sd;
int readComplete = 0;
int noMes=1;


void mesajLogout(char *mesaj)
{

   char message[200];
   strcpy(message,"logout");
   strcat(message,"|");
   char id[3];
   sprintf(id, "%d",userId);
   strcat(message,id);
   strcpy(mesaj,message);
   userId=-1;


}

void *continuouslyReadFromSocket(void *fd) {
    int socketFD = *(int*)fd;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    while(yes) {
        
        bytesRead = recv(socketFD, buffer, sizeof(buffer), 0);

        if (bytesRead > 0) {
            readComplete=0;
            buffer[bytesRead] = '\0'; 
            printf("\nReceived:%s\n", buffer);
            readComplete=1;             
            fflush(stdout);
           
            if(atoi(buffer)!=0)
            {
                userId=atoi(buffer);
            }
            
            
            
        } else if (bytesRead == 0) {
            printf("Connection closed by peer\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
       
    }
    
    close(socketFD); // Close the socket when done
    
    return NULL;
}




int handle_server()
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  		// mesajul trimis
  
  port = PORT;

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(IP);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }
 
 int yes=1;
 if (pthread_create(&thread_id, NULL, continuouslyReadFromSocket, (void*)&sd) != 0) {
        perror("Failed to create thread");
        return 1;
    }
    while(yes){
    
    char mesaj[BUFFER_SIZE]="";
    
    /* citirea mesajului */
    /*inainte de afisarea optiunilor principale vom adauga un meniu initial care sa opreasca utilizatorul din a face ceva inainte de a fi logat*/
    if(userId==-1){
    printf("Alegeti una din optiuni:\n1.sign up\n2.login\n");
    printf("[client]Introduceti o comanda: ");
    fflush(stdout);

    }

    else if(userId!=-1)
    {
       printf("Alegeti una din optiuni:\n1.send new message\n2.reply\n3.new messages\n4.see message history\n5.logout\n");
       printf("[client]Introduceti o comanda: ");
      fflush(stdout);

    }

    read(0,mesaj,sizeof(mesaj));

    mesaj[strlen(mesaj)] = '\0'; 
   
    char command[50];
    if(strncmp(mesaj,"login",5)==0 || strncmp(mesaj,"sign up",7)==0 || strncmp(mesaj,"send new message",16)==0)
    { 
       char username[50], password[50];
       char message[300]="";
         if(strncmp("login",mesaj,5)==0)
         {

        strcpy(command,"login");
        printf("Introduceți username-ul: ");
        scanf("%s", username);
        printf("Introduceți parola: ");
        scanf("%s", password);

         // Construirea mesajului
        snprintf(message, sizeof(message), "%s|%s|%s", command, username, password);

        strcpy(mesaj,message);
         }
         if(strncmp("sign up",mesaj,7)==0)
         {



        strcpy(command,"signUp");
        printf("Introduceți username-ul: ");
        scanf("%s", username);
         printf("Introduceți parola: ");
        scanf("%s", password);

       // Construirea mesajului
      snprintf(message, sizeof(message), "%s|%s|%s", command, username, password);

      strcpy(mesaj,message);


         }

        if(strncmp("send new message",mesaj,16)==0)
        {
          char textMesaj[100]="";
          char expeditor_id[3];
           sprintf(expeditor_id, "%d", userId);
           strcpy(command,"sendNewMessage");
           printf("Introduceți destinatarul:");  
           scanf("%s", username);
           fflush(stdout);
           printf("Introduceți textul mesajului:\n");
           fflush(stdout); 
           ssize_t num_read;
           num_read = read(STDIN_FILENO, textMesaj, sizeof(textMesaj) - 1);

     
           if (num_read > 0) {
            
            textMesaj[num_read] = '\0';

           
            if(textMesaj[num_read - 1] == '\n') {
                textMesaj[num_read - 1] = '\0';
           }

          
        } else {
        printf("Failed to read input or no input provided.\n");
       }

         
           snprintf(message, sizeof(message), "%s|%s|%s|%s",command,expeditor_id, username, textMesaj);
           strcpy(mesaj,message);
        

        }

    }

    else if(strncmp(mesaj,"logout",6)==0)
    {
      mesajLogout(mesaj);
    }
   else if(strncmp(mesaj,"new messages",12)==0)
{
    char comanda[25] = "newMessages|"; 
    char idUser[10];
    
    snprintf(idUser, sizeof(idUser), "%d", userId); 
    strncat(comanda, idUser, sizeof(comanda) - strlen(comanda) - 1); 
    strncpy(mesaj, comanda, sizeof(mesaj) - 1); 
    mesaj[sizeof(mesaj) - 1] = '\0'; 
}
else if(strncmp(mesaj,"see message history",19)==0)
{
  char comanda[25] = "messageHistory|"; 
    char idUser[10]; 
    
    snprintf(idUser, sizeof(idUser), "%d", userId); 
    strncat(comanda, idUser, sizeof(comanda) - strlen(comanda) - 1); 
    strncpy(mesaj, comanda, sizeof(mesaj) - 1); 
    mesaj[sizeof(mesaj) - 1] = '\0'; 
}
    
    else if(strncmp(mesaj,"reply",5)==0)
    {
        
        char comanda[30]="printare|";
        char textMesaj[100]="";
        char rasp[1023]="";
        char username[10],id[4],message[BUFFER_SIZE];
        char idUser[10]; 
        snprintf(idUser, sizeof(idUser), "%d", userId); 
        strcat(comanda,idUser);
        printf("Introduceti username-ul:");
        scanf("%s", username);
        strcat(comanda,"|");
        strcat(comanda,username);//o sa am printare|username
         if (write(sd, comanda, sizeof(comanda)) <= 0) 
             perror("[client]Eroare la write() spre server.\n");
        
         scanf("%s",id);
         fflush(stdout);
         if(atoi(id)!=0){
         printf("Introduceti textul mesajului:");
           fflush(stdout);
           ssize_t num_read;
           num_read = read(STDIN_FILENO, textMesaj, sizeof(textMesaj) - 1);

      
           if (num_read > 0) {
            
            textMesaj[num_read] = '\0';

            
            if(textMesaj[num_read - 1] == '\n') {
                textMesaj[num_read - 1] = '\0';
           }
          
        } else {
        printf("Failed to read input or no input provided.\n");
       }
       
         strcpy(comanda,"reply");
         snprintf(message, sizeof(message), "%s|%s|%s|%s|%s",comanda,idUser,username,textMesaj,id);
         strcpy(mesaj,message);
         }
    }
    
    
    /* trimiterea mesajului la server */
   
    if (write(sd, mesaj, sizeof(mesaj)) <= 0) {
        perror("[client]Eroare la write() spre server.\n");
        
    }
    
    sleep(1);
    printf("Doriti sa trimiteti o alta comanda? (da/nu): ");
    fflush(stdout);
    
    char raspuns[3];
    scanf("%s",raspuns);
    raspuns[strlen(raspuns)] = '\0'; 
    
    if (strncmp(raspuns, "nu", 2) == 0) {
        yes = 0;
        strcpy(mesaj,"");
        mesajLogout(mesaj);
        if (write(sd, mesaj, sizeof(mesaj)) <= 0) {//fac si delogarea utilizatorului
          perror("[client]Eroare la write() spre server.\n");
        }
        
        close(sd);
        pid_t pid=getpid();
        kill(pid,SIGKILL);
    
    }

    getchar();
    system("clear");
   
}
    
}


int main (int argc, char *argv[])
{
   
    handle_server();
    
    return 0;
  
}