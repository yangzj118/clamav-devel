/*
 *  Copyright (C) 2002 Nigel Horne <njh@bandsman.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Change History:
 * $Log: mbox.c,v $
 * Revision 1.42  2004/02/14 17:23:45  nigelhorne
 * Had deleted O_BINARY by mistake
 *
 * Revision 1.41  2004/02/12 18:43:58  nigelhorne
 * Use mkstemp on Solaris
 *
 * Revision 1.40  2004/02/11 08:15:59  nigelhorne
 * Use O_BINARY for cygwin
 *
 * Revision 1.39  2004/02/06 13:46:08  kojm
 * Support for clamav-config.h
 *
 * Revision 1.38  2004/02/04 13:29:48  nigelhorne
 * Handle partial writes - and print when write fails
 *
 * Revision 1.37  2004/02/03 22:54:59  nigelhorne
 * Catch another example of Worm.Dumaru.Y
 *
 * Revision 1.36  2004/02/02 09:52:57  nigelhorne
 * Some instances of Worm.Dumaru.Y got through the net
 *
 * Revision 1.35  2004/01/28 10:15:24  nigelhorne
 * Added support to scan some bounce messages
 *
 * Revision 1.34  2004/01/24 17:43:37  nigelhorne
 * Removed (incorrect) warning about uninitialised variable
 *
 * Revision 1.33  2004/01/23 10:38:22  nigelhorne
 * Fixed memory leak in handling some multipart messages
 *
 * Revision 1.32  2004/01/23 08:51:19  nigelhorne
 * Add detection of uuencoded viruses in single part multipart/mixed files
 *
 * Revision 1.31  2004/01/22 22:13:06  nigelhorne
 * Prevent infinite recursion on broken uuencoded files
 *
 * Revision 1.30  2004/01/13 10:12:05  nigelhorne
 * Remove duplicate code when handling multipart messages
 *
 * Revision 1.29  2004/01/09 18:27:11  nigelhorne
 * ParseMimeHeader could corrupt arg
 *
 * Revision 1.28  2004/01/09 15:07:42  nigelhorne
 * Re-engineered update 1.11 lost in recent changes
 *
 * Revision 1.27  2004/01/09 14:45:59  nigelhorne
 * Removed duplicated code in multipart handler
 *
 * Revision 1.26  2004/01/09 10:20:54  nigelhorne
 * Locate uuencoded viruses hidden in text poritions of multipart/mixed mime messages
 *
 * Revision 1.25  2004/01/06 14:41:18  nigelhorne
 * Handle headers which do not not have a space after the ':'
 *
 * Revision 1.24  2003/12/20 13:55:36  nigelhorne
 * Ensure multipart just save the bodies of attachments
 *
 * Revision 1.23  2003/12/14 18:07:01  nigelhorne
 * Some viruses in embedded messages were not being found
 *
 * Revision 1.22  2003/12/13 16:42:23  nigelhorne
 * call new cli_chomp
 *
 * Revision 1.21  2003/12/11 14:35:48  nigelhorne
 * Better handling of encapsulated messages
 *
 * Revision 1.20  2003/12/06 04:03:26  nigelhorne
 * Handle hand crafted emails that incorrectly set multipart headers
 *
 * Revision 1.19  2003/11/21 07:26:31  nigelhorne
 * Scan multipart alternatives that have no boundaries, finds some uuencoded happy99
 *
 * Revision 1.18  2003/11/17 08:13:21  nigelhorne
 * Handle spaces at the end of lines of MIME headers
 *
 * Revision 1.17  2003/11/06 05:06:42  nigelhorne
 * Some applications weren't being scanned
 *
 * Revision 1.16  2003/11/04 08:24:00  nigelhorne
 * Handle multipart messages that have no text portion
 *
 * Revision 1.15  2003/10/12 20:13:49  nigelhorne
 * Use NO_STRTOK_R consistent with message.c
 *
 * Revision 1.14  2003/10/12 12:37:11  nigelhorne
 * Appledouble encoded EICAR now found
 *
 * Revision 1.13  2003/10/01 09:27:42  nigelhorne
 * Handle content-type header going over to a new line
 *
 * Revision 1.12  2003/09/29 17:10:19  nigelhorne
 * Moved stub from heap to stack since its maximum size is known
 *
 * Revision 1.11  2003/09/29 12:58:32  nigelhorne
 * Handle Content-Type: /; name="eicar.com"
 *
 * Revision 1.10  2003/09/28 10:06:34  nigelhorne
 * Compilable under SCO; removed duplicate code with message.c
 *
 */
static	char	const	rcsid[] = "$Id: mbox.c,v 1.42 2004/02/14 17:23:45 nigelhorne Exp $";

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#ifndef	CL_DEBUG
/*#define	NDEBUG	/* map CLAMAV debug onto standard */
#endif

#ifdef CL_THREAD_SAFE
#ifndef	_REENTRANT
#define	_REENTRANT	/* for Solaris 2.8 */
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <clamav.h>

#include "table.h"
#include "mbox.h"
#include "blob.h"
#include "text.h"
#include "message.h"
#include "others.h"
#include "defaults.h"
#include "str.h"

#if	defined(NO_STRTOK_R) || !defined(CL_THREAD_SAFE)
#undef strtok_r
#undef __strtok_r
#define strtok_r(a,b,c)	strtok(a,b)
#endif

/* required for AIX and Tru64 */
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

typedef enum    { FALSE = 0, TRUE = 1 } bool;

static	message	*parseEmailHeaders(const message *m, const table_t *rfc821Table);
static	int	parseEmailHeader(message *m, const char *line, const table_t *rfc821Table);
static	int	parseEmailBody(message *messageIn, blob **blobsIn, int nBlobs, text *textIn, const char *dir, table_t *rfc821Table, table_t *subtypeTable);
static	int	boundaryStart(const char *line, const char *boundary);
static	int	endOfMessage(const char *line, const char *boundary);
static	int	initialiseTables(table_t **rfc821Table, table_t **subtypeTable);
static	int	getTextPart(message *const messages[], size_t size);
static	size_t	strip(char *buf, int len);
static	size_t	strstrip(char *s);
static	bool	continuationMarker(const char *line);
static	int	parseMimeHeader(message *m, const char *cmd, const table_t *rfc821Table, const char *arg);
static	void	saveTextPart(message *m, const char *dir);
static	bool	saveFile(const blob *b, const char *dir);

