/**
 * @file
 * @brief ESP32-FTP-Client Realization
 *
 *
 * @note
 * Copyright © 2019 Evgeniy Ivanov. Contacts: <strelok1290@gmail.com>
 * Copyright © 1996-2001, 2013, 2016 Thomas Pfau, tfpfau@gmail.com
 * All rights reserved.
 * @note
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	   http://www.apache.org/licenses/LICENSE-2.0
 * @note
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @note
 * This file is a part of JB_Lib.
 */


#include <string.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include "FtpClient.h"

#include "netdb.h"

#include "esp_log.h"

#if !defined FTP_CLIENT_DEFAULT_MODE
#define FTP_CLIENT_DEFAULT_MODE			FTP_CLIENT_PASSIVE
#endif

#define FTP_CLIENT_CONTROL					0
#define FTP_CLIENT_READ						1
#define FTP_CLIENT_WRITE					2

struct NetBuf {
	char* cput;
	char* cget;
	int handle;
	int cavail, cleft;
	char* buf;
	int dir;
	NetBuf_t* ctrl;
	NetBuf_t* data;
	int cmode;
	struct timeval idletime;
	FtpClientCallback_t idlecb;
	void* idlearg;
	unsigned long int xfered;
	unsigned long int cbbytes;
	unsigned long int xfered1;
	char response[FTP_CLIENT_RESPONSE_BUFFER_SIZE];
};

static bool isInitilized = false;
static FtpClient ftpClient_;

/*Internal use functions*/
static int socketWait(NetBuf_t* ctl);
static int readResponse(char c, NetBuf_t* nControl);
static int readLine(char* buffer, int max, NetBuf_t* ctl);
static int sendCommand(const char* cmd, char expresp, NetBuf_t* nControl);
static int xfer(const char* localfile, const char* path,
	NetBuf_t* nControl, int typ, int mode);
static int openPort(NetBuf_t* nControl, NetBuf_t** nData, int mode, int dir);
static int writeLine(const char* buf, int len, NetBuf_t* nData);
static int acceptConnection(NetBuf_t* nData, NetBuf_t* nControl);

/*Miscellaneous Functions*/
static int siteFtpClient(const char* cmd, NetBuf_t* nControl);
static char* getLastResponseFtpClient(NetBuf_t* nControl);
static int getSysTypeFtpClient(char* buf, int max, NetBuf_t* nControl);
static int getFileSizeFtpClient(const char* path,
	unsigned int* size, char mode, NetBuf_t* nControl);
static int getModDateFtpClient(const char* path, char* dt,
	int max, NetBuf_t* nControl);
static int setCallbackFtpClient(const FtpClientCallbackOptions_t* opt, NetBuf_t* nControl);
static int clearCallbackFtpClient(NetBuf_t* nControl);
/*Server connection*/
static int connectFtpClient(const char* host, uint16_t port, NetBuf_t** nControl);
static int loginFtpClient(const char* user, const char* pass, NetBuf_t* nControl);
static void quitFtpClient(NetBuf_t* nControl);
static int setOptionsFtpClient(int opt, long val, NetBuf_t* nControl);
/*Directory Functions*/
static int changeDirFtpClient(const char* path, NetBuf_t* nControl);
static int makeDirFtpClient(const char* path, NetBuf_t* nControl);
static int removeDirFtpClient(const char* path, NetBuf_t* nControl);
static int dirFtpClient(const char* outputfile, const char* path, NetBuf_t* nControl);
static int nlstFtpClient(const char* outputfile, const char* path,
	NetBuf_t* nControl);
static int mlsdFtpClient(const char* outputfile, const char* path,
	NetBuf_t* nControl);
static int changeDirUpFtpClient(NetBuf_t* nControl);
static int pwdFtpClient(char* path, int max, NetBuf_t* nControl);
/*File to File Transfer*/
static int getDataFtpClient(const char* outputfile, const char* path,
	char mode, NetBuf_t* nControl);
static int putDataFtpClient(const char* inputfile, const char* path, char mode,
	NetBuf_t* nControl);
static int deleteDataFtpClient(const char* fnm, NetBuf_t* nControl);
static int renameFtpClient(const char* src, const char* dst, NetBuf_t* nControl);
/*File to Program Transfer*/
static int accessFtpClient(const char* path, int typ, int mode, NetBuf_t* nControl,
	NetBuf_t** nData);
static int readFtpClient(void* buf, int max, NetBuf_t* nData);
static int writeFtpClient(const void* buf, int len, NetBuf_t* nData);
static int closeFtpClient(NetBuf_t* nData);


static const char *TAG = "FTP";


/*
 * socket_wait - wait for socket to receive or flush data
 *
 * return 1 if no user callback, otherwise, return value returned by
 * user callback
 */
