/*
   cf-keycrypt.c

   Copyright (C) 2017 cfengineers.net

   Written and maintained by Jon Henrik Bjornstad <jonhenrik@cfengineers.net>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#include <config.h>
#include <cf3.defs.h>
#include <cf3.extern.h>
#include <openssl/err.h>

#include <lastseen.h>
#include <conversion.h>
#include <files_hashes.h>
#include <locks.h>
#include <item_lib.h>
#include <known_dirs.h>
#include <dbm_api.h>
/*
#ifdef LMDB
#include <lmdb.h>
#endif
*/

#define STDIN     0
#define STDOUT    1

#define BUFSIZE 1024

void usage()
{
    printf("Workdir is defined as %s\n", WORKDIR);
    printf(
    "\n"
    "Usage: cf-keycrypt [-e public-key|-d private-key|-H hostname-or-ip] -o outfile -i infile [-h]\n"
    "\n"
    "Use CFEngine cryptographic keys to encrypt and decrypt files, eg. files containing\n"
    "passwords or certificates.\n"
    "\n"
    "  -e       Encrypt with key.\n"
    "  -d       Decrypt with key.\n"
    "  -i       File to encrypt/decrypt. '-' reads from stdin.\n"
    "  -o       File to write encrypted/decrypted contents to. '-' writes to stdout.\n"
    "  -H       Hostname/IP to encrypt for, gets data from lastseen database\n"
    "  -h       Print help.\n"
    "\n"
    "Examples:\n"
    "  Encrypt:\n"
    "    cf-keycrypt -e /var/cfengine/ppkeys/localhost.pub -i myplain.dat -o mycrypt.dat\n"
    "\n"
    "  Decrypt:\n"
    "    cf-keycrypt -d /var/cfengine/ppkeys/localhost.priv -i mycrypt.dat -o myplain.dat\n"
    "\n"
    "Written and maintained by Jon Henrik Bjornstad <jonhenrik@cfengineers.net>\n"
    "\n"
    "Copyright (C) cfengineers.net\n"
    "\n"
    );
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int file_exist(char *filename)
{
    struct stat buffer;
    return (stat (filename, &buffer) == 0);
}

char *get_host_pubkey(char *host)
{
    // char *key;                            // TODO: this was unused(!)
    // char value[BUFSIZE];                  // TODO: this was unused(!)
    char *buffer = (char *) malloc(BUFSIZE *sizeof(char));
    //char *keyname = NULL;
    char hash[CF_HOSTKEY_STRING_SIZE];
    //char buffer[BUFSIZE];
    char ipaddress[BUFSIZE];
    // int ecode;                            // TODO: this was unused(!)
    struct addrinfo *result;
    struct addrinfo *res;
    bool found = false;

    int error = getaddrinfo(host, NULL, NULL, &result);
    if (error != 0)
    {
        Log(LOG_LEVEL_ERR, "Failed to get IP from host (getaddrinfo: %s)",
            gai_strerror(error));
        return NULL;
    }

    for(res = result; res != NULL && !found; res = res->ai_next)
    {
        inet_ntop(res->ai_family, get_in_addr((struct sockaddr *)res->ai_addr), ipaddress, sizeof(ipaddress));
        if ((strcmp(ipaddress, "127.0.0.1") == 0) || (strcmp(ipaddress, "::1") == 0))
        {
            found = true;
            snprintf(buffer, BUFSIZE * sizeof(char), "%s/ppkeys/localhost.pub", WORKDIR);
            return buffer;
        }
        found = Address2Hostkey(hash, sizeof(hash), ipaddress);
    }
    if (found)
    {
        snprintf(buffer, BUFSIZE * sizeof(char), "%s/ppkeys/root-%s.pub", WORKDIR, hash);
        return buffer;
    }
    else
    {
        for(res = result; res != NULL; res = res->ai_next)
        {
            inet_ntop(res->ai_family, get_in_addr((struct sockaddr *)res->ai_addr), ipaddress, sizeof(ipaddress));
            snprintf(buffer, BUFSIZE * sizeof(char), "%s/ppkeys/root-%s.pub", WORKDIR, ipaddress);
            if (file_exist(buffer))
            {
                return buffer;
            }
        }
    }
    return NULL;
}

void *readseckey(char *secfile)
{
    FILE *fp = NULL;
    RSA* PRIVKEY = NULL;
    static char *passphrase = "Cfengine passphrase";
    unsigned long err;

    if ((fp = fopen(secfile,"r")) == NULL)
    {
        printf("Couldn't find a private key - use cf-key to get one");
        return (void *)NULL;
    }

    if ((PRIVKEY = PEM_read_RSAPrivateKey(fp,(RSA **)NULL,NULL,passphrase)) == NULL)
    {
        err = ERR_get_error();
        printf("PEM_readError reading Private Key = %s\n", ERR_reason_error_string(err));
        PRIVKEY = NULL;
        fclose(fp);
        return (void *)NULL;
    }
    else
    {
        return (void *)PRIVKEY;
    }
}

void *readpubkey(char *pubfile)
{
    FILE *fp;
    RSA *key = NULL;
    static char *passphrase = "Cfengine passphrase";

    if ((fp = fopen(pubfile, "r")) == NULL)
    {
        fprintf(stderr,"Error: Cannot locate Public Key file '%s'.\n", pubfile);
        return NULL;
    }
    if ((key = PEM_read_RSAPublicKey(fp,(RSA **)NULL,NULL,passphrase)) == NULL)
    {
        fprintf(stderr,"Error: failed reading Public Key in '%s' file.\n", pubfile);
        return NULL;
    }
    fclose(fp);
    return (void *)key;
}

long int rsa_encrypt(char *pubfile, char *filein, char *fileout)
{
    int ks = 0;
    unsigned long size = 0, len = 0, ciphsz = 0;
    RSA* key = NULL;
    FILE *infile = NULL;
    FILE *outfile = NULL;
    char *tmpciph = NULL, *tmpplain = NULL;

    key = (RSA *)readpubkey(pubfile);
    if (!key)
    {
        return -1;
    }

    if ((strcmp("-",filein) == 0))
    {
        infile = stdin;
    }
    else if (!(infile = fopen(filein, "r")))
    {
        fprintf(stderr, "Error: Cannot locate input file.\n");
        return -1;
    }

    if ((strcmp("-",fileout) == 0))
    {
        outfile = stdout;
        //printf("Write to stdout\n");
    }
    else if (!(outfile = fopen(fileout, "w")))
    {
        fprintf(stderr, "Error: Cannot create output file.\n");
        return -1;
    }

    ks = RSA_size(key);
    tmpplain = (unsigned char *)malloc(ks * sizeof(unsigned char));
    tmpciph = (unsigned char *)malloc(ks * sizeof(unsigned char));
    srand(time(NULL));

    while(!feof(infile))
    {
        memset(tmpplain, '\0', ks);
        memset(tmpciph, '\0', ks);
        len = fread(tmpplain, 1, ks-RSA_PKCS1_PADDING_SIZE, infile);
        if (len <= 0)
        {
            Log(LOG_LEVEL_ERR, "Could not read file '%s'(fread: %lu)",
                filein, len);
        }
        if ((size = RSA_public_encrypt(strlen(tmpplain), tmpplain, tmpciph, key, RSA_PKCS1_PADDING)) == -1)
        {
            fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), NULL));
            return -1;
        }
        fwrite(tmpciph,sizeof(unsigned char),ks,outfile);
    }
    fclose(infile);
    fclose(outfile);
    free(tmpciph);
    free(tmpplain);
    RSA_free(key);
    return ciphsz;
}