/* Maximum number of attachments that we accept */
#define	MAX_ATTACHMENTS	10

/* Maximum line length according to RFC821 */
#define	LINE_LENGTH	1000

/* Hashcodes for our hash tables */
#define	CONTENT_TYPE			1
#define	CONTENT_TRANSFER_ENCODING	2
#define	CONTENT_DISPOSITION		3

/* Mime sub types */
#define	PLAIN		1
#define	ENRICHED	2
#define	HTML		3
#define	RICHTEXT	4
#define	MIXED		5
#define	ALTERNATIVE	6
#define	DIGEST		7
#define	SIGNED		8
#define	PARALLEL	9
#define	RELATED		10	/* RFC2387 */
#define	REPORT		11	/* RFC1892 */
#define	APPLEDOUBLE	12	/* Handling of this in only noddy for now */

static	const	struct tableinit {
	const	char	*key;
	int	value;
} rfc821headers[] = {
	/* TODO: make these regular expressions */
	{	"Content-Type:",		CONTENT_TYPE		},
	{	"Content-Transfer-Encoding:",	CONTENT_TRANSFER_ENCODING	},
	{	"Content-Disposition:",		CONTENT_DISPOSITION	},
	{	NULL,				0			}
}, mimeSubtypes[] = {
		/* subtypes of Text */
	{	"plain",	PLAIN		},
	{	"enriched",	ENRICHED	},
	{	"html",		HTML		},
	{	"richtext",	RICHTEXT	},
		/* subtypes of Multipart */
	{	"mixed",	MIXED		},
	{	"alternative",	ALTERNATIVE	},
	{	"digest",	DIGEST		},
	{	"signed",	SIGNED		},
	{	"parallel",	PARALLEL	},
	{	"related",	RELATED		},
	{	"report",	REPORT		},
	{	"appledouble",	APPLEDOUBLE	},
	{	NULL,		0		}
};

/* Maximum filenames under various systems */
#ifndef	NAME_MAX	/* e.g. Linux */

#ifdef	MAXNAMELEN	/* e.g. Solaris */
#define	NAME_MAX	MAXNAMELEN
#else

#ifdef	FILENAME_MAX	/* e.g. SCO */
#define	NAME_MAX	FILENAME_MAX
#endif

#endif

#endif

#ifndef	O_BINARY
#define	O_BINARY	0
#endif

/*
 * TODO: when signal handling is added, need to remove temp files when a
 * signal is received
 * TODO: add option to scan in memory not via temp files, perhaps with a
 * named pipe or memory mapped file?
 * TODO: if debug is enabled, catch a segfault and dump the current e-mail
 * in it's entirety, then call abort()
 * TODO: parse .msg format files
 * TODO: fully handle AppleDouble format, see
 * http://www.lazerware.com/formats/Specs/AppleSingle_AppleDouble.pdf
 * TODO: ensure parseEmailHeaders is always called before parseEmailBody
 * TODO: create parseEmail which calls parseEmailHeaders then parseEmailBody
 */
int
cl_mbox(const char *dir, int desc)
{
	int retcode, i;
	message *m, *body;
	table_t	*rfc821Table, *subtypeTable;
	FILE *fd;
	char buffer[LINE_LENGTH];

	cli_dbgmsg("in mbox()\n");

	i = dup(desc);
	if((fd = fdopen(i, "rb")) == NULL) {
		cli_errmsg("Can't open descriptor %d\n", desc);
		close(i);
		return -1;
	}
	if(fgets(buffer, sizeof(buffer), fd) == NULL) {
		/* empty message */
		fclose(fd);
		return 0;
	}
	m = messageCreate();
	assert(m != NULL);

	if(initialiseTables(&rfc821Table, &subtypeTable) < 0) {
		messageDestroy(m);
		fclose(fd);
		return -1;
	}

	/*
	 * is it a UNIX style mbox with more than one
	 * mail message, or just a single mail message?
	 */
	if(strncmp(buffer, "From ", 5) == 0) {
		/*
		 * Have been asked to check a UNIX style mbox file, which
		 * may contain more than one e-mail message to decode
		 */
		bool lastLineWasEmpty = FALSE;

		do {
			/*cli_dbgmsg("read: %s", buffer);*/

			cli_chomp(buffer);
			if(lastLineWasEmpty && (strncmp(buffer, "From ", 5) == 0)) {
				/*
				 * End of a message in the mail box
				 */
				body = parseEmailHeaders(m, rfc821Table);
				messageDestroy(m);
				messageClean(body);
				if(messageGetBody(body))
					if(!parseEmailBody(body,  NULL, 0, NULL, dir, rfc821Table, subtypeTable))
						break;
				/*
				 * Starting a new message, throw away all the
				 * information about the old one
				 */
				m = body;
				messageReset(body);

				lastLineWasEmpty = TRUE;
				cli_dbgmsg("Finished processing message\n");
			} else
				lastLineWasEmpty = (bool)(buffer[0] == '\0');
			messageAddLine(m, buffer);
		} while(fgets(buffer, sizeof(buffer), fd) != NULL);
	} else
		/*
		 * It's a single message, parse the headers then the body
		 */
		do {
			cli_chomp(buffer);
			messageAddLine(m, buffer);
		} while(fgets(buffer, sizeof(buffer), fd) != NULL);

	fclose(fd);

	retcode = 0;

	body = parseEmailHeaders(m, rfc821Table);
	messageDestroy(m);
	/*
	 * Write out the last entry in the mailbox
	 */
	messageClean(body);
	if(messageGetBody(body))
		if(!parseEmailBody(body, NULL, 0, NULL, dir, rfc821Table, subtypeTable))
			retcode = -1;

	/*
	 * Tidy up and quit
	 */
	messageDestroy(body);

	tableDestroy(rfc821Table);
	tableDestroy(subtypeTable);

	cli_dbgmsg("cli_mbox returning %d\n", retcode);

	return retcode;
}

/*
 * The given message contains a raw e-mail.
 *
 * This function parses the headers of m and sets the message's arguments
 *
 * Returns the message's body with the correct arguments set
 */