static int socketWait(NetBuf_t* ctl)
{
	fd_set fd;
	fd_set* rfd = NULL;
	fd_set* wfd = NULL;
	struct timeval tv;
	int rv = 0;

	if ((ctl->dir == FTP_CLIENT_CONTROL) || (ctl->idlecb == NULL))
		return 1;
	if (ctl->dir == FTP_CLIENT_WRITE)
		wfd = &fd;
	else
		rfd = &fd;
	FD_ZERO(&fd);
	do
	{
		FD_SET(ctl->handle, &fd);
		tv = ctl->idletime;
		rv = select((ctl->handle + 1), rfd, wfd, NULL, &tv);
		if (rv == -1) {
			rv = 0;
			strncpy(ctl->ctrl->response, strerror(errno),
						sizeof(ctl->ctrl->response));
			break;
		}
		else if (rv > 0) {
			rv = 1;
			break;
		}
	}
	while ((rv = ctl->idlecb(ctl, ctl->xfered, ctl->idlearg)));
	return rv;
}



/*
 * read a line of text
 *
 * return -1 on error or bytecount
 */
static int readLine(char* buffer, int max, NetBuf_t* ctl)
{
	if ((ctl->dir != FTP_CLIENT_CONTROL) && (ctl->dir != FTP_CLIENT_READ))
		return -1;
	if (max == 0)
		return 0;

	int x,retval = 0;
	char *end,*bp = buffer;
	int eof = 0;
	while (1) {
		if (ctl->cavail > 0) {
			x = (max >= ctl->cavail) ? ctl->cavail : (max-1);
			end = memccpy(bp, ctl->cget, '\n',x);
			if (end != NULL)
				x = end - bp;
			retval += x;
			bp += x;
			*bp = '\0';
			max -= x;
			ctl->cget += x;
			ctl->cavail -= x;
			if (end != NULL)
			{
				bp -= 2;
				if (strcmp(bp,"\r\n") == 0) {
					*bp++ = '\n';
					*bp++ = '\0';
					--retval;
				}
				break;
			}
		}
		if (max == 1) {
			*buffer = '\0';
			break;
		}
		if (ctl->cput == ctl->cget) {
			ctl->cput = ctl->cget = ctl->buf;
			ctl->cavail = 0;
			ctl->cleft = FTP_CLIENT_BUFFER_SIZE;
		}
		if (eof) {
			if (retval == 0)
				retval = -1;
			break;
		}
		if (!socketWait(ctl))
			return retval;
		if ((x = recv(ctl->handle, ctl->cput,ctl->cleft, 0)) == -1) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client Error: realLine, read");
			#endif
			retval = -1;
			break;
		}
		if (x == 0)
			eof = 1;
		ctl->cleft -= x;
		ctl->cavail += x;
		ctl->cput += x;
	}
	return retval;
}



/*
 * read a response from the server
 *
 * return 0 if first char doesn't match
 * return 1 if first char matches
 */
static int readResponse(char c, NetBuf_t* nControl)
{
	char match[5];
	if (readLine(nControl->response,
			FTP_CLIENT_RESPONSE_BUFFER_SIZE, nControl) == -1) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client Error: readResponse, read failed");
		#endif
		return 0;
	}
	#if FTP_CLIENT_DEBUG == 2
	printf("FTP Client Response: %s\n\r", nControl->response);
	#endif
	if (nControl->response[3] == '-')
	{
		strncpy(match, nControl->response, 3);
		match[3] = ' ';
		match[4] = '\0';
		do {
			if (readLine(nControl->response,
					FTP_CLIENT_RESPONSE_BUFFER_SIZE, nControl) == -1) {
				#if FTP_CLIENT_DEBUG
				perror("FTP Client Error: readResponse, read failed");
				#endif
				return 0;
			}
			#if FTP_CLIENT_DEBUG == 2
			printf("FTP Client Response: %s\n\r", nControl->response);
			#endif
		}
		while (strncmp(nControl->response, match, 4));
	}
	if(nControl->response[0] == c)
		return 1;
	else
		return 0;
}



/*
 * sendCommand - send a command and wait for expected response
 *
 * return 1 if proper response received, 0 otherwise
 */
static int sendCommand(const char* cmd, char expresp, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if (nControl->dir != FTP_CLIENT_CONTROL)
		return 0;
	#if FTP_CLIENT_DEBUG == 2
	printf("FTP Client sendCommand: %s\n\r", cmd);
	#endif
	if ((strlen(cmd) + 3) > sizeof(buf))
		return 0;
	sprintf(buf, "%s\r\n", cmd);
	if (send(nControl->handle, buf, strlen(buf), 0) <= 0) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client sendCommand: write");
		#endif
		return 0;
	}
	return readResponse(expresp, nControl);
}



