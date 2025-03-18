/* servTCPConcTh2.c 


CREATE TABLE Users (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL,
    parola TEXT NOT NULL,
    logged INTEGER
);


CREATE TABLE Mesaje (
    mesaj_id INTEGER PRIMARY KEY,
    conversatie_id INTEGER,
    text_mesaj TEXT,
    citit INTEGER,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    raspuns_la_mesaj_id INTEGER, 
    FOREIGN KEY (conversatie_id) REFERENCES conversatii(conversatie_id),
    FOREIGN KEY (raspuns_la_mesaj_id) REFERENCES mesaje(mesaj_id)
);

CREATE TABLE Conversatii (
    conversatie_id INTEGER PRIMARY KEY,
    expeditor_id INTEGER, 
    destinatar_id INTEGER, 
    FOREIGN KEY (expeditor_id) REFERENCES users(user_id),
    FOREIGN KEY (destinatar_id) REFERENCES users(user_id)
);



*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include<sqlite3.h>

/* portul folosit */
#define PORT 3458
#define BUFFER_SIZE 1023

int port=PORT;
int logged=0;
int mesExist=0;
/* codul de eroare returnat de anumite apeluri */
extern int errno;


typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
    char mesaj[BUFFER_SIZE];
}thData;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);/*functia de tratare a raspunsurilor*/


typedef struct {
    int sd;     // Socket descriptor
    int userId; // User ID
} LoggedUser;//structura utilizata pentru salvarea intr-o lista a userilor conectati la sistem

typedef struct Node {
    LoggedUser user;
    struct Node* next;
} Node;//definirea unui nod din lista de useri

Node* newNode(int sd, int userId) {//pentru adaugarea de noi elemente in lista de useri
    Node* node = (Node*) malloc(sizeof(Node));
    node->user.sd = sd;
    node->user.userId = userId;
    node->next = NULL;
    return node;
}

Node* head = NULL;//cream aici lista pentru a fi globala



Node* findUserById(Node* head, int userId) {//functia de cautare a unui user dupa id
    Node* current = head;
    while (current != NULL) {
        if (current->user.userId == userId) {
            return current; 
        }
        current = current->next; 
    }
    return NULL; // User not found
}

int deleteUserById(Node** head, int userId) {//functie utilizata pentru atunci cand vrem sa eliminam un user din lista de utilizatori conectati
    Node* current = *head;
    Node* previous = NULL;

    // verificam daca lista este goala
    if (current == NULL) {
        return -1; 
    }

    
    if (current->user.userId == userId) {//pt head
        *head = current->next; 
        free(current); 
        return 0; 
    }

    // Cautarea nodului pe care vrem sa il stergem
    while (current != NULL && current->user.userId != userId) {
        previous = current;
        current = current->next;
    }

    
    if (current == NULL) {
        return -1; 
    }

    
    previous->next = current->next;

    free(current); 
    return 0; 
}



static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);



void printList(Node* node) {/*functie utilizata pentru afisarea listei utilizatorilor conectati*/
    while (node != NULL) {
        printf("%d %d\n", node->user.sd,node->user.userId);
        node = node->next;
    }
    printf("\n");
}

        
int exists(char *username, char *parola) {/*functie pentru verificarea existentei utilizatorlui in baza de date-->login*/
    
    sqlite3 *db;
    int rc, found = 0;
    sqlite3_stmt *stmt;

    rc = sqlite3_open("MessagingDB.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Nu se poate deschide baza de date: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "SELECT * FROM Users WHERE username = ? AND parola = ?;";/*CAUTARE USER IN BAZA DE DATE DUPA USERNAME SI PAROLA*/
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, parola, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (found) {
        printf("Utilizatorul exista\n");
        
    } else {
        printf("Utilizatorul nu exista\n");
    }
    return found;
}

int findInDB(char *username)/*cauta un utilizator doar dupa username*/
{

    sqlite3 *db;
    int rc, found = 0;
    sqlite3_stmt *stmt;

    rc = sqlite3_open("MessagingDB.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Nu se poate deschide baza de date: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    const char *sql = "SELECT * FROM Users WHERE username = ?;";/*SELECTARE USER DUPA USERNAME*/
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
     while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (found) {
        printf("Utilizatorul exista\n");
        
    } else {
        printf("Utilizatorul nu exista\n");
    }
    return found;

}

void createNewUser(char *username, char *parola) {/*introducerea unui nou utilizator in baza de date-->sign up*/
    sqlite3 *db;
    int rc;
    sqlite3_stmt *stmt;

    rc = sqlite3_open("MessagingDB.db", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Nu se poate deschide baza de date: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return; 
    }

    const char *sql = "INSERT INTO Users (username, parola, logged) VALUES (?, ?, 1);";/*CREARE USER NOU*/

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return; 
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, parola, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt); 

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Utilizator adăugat cu succes\n");
    }

    sqlite3_finalize(stmt); 
    sqlite3_close(db); 
}


