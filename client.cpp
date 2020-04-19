// Client side C/C++ Chat Program
// Jose Rodrigo Martinez, Antonio Reyes y Dieter de Wit
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <iostream>
#include <thread>
#include <curses.h>
#include <list>
#include "mensaje.pb.h"
#define PORT 8080
using namespace std;
using namespace chat;

// VARIABLES
bool exitProgram;
bool synAck;
int userId;
thread readThread;
thread inputThread;

int pantalla = 1; // Dice en que pantalla se encuentra el usuario
/*
        0: Pantalla de inicio
        1: Main Menu
        2: Broadcast
        3: Mensaje directo - lista de usuarios para seleccionar
        4: Mensaje directo - chat con persona seleccionado
        5: Cambio de estatus
        6: Lista de usuarios conectados
        7: Informacion de usuarios - Lista de usuarios
        8: Informacion de usuarios - Informacion de usuario seleccionado
        9: Ayuda
    */
list<char *> broadcastMessages;
list<char *> notificacionesArray;

//	IS SOCKET CLOSED
bool isSockClosed(int sock);

void handshake(int sock);

// THREAD FUNCTIONS
void writeText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin);
void readText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin);

// DIBUJAR
// Input
void getInput(WINDOW *win);

// Notifications
void printNotifications(WINDOW *win);

// Main
void renderMainWindow(WINDOW *win);
void printMainMenu(WINDOW *win);
void printInicio(WINDOW *win);
void printBroadcast(WINDOW *win);
void printChangeStatus(WINDOW *win);
void printAyuda(WINDOW *win);

// Funciones generales
char *getMessage(list<char *> _list, int _i);
void sendToServer(int sock, ClientMessage m);

int main(int argc, char const *argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	exitProgram = false;
	synAck = false;
	userId = 0;

	int sock = 0, valread;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
	{
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("\nConnection Failed \n");
		exit(2);
		return -1;
	}
	cout << "------> CONNECTED TO SOCKET" << endl << endl;

	handshake(sock);

	// INTERFAZ GRAFICA
	// Crear ventanas
	initscr();
	keypad(stdscr, TRUE);
	WINDOW *winMain = newwin(20, 120, 0, 0);
	WINDOW *inputWindow = newwin(3, 120, 20, 0);
	WINDOW *notificationsWindow = newwin(23, 60, 0, 122);

	renderMainWindow(winMain);
	printNotifications(notificationsWindow);

	readThread = thread(readText, sock, winMain, inputWindow, notificationsWindow);
	inputThread = thread(writeText, sock, winMain, inputWindow, notificationsWindow);

	while (!exitProgram)
	{
	}

	readThread.detach();
	inputThread.detach();

	//cout << "\r\e[A" << flush;
	cout << endl << "------> CLOSING SOCKET" << endl;
	close(sock);

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}

// CHECK IF SOCJET IS CLOSED
bool isSockClosed(int sock)
{
	char x;
interrupted:
	ssize_t r = recv(sock, &x, 1, MSG_DONTWAIT | MSG_PEEK);
	if (r < 0)
	{
		switch (errno)
		{
		case EINTR:
			goto interrupted;
		case EAGAIN:
			break;
		case ETIMEDOUT:
			break;
		case ENOTCONN:
			break;
		default:
			throw(errno);
		}
	}

	return r == 0;
}