/*
 * Xfer - issue a command and transfer data
 *
 * return 1 if successful, 0 otherwise
 */
static int xfer(const char* localfile, const char* path,
	NetBuf_t* nControl, int typ, int mode)
{
	FILE* local = NULL;
	NetBuf_t* nData;

	if (localfile != NULL) {
		char ac[4];
		memset( ac, 0, sizeof(ac) );
		if (typ == FTP_CLIENT_FILE_WRITE)
			ac[0] = 'r';
		else
			ac[0] = 'w';
		if (mode == FTP_CLIENT_IMAGE)
			ac[1] = 'b';
		local = fopen(localfile, ac);
		if (local == NULL) {
			strncpy(nControl->response, strerror(errno),
						sizeof(nControl->response));
			return 0;
		}
	}
	if(local == NULL)
		local = (typ == FTP_CLIENT_FILE_WRITE) ? stdin : stdout;
	if (!accessFtpClient(path, typ, mode, nControl, &nData)) {
		if (localfile) {
			fclose(local);
			if (typ == FTP_CLIENT_FILE_READ)
				unlink(localfile);
		}
		return 0;
	}

	int rv = 1;
	int l = 0;
	char* dbuf = malloc(FTP_CLIENT_BUFFER_SIZE);
	if (typ == FTP_CLIENT_FILE_WRITE) {
		while ((l = fread(dbuf, 1, FTP_CLIENT_BUFFER_SIZE, local)) > 0) {
			int c = writeFtpClient(dbuf, l, nData);
			if (c < l) {
				printf("Ftp Client xfer short write: passed %d, wrote %d\n", l, c);
				rv = 0;
				break;
			}
		}
	}
	else {
		while ((l = readFtpClient(dbuf, FTP_CLIENT_BUFFER_SIZE, nData)) > 0) {
			if (fwrite(dbuf, 1, l, local) == 0) {
				#if FTP_CLIENT_DEBUG
				perror("FTP Client xfer localfile write");
				#endif
				rv = 0;
				break;
			}
		}
	}
	free(dbuf);
	fflush(local);
	if(localfile != NULL){
		fclose(local);
		if(rv != 1)
			unlink(localfile);
	}
	closeFtpClient(nData);
	return rv;
}



/*
 * openPort - set up data connection
 *
 * return 1 if successful, 0 otherwise
 */
static int openPort(NetBuf_t* nControl, NetBuf_t** nData, int mode, int dir)
{
	union
	{
		struct sockaddr sa;
		struct sockaddr_in in;
	} sin;

	if (nControl->dir != FTP_CLIENT_CONTROL)
		return -1;
	if ((dir != FTP_CLIENT_READ) && (dir != FTP_CLIENT_WRITE)) {
		sprintf(nControl->response, "Invalid direction %d\n", dir);
		return -1;
	}
	if ((mode != FTP_CLIENT_ASCII) && (mode != FTP_CLIENT_IMAGE)) {
		sprintf(nControl->response, "Invalid mode %c\n", mode);
		return -1;
	}
	unsigned int l = sizeof(sin);
	if (nControl->cmode == FTP_CLIENT_PASSIVE) {
		memset(&sin, 0, l);
		sin.in.sin_family = AF_INET;
		if (!sendCommand("PASV", '2', nControl))
			return -1;
		char* cp = strchr(nControl->response,'(');
		if (cp == NULL)
			return -1;
		cp++;
		unsigned int v[6];
		sscanf(cp,"%u,%u,%u,%u,%u,%u",&v[2],&v[3],&v[4],&v[5],&v[0],&v[1]);
		sin.sa.sa_data[2] = v[2];
		sin.sa.sa_data[3] = v[3];
		sin.sa.sa_data[4] = v[4];
		sin.sa.sa_data[5] = v[5];
		sin.sa.sa_data[0] = v[0];
		sin.sa.sa_data[1] = v[1];
	}
	else {
		if(getsockname(nControl->handle, &sin.sa, &l) < 0) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client openPort: getsockname");
			#endif
			return -1;
		}
	}
	int sData = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sData == -1) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client openPort: socket");
		#endif
		return -1;
	}
	if (nControl->cmode == FTP_CLIENT_PASSIVE) {
		if (connect(sData, &sin.sa, sizeof(sin.sa)) == -1) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client openPort: connect");
			#endif
			closesocket(sData);
			return -1;
		}
	}
	else {
		sin.in.sin_port = 0;
		if (bind(sData, &sin.sa, sizeof(sin)) == -1) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client openPort: bind");
			#endif
			closesocket(sData);
			return -1;
		}
		if (listen(sData, 1) < 0) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client openPort: listen");
			#endif
			closesocket(sData);
			return -1;
		}
		if (getsockname(sData, &sin.sa, &l) < 0)
			return -1;
		char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
		sprintf(buf, "PORT %d,%d,%d,%d,%d,%d",
			(unsigned char) sin.sa.sa_data[2],
			(unsigned char) sin.sa.sa_data[3],
			(unsigned char) sin.sa.sa_data[4],
			(unsigned char) sin.sa.sa_data[5],
			(unsigned char) sin.sa.sa_data[0],
			(unsigned char) sin.sa.sa_data[1]);
		if (!sendCommand(buf, '2', nControl)) {
			closesocket(sData);
			return -1;
		}
	}
	NetBuf_t* ctrl = calloc(1, sizeof(NetBuf_t));
	if (ctrl == NULL) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client openPort: calloc ctrl");
		#endif
		closesocket(sData);
		return -1;
	}
	if ((mode == 'A') && ((ctrl->buf = malloc(FTP_CLIENT_BUFFER_SIZE)) == NULL)) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client openPort: calloc ctrl->buf");
		#endif
		closesocket(sData);
		free(ctrl);
		return -1;
	}
	ctrl->handle = sData;
	ctrl->dir = dir;
	ctrl->idletime = nControl->idletime;
	ctrl->idlearg = nControl->idlearg;
	ctrl->xfered = 0;
	ctrl->xfered1 = 0;
	ctrl->cbbytes = nControl->cbbytes;
	ctrl->ctrl = nControl;
	if (ctrl->idletime.tv_sec || ctrl->idletime.tv_usec || ctrl->cbbytes)
		ctrl->idlecb = nControl->idlecb;
	else
		ctrl->idlecb = NULL;
	nControl->data = ctrl;
	*nData = ctrl;
	return 1;
}



