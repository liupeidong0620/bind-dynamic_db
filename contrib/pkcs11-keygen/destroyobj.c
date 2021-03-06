/* destroyobj [-s $slot] [-i $id | -l $label] [-p $pin] */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifndef OPENCRYPTOKI
#include <security/cryptoki.h>
#include <security/pkcs11.h>
#else
#include <opencryptoki/pkcs11.h>
#endif

int
main(int argc, char *argv[])
{
    CK_RV rv;
    CK_SLOT_ID slot = 0;
    CK_SESSION_HANDLE hSession;
    CK_UTF8CHAR *pin = NULL;
    CK_BYTE attr_id[2];
    CK_OBJECT_HANDLE akey[50];
    char *label = NULL;
    int error = 0;
    int id = 0, i = 0;
    int c, errflg = 0;
    CK_ULONG ulObjectCount;
    CK_ATTRIBUTE search_template[] = {
	{CKA_ID, &attr_id, sizeof(attr_id)}
    };
    extern char *optarg;
    extern int optopt;

    while ((c = getopt(argc, argv, ":s:i:l:p:")) != -1) {
	switch (c) {
	case 's':
	    slot = atoi(optarg);
	    break;
	case 'i':
	    id = atoi(optarg);
	    id &= 0xffff;
	    break;
	case 'l':
	    label = optarg;
	    break;
	case 'p':
	    pin = (CK_UTF8CHAR *)optarg;
	    break;
	case ':':
	    fprintf(stderr, "Option -%c requires an operand\n", optopt);
	    errflg++;
	    break;
	case '?':
	default:
	    fprintf(stderr, "Unrecognised option: -%c\n", optopt);
	    errflg++;
	}
    }
    if (errflg || ((!id) && (!label))) {
	fprintf(stderr,
		"usage: destroykey [-s slot] [-i id | -l label] [-p pin]\n");
	exit(1);
    }
    if (id) {
	printf("id %i\n", id);
	attr_id[0] = (id >> 8) & 0xff;
	attr_id[1] = id & 0xff;
    } else if (label) {
	printf("label %s\n", label);
	search_template[0].type = CKA_LABEL;
	search_template[0].pValue = label;
	search_template[0].ulValueLen = strlen(label);
    }

    /* Initialize the CRYPTOKI library */
    rv = C_Initialize(NULL_PTR);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_Initialize: Error = 0x%.8X\n", rv);
	exit(1);
    }

    /* Open a session on the slot found */
    rv = C_OpenSession(slot, CKF_RW_SESSION+CKF_SERIAL_SESSION,
		       NULL_PTR, NULL_PTR, &hSession);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_OpenSession: Error = 0x%.8X\n", rv);
	error = 1;
	goto exit_program;
    }

    /* Login to the Token (Keystore) */
    if (!pin)
#ifndef OPENCRYPTOKI
	pin = (CK_UTF8CHAR *)getpassphrase("Enter Pin: ");
#else
	pin = (CK_UTF8CHAR *)getpass("Enter Pin: ");
#endif
    rv = C_Login(hSession, CKU_USER, pin, strlen((char *)pin));
    memset(pin, 0, strlen((char *)pin));
    if (rv != CKR_OK) {
	fprintf(stderr, "C_Login: Error = 0x%.8X\n", rv);
	error = 1;
	goto exit_session;
    }

    rv = C_FindObjectsInit(hSession, search_template,
			   ((id != 0) || (label != NULL)) ? 1 : 0); 
    if (rv != CKR_OK) {
	fprintf(stderr, "C_FindObjectsInit: Error = 0x%.8X\n", rv);
	error = 1;
	goto exit_session;
    }
    
    rv = C_FindObjects(hSession, akey, 50, &ulObjectCount);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_FindObjects: Error = 0x%.8X\n", rv);
	error = 1;
	goto exit_search;
    }

    for (i = 0; i < ulObjectCount; i++) {
	CK_OBJECT_CLASS oclass = 0;
	CK_BYTE labelbuf[64 + 1];
	CK_BYTE idbuf[64];
	CK_ATTRIBUTE attr_template[] = {
	    {CKA_CLASS, &oclass, sizeof(oclass)},
	    {CKA_LABEL, labelbuf, sizeof(labelbuf) - 1},
	    {CKA_ID, idbuf, sizeof(idbuf)}
	};
	int j, len;

	memset(labelbuf, 0, sizeof(labelbuf));
	memset(idbuf, 0, sizeof(idbuf));

	rv = C_GetAttributeValue(hSession, akey[i], attr_template, 3);
	if (rv != CKR_OK) {
	    fprintf(stderr, "C_GetAttributeValue[%d]: rv = 0x%.8X\n", i, rv);
	    error = 1;
	    goto exit_search;
	}
	len = attr_template[2].ulValueLen;
	printf("object[%d]: class %d label '%s' id[%u] ",
	       i, oclass, labelbuf, attr_template[2].ulValueLen);
	if (len > 4)
	    len = 4;
	for (j = 0; j < len; j++)
	    printf("%02x", idbuf[j]);
	if (attr_template[2].ulValueLen > len)
	    printf("...\n");
	else
	    printf("\n");
    }

    /* give a chance to kill this */
    printf("sleeping 5 seconds...\n");
    sleep(5);

    for (i = 0; i < ulObjectCount; i++) {
	rv = C_DestroyObject(hSession, akey[i]);
	if (rv != CKR_OK) {
	    fprintf(stderr, "C_DestroyObject[%d]: rv = 0x%.8X\n", i, rv);
	    error = 1;
	}
    }

 exit_search:
    rv = C_FindObjectsFinal(hSession);
    if (rv != CKR_OK) {
	fprintf(stderr, "C_FindObjectsFinal: Error = 0x%.8X\n", rv);
	error = 1;
    }

 exit_session:
    (void) C_CloseSession(hSession);

 exit_program:
    (void) C_Finalize(NULL_PTR);

    exit(error);
}