int findId(char *username){/*-1 daca userul nu exista sau id_ul userului in cazul in care exista*/

        int rc,userId=-1;
        sqlite3 *db;
        rc = sqlite3_open("MessagingDB.db", &db);
        sqlite3_stmt *stmt2;

        const char *sql2 = "SELECT  id FROM Users WHERE username = ?;";/*SELECTARE ID USER DUPA USERNAME*/
        
        sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL);
        sqlite3_bind_text(stmt2, 1, username, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt2) == SQLITE_ROW) {
            userId = sqlite3_column_int(stmt2, 0); 


        // Verifică dacă ID-ul utilizatorului a fost găsit
        if (userId != -1) 
        {
            printf("ID-ul utilizatorului este %d\n", userId);
            sqlite3_finalize(stmt2);
            return userId;
        }
        }
      return -1;
}

int findIdConversatie(int id_expeditor,int id_destinatar)/*gasirea unei conversatii cand avem destinatarul si expeditorul-->send message*/
{

        int rc;
        sqlite3 *db;
        sqlite3_stmt *stmt;
        rc = sqlite3_open("MessagingDB.db", &db);

         if (rc != SQLITE_OK) {
            fprintf(stderr, "Eroare la deschiderea bazei de date: %s\n", sqlite3_errmsg(db));
            return 1;
    }


        const char *sql = "SELECT conversatie_id,expeditor_id,destinatar_id FROM Conversatii WHERE expeditor_id=? AND destinatar_id=? ;";/*CAUTARE CONVERSATIE DUPA EXPEDITOR SI DESTINATAR*/
        
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
         if (rc != SQLITE_OK) {
            fprintf(stderr, "Eroare la pregătirea interogării: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 1;
    }
        sqlite3_bind_int(stmt, 1, id_expeditor);
        sqlite3_bind_int(stmt, 2, id_destinatar);
        
         if((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int id_conversatie = sqlite3_column_int(stmt, 0); 
            printf("ID-ul conversatiei este: %d\n", id_conversatie);
            sqlite3_finalize(stmt); 
            sqlite3_close(db); 
            return id_conversatie;        

         }
         else
         {
            sqlite3_finalize(stmt); 
            sqlite3_close(db); 
            return -1;

         }
}

  
void newMessagesInDB(int id_user, char *r) {/*printare mesaje noi-->see new messages*/
    
    int rc;
    sqlite3 *db;
    sqlite3_stmt *stmt;

    rc = sqlite3_open("MessagingDB.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database: %s\n", sqlite3_errmsg(db));
        return;
    }

    
    const char *sql = "SELECT m.mesaj_id, u.username, m.text_mesaj FROM Mesaje m JOIN Conversatii c ON c.conversatie_id=m.conversatie_id JOIN Users u ON u.id=c.destinatar_id WHERE m.citit=0 AND c.destinatar_id=?";/*SELECTAREA MESAJE NECITITE*/
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_int(stmt, 1, id_user);

    int foundMessages = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        foundMessages = 1;
        int mesaj_id = sqlite3_column_int(stmt, 0);
        const unsigned char *username = sqlite3_column_text(stmt, 1);
        char usernameStr[50]; 

        if(username != NULL) {
    strncpy(usernameStr, (const char *)username, sizeof(usernameStr) - 1);
    usernameStr[sizeof(usernameStr) - 1] = '\0';    
    } 
    else {/*username null*/
    strcpy(usernameStr, "Unknown User");
    }

        const char *text_mesaj = (const char *)sqlite3_column_text(stmt, 2);

        
        char line[1023]; 
        sprintf(line, "expeditor:%s mesaj:%s\n", usernameStr, text_mesaj);
        strcat(r, line); 
        
        sqlite3_stmt *updateStmt;
        const char *updateSql = "UPDATE Mesaje SET citit=1 WHERE mesaj_id=?";/*MARCAREA CITIRII MESAJULUI*/
        rc = sqlite3_prepare_v2(db, updateSql, -1, &updateStmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(updateStmt, 1, mesaj_id);
            sqlite3_step(updateStmt);
            sqlite3_finalize(updateStmt); 
        } else {
            fprintf(stderr, "Failed to prepare update statement: %s\n", sqlite3_errmsg(db));
        }
      
    }
      sqlite3_finalize(stmt);
      sqlite3_close(db);
    if (!foundMessages) {
        strcpy(r, "Nu exista mesaje noi"); 
    }

    
    
}