/*
 * write lines of text
 *
 * return -1 on error or bytecount
 */
static int writeLine(const char* buf, int len, NetBuf_t* nData)
{
	if (nData->dir != FTP_CLIENT_WRITE)
		return -1;
	char* nbp = nData->buf;
	int nb = 0;
	int w = 0;
	const char* ubp = buf;
	char lc = 0;
	int x = 0;

	for (x = 0; x < len; x++) {
		if ((*ubp == '\n') && (lc != '\r')) {
			if (nb == FTP_CLIENT_BUFFER_SIZE) {
				if (!socketWait(nData))
					return x;
				w = send(nData->handle, nbp, FTP_CLIENT_BUFFER_SIZE, 0);
				if (w != FTP_CLIENT_BUFFER_SIZE) {
					#if FTP_CLIENT_DEBUG
					printf("Ftp client write line: net_write(1) returned %d, errno = %d\n",
							w, errno);
					#endif
					return(-1);
				}
				nb = 0;
			}
			nbp[nb++] = '\r';
		}
		if (nb == FTP_CLIENT_BUFFER_SIZE) {
			if (!socketWait(nData))
				return x;
			w = send(nData->handle, nbp, FTP_CLIENT_BUFFER_SIZE, 0);
			if (w != FTP_CLIENT_BUFFER_SIZE) {
				#if FTP_CLIENT_DEBUG
				printf("Ftp client write line: net_write(2) returned %d, errno = %d\n",
						w, errno);
				#endif
				return(-1);
			}
			nb = 0;
		}
		nbp[nb++] = lc = *ubp++;
	}
	if (nb){
		if (!socketWait(nData))
			return x;
		w = send(nData->handle, nbp, nb, 0);
		if (w != nb) {
			#if FTP_CLIENT_DEBUG
			printf("Ftp client write line: net_write(3) returned %d, errno = %d\n",
					w, errno);
			#endif
			return(-1);
		}
	}
	return len;
}



/*
 * acceptConnection - accept connection from server
 *
 * return 1 if successful, 0 otherwise
 */
static int acceptConnection(NetBuf_t* nData, NetBuf_t* nControl)
{
	int rv = 0;
	fd_set mask;
	FD_ZERO(&mask);
	FD_SET(nControl->handle, &mask);
	FD_SET(nData->handle, &mask);
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = FTP_CLIENT_ACCEPT_TIMEOUT ;
	int i = nControl->handle;
	if (i < nData->handle)
		i = nData->handle;
	i = select(i+1, &mask, NULL, NULL, &tv);
	if (i == -1) {
		strncpy(nControl->response, strerror(errno),
				sizeof(nControl->response));
		closesocket(nData->handle);
		nData->handle = 0;
		rv = 0;
	}
	else if (i == 0) {
		strcpy(nControl->response, "FTP Client accept connection "
				"timed out waiting for connection");
		closesocket(nData->handle);
		nData->handle = 0;
		rv = 0;
	}
	else {
		if (FD_ISSET(nData->handle, &mask)) {
			struct sockaddr addr;
			unsigned int l = sizeof(addr);
			int sData = accept(nData->handle, &addr, &l);
			i = errno;
			closesocket(nData->handle);
			if (sData > 0) {
				rv = 1;
				nData->handle = sData;
			}
			else {
				strncpy(nControl->response, strerror(i),
								sizeof(nControl->response));
				nData->handle = 0;
				rv = 0;
			}
		}
		else if (FD_ISSET(nControl->handle, &mask)) {
			closesocket(nData->handle);
			nData->handle = 0;
			readResponse('2', nControl);
			rv = 0;
		}
	}
	return rv;
}