// THREAD FUNCTIONS
void writeText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin)
{
	ClientMessage m;
	bool exitLoop = false;
	box(inputWin, 0, 0);
	waddstr(inputWin, "Input");
	wrefresh(inputWin);
	while (!exitLoop && !isSockClosed(sock))
	{

		int ch;
		nodelay(stdscr, TRUE);
		bool isIn = TRUE;
		char str[50];

		ch = mvwgetnstr(inputWin, 1, 3, str, 49);
		wmove(inputWin, 1, 3);
		waddstr(inputWin, "                                                  ");
		wrefresh(inputWin);

		// PANTALLA MENU
		if(pantalla == 1)
		{
			int opcion = 0;
			bool noHayError = true;
			try
			{
				opcion = atoi(str);
			}
			catch (const std::exception& e)
			{
				noHayError = false;
				notificacionesArray.push_back("ERROR - Ingresa una de las opciones.");
				printNotifications(notificationWin);
			}

			if(noHayError)
			{
				if( 0 < opcion && opcion < 8)
				{
					switch (opcion)
					{
					case 1:
						pantalla = 2;
						renderMainWindow(mainWin);
						break;
					case 2:
						pantalla = 3;
						renderMainWindow(mainWin);
						break;
					case 3:
						pantalla = 5;
						renderMainWindow(mainWin);
						break;
					case 4:
						pantalla = 6;
						renderMainWindow(mainWin);
						break;
					case 5:
						pantalla = 7;
						renderMainWindow(mainWin);
						break;
					case 6:
						pantalla = 9;
						renderMainWindow(mainWin);
						break;
					case 7:
						/* code */
						break;
					
					default:
						notificacionesArray.push_back("ERROR - Ingresa una de las opciones.");
						printNotifications(notificationWin);
						break;
					}
				} else {
					notificacionesArray.push_back("ERROR - Ingresa una de las opciones.");
					printNotifications(notificationWin);
				}
			}

		}

		// PANTALLA BOADCASTING
		else if (pantalla == 2)
		{
			BroadcastRequest *br(new BroadcastRequest);
			br->set_message(str);

			ClientMessage m;
			m.set_option(4);
			m.set_allocated_broadcast(br);

			sendToServer(sock, m);
		}
		
	}
	exitProgram = true;
}

void readText(int sock, WINDOW *mainWin, WINDOW *inputWin, WINDOW *notificationWin)
{
	int valread;
	char buffer[8192];
	while (!isSockClosed(sock))
	{
		valread = read(sock, buffer, 8192);
		//printf("%s\r",buffer );

		string ret(buffer, 8192);

		ServerMessage m2;
		m2.ParseFromString(ret);

		cout << endl
			 << "Mensaje Recibido!" << endl;
		cout << "Option: " << m2.option() << endl;
	}
	exitProgram = true;
}

// 3 Way handshake function
void handshake(int sock)
{
	cout << "HANDSHAKE INITIATED " << endl;

	// Configuring Synchronize Protobuf Message
	ClientMessage m;
	m.set_option(1);
	MyInfoSynchronize *synchronize(new MyInfoSynchronize);
	// TODO: Replace hardcoded username to a preset arg at runtime
	synchronize->set_username("Mochi");
	m.set_allocated_synchronize(synchronize);

	// Message Serialization
	string binary;
	m.SerializeToString(&binary);

	// Conversion to a buffer of char
	char wBuffer[binary.size() + 1];
	strcpy(wBuffer, binary.c_str());

	// Send Synchronize Request to server
	send(sock, wBuffer, strlen(wBuffer), 0);

	cout << "Synchronize Request sent to Server" << endl;

	// Read Socket sequence, waits for SYN/ACK
	int valueToRead;
	char buffer[8192];
	while (!synAck)
	{
		// Reading Socket
		valueToRead = read(sock, buffer, 8192);
		string ret(buffer, 8192);

		// Parse Protobuf Message received
		ServerMessage m2;
		m2.ParseFromString(ret);

		cout << endl
			 << "SYN/ACK Received from Server" << endl;

		// Receive and Save the UserId returned by the server
		userId = m2.myinforesponse().userid();
		cout << "UserId Assigned: " << userId << endl;

		if (m2.option() == 4 && userId != 0)
		{
			// Configuring Acknowledge Protobuf Message
			ClientMessage m;
			m.set_option(6);
			MyInfoAcknowledge *acknowledge(new MyInfoAcknowledge);
			acknowledge->set_userid(userId);
			m.set_allocated_acknowledge(acknowledge);

			// Message Serialization
			string binary;
			m.SerializeToString(&binary);

			// Conversion to a buffer of char
			char wBuffer[binary.size() + 1];
			strcpy(wBuffer, binary.c_str());

			// Send Synchronize Request to server
			send(sock, wBuffer, strlen(wBuffer), 0);

			cout << "Acknowledge Response sent to Server" << endl;
			cout << "HANDSHAKE COMPLETED" << endl;

			// Finish 3 Way Handshake
			synAck = true;
		}
	}
}

// --------------------------------------- MAIN WINDOW --------------------------------------- //
void renderMainWindow(WINDOW *win)
{
    switch (pantalla)
    {
    case 0:
        printInicio(win);
        break;

    case 1:
        printMainMenu(win);
        break;

    case 2:
        printBroadcast(win);
        break;

    case 5:
        printChangeStatus(win);
        break;

    case 9:
        printAyuda(win);
        break;
    
    default:
        printMainMenu(win);
        break;
    }
}