/*FUNCTIA DE TRIMITERE A MESAJELOR PRIN INTERMEDIUL SOCKETULUI*/

void sendMessageBySocket(Node* User,char *mesaj)
{
    int sd=User->user.sd;
    ssize_t bytes_written = write(sd, mesaj, strlen(mesaj));
    if (bytes_written < 0) {
        perror("Failed to write to socket");
       
    }
    
}


void sendMessageDB(int id_conversatie,char *mesaj,int read,int id_mesaj_replied)/*salvarea mesajelor in baza de date-->send message*/
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open("MessagingDB.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    
    const char *sql = "INSERT INTO Mesaje (conversatie_id, text_mesaj, citit, timestamp,raspuns_la_mesaj_id) VALUES (?, ?, ?, CURRENT_TIMESTAMP,?);";/*INTRODUCERE MESAJ IN BAZA DE DATE*/

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

   
    sqlite3_bind_int(stmt, 1, id_conversatie);
    sqlite3_bind_text(stmt, 2, mesaj, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, read);
    sqlite3_bind_int(stmt, 4, id_mesaj_replied);

    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Insertion failed: %s\n", sqlite3_errmsg(db));
    } else {
        printf("Message inserted successfully\n");
    }

  
    sqlite3_finalize(stmt);
    sqlite3_close(db);


}


void history(char *response,int userId)
{
     int rc;
    sqlite3 *db;
    sqlite3_stmt *stmt;

    rc = sqlite3_open("MessagingDB.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database: %s\n", sqlite3_errmsg(db));
        return;
    }

    
    const char *sql = "SELECT m.text_mesaj, m.timestamp, m.citit, u1.username, u2.username FROM Mesaje m JOIN Conversatii c ON m.conversatie_id = c.conversatie_id JOIN Users u1 on u1.id=c.expeditor_id JOIN Users u2 ON u2.id=c.destinatar_id WHERE c.expeditor_id = ? OR c.destinatar_id = ? ORDER BY c.conversatie_id,m.timestamp;";/*AFISARE ISTORIC CONVERSATIE*/
    
   rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
if (rc != SQLITE_OK) {
    fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return;
}

sqlite3_bind_int(stmt, 1, userId);
sqlite3_bind_int(stmt, 2, userId);


strcpy(response, "");

while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *mesaj = (const char *)sqlite3_column_text(stmt, 0);
    const char *timestamp = (const char *)sqlite3_column_text(stmt, 1);
    int citit = sqlite3_column_int(stmt, 2);
    const char *expeditor= (const char *)sqlite3_column_text(stmt, 3);
    const char *destinatar = (const char *)sqlite3_column_text(stmt, 4);
    
    char line[1024]; 
    snprintf(line, sizeof(line), "Conversatie intre:%s->%s, Mesaj: %s, Timestamp: %s\n", expeditor,destinatar,mesaj, timestamp);
    strncat(response, line, 1023 - strlen(response) - 1); 
}



if(strlen(response) == 0) {
    strcpy(response, "Nu exista mesaje noi");
}

sqlite3_finalize(stmt);
sqlite3_close(db);
}