/*
 * siteFtpClient - send a SITE command
 *
 * return 1 if command successful, 0 otherwise
 */
static int siteFtpClient(const char* cmd, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(cmd) + 7) > sizeof(buf))
		return 0;
	sprintf(buf, "SITE %s", cmd);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * getLastResponseFtpClient - return a pointer to the last response received
 */
static char* getLastResponseFtpClient(NetBuf_t* nControl)
{
	if ((nControl) && (nControl->dir == FTP_CLIENT_CONTROL))
		return nControl->response;
	else
		return NULL;
}



/*
 * getSysTypeFtpClient - send a SYST command
 *
 * Fills in the user buffer with the remote system type.  If more
 * information from the response is required, the user can parse
 * it out of the response buffer returned by FtpLastResponse().
 *
 * return 1 if command successful, 0 otherwise
 */
static int getSysTypeFtpClient(char* buf, int max, NetBuf_t* nControl)
{
	if (!sendCommand("SYST", '2', nControl))
		return 0;
	char* s = &nControl->response[4];
	int l = max;
	char* b = buf;
	while ((--l) && (*s != ' '))
		*b++ = *s++;
	*b++ = '\0';
	return 1;
}



/*
 * getFileSizeFtpClient - determine the size of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
static int getFileSizeFtpClient(const char* path,
		unsigned int* size, char mode, NetBuf_t* nControl)
{
	char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 7) > sizeof(cmd))
		return 0;
	sprintf(cmd, "TYPE %c", mode);
	if (!sendCommand(cmd, '2', nControl))
		return 0;
	int rv = 1;
	sprintf(cmd,"SIZE %s", path);
	if(!sendCommand(cmd, '2', nControl))
		rv = 0;
	else {
		int resp;
		unsigned int sz;
		if (sscanf(nControl->response, "%d %u", &resp, &sz) == 2)
			*size = sz;
		else
			rv = 0;
	}
	return rv;
}



/*
 * getModDateFtpClient - determine the modification date of a remote file
 *
 * return 1 if successful, 0 otherwise
 */
static int getModDateFtpClient(const char* path, char* dt,
		int max, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 7) > sizeof(buf))
		return 0;
	sprintf(buf, "MDTM %s", path);
	int rv = 1;
	if (!sendCommand(buf, '2', nControl))
		rv = 0;
	else
		strncpy(dt, &nControl->response[4], max);
	return rv;
}



static int setCallbackFtpClient(const FtpClientCallbackOptions_t* opt, NetBuf_t* nControl)
{
   nControl->idlecb = opt->cbFunc;
   nControl->idlearg = opt->cbArg;
   nControl->idletime.tv_sec = opt->idleTime / 1000;
   nControl->idletime.tv_usec = (opt->idleTime % 1000) * 1000;
   nControl->cbbytes = opt->bytesXferred;
   return 1;
}



static int clearCallbackFtpClient(NetBuf_t* nControl)
{
   nControl->idlecb = NULL;
   nControl->idlearg = NULL;
   nControl->idletime.tv_sec = 0;
   nControl->idletime.tv_usec = 0;
   nControl->cbbytes = 0;
   return 1;
}



/*
 * connect - connect to remote server
 *
 * return 1 if connected, 0 if not
 */