long int rsa_decrypt(char *secfile, char *cryptfile, char *plainfile)
{
    unsigned long int plsz = 0, size = 0, ks = 0, len = 0; // TODO: Unused: len = 0;
    RSA *key = NULL;
    char *tmpplain = NULL, *tmpciph = NULL;

    FILE *infile = NULL;
    FILE *outfile = NULL;

    key = (RSA *)readseckey(secfile);
    if (!key) {
        return -1;
    }

    if ((strcmp("-",cryptfile) == 0)) {
        infile = stdin;
    }else if (!(infile = fopen(cryptfile, "r"))) {
        fprintf(stderr, "Error: Cannot locate input file.\n");
        return -1;
    }

    if ((strcmp("-",plainfile) == 0)) {
        outfile = stdout;
        //printf("Write to stdout\n");
    }else if (!(outfile = fopen(plainfile, "w"))){
        fprintf(stderr, "Error: Cannot create output file.\n");
        return -1;
    }

    ks = RSA_size(key);
    tmpciph = (unsigned char *)malloc(ks * sizeof(unsigned char));
    tmpplain = (unsigned char *)malloc(ks * sizeof(unsigned char));
    //printf("Keysize: %d\n", ks);
    while(!feof(infile))
    {
        memset(tmpciph, '\0', ks);
        memset(tmpplain, '\0', ks);
        len = fread(tmpciph, 1, ks, infile);
        if (len > 0){
            if ((size = RSA_private_decrypt(ks, tmpciph, tmpplain, key, RSA_PKCS1_PADDING)) == -1)
            {
                fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), NULL));
                return -1;
            }
        }
        fwrite(tmpplain,1,strlen(tmpplain),outfile);
    }
    fclose(infile);
    fclose(outfile);
    free(tmpplain);
    free(tmpciph);
    RSA_free(key);
    return plsz;
}

int main(int argc, char *argv[])
{
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    opterr = 0;
    char *key = NULL;
    char *infile = NULL;
    char *outfile = NULL;
    char *host = NULL;
    int encrypt = 0;
    int decrypt = 0;
    int c = 0;
    int size = 0;

    //strcpy(WORKDIR,CFCR_WORKDIR);

    while ((c = getopt (argc, argv, "he:d:i:o:H:")) != -1)
    {
        switch (c)
        {
            case 'e':
                encrypt = 1;
                key = optarg;
                break;
            case 'd':
                decrypt = 1;
                key = optarg;
                break;
            case 'i':
                infile = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            case 'H':
                encrypt = 1;
                host = optarg;
                break;
            case 'h':
                usage();
                exit(1);
            default:
                printf("ERROR: Unknown option '-%c'\n", optopt);
                usage();
                exit(1);
        }
    }
    if (host)
    {
        key = get_host_pubkey(host);
        if (!key)
        {
            fprintf(stderr,"ERROR: Unable to locate public key for host %s\n", host);
            exit(1);
        }
    }

    if (encrypt > 0 && key && infile && outfile)
    {
        size = rsa_encrypt(key, infile, outfile);
        if (size < 0)
            exit(1);
    }
    else if (decrypt > 0 && key && infile && outfile)
    {
        size = rsa_decrypt(key, infile, outfile);
        if (size < 0)
            exit(1);
    }
    else
    {
        usage();
        exit(1);
    }
    exit(0);
}