int printare(int userId,int replyId)
{
     int rc;
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char r[1023]="";
    rc = sqlite3_open("MessagingDB.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database: %s\n", sqlite3_errmsg(db));
        return -4;
    }
    

    const char *sql = "SELECT m.mesaj_id, m.text_mesaj FROM Mesaje m JOIN Conversatii c ON c.conversatie_id=m.conversatie_id JOIN Users u ON u.id=c.destinatar_id WHERE c.destinatar_id=? AND c.expeditor_id=?";/*PRINTARE MESAJE CU TOT CU ID*/
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -4;
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, replyId);

    strcpy(r, "");

    int foundMessages = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        foundMessages = 1;

        int mesaj_id = sqlite3_column_int(stmt, 0);
        const char *text_mesaj = (const char *)sqlite3_column_text(stmt, 1);

        
        char line[1024];
        snprintf(line, sizeof(line), "mesaj_id:%d mesaj:%s\n", mesaj_id, text_mesaj);
        strcat(r, line); 
    }
    Node* User=findUserById(head,userId);
    int socket=User->user.sd;
    ssize_t bytes_written = write(socket,r, strlen(r));
    if (bytes_written < 0) {
    perror("Failed to write to socket");
    
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
     if (foundMessages==1)
        return 1;
    else
        return 0;
    

}


int sendMessage(int id_expeditor,int id_destinatar,char *mesaj,int citit,int replyId)/*handle pentru send a new message*/
{

            
            
         if(findIdConversatie(id_expeditor,id_destinatar)!=-1)//daca conversatia deja exista pur si simplu vom trimite mesajul
         {
            
            sendMessageDB(findIdConversatie(id_expeditor,id_destinatar),mesaj,citit,replyId);
         }
        else
        {
        printf("Inceput conversatie.\n");
        int rc;
        sqlite3 *db;
        sqlite3_stmt *stmt;
        rc = sqlite3_open("MessagingDB.db", &db);
         
         if (rc != SQLITE_OK) {
            fprintf(stderr, "Eroare la deschiderea bazei de date: %s\n", sqlite3_errmsg(db));
            return 1;
        }
        

        const char *sql="INSERT INTO Conversatii(expeditor_id,destinatar_id) VALUES (?,?);";/*CREAREA UNEI NOI CONVERSATII*/
         rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

        sqlite3_bind_int(stmt, 1, id_expeditor);
        sqlite3_bind_int(stmt, 2, id_destinatar);

        rc = sqlite3_step(stmt);
  
        if (rc != SQLITE_DONE) {
        fprintf(stderr, "Insertion failed: %s\n", sqlite3_errmsg(db));
        } else {
        printf("Message inserted successfully\n");
         }
         sqlite3_finalize(stmt);
         sqlite3_close(db);
        int id_conversatie=findIdConversatie(id_expeditor,id_destinatar);
        sendMessageDB(id_conversatie,mesaj,citit,replyId);
        }
        return 1;

   
      
}

int signUp(char *u,char* p)/*handle pentru sign up*/
{
    if(exists(u,p))
        return -1;//utilizatorul exista in baza de date deci trebuie sa sugeram logarea acestuia
    else if(findInDB(u)){
        printf("gasit!");
        return -2;//mumele de utilizator exista,deci sugeram alt nume de utilizator
    }
    else 
        {
            printf("se incearca logarea!");
            createNewUser(u,p);//deoarece nu exista nici utilizatorul nici parola vom introduce un nou utilizator in baza de date
            return findId(u);//trimitem id ul ca sa stim ce utilizator s-a logat la baza de date
        }
        
}

int login(char *username, char *parola) /*handle pentru login*/
{
   
    
    if (exists(username, parola) == 1)
     {
        int rc;
        sqlite3 *db;
        rc = sqlite3_open("MessagingDB.db", &db);
        const char *sql1 = "UPDATE Users SET logged = 1 WHERE username = ?;";/*SETAM STATUSUL UTILIZATORULUI*/

        sqlite3_stmt *stmt1;
        rc = sqlite3_prepare_v2(db, sql1, -1, &stmt1, NULL);

        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 0;
        }

        sqlite3_bind_text(stmt1, 1, username, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt1);

        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Update failed: %s\n", sqlite3_errmsg(db));

        } else {
            printf("Utilizatorul a fost setat ca si logat.\n");
        }

        sqlite3_finalize(stmt1);
        sqlite3_close(db);
       
        findId(username);
}

    else 
        return 0;
}