static int connectFtpClient(const char* host, uint16_t port, NetBuf_t** nControl)
{
	ESP_LOGD(TAG, "connectFtpClient");
	struct sockaddr_in sin;
	memset(&sin,0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(host);
	ESP_LOGD(TAG, "sin.sin_addr.s_addr=%x", sin.sin_addr.s_addr);
	if (sin.sin_addr.s_addr == 0xffffffff) {
		struct hostent *hp;
		hp = gethostbyname(host);
		if (hp == NULL) {
			#if FTP_CLIENT_DEBUG
			perror("FTP Client Error: Connect, gethostbyname");
			#endif
			return 0;
		}
		struct ip4_addr *ip4_addr;
		ip4_addr = (struct ip4_addr *)hp->h_addr;
		sin.sin_addr.s_addr = ip4_addr->addr;
	}

	int sControl = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	ESP_LOGD(TAG, "sControl=%d", sControl);
	if (sControl == -1) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client Error: Connect, socket");
		#endif
		return 0;
	}
	if(connect(sControl, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client Error: Connect, connect");
		#endif
		closesocket(sControl);
		return 0;
	}
	NetBuf_t* ctrl = calloc(1, sizeof(NetBuf_t));
	if (ctrl == NULL) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client Error: Connect, calloc ctrl");
		#endif
		closesocket(sControl);
		return 0;
	}
	ctrl->buf = malloc(FTP_CLIENT_BUFFER_SIZE);
	if (ctrl->buf == NULL) {
		#if FTP_CLIENT_DEBUG
		perror("FTP Client Error: Connect, calloc ctrl->buf");
		#endif
		closesocket(sControl);
		free(ctrl);
		return 0;
	}
	ctrl->handle = sControl;
	ctrl->dir = FTP_CLIENT_CONTROL;
	ctrl->ctrl = NULL;
	ctrl->data = NULL;
	ctrl->cmode = FTP_CLIENT_DEFAULT_MODE;
	ctrl->idlecb = NULL;
	ctrl->idletime.tv_sec = ctrl->idletime.tv_usec = 0;
	ctrl->idlearg = NULL;
	ctrl->xfered = 0;
	ctrl->xfered1 = 0;
	ctrl->cbbytes = 0;
	if (readResponse('2', ctrl) == 0) {
		closesocket(sControl);
		free(ctrl->buf);
		free(ctrl);
		return 0;
	}
	*nControl = ctrl;
	return 1;
}

/*
 * login - log in to remote server
 *
 * return 1 if logged in, 0 otherwise
 */
static int loginFtpClient(const char* user, const char* pass, NetBuf_t* nControl)
{
	char tempbuf[64];
	if (((strlen(user) + 7) > sizeof(tempbuf)) ||
			((strlen(pass) + 7) > sizeof(tempbuf)))
		return 0;
	sprintf(tempbuf,"USER %s",user);
	if (!sendCommand(tempbuf, '3', nControl)) {
		if (nControl->response[0] == '2')
			return 1;
		return 0;
	}
	sprintf(tempbuf, "PASS %s", pass);
	return sendCommand(tempbuf, '2', nControl);
}



/*
 * quitFtpClient - disconnect from remote
 *
 * return 1 if successful, 0 otherwise
 */
static void quitFtpClient(NetBuf_t* nControl)
{
	if (nControl->dir != FTP_CLIENT_CONTROL)
		return;
	sendCommand("QUIT", '2', nControl);
	closesocket(nControl->handle);
	free(nControl->buf);
	free(nControl);
}



/*
 * setOptionsFtpClient - change connection options
 *
 * returns 1 if successful, 0 on error
 */
static int setOptionsFtpClient(int opt, long val, NetBuf_t* nControl)
{
	int v,rv = 0;
	switch (opt)
	{
		case FTP_CLIENT_CONNMODE:
		{
			v = (int) val;
			if ((v == FTP_CLIENT_PASSIVE)
					|| (v == FTP_CLIENT_ACTIVE)) {
				nControl->cmode = v;
				rv = 1;
			}
		}
		break;

		case FTP_CLIENT_CALLBACK:
		{
			nControl->idlecb = (FtpClientCallback_t) val;
			rv = 1;
		}
		break;

		case FTP_CLIENT_IDLETIME:
		{
			v = (int) val;
			rv = 1;
			nControl->idletime.tv_sec = v / 1000;
			nControl->idletime.tv_usec = (v % 1000) * 1000;
		}
		break;

		case FTP_CLIENT_CALLBACKARG:
		{
			rv = 1;
			nControl->idlearg = (void *) val;
		}
		break;

		case FTP_CLIENT_CALLBACKBYTES:
		{
			rv = 1;
			nControl->cbbytes = (int) val;
		}
		break;
	}
	return rv;
}



/*
 * changeDirFtpClient - change path at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int changeDirFtpClient(const char* path, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "CWD %s", path);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * makeDirFtpClient - create a directory at server
 *
 * return 1 if successful, 0 otherwise
 */
static int makeDirFtpClient(const char* path, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "MKD %s", path);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * removeDirFtpClient - remove directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int removeDirFtpClient(const char* path, NetBuf_t* nControl)
{
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(path) + 6) > sizeof(buf))
		return 0;
	sprintf(buf, "RMD %s", path);
	if (!sendCommand(buf,'2',nControl))
		return 0;
	else
		return 1;
}



/*
 * dirFtpClient - issue a LIST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
static int dirFtpClient(const char* outputfile, const char* path, NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTP_CLIENT_DIR_VERBOSE, FTP_CLIENT_ASCII);
}



/*
 * nlstFtpClient - issue an NLST command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
static int nlstFtpClient(const char* outputfile, const char* path,
	NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTP_CLIENT_DIR, FTP_CLIENT_ASCII);
}



/*
 * nlstFtpClient - issue an MLSD command and write response to output
 *
 * return 1 if successful, 0 otherwise
 */