static message *
parseEmailHeaders(const message *m, const table_t *rfc821Table)
{
	bool inContinuationHeader = FALSE;
	bool inHeader = TRUE;
	text *t, *msgText;
	message *ret;

	if(m == NULL)
		return NULL;

	msgText = messageToText(m);
	if(msgText == NULL)
		return NULL;

	t = msgText;
	ret = messageCreate();

	do {
		char *buffer = strdup(t->t_text);
#ifdef CL_THREAD_SAFE
		char *strptr;
#endif

		cli_chomp(buffer);

		/*
		 * Section B.2 of RFC822 says TAB or SPACE means
		 * a continuation of the previous entry.
		 */
		if(inHeader && ((buffer[0] == '\t') || (buffer[0] == ' ')))
			inContinuationHeader = TRUE;

		if(inContinuationHeader) {
			const char *ptr;

			if(!continuationMarker(buffer))
				inContinuationHeader = FALSE;	 /* no more args */

			/*
			 * Add all the arguments on the line
			 */
			for(ptr = strtok_r(buffer, ";", &strptr); ptr; ptr = strtok_r(NULL, ":", &strptr))
				messageAddArgument(ret, ptr);
		} else if(inHeader) {
			cli_dbgmsg("Deal with header %s\n", buffer);

			/*
			 * A blank line signifies the end of the header and
			 * the start of the text
			 */
			if(strstrip(buffer) == 0) {
				cli_dbgmsg("End of header information\n");
				inHeader = FALSE;
			} else if(parseEmailHeader(ret, buffer, rfc821Table) == CONTENT_TYPE)
				inContinuationHeader = continuationMarker(buffer);

		} else
			messageAddLine(ret, buffer);
		free(buffer);
	} while((t = t->t_next) != NULL);

	textDestroy(msgText);

	return ret;
}

/*
 * Handle a header line of an email message
 * TODO: handle spaces before the ':'
 */
static int
parseEmailHeader(message *m, const char *line, const table_t *rfc821Table)
{
	char *copy = strdup(line);
	char *cmd;
	int ret = -1;
#ifdef CL_THREAD_SAFE
	char *strptr;
#endif

	cmd = strtok_r(copy, " \t", &strptr);

	if(*cmd) {
		char *arg = strtok_r(NULL, "", &strptr);

		if(arg)
			/*
			 * Found a header such as
			 * Content-Type: multipart/mixed;
			 * set arg to be
			 * "multipart/mixed" and cmd to
			 * be "Content-Type:"
			 */
			ret = parseMimeHeader(m, cmd, rfc821Table, arg);
		else {
			/*
			 * Handle the case where the
			 * header does not have a space
			 * after the ':', e.g.
			 * Content-Type:multipart/mixed;
			 */
			arg = strchr(cmd, ':');
			if(arg && (*++arg != '\0')) {
				char *p;

				cmd = strdup(cmd);
				p = strchr(cmd, ':');
				*++p = '\0';
				ret = parseMimeHeader(m, cmd, rfc821Table, arg);
				free(cmd);
			}
		}
	}
	free(copy);

	return ret;
}

/*
 * This is a recursive routine.
 *
 * This function parses the body of mainMessage and saves its attachments in dir
 *
 * mainMessage is the buffer to be parsed, it contains an e-mail's body, without
 * any headers. First
 * time of calling it'll be
 *	the whole message. Later it'll be parts of a multipart message
 * textIn is the plain text message being built up so far
 * blobsIn contains the array of attachments found so far
 *
 * Returns:
 *	0 for fail
 *	1 for success, attachments saved
 *	2 for success, attachments not saved
 */