int logout(char *id)/*handle pentru logout*/
{
    int rc;
        sqlite3 *db;
        rc = sqlite3_open("MessagingDB.db", &db);
        const char *sql1 = "UPDATE Users SET logged = 0 WHERE id = ?;";/*DELOGAREA UTILIZATORULUI*/

        sqlite3_stmt *stmt1;
        rc = sqlite3_prepare_v2(db, sql1, -1, &stmt1, NULL);

        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return 0;
        }

        sqlite3_bind_text(stmt1, 1, id, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt1);

        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Update failed: %s\n", sqlite3_errmsg(db));

        } else {
            printf("Utilizatorul a fost setat ca si delogat.\n");
        }

        sqlite3_finalize(stmt1);
        sqlite3_close(db);
       

}



int sendNewMessage(int expeditor_id,char* destinatar,char* mesaj,int id_mesaj)/*handle pentru new message*/
{
    if(findId(destinatar)==-1)
    {
        return -2;//-2 in cazul in care destinatarul nu se afla in baza de date
    }
    else
    {

        int id_destinatar=findId(destinatar);
        if(expeditor_id==id_destinatar)
            return 0;
        //verificam daca utilizatorul este logat si daca este logat ii vom trimite mesajul prin intermediul socketului, dar il vom salva si in baza de date,altfel il vom salva doar in baza
        Node* User=findUserById(head,id_destinatar);
        if(User)
        {   
            sendMessageBySocket(User,mesaj);
            sendMessage(expeditor_id,id_destinatar,mesaj,1,id_mesaj);
       
        }
        else if(User==NULL)
        {
              sendMessage(expeditor_id,id_destinatar,mesaj,0,id_mesaj);
        }
         else 
            return 0;
    }
   
    
}


void notAnOption(char *r)/*functia pentru cand comanda nu exista*/
{

    strcpy(r,"Comanda invalida!");

}


void handle(char *mesaj,char *raspuns)/*hadle comenzi si prelucrarea acestora*/
{

    char *comanda;
    
    if(strchr(mesaj,'|')!=NULL)
    {
    comanda= strtok(mesaj, "|");
    }
    else
        comanda=mesaj;
    printf("%s",comanda);
    if(comanda!=NULL){
    if(strncmp(comanda,"signUp",7)==0)
    {
        char *username = strtok(NULL, "|");
        char *parola = strtok(NULL, "|");

      if (comanda && username && parola) {
       int r=signUp(username, parola);
        if(r==-1)
            strcpy(raspuns,"Utilizator& parola existente!");
        else if(r==-2)
            strcpy(raspuns,"Username existent!");
        else
             sprintf(raspuns, "%d",r);
    } 
       

    }

    else if(strncmp(comanda,"login",5)==0)
    {
        printf("login");
        int id_utilizator=-1;
        char *username = strtok(NULL, "|");
        char *parola = strtok(NULL, "|");

      if (comanda && username && parola) {
       int r=login(username, parola);
       printf("%d\n",r);
       if(r==0)
            strcpy(raspuns,"Imposibil de conectat!");
        else
        {
            
             sprintf(raspuns, "%d",r);
             

        }
           
    }
    }
    
    else if(strncmp(comanda,"logout",6)==0)//aici facem si stergerea utilizatorului din lista de useri logati
    {
        char *id = strtok(NULL, "|");
        logout(id);
        deleteUserById(&head,atoi(id));//eliminam utilizatorul din lista utilizatorilor conectati
        strcpy(raspuns,"Ati fost delogat");
    }
    else if(strncmp(comanda,"sendNewMessage",14)==0)
    {
         int expeditor_id=atoi(strtok(NULL, "|"));
         char* destinatar=strtok(NULL, "|");
         char *text_mesaj=strtok(NULL, "|");
         printf("%i",expeditor_id);
         printf("%s",destinatar);
         printf("%s",mesaj);
         if(sendNewMessage(expeditor_id,destinatar,text_mesaj,0)!=-2)
            strcpy(raspuns,"Mesajul a fost trimis.");
        else if(sendNewMessage(expeditor_id,destinatar,text_mesaj,0)==-2)
            strcpy(raspuns,"Mesaj imposibil de trimis! Destinatar invalid!");

    }
    else if(strncmp(comanda,"printare",8)==0)
    {
        int user=atoi(strtok(NULL, "|"));//aici avem userul nostru 
        char *us2=strtok(NULL,"|");
        int replyUser=findId(us2);
        if(printare(user,replyUser)==1)
        {
            mesExist=1;
            strcpy(raspuns,"Introduceti id-ul mesajului din conversatie la care doriti sa raspundeti:");
           
        }
        else
        {
            mesExist=0;
            strcpy(raspuns,"Nu exista mesaje!Introduceti textul exit!");
        }
        sleep(2);
       
    }
    else if(strncmp(comanda,"reply",5)==0)
    {
        if(mesExist==1){
       int expeditor_id=atoi(strtok(NULL, "|"));
       char* destinatar=strtok(NULL, "|");
       char *text_mesaj=strtok(NULL, "|");
       int id_mesaj=atoi(strtok(NULL, "|"));
        if(sendNewMessage(expeditor_id,destinatar,text_mesaj,id_mesaj)!=-2)
            strcpy(raspuns,"Reply-ul la mesaj a fost trimis.");
        else if(sendNewMessage(expeditor_id,destinatar,text_mesaj,id_mesaj)==-2)
            strcpy(raspuns,"Mesaj imposibil de trimis! Destinatar invalid!");
        }
        else if(mesExist==0)
             strcpy(raspuns,"Nu ati putut da reply deoarece nu exista mesaje in conversatie!");
    }
    else if(strncmp(comanda,"newMessages",11)==0)
    {
         int user_id=atoi(strtok(NULL, "|"));
        newMessagesInDB(user_id,raspuns);

    }
    else if(strncmp(comanda,"messageHistory",14)==0)
    {
        int user_id=atoi(strtok(NULL, "|"));
        history(raspuns,user_id);
    }
    else
    {
        notAnOption(raspuns);

    }
    }
    
}