static int mlsdFtpClient(const char* outputfile, const char* path,
	NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTP_CLIENT_MLSD, FTP_CLIENT_ASCII);
}



/*
 * changeDirUpFtpClient - move to parent directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int changeDirUpFtpClient(NetBuf_t* nControl)
{
	if (!sendCommand("CDUP", '2', nControl))
		return 0;
	else
		return 1;
}


/*
 * pwdFtpClient - get working directory at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int pwdFtpClient(char* path, int max, NetBuf_t* nControl)
{
	if (!sendCommand("PWD",'2',nControl))
		return 0;
	char* s = strchr(nControl->response, '"');
	if (s == NULL)
		return 0;
	s++;
	int l = max;
	char* b = path;
	while ((--l) && (*s) && (*s != '"'))
		*b++ = *s++;
	*b++ = '\0';
	return 1;
}



/*
 * getDataFtpClient - issue a GET command and write received data to output
 *
 * return 1 if successful, 0 otherwise
 */
static int getDataFtpClient(const char* outputfile, const char* path,
		char mode, NetBuf_t* nControl)
{
	return xfer(outputfile, path, nControl, FTP_CLIENT_FILE_READ, mode);
}



/*
 * putDataFtpClient - issue a PUT command and send data from input
 *
 * return 1 if successful, 0 otherwise
 */
static int putDataFtpClient(const char* inputfile, const char* path, char mode,
	NetBuf_t* nControl)
{
	return xfer(inputfile, path, nControl, FTP_CLIENT_FILE_WRITE, mode);
}



/*
 * deleteFtpClient - delete a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int deleteDataFtpClient(const char* fnm, NetBuf_t* nControl)
{
	char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if ((strlen(fnm) + 7) > sizeof(cmd))
		return 0;
	sprintf(cmd, "DELE %s", fnm);
	if(!sendCommand(cmd, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * renameFtpClient - rename a file at remote
 *
 * return 1 if successful, 0 otherwise
 */
static int renameFtpClient(const char* src, const char* dst, NetBuf_t* nControl)
{
	char cmd[FTP_CLIENT_TEMP_BUFFER_SIZE];
	if (((strlen(src) + 7) > sizeof(cmd)) ||
		((strlen(dst) + 7) > sizeof(cmd)))
		return 0;
	sprintf(cmd, "RNFR %s", src);
	if (!sendCommand(cmd, '3', nControl))
		return 0;
	sprintf(cmd,"RNTO %s",dst);
	if (!sendCommand(cmd, '2', nControl))
		return 0;
	else
		return 1;
}



/*
 * accessFtpClient - return a handle for a data stream
 *
 * return 1 if successful, 0 otherwise
 */
static int accessFtpClient(const char* path, int typ, int mode, NetBuf_t* nControl,
	NetBuf_t** nData)
{
	if ((path == NULL) &&
		((typ == FTP_CLIENT_FILE_WRITE) || (typ == FTP_CLIENT_FILE_READ))) {
		sprintf(nControl->response,
					"Missing path argument for file transfer\n");
		return 0;
	}
	char buf[FTP_CLIENT_TEMP_BUFFER_SIZE];
	sprintf(buf, "TYPE %c", mode);
	if (!sendCommand(buf, '2', nControl))
		return 0;
	int dir;
	switch (typ) {
		case FTP_CLIENT_DIR:
		{
			strcpy(buf, "NLST");
			dir = FTP_CLIENT_READ;
		}
		break;

		case FTP_CLIENT_DIR_VERBOSE:
		{
			strcpy(buf, "LIST");
			dir = FTP_CLIENT_READ;
		}
		break;

		case FTP_CLIENT_FILE_READ:
		{
			strcpy(buf, "RETR");
			dir = FTP_CLIENT_READ;
		}
		break;

		case FTP_CLIENT_FILE_WRITE:
		{
			strcpy(buf, "STOR");
			dir = FTP_CLIENT_WRITE;
		}
		break;

		case FTP_CLIENT_MLSD:
		{
			strcpy(buf, "MLSD");
			dir = FTP_CLIENT_READ;
		}
		break;

		default:
		{
			sprintf(nControl->response, "Invalid open type %d\n", typ);
			return 0;
		}
	}

	if (path != NULL) {
		int i = strlen(buf);
		buf[i++] = ' ';
		if ((strlen(path) + i + 1) >= sizeof(buf))
			return 0;
		strcpy(&buf[i], path);
	}

	if (openPort(nControl, nData, mode, dir) == -1)
		return 0;
	if (!sendCommand(buf, '1', nControl)) {
		closeFtpClient(*nData);
		*nData = NULL;
		return 0;
	}
	if (nControl->cmode == FTP_CLIENT_ACTIVE) {
		if (!acceptConnection(*nData,nControl)) {
			closeFtpClient(*nData);
			*nData = NULL;
			nControl->data = NULL;
			return 0;
		}
	}
	return 1;
}