static int	/* success or fail */
parseEmailBody(message *messageIn, blob **blobsIn, int nBlobs, text *textIn, const char *dir, table_t *rfc821Table, table_t *subtypeTable)
{
	message *messages[MAXALTERNATIVE];
	int inhead, inMimeHead, i, rc = 1, htmltextPart, multiparts = 0;
	text *aText;
	blob *blobList[MAX_ATTACHMENTS], **blobs;
	const char *cptr;
	message *mainMessage;

	cli_dbgmsg("in parseEmailBody(nBlobs = %d)\n", nBlobs);

	/* Pre-assertions */
	if(nBlobs >= MAX_ATTACHMENTS) {
		cli_warnmsg("Not all attachments will be scanned\n");
		return 2;
	}

	aText = textIn;
	blobs = blobsIn;
	mainMessage = messageIn;

	/* Anything left to be parsed? */
	if(mainMessage && (messageGetBody(mainMessage) != NULL)) {
		int numberOfAttachments = 0;
		mime_type mimeType;
		const char *mimeSubtype;
		const text *t_line;
		/*bool isAlternative;*/
		const char *boundary;
		message *aMessage;

		cli_dbgmsg("Parsing mail file\n");

		mimeType = messageGetMimeType(mainMessage);
		mimeSubtype = messageGetMimeSubtype(mainMessage);

		if((mimeType == TEXT) && (tableFind(subtypeTable, mimeSubtype) == PLAIN)) {
			/*
			 * This is effectively no encoding, notice that we
			 * don't check that charset is us-ascii
			 */
			cli_dbgmsg("assume no encoding\n");
			mimeType = NOMIME;
		}

		cli_dbgmsg("mimeType = %d\n", mimeType);

		switch(mimeType) {
		case NOMIME:
			aText = textAddMessage(aText, mainMessage);
			break;
		case TEXT:
			if(tableFind(subtypeTable, mimeSubtype) == PLAIN)
				aText = textCopy(messageGetBody(mainMessage));
			break;
		case MULTIPART:

			assert(mimeSubtype[0] != '\0');

			boundary = messageFindArgument(mainMessage, "boundary");

			if(boundary == NULL) {
				cli_warnmsg("Multipart MIME message contains no boundaries\n");
				/* Broken e-mail message */
				mimeType = NOMIME;
				/*
				 * The break means that we will still
				 * check if the file contains a uuencoded file
				 */
				break;
			}

			/*
			 * Get to the start of the first message
			 */
			for(t_line = messageGetBody(mainMessage); t_line; t_line = t_line->t_next)
				if(boundaryStart(t_line->t_text, boundary))
					break;

			if(t_line == NULL) {
				cli_warnmsg("Multipart MIME message contains no parts\n");
				/*
				 * Free added by Thomas Lamy
				 * <Thomas.Lamy@in-online.net>
				 */
				free((char *)boundary);
				mimeType = NOMIME;
				/*
				 * The break means that we will still
				 * check if the file contains a uuencoded file
				 */
				break;
			}
			/*
			 * Build up a table of all of the parts of this
			 * multipart message. Remember, each part may itself
			 * be a multipart message.
			 */
			inhead = 1;
			inMimeHead = 0;

			/*
			 * This looks like parseEmailHeaders() - maybe there's
			 * some duplication of code to be cleaned up
			 */
			for(multiparts = 0; t_line && (multiparts < MAXALTERNATIVE); multiparts++) {
				aMessage = messages[multiparts] = messageCreate();

				cli_dbgmsg("Now read in part %d\n", multiparts);

				/*
				 * Ignore blank lines. There shouldn't be ANY
				 * but some viruses insert them
				 */
				while((t_line = t_line->t_next) != NULL) {
					cli_chomp(t_line->t_text);
					if(strlen(t_line->t_text) != 0)
						break;
				}

				if(t_line == NULL) {
					cli_dbgmsg("Empty part\n");
					continue;
				}

				do {
					const char *line = t_line->t_text;

					/*cli_dbgmsg("inMimeHead %d inhead %d boundary %s line '%s' next '%s'\n",
						inMimeHead, inhead, boundary, line, t_line->t_next ? t_line->t_next->t_text : "(null)");*/

					if(inMimeHead) {
						cli_dbgmsg("About to add mime Argument '%s'\n",
							line);
						while(isspace((int)*line))
							line++;

						if(*line == '\0') {
							inhead = inMimeHead = 0;
							continue;
						}
						/*
						 * This may cause a trailing ';'
						 * to be added if this test
						 * fails - TODO: verify this
						 */
						inMimeHead = continuationMarker(line);
						messageAddArgument(aMessage, line);
					} else if(inhead) {
						if(strlen(line) == 0) {
							inhead = 0;
							continue;
						}
						if(isspace((int)*line)) {
							/*
							 * The first line is
							 * continuation line.
							 * This is tricky
							 * to handle, but
							 * all we can do is our
							 * best
							 */
							cli_dbgmsg("Part %d starts with a continuation line\n",
								multiparts);
							messageAddArgument(aMessage, line);
							/*
							 * Give it a default
							 * MIME type since
							 * that may be the
							 * missing line
							 *
							 * Choose application to
							 * force a save
							 */
							if(messageGetMimeType(aMessage) == NOMIME)
								messageSetMimeType(aMessage, "application");
							continue;
						}

						/*
						 * Some clients are broken and
						 * put white space after the ;
						 */
						inMimeHead = continuationMarker(line);
						if(!inMimeHead)
							if(t_line->t_next && ((t_line->t_next->t_text[0] == '\t') || (t_line->t_next->t_text[0] == ' ')))
								inMimeHead = TRUE;

						parseEmailHeader(aMessage, line, rfc821Table);
					} else if(boundaryStart(line, boundary)) {
						inhead = 1;
						break;
					} else if(endOfMessage(line, boundary)) {
						/*
						 * Some viruses put information
						 * *after* the end of message,
						 * which presumably some broken
						 * mail clients find, so we
						 * can't assume that this
						 * is the end of the message
						 */
						/* t_line = NULL;*/
						break;
					} else
						messageAddLine(aMessage, line);
				} while((t_line = t_line->t_next) != NULL);

				messageClean(aMessage);
			}

			free((char *)boundary);

			if(multiparts == 0) {
				if(mainMessage && (mainMessage != messageIn))
					messageDestroy(mainMessage);
				return 2;	/* Nothing to do */
			}

			cli_dbgmsg("The message has %d parts\n", multiparts);
			cli_dbgmsg("Find out the multipart type(%s)\n", mimeSubtype);

			switch(tableFind(subtypeTable, mimeSubtype)) {
			case RELATED:
				cli_dbgmsg("Multipart related handler\n");
				/*
				 * Have a look to see if there's HTML code
				 * which will need scanning
				 */
				aMessage = NULL;
				assert(multiparts > 0);

				htmltextPart = getTextPart(messages, multiparts);

				if(htmltextPart >= 0)
					aText = textAddMessage(aText, messages[htmltextPart]);
				else
					/*
					 * There isn't an HTML bit. If there's a
					 * multipart bit, it'll may be in there
					 * somewhere
					 */
					for(i = 0; i < multiparts; i++)
						if(messageGetMimeType(messages[i]) == MULTIPART) {
							aMessage = messages[i];
							htmltextPart = i;
							break;
						}

				if(htmltextPart == -1) {
					cli_dbgmsg("No HTML code found to be scanned");
					rc = 0;
				} else
					rc = parseEmailBody(aMessage, blobs, nBlobs, aText, dir, rfc821Table, subtypeTable);
				blobArrayDestroy(blobs, nBlobs);
				blobs = NULL;
				nBlobs = 0;

				/*
				 * Fixed based on an idea from Stephen White <stephen@earth.li>
				 * The message is confused about the difference
				 * between alternative and related. Badtrans.B
				 * suffers from this problem.
				 *
				 * Fall through in this case:
				 * Content-Type: multipart/related;
				 *	type="multipart/alternative"
				 */
				/*
				 * Changed to always fall through based on
				 * an idea from Michael Dankov <misha@btrc.ru>
				 * that some viruses are completely confused
				 * about the difference between related
				 * and mixed
				 */
				/*cptr = messageFindArgument(mainMessage, "type");
				if(cptr == NULL)
					break;
				isAlternative = (bool)(strcasecmp(cptr, "multipart/alternative") == 0);
				free((char *)cptr);
				if(!isAlternative)
					break;*/
			case ALTERNATIVE:
				cli_dbgmsg("Multipart alternative handler\n");

				htmltextPart = getTextPart(messages, multiparts);

				if(htmltextPart == -1)
					htmltextPart = 0;

				aMessage = messages[htmltextPart];
				aText = textAddMessage(aText, aMessage);

				rc = parseEmailBody(NULL, blobs, nBlobs, aText, dir, rfc821Table, subtypeTable);
				if(rc == 1) {
					/*
					 * Alternative message has saved its
					 * attachments, ensure we don't do
					 * the same thing
					 */
					blobArrayDestroy(blobs, nBlobs);
					blobs = NULL;
					nBlobs = 0;
					rc = 2;
				}
				/*
				 * Fall through - some clients are broken and
				 * say alternative instead of mixed. The Klez
				 * virus is broken that way
				 */
			case REPORT:
				/*
				 * According to section 1 of RFC1892, the
				 * syntax of multipart/report is the same
				 * as multipart/mixed. There are some required
				 * parameters, but there's no need for us to
				 * verify that they exist
				 */
			case MIXED:
			case APPLEDOUBLE:	/* not really supported */
				/*
				 * Look for attachments
				 *
				 * Not all formats are supported. If an
				 * unsupported format turns out to be
				 * common enough to implement, it is a simple
				 * matter to add it
				 */
				if(aText) {
					if(mainMessage && (mainMessage != messageIn))
						messageDestroy(mainMessage);
					mainMessage = NULL;
				}

				cli_dbgmsg("Mixed message with %d parts\n", multiparts);
				for(i = 0; i < multiparts; i++) {
					bool addAttachment = FALSE;
					bool addToText = FALSE;
					const char *dtype;
					text *t;
					message *body;

					aMessage = messages[i];

					assert(aMessage != NULL);

					dtype = messageGetDispositionType(aMessage);
					cptr = messageGetMimeSubtype(aMessage);

					cli_dbgmsg("Mixed message part %d is of type %d\n",
						i, messageGetMimeType(aMessage));

					switch(messageGetMimeType(aMessage)) {
					case APPLICATION:
#if	0
						/* strict checking... */
						if((strcasecmp(dtype, "attachment") == 0) ||
						   (strcasecmp(cptr, "x-msdownload") == 0) ||
						   (strcasecmp(cptr, "octet-stream") == 0) ||
						   (strcasecmp(dtype, "octet-stream") == 0))
							addAttachment = TRUE;
						else {
							cli_dbgmsg("Discarded mixed/application not sent as attachment\n");
							continue;
						}
#endif
						addAttachment = TRUE;

						break;
					case NOMIME:
						if(mainMessage && (mainMessage != messageIn))
							messageDestroy(mainMessage);
						mainMessage = NULL;
						addToText = TRUE;
						if(messageGetBody(aMessage) == NULL)
							/*
							 * No plain text version
							 */
							messageAddLine(aMessage, "No plain text alternative");
						assert(messageGetBody(aMessage) != NULL);
						break;
					case TEXT:
						cli_dbgmsg("Mixed message text part disposition \"%s\"\n",
							dtype);
						if(strcasecmp(dtype, "attachment") == 0)
							addAttachment = TRUE;
						else if((*dtype == '\0') || (strcasecmp(dtype, "inline") == 0)) {
							const text *t_line = uuencodeBegin(aMessage);

							if(mainMessage && (mainMessage != messageIn))
								messageDestroy(mainMessage);
							mainMessage = NULL;
							if(t_line) {
								cli_dbgmsg("Found uuencoded message in multipart/mixed text portion\n");
								messageSetEncoding(aMessage, "x-uuencode");
								addAttachment = TRUE;
							} else if(strcasecmp(messageGetMimeSubtype(aMessage), "plain") == 0) {
								/*
								 * Strictly speaking
								 * a text/html part is
								 * not an attachment. We
								 * pretend it is so that
								 * we can decode and
								 * scan it
								 */
								cli_dbgmsg("Adding part to main message\n");
								addToText = TRUE;
							} else {
								messageAddArgument(aMessage, "filename=textportion");
								addAttachment = TRUE;
							}
						} else {
							cli_dbgmsg("Text type %s is not supported", dtype);
							continue;
						}
						break;
					case MESSAGE:
						cli_dbgmsg("Found message inside multipart\n");
						body = parseEmailHeaders(aMessage, rfc821Table);
						if(body) {
							rc = parseEmailBody(body, blobs, nBlobs, NULL, dir, rfc821Table, subtypeTable);
							messageDestroy(body);
						}
						continue;
					case MULTIPART:
						/*
						 * It's a multi part within a multi part
						 * Run the message parser on this bit, it won't
						 * be an attachment
						 *
						 */
						cli_dbgmsg("Found multipart inside multipart\n");
						if(aMessage) {
							body = parseEmailHeaders(aMessage, rfc821Table);
							if(body) {
								t = messageToText(body);

								rc = parseEmailBody(body, blobs, nBlobs, t, dir, rfc821Table, subtypeTable);
								cli_dbgmsg("Finished recursion\n");
								textDestroy(t);
								if(mainMessage && (mainMessage != messageIn))
									messageDestroy(mainMessage);

								mainMessage = body;
							}
						} else {
							rc = parseEmailBody(NULL, blobs, nBlobs, NULL, dir, rfc821Table, subtypeTable);
							if(mainMessage && (mainMessage != messageIn))
								messageDestroy(mainMessage);
							mainMessage = NULL;
						}
						continue;
					case AUDIO:
					case IMAGE:
						/*
						 * TODO: it may be nice to
						 * have an option to throw
						 * away all images and sound
						 * files for ultra-secure sites
						 */
						addAttachment = TRUE;
						break;
					default:
						cli_dbgmsg("Only text and application attachments are supported, type = %d\n",
							messageGetMimeType(aMessage));
						continue;
					}

					/*
					 * It must be either text or
					 * an attachment. It can't be both
					 */
					assert(addToText || addAttachment);
					assert(!(addToText && addAttachment));

					if(addToText)
						aText = textAdd(aText, messageGetBody(aMessage));
					else if(addAttachment) {
						blob *aBlob = messageToBlob(aMessage);

						if(aBlob) {
							assert(blobGetFilename(aBlob) != NULL);
							/*if(blobGetDataSize(aBlob) > 0)*/
								blobList[numberOfAttachments++] = aBlob;
						}
					}
				}

				if(numberOfAttachments == 0) {
					/* No usable attachment was found */
					rc = parseEmailBody(NULL, NULL, 0, aText, dir, rfc821Table, subtypeTable);
					break;
				}
				/*
				 * Store any existing attachments at the end of
				 * the list we've just built up
				 */
				for(i = 0; i < nBlobs; i++) {
#ifdef	CL_DEBUG
					assert(blobs[i]->magic == BLOB);
#endif
					blobList[numberOfAttachments++] = blobs[i];
				}

				/*
				 * If there's only one part of the MULTIPART
				 * we already have the body to decode so
				 * there's no more work to do.
				 *
				 * This is mostly for the situation where
				 * broken code which claim to be multipart
				 * which aren't was causing us to go into
				 * infinite recursion
				 */
				if(multiparts > 1)
					rc = parseEmailBody(mainMessage, blobList, numberOfAttachments, aText, dir, rfc821Table, subtypeTable);
				else if(numberOfAttachments == 1) {
					(void)saveFile(blobList[0], dir);
					blobDestroy(blobList[0]);
				}
				break;
			case DIGEST:
				/*
				 * TODO:
				 * According to section 5.1.5 RFC2046, the
				 * default mime type of multipart/digest parts
				 * is message/rfc822
				 */
			case SIGNED:
			case PARALLEL:
				/*
				 * If we're here it could be because we have a
				 * multipart/mixed message, consisting of a
				 * message followed by an attachment. That
				 * message itself is a multipart/alternative
				 * message and we need to dig out the plain
				 * text part of that alternative
				 */
				htmltextPart = getTextPart(messages, multiparts);
				if(htmltextPart == -1)
					htmltextPart = 0;

				rc = parseEmailBody(messages[htmltextPart], blobs, nBlobs, aText, dir, rfc821Table, subtypeTable);
				blobArrayDestroy(blobs, nBlobs);
				blobs = NULL;
				nBlobs = 0;
				break;
			default:
				/*
				 * According to section 7.2.6 of RFC1521,
				 * unrecognised multiparts should be treated as
				 * multipart/mixed. I don't do this yet so
				 * that I can see what comes along...
				 */
				cli_warnmsg("Unsupported multipart format `%s'\n", mimeSubtype);
				rc = 0;
			}

			for(i = 0; i < multiparts; i++)
				messageDestroy(messages[i]);

			if(blobs && (blobsIn == NULL))
				puts("arraydestroy");

			if(mainMessage && (mainMessage != messageIn))
				messageDestroy(mainMessage);

			if(aText && (textIn == NULL))
				textDestroy(aText);

			return rc;

		case MESSAGE:
			/*
			 * Check for forbidden encodings
			 */
			switch(messageGetEncoding(mainMessage)) {
				case NOENCODING:
				case EIGHTBIT:
				case BINARY:
					break;
				default:
					cli_warnmsg("MIME type 'message' cannot be decoded\n");
					break;
			}
			if((strcasecmp(mimeSubtype, "rfc822") == 0) ||
			   (strcasecmp(mimeSubtype, "delivery-status") == 0)) {
				/*
				 * Found a message encapsulated within
				 * another message
				 *
				 * Thomas Lamy <Thomas.Lamy@in-online.net>:
				 * ensure t is correctly freed
				 */
				text *t, *msgText = messageToText(mainMessage);
				message *m, *body;

				t = msgText;
				assert(t != NULL);

				m = messageCreate();
				assert(m != NULL);

				cli_dbgmsg("Decode rfc822");

				do {
					char *buffer = strdup(t->t_text);

					cli_chomp(buffer);
					messageAddLine(m, buffer);
					free(buffer);
				} while((t = t->t_next) != NULL);

				textDestroy(msgText);

				body = parseEmailHeaders(m, rfc821Table);

				messageDestroy(m);
				m = body;

				messageClean(m);

				if(messageGetBody(m))
					rc = parseEmailBody(m, NULL, 0, NULL, dir, rfc821Table, subtypeTable);

				messageDestroy(m);

				break;
			} else if(strcasecmp(mimeSubtype, "partial") == 0)
				/* TODO */
				cli_warnmsg("Content-type message/partial not yet supported");
			else if(strcasecmp(mimeSubtype, "external-body") == 0)
				/*
				 * I don't believe that we should be going
				 * around the Internet looking for referenced
				 * files...
				 */
				cli_warnmsg("Attempt to send Content-type message/external-body trapped");
			else
				cli_warnmsg("Unsupported message format `%s'\n", mimeSubtype);

			if(mainMessage && (mainMessage != messageIn))
				messageDestroy(mainMessage);
			return 0;

		case APPLICATION:
			cptr = messageGetMimeSubtype(mainMessage);

			/*if((strcasecmp(cptr, "octet-stream") == 0) ||
			   (strcasecmp(cptr, "x-msdownload") == 0)) {*/
			{
				blob *aBlob = messageToBlob(mainMessage);

				if(aBlob) {
					cli_dbgmsg("Saving main message as attachment %d\n", nBlobs);
					assert(blobGetFilename(aBlob) != NULL);
					/*
					 * It's likely that we won't have built
					 * a set of attachments
					 */
					if(blobs == NULL)
						blobs = blobList;
					for(i = 0; i < nBlobs; i++)
						if(blobs[i] == NULL)
							break;
					blobs[i] = aBlob;
					if(i == nBlobs) {
						nBlobs++;
						assert(nBlobs < MAX_ATTACHMENTS);
					}
				}
			} /*else
				cli_warnmsg("Discarded application not sent as attachment\n");*/
			break;

		case AUDIO:
		case VIDEO:
		case IMAGE:
			break;

		default:
			cli_warnmsg("Message received with unknown mime encoding");
			break;
		}
	}

	cli_dbgmsg("%d attachments found\n", nBlobs);

	if(nBlobs == 0) {
		blob *b;

		/*
		 * No attachments - scan the text portions, often files
		 * are hidden in HTML code
		 */
		cli_dbgmsg("%d multiparts found\n", multiparts);
		for(i = 0; i < multiparts; i++) {
			b = messageToBlob(messages[i]);

			assert(b != NULL);

			cli_dbgmsg("Saving multipart %d, encoded with scheme %d\n",
				i, messageGetEncoding(messages[i]));

			(void)saveFile(b, dir);

			blobDestroy(b);
		}

		if(mainMessage) {
			/*
			 * Look for uu-encoded main file
			 */
			const text *t_line;

			if((t_line = uuencodeBegin(mainMessage)) != NULL) {
				cli_dbgmsg("Found uuencoded file\n");

				/*
				 * Main part contains uuencoded section
				 */
				messageSetEncoding(mainMessage, "x-uuencode");

				if((b = messageToBlob(mainMessage)) != NULL) {
					if((cptr = blobGetFilename(b)) != NULL) {
						cli_dbgmsg("Found uuencoded message %s\n", cptr);

						(void)saveFile(b, dir);
					}
					blobDestroy(b);
				}
			} else if((t_line = bounceBegin(mainMessage)) != NULL) {
				/*
				 * Attempt to save the original (unbounced)
				 * message - clamscan will find that in the
				 * directory and call us again (with any luck)
				 * having found an e-mail message to handle
				 */

				/*
				 * Ignore the blank lines before the message
				 * proper
				 */
				while((t_line = t_line->t_next) != NULL)
					if(strcmp(t_line->t_text, "") != 0)
						break;

				if(t_line == NULL) {
					cli_dbgmsg("Not found bounce message\n");
					saveTextPart(mainMessage, dir);
				} else if((b = blobCreate()) != NULL) {
					cli_dbgmsg("Found a bounce message\n");
					do {
						blobAddData(b, (unsigned char *)t_line->t_text, strlen(t_line->t_text));
						blobAddData(b, (unsigned char *)"\n", 1);
					} while((t_line = t_line->t_next) != NULL);

					saveFile(b, dir);

					blobDestroy(b);
				}
			} else {
				cli_dbgmsg("Not found uuencoded file\n");

				saveTextPart(mainMessage, dir);
			}
		} else
			rc = (multiparts) ? 1 : 2;	/* anything saved? */
	} else {
		short attachmentNumber;

		for(attachmentNumber = 0; attachmentNumber < nBlobs; attachmentNumber++) {
			blob *b = blobs[attachmentNumber];

			if(b) {
				if(!saveFile(b, dir))
					break;
				blobDestroy(b);
				blobs[attachmentNumber] = NULL;
			}
		}
	}

	if(aText && (textIn == NULL))
		textDestroy(aText);

	/* Already done */
	if(blobs && (blobsIn == NULL))
		blobArrayDestroy(blobs, nBlobs);

	if(mainMessage && (mainMessage != messageIn))
		messageDestroy(mainMessage);

	cli_dbgmsg("parseEmailBody() returning %d\n", rc);

	return rc;
}

