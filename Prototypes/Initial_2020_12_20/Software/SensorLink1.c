#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <windows.h>
#include <strsafe.h>
#include <initguid.h>
#include "lmusbdll.h"
#include "tiva_guids.h"

#include <mysqld_error.h>
#include <mysql.h>




// Mariadb Buffer Definitions =======================================================================================
#define STR_BUFFER_LEN 50
#define STATEMENT_LEN 100
#define QUERRY_LEN 200
#define DATA_LEN 20

// USB Buffer Definitions ===========================================================================================
#define MAX_STRING_LEN 256
#define MAX_ENTRY_LEN 256
#define USB_BUFFER_LEN 1216
#define ECHO_PACKET_SIZE 64
#define BLDVER "2.2.0.295"

// Buffer into which error messages are written:
TCHAR g_pcErrorString[MAX_STRING_LEN];

// The number of bytes transfered in the last measurement interval:
ULONG g_ulByteCount = 0;


// The total number of packets transfered
ULONG g_ulPacketCount = 0;

LPTSTR GetSystemErrorString(DWORD dwError){
/* Return a string describing the supplied system error code.
	
   \param dwError is a Windows system error code.
	
   This function returns a pointer to a string describing the error code
passed in the dwError parameter. If no description string can be found
the string "Unknown" is returned.
	
   \return Returns a pointer to a string describing the error.
*/

	DWORD dwRetcode;

	//	 Ask Windows for the error message description.
	dwRetcode = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, "%0", dwError, 0,
		g_pcErrorString, MAX_STRING_LEN, NULL);

	if (dwRetcode == 0)
	{
		return((LPTSTR)L"Unknown");
	}
	else
	{
		//
		// Remove the trailing "\n\r" if present.
		//
		if (dwRetcode >= 2)
		{
			if (g_pcErrorString[dwRetcode - 2] == '\r')
			{
				g_pcErrorString[dwRetcode - 2] = '\0';
			}
		}

		return(g_pcErrorString);
	}
}


double ReturnTemperature(char * pSensorOutput) {
	 double tempC, tempResult;

	tempC = atof(pSensorOutput); 

	tempResult = 0.0863 * tempC - 2.1500;

	return tempResult;
}


void UpdateThroughput(void){
// Print the throughput in terms of Kbps once per second.
	static ULONG ulStartTime = 0;
	static ULONG ulLast = 0;
	ULONG ulNow;
	ULONG ulElapsed;
	SYSTEMTIME sSysTime;

	//
	// Get the current system time.
	//
	GetSystemTime(&sSysTime);
	ulNow = (((((sSysTime.wHour * 60) +
		sSysTime.wMinute) * 60) +
		sSysTime.wSecond) * 1000) + sSysTime.wMilliseconds;

	//
	// If this is the first call, set the start time.
	//
	if (ulStartTime == 0)
	{
		ulStartTime = ulNow;
		ulLast = ulNow;
		return;
	}

	//
	// How much time has elapsed since the last measurement?
	//
	ulElapsed = (ulNow > ulStartTime) ? (ulNow - ulStartTime) : (ulStartTime - ulNow);

	//
	// We dump a new measurement every second.
	//
	if (ulElapsed > 1000)
	{

		//printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
		printf("\r%6dKbps Packets: %10d ", ((g_ulByteCount * 8) / ulElapsed), g_ulPacketCount);
		g_ulByteCount = 0;
		ulStartTime = ulNow;
	}
}