int conectareClient()/*comunicarea client->server*/
{
  struct sockaddr_in server;	// structura folosita de server
  struct sockaddr_in from;	
  char mesaj[BUFFER_SIZE];
  int sd;		//descriptorul de socket 
  int pid;
  pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
  int i=0;
  

  /* crearea unui socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
  /* utilizarea optiunii SO_REUSEADDR */
  int on=1;
  setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
  
  /* pregatirea structurilor de date */
  bzero (&server, sizeof (server));
  bzero (&from, sizeof (from));
  
  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;	
  /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
  /* utilizam un port utilizator */
    server.sin_port = htons (PORT);
  
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
      perror ("[server]Eroare la bind().\n");
      return errno;
    }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 2) == -1)
    {
      perror ("[server]Eroare la listen().\n");
      return errno;
    }
  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
    {
      int client;
      thData * td; //parametru functia executata de thread     
      int length = sizeof (from);

      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);

     
      /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
	{
	  perror ("[server]Eroare la accept().\n");
	  continue;
	}
	
        /* s-a realizat conexiunea, se astepta mesajul */
    
	

	td=(struct thData*)malloc(sizeof(struct thData));	
	td->idThread=i++;
	td->cl=client;

	pthread_create(&th[i], NULL, &treat, td);	      
				
	} 
}

static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
		pthread_detach(pthread_self());		
		raspunde((struct thData*)arg);
		/* am terminat cu acest client, inchidem conexiunea */
		close ((intptr_t)arg);
		return(NULL);	

}



int main ()
{
    conectareClient();
  
};



void raspunde(void *arg)/*comunicare server->client*/
{   
    int i=0;
	struct thData tdL; 
    char mesaj[BUFFER_SIZE];
    char raspuns[BUFFER_SIZE]="";
	tdL= *((struct thData*)arg);
    while(1){
	if (read (tdL.cl, tdL.mesaj,sizeof(tdL.mesaj)) <= 0)
			{
			  printf("[Thread %d]\n",tdL.idThread);
			  perror ("Am incheiat conexiunea cu acest client\n");
              break;
			
			}
	
	printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread,tdL.mesaj);
		      
    strcpy(mesaj,tdL.mesaj);
    strcpy(raspuns,"");
    handle(mesaj,raspuns);
    printf("[server]Raspuns:%s\n",raspuns);

     if(atoi(raspuns)!=0)//aici clar se returneaza prin raspuns id_ul userului logat
    {
        if(head==NULL)
            head = newNode(tdL.cl,atoi(raspuns));
        else
             head->next = newNode(tdL.cl,atoi(raspuns));

    }
    
	 /* returnam mesajul clientului */
	 if (write (tdL.cl, raspuns, sizeof(raspuns)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() catre client.\n");
         break;
		}
	else
		printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);	
}
}