/*
 * Is the current line the start of a new section?
 *
 * New sections start with --boundary
 */
static int
boundaryStart(const char *line, const char *boundary)
{
	/*
	 * Gibe.B3 is broken it has:
	 *	boundary="---- =_NextPart_000_01C31177.9DC7C000"
	 * but it's boundaries look like
	 *	------ =_NextPart_000_01C31177.9DC7C000
	 * notice the extra '-'
	 */
	if(strstr(line, boundary) != NULL) {
		cli_dbgmsg("found %s in %s\n", boundary, line);
		return 1;
	}
	if(*line++ != '-')
		return 0;
	if(*line++ != '-')
		return 0;
	return strcasecmp(line, boundary) == 0;
}

/*
 * Is the current line the end?
 *
 * The message ends with with --boundary--
 */
static int
endOfMessage(const char *line, const char *boundary)
{
	size_t len;

	if(*line++ != '-')
		return 0;
	if(*line++ != '-')
		return 0;
	len = strlen(boundary);
	if(strncasecmp(line, boundary, len) != 0)
		return 0;
	if(strlen(line) != (len + 2))
		return 0;
	line = &line[len];
	if(*line++ != '-')
		return 0;
	return *line == '-';
}

/*
 * Initialise the various lookup tables
 */
static int
initialiseTables(table_t **rfc821Table, table_t **subtypeTable)
{
	const struct tableinit *tableinit;

	/*
	 * Initialise the various look up tables
	 */
	*rfc821Table = tableCreate();
	assert(*rfc821Table != NULL);

	for(tableinit = rfc821headers; tableinit->key; tableinit++)
		if(tableInsert(*rfc821Table, tableinit->key, tableinit->value) < 0)
			return -1;

	*subtypeTable = tableCreate();
	assert(*subtypeTable != NULL);

	for(tableinit = mimeSubtypes; tableinit->key; tableinit++)
		if(tableInsert(*subtypeTable, tableinit->key, tableinit->value) < 0) {
			tableDestroy(*rfc821Table);
			return -1;
		}

	return 0;
}