int LoadLoginData(char * pAddress, char * pPort,
				  char * pUsername, char * pPassword,
				  char * pName                       ) {

	char buffAddress[STR_BUFFER_LEN], buffPort[STR_BUFFER_LEN],
		 buffUsername[STR_BUFFER_LEN], buffPassword[STR_BUFFER_LEN],
		 buffName[STR_BUFFER_LEN];

	FILE * pLoginData;

	if (fopen_s(&pLoginData, "LoginData.txt", "r") != 0) {
		return 1; // Error while openning file
	}

	fgets(buffAddress, STR_BUFFER_LEN, pLoginData);
	fgets(buffPort, STR_BUFFER_LEN, pLoginData);
	fgets(buffUsername, STR_BUFFER_LEN, pLoginData);
	fgets(buffPassword, STR_BUFFER_LEN, pLoginData);
	fgets(buffName, STR_BUFFER_LEN, pLoginData);


    buffAddress[strcspn(buffAddress, "\n")] = 0;
	buffPort[strcspn(buffPort, "\n")] = 0;
	buffUsername[strcspn(buffUsername, "\n")] = 0;
	buffPassword[strcspn(buffPassword, "\n")] = 0;
	buffName[strcspn(buffName, "\n")] = 0;

	memcpy_s(pAddress, STR_BUFFER_LEN, buffAddress, STR_BUFFER_LEN);
	memcpy_s(pPort, STR_BUFFER_LEN, buffPort, STR_BUFFER_LEN);
	memcpy_s(pUsername, STR_BUFFER_LEN, buffUsername, STR_BUFFER_LEN);
	memcpy_s(pPassword, STR_BUFFER_LEN, buffPassword, STR_BUFFER_LEN);
	memcpy_s(pName, STR_BUFFER_LEN, buffName, STR_BUFFER_LEN);

	fclose(pLoginData);

	return 0;
}



int VerifyTableExistance(MYSQL * mysql, char * pName) {

	MYSQL_STMT * statement;
	char querryBuffer[200];

	statement = mysql_stmt_init(mysql);



	if (statement == NULL) {
		printf("Failed to initialize statement \n"); // IS THIS CORRECT?
		getchar();
		return 0;
	}
	else {
		//printf("Statement initialized \n");
	}

	char* message1 = "CREATE TABLE sensor1data(temperature VARCHAR(13), tm_year SMALLINT,"
		"tm_mon	TINYINT, tm_day TINYINT, tm_hour TINYINT, tm_min TINYINT, tm_sec TINYINT)";
	strncpy_s(querryBuffer, QUERRY_LEN, message1, strlen(message1));


	if (mysql_stmt_prepare(statement, querryBuffer, strlen(querryBuffer) == 0)) {
		//printf(" Statement prepared \n");
	}
	else {
		fprintf(stderr, " %s\n", mysql_error(mysql));
	}

	if (mysql_stmt_execute(statement) == 0) {

	}
	else {
		fprintf(stderr, " %s\n", mysql_error(mysql));
		//getchar();
		return 0;
	}


	mysql_stmt_close(statement);

	return 0;
}