void printMainMenu(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Menu Principal");
    wmove(win, 1, 2);
    waddstr(win, "Seleccione una opcion: ");

    wmove(win, 3, 2);
    waddstr(win, "1) Broadcasting");

    wmove(win, 4, 2);
    waddstr(win, "2) Mensaje directo (DM)");

    wmove(win, 5, 2);
    waddstr(win, "3) Cambiar status");

    wmove(win, 6, 2);
    waddstr(win, "4) Listar usuarios conectados");

    wmove(win, 7, 2);
    waddstr(win, "5) Informacion de usuario");

    wmove(win, 8, 2);
    waddstr(win, "6) Ayuda");

    wmove(win, 9, 2);
    waddstr(win, "7) Salir");
    wrefresh(win);
}

void printInicio(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Bienvenido!");
    wmove(win, 1, 2);
    waddstr(win, "Escribe tu nombre:");
    wrefresh(win);
}

void printBroadcast(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Broadcast");

    int listBroadSize = broadcastMessages.size();
    int listIndex = 0;
    if(listBroadSize > 18)
    {
        listIndex = listBroadSize - 18;
    }

    int cursorPositionY = 1;

    for(int i = listIndex; i < listBroadSize; i++)
    {
        wmove(win, cursorPositionY, 2);
        waddstr(win, getMessage(broadcastMessages, i));
        cursorPositionY++;
    }

    wrefresh(win);
}

void printChangeStatus(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Cambiar status");

    wmove(win, 1, 2);
    waddstr(win, "Seleccione una de las opciones:");

    wmove(win, 3, 2);
    waddstr(win, "1) Conectado");

    wmove(win, 4, 2);
    waddstr(win, "2) Lejos");

    wmove(win, 5, 2);
    waddstr(win, "3) Ocupado");

    wrefresh(win);
}

void printAyuda(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Ayuda");

    wmove(win, 2, 2);
    waddstr(win, "Bienvenido al programa del chat, aqui te daremos unos puntos importantes para poder utilizar este programa:");

    wmove(win, 4, 2);
    waddstr(win, "- Si ves numeros como '1)', '2)'... significa que tienes que seleccionar alguna opcion escribiendo el numero.");

    wmove(win, 5, 2);
    waddstr(win, "- En caso de que no aparezcan numeros entre las opciones puedes escribir lo que tu quieras.");

    wmove(win, 6, 2);
    waddstr(win, "- Si quieres regresar a una pantalla solo escribe 'back' en la caja de Input.");

    wrefresh(win);
}

// --------------------------------------- INPUT WINDOW --------------------------------------- //
void getInput(WINDOW *win)
{
    box(win, 0, 0);
    waddstr(win, "Input");
    wrefresh(win);

    int ch;
    nodelay(stdscr, TRUE);
    bool isIn = TRUE;
    char str[50];

    while(TRUE) {
        ch = mvwgetnstr(win, 1, 3, str, 49);
        wmove(win, 1, 3);
        waddstr(win, "                                                  ");
        wrefresh(win);
    }
}

// --------------------------------------- NOTIFICATIONS WINDOW --------------------------------------- //
void printNotifications(WINDOW *win)
{
    wclear(win);
    box(win, 0, 0);
    waddstr(win, "Notificaciones");

    int listNotificacionesSize = notificacionesArray.size();
    int listIndex = 0;
    if(listNotificacionesSize > 21)
    {
        listIndex = listNotificacionesSize - 21;
    }

    int cursorPositionY = 1;

    for(int i = listIndex; i < listNotificacionesSize; i++)
    {
        wmove(win, cursorPositionY, 2);
        waddstr(win, getMessage(notificacionesArray, i));
        cursorPositionY++;
    }

    wrefresh(win);
}

// --------------------------------------- FUNCIONES GENERALES --------------------------------------- //
char* getMessage(list<char*> _list, int _i){
    list<char*>::iterator it = _list.begin();
    for(int i=0; i<_i; i++){
        ++it;
    }
    return *it;
}

void sendToServer(int sock, ClientMessage m)
{
	string binary;
	m.SerializeToString(&binary);
	char cstr[binary.size() + 1];
	strcpy(cstr, binary.c_str());
	send(sock , cstr , strlen(cstr) , 0 );
}