/*
 * If there's a HTML text version use that, otherwise
 * use the first text part, otherwise just use the
 * first one around. HTML text is most likely to include
 * a scripting worm
 *
 * If we can't find one, return -1
 */
static int
getTextPart(message *const messages[], size_t size)
{
	size_t i;

	for(i = 0; i < size; i++) {
		assert(messages[i] != NULL);
		if((messageGetMimeType(messages[i]) == TEXT) &&
		   (strcasecmp(messageGetMimeSubtype(messages[i]), "html") == 0))
			return (int)i;
	}
	for(i = 0; i < size; i++)
		if(messageGetMimeType(messages[i]) == TEXT)
			return (int)i;

	return -1;
}

/*
 * strip -
 *	Remove the trailing spaces from a buffer
 * Returns it's new length (a la strlen)
 *
 * len must be int not size_t because of the >= 0 test, it is sizeof(buf)
 *	not strlen(buf)
 */
static size_t
strip(char *buf, int len)
{
	register char *ptr;
	register size_t i;

	if((buf == NULL) || (len <= 0))
		return(0);

	i = strlen(buf);
	if(len > (int)(i + 1))
		return(i);

	ptr = &buf[--len];

#if	defined(UNIX) || defined(C_LINUX) || defined(C_DARWIN)	/* watch - it may be in shared text area */
	do
		if(*ptr)
			*ptr = '\0';
	while((--len >= 0) && !isgraph(*--ptr) && (*ptr != '\n') && (*ptr != '\r'));
#else	/* more characters can be displayed on DOS */
	do
#ifndef	REAL_MODE_DOS
		if(*ptr)	/* C8.0 puts into a text area */
#endif
			*ptr = '\0';
	while((--len >= 0) && ((*--ptr == '\0') || (isspace((int)*ptr))));
#endif
	return((size_t)(len + 1));
}