int UploadSensorData(MYSQL * mysql, char *pData) {

	//char querryBuffer[QUERRY_LEN];

	MYSQL_STMT* statement;
	MYSQL_BIND bind[7];

	unsigned long dataLength;

	time_t rawtime = time(NULL);
	struct tm  timeinfo;

	dataLength = strlen(pData);

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime); // Microsoft's implementation of localtime_s is incompatible
									  //with the C standard due to reverse parameters.


	statement = mysql_stmt_init(mysql);
	if (!statement) {
		printf(" Failed to initialize statement when uploading data \n");
		return 1;
	}


	char* message1 = "INSERT INTO  sensor1data(temperature, tm_year, tm_mon, tm_day, "
		"tm_hour, tm_min, tm_sec) VALUES(?,?,?,?,?,?,?)";
	//printf("%s \n", message1);

	if (mysql_stmt_prepare(statement, message1, strlen(message1))) {
		fprintf(stderr, " mysql_stmt_prepare(), INSERT failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(statement));
		return 2;
	}

	printf("%s \n", pData);
	printf("%i \n", strlen(pData));

	// Bind the data:
	memset(bind, 0, sizeof(bind));
	
	// CHAR Temperature
	bind[0].buffer_type = MYSQL_TYPE_STRING;
	bind[0].buffer = (char*)pData;
	bind[1].buffer_length = sizeof(pData);
	bind[0].is_null = 0;
	bind[0].length = &dataLength;

	// SMALLINT tm_year
	bind[1].buffer_type = MYSQL_TYPE_SHORT;
	bind[1].buffer = (char*)&timeinfo.tm_year;
	bind[1].is_null = 0;
	bind[1].length =0;

	// TINYINT tm_mon
	bind[2].buffer_type = MYSQL_TYPE_SHORT;
	bind[2].buffer = (char*)&timeinfo.tm_mon;
	bind[2].is_null = 0;
	bind[2].length = 0;

	// TINYINT tm_day
	bind[3].buffer_type = MYSQL_TYPE_SHORT;
	bind[3].buffer = (char*)&timeinfo.tm_mday;
	bind[3].is_null = 0;
	bind[3].length = 0;

	// TINYINT tm_hour
	bind[4].buffer_type = MYSQL_TYPE_SHORT;
	bind[4].buffer = (char*)&timeinfo.tm_hour;
	bind[4].is_null = 0;
	bind[4].length = 0;
	
	// TINYINT tm_min
	bind[5].buffer_type = MYSQL_TYPE_SHORT;
	bind[5].buffer = (char*)&timeinfo.tm_min;
	bind[5].is_null = 0;
	bind[5].length = 0;

	// TINYINT tm_mon
	bind[6].buffer_type = MYSQL_TYPE_SHORT;
	bind[6].buffer = (char*)&timeinfo.tm_sec;
	bind[6].is_null = 0;
	bind[6].length = 0;


	
	// Bind the buffers:
	if (mysql_stmt_bind_param(statement, bind)) {
		fprintf(stderr, " mysql_stmt_bind_param() failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(statement));
		return 3;
	}
	
	// Execute statement:
	if (mysql_stmt_execute(statement))
	{
		fprintf(stderr, " mysql_stmt_execute(), 1 failed\n");
		fprintf(stderr, " %s\n", mysql_stmt_error(statement));
		return 4;
	}
	

	mysql_stmt_close(statement);
	return 0;
}



int main(int argc, char* argv[]) {
	char dbAddress[STR_BUFFER_LEN], dbPort[STR_BUFFER_LEN];
	char dbUsername[STR_BUFFER_LEN], dbPassword[STR_BUFFER_LEN], dbName[STR_BUFFER_LEN];

	BOOL bResult;
	BOOL bDriverInstalled;
	BOOL bEcho;
	char szBuffer[USB_BUFFER_LEN];
	ULONG ulWritten;
	ULONG ulRead;
	ULONG ulLength;
	DWORD dwError;
	LMUSB_HANDLE hUSB;
	
	MYSQL* mysql = NULL;

	if (LoadLoginData(dbAddress, dbPort,
					  dbUsername, dbPassword, dbName) != 0 ) {
		printf("Error loading login data \n");
		getchar();
		return 1;
	}

	printf("Login information: \n");
	printf("IP: %s \n", dbAddress);
	printf("Port: %s \n", dbPort);
	printf("Username: %s \n", dbUsername);
	printf("Password: %s \n", dbPassword);
	printf("Database: %s \n", dbName);



	// Attempt connection to Mariadb:
	// Initialize MySQL handle:
	if (mysql = mysql_init(mysql)) {
		printf("MySQL handle initialized \n");
	} 
	else{
		printf(" Failed to initialize MySQL handle \n");
		getchar();
		return 1;
	}


	// Connect to Mariadb:
	if (!mysql_real_connect( mysql,
		dbAddress,
		dbUsername,
		dbPassword,
		dbName,
		atoi(dbPort),
		NULL,
		0
	                               )== 0) {
		printf(" Connected \n");
	}
	else {
		printf(" Failed to connect \n");
	}


	VerifyTableExistance(mysql, dbName);
	
	// Find our USB device and prepare it for communication:
	hUSB = InitializeDevice(BULK_VID, BULK_PID,
		(LPGUID) & (GUID_DEVINTERFACE_TIVA_BULK),
		&bDriverInstalled);


	bEcho = ((argc > 1) && (argv[1][1] == 'e')) ? TRUE : FALSE;
	if (!bEcho){
		printf("Echo mode configured\n");;
	}
	else{
		printf("Error while configuring 'echo mode' \n");
		getchar();
		return 1;
	}


	if (hUSB) { // Connected to the device:

			while(1)
			{

				//
				// The device was found and successfully configured. Now get a string from
				// the user...
				//
				do
				{
					printf("\nEnter a string (EXIT to exit): ");
					fgets(szBuffer, MAX_ENTRY_LEN, stdin);
					printf("\n");

					//
					// How many characters were entered (including the trailing '\n')?
					//
					ulLength = (ULONG)strlen(szBuffer);

					if (ulLength <= 1)
					{
						//
						// The string is either nothing at all or a single '\n' so reprompt the user.
						//
						printf("\nPlease enter some text.\n");
						ulLength = 0;
					}
					else
					{
						//
						// Get rid of the trailing '\n' if there is one there.
						//
						if (szBuffer[ulLength - 1] == '\n')
						{
							szBuffer[ulLength - 1] = '\0';
							ulLength--;
						}
					}
				} while (ulLength == 0);

				//
				// Are we being asked to exit the application?
				//
				if (!(strcmp("EXIT", szBuffer)))
				{
					//
					// Yes - drop out and exit.
					//
					printf("Exiting on user request.\n");
					break;
				}

				//
				// Write the user's string to the device.
				//
				bResult = WriteUSBPacket(hUSB, szBuffer, ulLength, &ulWritten);
				if (!bResult)
				{
					// We failed to write the data for some reason.
					dwError = GetLastError();
					printf("Error %d (%S) writing to bulk OUT pipe.\n", dwError,
						GetSystemErrorString(dwError));
				}
				else
				{
					// We wrote data successfully so now read it back.
					printf("Wrote %d bytes to the device. Expected %d\n",
						ulWritten, ulLength);

					// We expect the same number of bytes as we just sent.
					dwError = ReadUSBPacket(hUSB, szBuffer, ulWritten, &ulRead,
						INFINITE, NULL);

					if (dwError != ERROR_SUCCESS)
					{
						// We failed to read from the device.
						printf("Error %d (%S) reading from bulk IN pipe.\n", dwError,
							GetSystemErrorString(dwError));
					}
					else
					{
						// Add a string terminator to the returned data (this
						// should already be there but, just in case...)
						szBuffer[ulRead] = '\0';

						printf("Read %d bytes from device. Expected %d\n",
							ulRead, ulWritten);
						printf("\nReturned string: \"%s\"\n", szBuffer);

						double measuredTemperature = ReturnTemperature(szBuffer);
						char tempBuffer[50];


						printf(" Temperature is: %f \n \n", measuredTemperature);


						_gcvt_s(tempBuffer, sizeof(tempBuffer), measuredTemperature, 3);

						UploadSensorData(mysql,  tempBuffer);

					}
				}
			}
	}
	else { // If an error was reporte while trying to connect to the device:
		dwError = GetLastError();

		printf(" Unable to initialize device \n");
		printf("Error code is %d (%S)\n\n", dwError, GetSystemErrorString(dwError));
		getchar();
		return 1;
	}


	//UploadSensorData(mysql, "28.600");

	
	
	mysql_close(mysql);


	printf(" Program end \n");
	getchar();

	return 0;
}


/*
https://en.cppreference.com/w/c/chrono/localtime
https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/localtime-s-localtime32-s-localtime64-s?view=msvc-160&viewFallbackFrom=vs-2019

https://dev.mysql.com/doc/c-api/8.0/en/mysql-stmt-bind-param.html
https://dev.mysql.com/doc/c-api/8.0/en/mysql-stmt-execute.html
https://dev.mysql.com/doc/c-api/8.0/en/c-api-prepared-statement-data-structures.html
https://stackoverflow.com/questions/34286962/c-mysql-prepared-statement-problems-with-bind


https://docs.microsoft.com/pt-br/cpp/c-runtime-library/reference/gcvt-s?view=msvc-160
*/