/*
 * readFtpClient - read from a data connection
 */
static int readFtpClient(void* buf, int max, NetBuf_t* nData)
{
	if (nData->dir != FTP_CLIENT_READ)
		return 0;
	int i = 0;
	if (nData->buf){
		i = readLine(buf, max, nData);
	}
	else {
		i = socketWait(nData);
		if (i != 1)
			return 0;
		i = recv(nData->handle, buf, max, 0);
	}
	if (i == -1)
		return 0;
	nData->xfered += i;
	if (nData->idlecb && nData->cbbytes) {
		nData->xfered1 += i;
		if (nData->xfered1 > nData->cbbytes) {
			if (nData->idlecb(nData, nData->xfered, nData->idlearg) == 0)
				return 0;
			nData->xfered1 = 0;
		}
	}
	return i;
}



/*
 * writeFtpClient - write to a data connection
 */
static int writeFtpClient(const void* buf, int len, NetBuf_t* nData)
{
	int i = 0;
	if (nData->dir != FTP_CLIENT_WRITE)
		return 0;
	if (nData->buf)
		i = writeLine(buf, len, nData);
	else {
		socketWait(nData);
		i = send(nData->handle, buf, len, 0);
	}
	if (i == -1)
		return 0;
	nData->xfered += i;
	if (nData->idlecb && nData->cbbytes) {
		nData->xfered1 += i;
		if (nData->xfered1 > nData->cbbytes) {
			nData->idlecb(nData, nData->xfered, nData->idlearg);
			nData->xfered1 = 0;
		}
	}
	return i;
}



/*
 * closeFtpClient - close a data connection
 */
static int closeFtpClient(NetBuf_t* nData)
{
	switch (nData->dir)
	{
		case FTP_CLIENT_WRITE:
		case FTP_CLIENT_READ:
			if (nData->buf)
				free(nData->buf);
			shutdown(nData->handle, 2);
			closesocket(nData->handle);
			NetBuf_t* ctrl = nData->ctrl;
			free(nData);
			ctrl->data = NULL;
			if (ctrl && ctrl->response[0] != '4' && ctrl->response[0] != '5')
				return(readResponse('2', ctrl));
			return 1;

		case FTP_CLIENT_CONTROL:
			if (nData->data) {
				nData->ctrl = NULL;
				closeFtpClient(nData->data);
			}
			closesocket(nData->handle);
			free(nData);
			return 0;
	}
	return 1;
}



FtpClient* getFtpClient(void)
{
	if(!isInitilized) {
		ftpClient_.ftpClientSite = siteFtpClient;
		ftpClient_.ftpClientGetLastResponse = getLastResponseFtpClient;
		ftpClient_.ftpClientGetSysType = getSysTypeFtpClient;
		ftpClient_.ftpClientGetFileSize = getFileSizeFtpClient;
		ftpClient_.ftpClientGetModDate = getModDateFtpClient;
		ftpClient_.ftpClientSetCallback = setCallbackFtpClient;
		ftpClient_.ftpClientClearCallback = clearCallbackFtpClient;
		ftpClient_.ftpClientConnect = connectFtpClient;
		ftpClient_.ftpClientLogin = loginFtpClient;
		ftpClient_.ftpClientQuit = quitFtpClient;
		ftpClient_.ftpClientSetOptions = setOptionsFtpClient;
		ftpClient_.ftpClientChangeDir = changeDirFtpClient;
		ftpClient_.ftpClientMakeDir = makeDirFtpClient;
		ftpClient_.ftpClientRemoveDir = removeDirFtpClient;
		ftpClient_.ftpClientDir = dirFtpClient;
		ftpClient_.ftpClientNlst = nlstFtpClient;
		ftpClient_.ftpClientMlsd = mlsdFtpClient;
		ftpClient_.ftpClientChangeDirUp = changeDirUpFtpClient;
		ftpClient_.ftpClientPwd = pwdFtpClient;
		ftpClient_.ftpClientGet = getDataFtpClient;
		ftpClient_.ftpClientPut = putDataFtpClient;
		ftpClient_.ftpClientDelete = deleteDataFtpClient;
		ftpClient_.ftpClientRename = renameFtpClient;
		ftpClient_.ftpClientAccess = accessFtpClient;
		ftpClient_.ftpClientRead = readFtpClient;
		ftpClient_.ftpClientWrite = writeFtpClient;
		ftpClient_.ftpClientClose = closeFtpClient;
		isInitilized = true;
	}
	return &ftpClient_;
}