/*
 * strstrip:
 *	Strip a given string
 */
static size_t
strstrip(char *s)
{
	if(s == (char *)NULL)
		return(0);
	return(strip(s, strlen(s) + 1));
}

/*
 * When parsing a MIME header see if this spans more than one line. A
 * semi-colon at the end of the line indicates that the MIME information
 * is continued on the next line.
 *
 * Some clients are broken and put white space after the ;
 */
static bool
continuationMarker(const char *line)
{
	const char *ptr;

	assert(line != NULL);

#ifdef	CL_DEBUG
	cli_dbgmsg("continuationMarker(%s)\n", line);
#endif

	if(strlen(line) == 0)
		return FALSE;

	ptr = strchr(line, '\0');

	assert(ptr != NULL);

	while(ptr > line)
		switch(*--ptr) {
			case '\n':
			case '\r':
			case ' ':
			case '\t':
				continue;
			case ';':
				return TRUE;
			default:
				return FALSE;
		}

	return FALSE;
}

static int
parseMimeHeader(message *m, const char *cmd, const table_t *rfc821Table, const char *arg)
{
	int type = tableFind(rfc821Table, cmd);
#ifdef CL_THREAD_SAFE
	char *strptr;
#endif
	char *copy = strdup(arg);
	char *ptr = copy;

	cli_dbgmsg("parseMimeHeader: cmd='%s', arg='%s'\n", cmd, arg);
	strstrip(copy);

	switch(type) {
		case CONTENT_TYPE:
			/*
			 * Fix for non RFC1521 compliant mailers
			 * that send content-type: Text instead
			 * of content-type: Text/Plain, or
			 * just simply "Content-Type:"
			 */
			if(arg == NULL)
				  cli_warnmsg("Empty content-type received, no subtype specified, assuming text/plain; charset=us-ascii\n");
			else if(strchr(copy, '/') == NULL)
				  cli_warnmsg("Invalid content-type '%s' received, no subtype specified, assuming text/plain; charset=us-ascii\n", copy);
			else {
				/*
				 * Some clients are broken and
				 * put white space after the ;
				 */
				strstrip(copy);
				if(*arg == '/') {
					cli_warnmsg("Content-type '/' received, assuming application/octet-stream\n");
					messageSetMimeType(m, "application");
					messageSetMimeSubtype(m, "octet-stream");
					strtok_r(copy, ";", &strptr);
				} else {
					char *s;

					messageSetMimeType(m, strtok_r(copy, "/", &strptr));

					/*
					 * Stephen White <stephen@earth.li>
					 * Some clients put space after
					 * the mime type but before
					 * the ;
					 */
					s = strtok_r(NULL, ";", &strptr);
					strstrip(s);
					messageSetMimeSubtype(m, s);
				}

				/*
				 * Add in all the arguments.
				 */
				while((copy = strtok_r(NULL, "\r\n \t", &strptr)))
					messageAddArgument(m, copy);
			}
			break;
		case CONTENT_TRANSFER_ENCODING:
			messageSetEncoding(m, copy);
			break;
		case CONTENT_DISPOSITION:
			messageSetDispositionType(m, strtok_r(copy, ";", &strptr));
			messageAddArgument(m, strtok_r(NULL, "\r\n", &strptr));
	}
	free(ptr);

	return type;
}

/*
 * Save the text portion of the message
 */
static void
saveTextPart(message *m, const char *dir)
{
	blob *b;

	messageAddArgument(m, "filename=textportion");
	if((b = messageToBlob(m)) != NULL) {
		/*
		 * Save main part to scan that
		 */
		cli_dbgmsg("Saving main message, encoded with scheme %d\n",
				messageGetEncoding(m));

		(void)saveFile(b, dir);

		blobDestroy(b);
	}
}

/*
 * Save some data as a unique file in the given directory.
 */
static bool
saveFile(const blob *b, const char *dir)
{
	unsigned long nbytes = blobGetDataSize(b);
	size_t len = 0;
	int fd;
	const char *cptr, *suffix;
	unsigned char *data;
	char filename[NAME_MAX + 1];

	assert(dir != NULL);

	if(nbytes == 0)
		return TRUE;

	cptr = blobGetFilename(b);

	if(cptr == NULL) {
		cptr = "unknown";
		suffix = "";
	} else {
		/*
		 * Some programs are broken and use an idea of a ".suffix"
		 * to determine the file type rather than looking up the
		 * magic number. CPM has a lot to answer for...
		 * FIXME: the suffix now appears twice in the filename...
		 */
		suffix = strrchr(cptr, '.');
		if(suffix == NULL)
			suffix = "";
		else
			len = strlen(suffix);
	}
	cli_dbgmsg("Saving attachment in %s/%s\n", dir, cptr);

	/*
	 * Allow for very long filenames. We have to truncate them to fit
	 */
	snprintf(filename, sizeof(filename) - 1 - len, "%s/%.*sXXXXXX", dir,
		(int)(sizeof(filename) - 9 - len - strlen(dir)), cptr);

	/*
	 * TODO: add a HAVE_MKSTEMP property
	 */
#if	defined(C_LINUX) || defined(C_BSD) || defined(HAVE_MKSTEMP) || defined(C_SOLARIS)
	fd = mkstemp(filename);
#else
	(void)mktemp(filename);
	fd = open(filename, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC|O_BINARY, 0600);
#endif

	if(fd < 0) {
		cli_errmsg("Can't create temporary file %s: %s\n", filename, strerror(errno));
		return FALSE;
	}

	/*
	 * Add the suffix back to the end of the filename. Tut-tut, filenames
	 * should be independant of their usage on UNIX type systems.
	 */
	if(len > 1) {
		char stub[NAME_MAX + 1];

		snprintf(stub, sizeof(stub), "%s%s", filename, suffix);
#ifdef	C_LINUX
		rename(stub, filename);
#else
		link(stub, filename);
		unlink(stub);
#endif
	}

	cli_dbgmsg("Saving attachment as %s (%lu bytes long)\n",
		filename, nbytes);

	data = blobGetData(b);
	while(nbytes > 0) {
		int rc = write(fd, data, (size_t)nbytes);
		if(rc < 0) {
			perror(filename);
			close(fd);
			return FALSE;
		}
		nbytes -= rc;
		data = &data[rc];
	}

	return (close(fd) >= 0);
}
