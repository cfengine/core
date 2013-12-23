#include <test.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <cmockery.h>
#include <server_common.h>
#include <crypto.h>
#include <tls_generic.h>
#include <tls_server.h>
#include <tls_client.h>
#include <connection_info.h>

/*
 * We create the appropriate SSL structures before starting the tests.
 * We delete them afterwards
 */
static SSL_CTX *SSLSERVERCONTEXT = NULL;
static X509 *SSLSERVERCERT = NULL;
static SSL_CTX *SSLCLIENTCONTEXT = NULL;
static X509 *SSLCLIENTCERT = NULL;
static bool correctly_initialized = false;
static pid_t pid = -1;
static int server_public_key_file = -1;
static int certificate_file = -1;
static char server_name_template_public[] = "/tmp/tls_test/ghijklXXXXXX";
static char server_certificate_template_public[] = "/tmp/tls_test/certXXXXXX";
static char temporary_folder[] = "/tmp/tls_test";
/*
 * Helper functions, used to start a server and a client.
 * Notice that the child is the server, not the other way around.
 */
int ssl_server_init();
void ssl_client_init();

void child_cycle(int channel)
{
    int message = 0;
    int result = 0;
    int local_socket = 0;
    int remote_socket = 0;
    struct sockaddr_in my_addr, peer_addr;
    socklen_t peer_addr_size = 0;

    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    memset(&peer_addr, 0, sizeof(struct sockaddr_in));
    /*
     * Do the TLS initialization dance.
     */
    result = ssl_server_init();
    if (result < 0)
    {
        message = -1;
        result = write(channel, &message, sizeof(int));
        exit(EXIT_SUCCESS);
    }
    /*
     * Create a unix socket
     */
    local_socket = socket(AF_INET, SOCK_STREAM, 0);
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(8035);
    /* Avoid spurious failures when rerunning the test due to socket not yet
     * being released. */
    int opt = 1;
    setsockopt(local_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    /*
     * Bind it
     */
    result = bind(local_socket, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));
    if (result < 0)
    {
        message = -1;
        result = write(channel, &message, sizeof(int));
        exit(EXIT_SUCCESS);
    }
    /*
     * Start listening for connections
     */
    result = listen(local_socket, 5);
    if (result < 0)
    {
        message = -1;
        result = write(channel, &message, sizeof(int));
        exit(EXIT_SUCCESS);
    }
    /*
     * Signal the parent that we are ok.
     */
    result = write(channel, &message, sizeof(int));
    /*
     * If this did not work, then we abort.
     */
    if (result < 0)
    {
        exit(EXIT_SUCCESS);
    }
    /*
     * Send the name of the public key file.
     */
    result = write(channel, server_name_template_public, strlen(server_name_template_public));
    if (result < 0)
    {
        exit(EXIT_SUCCESS);
    }
    /*
     * Send the name of the certificate file.
     */
    result = write(channel, server_certificate_template_public, strlen(server_certificate_template_public));
    if (result < 0)
    {
        exit(EXIT_SUCCESS);
    }
    /*
     * Now wait until somebody calls.
     */
    peer_addr_size = sizeof(struct sockaddr_in);
    while (true)
    {
        remote_socket = accept(local_socket, (struct sockaddr *)&peer_addr, &peer_addr_size);
        if (remote_socket < 0)
        {
            Log (LOG_LEVEL_CRIT, "Could not accept connection");
            continue;
        }
        /*
         * We are not testing the server, we are testing the functions to send and receive data
         * over TLS. We do not need a full fletched server for that, we just need to send and
         * receive data and try the error conditions.
         */
        SSL *ssl = SSL_new(SSLSERVERCONTEXT);
        if (!ssl)
        {
            Log(LOG_LEVEL_CRIT, "Could not create SSL structure on the server side");
            SSL_free(ssl);
            close (remote_socket);
            remote_socket = -1;
            continue;
        }
        SSL_set_fd(ssl, remote_socket);
        result = SSL_accept(ssl);
        if (result < 0)
        {
            Log(LOG_LEVEL_CRIT, "Could not accept a TLS connection");
            close (remote_socket);
            remote_socket = -1;
            continue;
        }
        /*
         * Our mission is pretty simple, receive data and send it back.
         */
        int received = 0;
        int sent = 0;
        char buffer[4096];
        do {
            received = SSL_read(ssl, buffer, 4096);
            if (received < 0)
            {
                Log(LOG_LEVEL_CRIT, "Failure while receiving data over TLS");
                break;
            }
            sent = SSL_write(ssl, buffer, received);
            if (sent < 0)
            {
                Log(LOG_LEVEL_CRIT, "Failure while sending data over TLS");
                break;
            }
        } while (received > 0);
        /*
         * Mission completed, start again.
         */
        SSL_shutdown(ssl);
        SSL_free(ssl);
        remote_socket = -1;
    }
    exit(EXIT_SUCCESS);
}

int start_child_process()
{
    int result = 0;
    int channel[2];

    result = pipe(channel);
    if (result < 0)
    {
        return -1;
    }
    pid = fork();
    if (pid < 0)
    {
        return -1;
    }
    else if (pid == 0)
    {
        /*
         * Child
         * The child process is the one running the server.
         */
        close (channel[0]);
        child_cycle(channel[1]);
    }
    else
    {
        /*
         * Parent
         * The parent process is the process runing the test.
         */
        close (channel[1]);
        int message = 0;
        result = read(channel[0], &message, sizeof(int));
        if ((result < 0) || (message < 0))
        {
            close (channel[0]);
            /*
             * Wait for child process
             */
            wait(NULL);
            pid = -1;
            return -1;
        }
        /*
         * Get the name of the public key file
         */
        result = read(channel[0], server_name_template_public, strlen(server_name_template_public));
        if (result < 0)
        {
            close (channel[0]);
            /*
             * Wait for child process
             */
            wait(NULL);
            pid = -1;
            return -1;
        }
        server_name_template_public[result] = '\0';
        /*
         * Get the name of the certificate file
         */
        result = read(channel[0], server_certificate_template_public, strlen(server_certificate_template_public));
        if (result < 0)
        {
            close (channel[0]);
            /*
             * Wait for child process
             */
            wait(NULL);
            pid = -1;
            return -1;
        }
        server_certificate_template_public[result] = '\0';
    }
    return 0;
}

int ssl_server_init()
{
    /*
     * This is twisted. We can generate the required keys by calling RSA_generate_key,
     * however we cannot put the private part and the public part in the two containers.
     * For that we need to save each part to a file and then load each part from
     * the respective file.
     */
    RSA *key = NULL;
    key = RSA_generate_key(1024, 17, NULL, NULL);
    if (!key)
    {
        correctly_initialized = false;
        return -1;
    }
    char name_template_private[] = "/tmp/tls_test/abcdefXXXXXX";
    int private_key_file = 0;
    FILE *private_key_stream = NULL;
    int ret = 0;

    private_key_file = mkstemp(name_template_private);
    if (private_key_file < 0)
    {
        correctly_initialized = false;
        return -1;
    }
    private_key_stream = fdopen(private_key_file, "w+");
    if (!private_key_stream)
    {
        correctly_initialized = false;
        return -1;
    }
    ret = PEM_write_RSAPrivateKey(private_key_stream, key, NULL, NULL, 0, 0, NULL);
    if (ret == 0)
    {
        correctly_initialized = false;
        return -1;
    }
    fseek(private_key_stream, 0L, SEEK_SET);
    PRIVKEY = PEM_read_RSAPrivateKey(private_key_stream, (RSA **)NULL, NULL, NULL);
    if (!PRIVKEY)
    {
        correctly_initialized = false;
        return -1;
    }
    fclose(private_key_stream);

    int public_key_file = 0;
    FILE *public_key_stream = NULL;
    public_key_file = mkstemp(server_name_template_public);
    if (public_key_file < 0)
    {
        correctly_initialized = false;
        return -1;
    }
    server_public_key_file = public_key_file;
    public_key_stream = fdopen(public_key_file, "w+");
    if (!public_key_stream)
    {
        correctly_initialized = false;
        return -1;
    }
    ret = PEM_write_RSAPublicKey(public_key_stream, key);
    if (ret == 0)
    {
        correctly_initialized = false;
        return -1;
    }
    fflush(public_key_stream);
    fsync(public_key_file);
    fseek(public_key_stream, 0L, SEEK_SET);
    PUBKEY = PEM_read_RSAPublicKey(public_key_stream, (RSA **)NULL, NULL, NULL);
    if (!PUBKEY)
    {
        correctly_initialized = false;
        return -1;
    }
    RSA_free(key);

    assert(SSLSERVERCONTEXT == NULL);
    SSLSERVERCONTEXT = SSL_CTX_new(SSLv23_server_method());
    if (SSLSERVERCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err1;
    }

    /* Use only TLS v1 or later.
       TODO option for SSL_OP_NO_TLSv{1,1_1} */
    SSL_CTX_set_options(SSLSERVERCONTEXT,
                        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(SSLSERVERCONTEXT, SSL_MODE_AUTO_RETRY);

    /*
     * Create cert into memory and load it into SSL context.
     */

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        Log(LOG_LEVEL_ERR,
            "No public/private key pair is loaded, create one with cf-key");
        goto err2;
    }
    assert(SSLSERVERCERT == NULL);
    /* Generate self-signed cert valid from now to 50 years later. */
    {
        X509 *x509 = X509_new();
        X509_gmtime_adj(X509_get_notBefore(x509), 0);
        X509_time_adj(X509_get_notAfter(x509), 60*60*24*365*50, NULL);
        EVP_PKEY *pkey = EVP_PKEY_new();
        EVP_PKEY_set1_RSA(pkey, PRIVKEY);
        X509_NAME *name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const char *) "",
                                   -1, -1, 0);
        X509_set_issuer_name(x509, name);
        X509_set_pubkey(x509, pkey);

        const EVP_MD *md = EVP_get_digestbyname("sha384");
        if (md == NULL)
        {
            Log(LOG_LEVEL_ERR, "Uknown digest algorithm %s",
                "sha384");
            correctly_initialized = false;
            return -1;
        }
        ret = X509_sign(x509, pkey, md);

        EVP_PKEY_free(pkey);
        SSLSERVERCERT = x509;

        if (ret <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Couldn't sign the public key for the TLS handshake: %s",
                ERR_reason_error_string(ERR_get_error()));
            goto err3;
        }
        /*
         * Create a temporary file and save the certificate there
         */
        certificate_file = mkstemp(server_certificate_template_public);
        if (certificate_file < 0)
        {
            correctly_initialized = false;
            return -1;
        }
        FILE *certificate_stream = fdopen(certificate_file, "w+");
        if (!certificate_stream)
        {
            correctly_initialized = false;
            return -1;
        }
        PEM_write_X509(certificate_stream, x509);
        fflush(certificate_stream);
        fsync(certificate_file);
    }

    SSL_CTX_use_certificate(SSLSERVERCONTEXT, SSLSERVERCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLSERVERCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLSERVERCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Set options to always request a certificate from the peer, either we
     * are client or server. */
    SSL_CTX_set_verify(SSLSERVERCONTEXT, SSL_VERIFY_PEER, NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(SSLSERVERCONTEXT, TLSVerifyCallback, NULL);
    correctly_initialized = true;
    return 0;
  err3:
    X509_free(SSLSERVERCERT);
    SSLSERVERCERT = NULL;
  err2:
    SSL_CTX_free(SSLSERVERCONTEXT);
    SSLSERVERCONTEXT = NULL;
  err1:
    correctly_initialized = false;
    return -1;
}

void ssl_client_init()
{
    /*
     * This is twisted. We can generate the required keys by calling RSA_generate_key,
     * however we cannot put the private part and the public part in the two containers.
     * For that we need to save each part to a file and then load each part from
     * the respective file.
     */
    RSA *key = NULL;
    key = RSA_generate_key(1024, 17, NULL, NULL);
    if (!key)
    {
        correctly_initialized = false;
        return;
    }
    char name_template_private[] = "/tmp/tls_test/mnopqrXXXXXX";
    char name_template_public[] = "/tmp/tls_test/stuvwxXXXXXX";
    int private_key_file = 0;
    FILE *private_key_stream = NULL;
    int ret = 0;

    private_key_file = mkstemp(name_template_private);
    if (private_key_file < 0)
    {
        correctly_initialized = false;
        return;
    }
    private_key_stream = fdopen(private_key_file, "w+");
    if (!private_key_stream)
    {
        correctly_initialized = false;
        return;
    }
    ret = PEM_write_RSAPrivateKey(private_key_stream, key, NULL, NULL, 0, 0, NULL);
    if (ret == 0)
    {
        correctly_initialized = false;
        return;
    }
    fseek(private_key_stream, 0L, SEEK_SET);
    PRIVKEY = PEM_read_RSAPrivateKey(private_key_stream, (RSA **)NULL, NULL, NULL);
    if (!PRIVKEY)
    {
        correctly_initialized = false;
        return;
    }
    fclose(private_key_stream);

    int public_key_file = 0;
    FILE *public_key_stream = NULL;
    public_key_file = mkstemp(name_template_public);
    if (public_key_file < 0)
    {
        correctly_initialized = false;
        return;
    }
    public_key_stream = fdopen(public_key_file, "w+");
    if (!public_key_stream)
    {
        correctly_initialized = false;
        return;
    }
    ret = PEM_write_RSAPublicKey(public_key_stream, key);
    if (ret == 0)
    {
        correctly_initialized = false;
        return;
    }
    fseek(public_key_stream, 0L, SEEK_SET);
    PUBKEY = PEM_read_RSAPublicKey(public_key_stream, (RSA **)NULL, NULL, NULL);
    if (!PUBKEY)
    {
        correctly_initialized = false;
        return;
    }
    fclose(public_key_stream);
    RSA_free(key);

    SSLCLIENTCONTEXT = SSL_CTX_new(SSLv23_client_method());
    if (SSLCLIENTCONTEXT == NULL)
    {
        Log(LOG_LEVEL_ERR, "SSL_CTX_new: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err1;
    }

    /* Use only TLS v1 or later.
       TODO option for SSL_OP_NO_TLSv{1,1_1} */
    SSL_CTX_set_options(SSLCLIENTCONTEXT,
                        SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    /* Never bother with retransmissions, SSL_write() should
     * always either write the whole amount or fail. */
    SSL_CTX_set_mode(SSLCLIENTCONTEXT, SSL_MODE_AUTO_RETRY);

    /*
     * Create cert into memory and load it into SSL context.
     */

    if (PRIVKEY == NULL || PUBKEY == NULL)
    {
        correctly_initialized = false;
        return;
    }

    /* Generate self-signed cert valid from now to 50 years later. */
    {
        X509 *x509 = X509_new();
        X509_gmtime_adj(X509_get_notBefore(x509), 0);
        X509_time_adj(X509_get_notAfter(x509), 60*60*24*365*50, NULL);
        EVP_PKEY *pkey = EVP_PKEY_new();
        EVP_PKEY_set1_RSA(pkey, PRIVKEY);
        X509_NAME *name = X509_get_subject_name(x509);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   (const char *) "",
                                   -1, -1, 0);
        X509_set_issuer_name(x509, name);
        X509_set_pubkey(x509, pkey);

        const EVP_MD *md = EVP_get_digestbyname("sha384");
        if (md == NULL)
        {
            correctly_initialized = false;
            return;
        }
        ret = X509_sign(x509, pkey, md);

        EVP_PKEY_free(pkey);
        SSLCLIENTCERT = x509;

        if (ret <= 0)
        {
            Log(LOG_LEVEL_ERR,
                "Couldn't sign the public key for the TLS handshake: %s",
                ERR_reason_error_string(ERR_get_error()));
            goto err3;
        }
    }
    /* Log(LOG_LEVEL_ERR, "generate cert from priv key: %s", */
    /*     ERR_reason_error_string(ERR_get_error())); */

    SSL_CTX_use_certificate(SSLCLIENTCONTEXT, SSLCLIENTCERT);

    ret = SSL_CTX_use_RSAPrivateKey(SSLCLIENTCONTEXT, PRIVKEY);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Failed to use RSA private key: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Verify cert consistency. */
    ret = SSL_CTX_check_private_key(SSLCLIENTCONTEXT);
    if (ret != 1)
    {
        Log(LOG_LEVEL_ERR, "Inconsistent key and TLS cert: %s",
            ERR_reason_error_string(ERR_get_error()));
        goto err3;
    }

    /* Set options to always request a certificate from the peer, either we
     * are client or server. */
    SSL_CTX_set_verify(SSLCLIENTCONTEXT, SSL_VERIFY_PEER, NULL);
    /* Always accept that certificate, we do proper checking after TLS
     * connection is established since OpenSSL can't pass a connection
     * specific pointer to the callback (so we would have to lock).  */
    SSL_CTX_set_cert_verify_callback(SSLCLIENTCONTEXT, TLSVerifyCallback, NULL);

    correctly_initialized = true;
    return;

  err3:
    X509_free(SSLCLIENTCERT);
    SSLCLIENTCERT = NULL;
    SSL_CTX_free(SSLCLIENTCONTEXT);
    SSLCLIENTCONTEXT = NULL;
  err1:
    correctly_initialized = false;
    return;
}

void tests_setup(void)
{
    int ret = 0;

    /*
     * Create a temporary folder to store our files.
     * We do not use mkdtemp to avoid putting our temporary files in the wrong place.
     * In any case, mkdir fails if the folder already exists.
     */
    ret = mkdir(temporary_folder, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    if (ret < 0)
    {
        printf("could not create folder %s\n", temporary_folder);
        correctly_initialized = false;
        return;
    }
    /* OpenSSL is needed for our new protocol over TLS. */
    SSL_library_init();
    SSL_load_error_strings();

    /*
     * First we start a new process to have a server for our tests.
     */
    ret = start_child_process();
    if (ret < 0)
    {
        correctly_initialized = false;
        return;
    }
    /*
     * If the initialization went without problems, then at this point
     * there is a second process waiting for connections.
     */
    ssl_client_init();
}

void tests_teardown(void)
{
    if (SSLSERVERCERT)
    {
        X509_free(SSLSERVERCERT);
        SSLSERVERCERT = NULL;
    }
    if (SSLSERVERCONTEXT)
    {
        SSL_CTX_free(SSLSERVERCONTEXT);
        SSLSERVERCONTEXT = NULL;
    }
    if (PRIVKEY)
    {
        RSA_free(PRIVKEY);
        PRIVKEY = NULL;
    }
    if (PUBKEY)
    {
        RSA_free(PUBKEY);
        PUBKEY = NULL;
    }
    if (server_public_key_file != -1)
    {
        close (server_public_key_file);
    }
    if (certificate_file != -1)
    {
        close (server_public_key_file);
    }
    if (pid > 0)
    {
        /*
         * Kill child process
         */
        kill(pid, SIGKILL);
    }
    /* Delete temporary folder and files */
    DIR *folder = opendir(temporary_folder);
    if (folder)
    {
        struct dirent *entry = NULL;
        for (entry = readdir(folder); entry; entry = readdir(folder))
        {
            if (entry->d_name[0] == '.')
            {
                /* Skip . and .. */
                continue;
            }
            char *name = (char *)xmalloc (strlen(temporary_folder) + strlen(entry->d_name) + 2);
            sprintf(name, "%s/%s", temporary_folder, entry->d_name);
            unlink(name);
            free (name);
        }
        closedir(folder);
        rmdir(temporary_folder);
    }
}

/*
 * Functions to mock
 * int SSL_write(SSL *s, const void *buf, int num)
 * int SSL_read(SSL *s, void *buf, int num)
 * int SSL_get_shutdown(SSL *s)
 * X509 *SSL_get_peer_certificate(const SSL *ssl)
 * EVP_PKEY *X509_get_pubkey(X509_PUBKEY *key)
 */
static bool original_function_SSL_write = true;
static bool original_function_SSL_read = true;
static bool original_function_SSL_get_shutdown = true;
static bool original_function_SSL_get_peer_certificate = true;
static bool original_function_X509_get_pubkey = true;
static bool original_function_EVP_PKEY_type = true;
static bool original_function_X509_verify = true;
static bool original_function_HavePublicKey = true;
static bool original_function_EVP_PKEY_cmp = true;
static int SSL_write_result = -1;
static int SSL_read_result = -1;
static char *SSL_read_buffer = NULL;
static int SSL_get_shutdown_result = -1;
static X509 *SSL_get_peer_certificate_result = NULL;
static EVP_PKEY *X509_get_pubkey_result = NULL;
static int EVP_PKEY_type_result = -1;
static int X509_verify_result = -1;
static RSA *HavePublicKey_result = NULL;
static int EVP_PKEY_cmp_result = -1;
#define RESET_STATUS \
    original_function_SSL_write = true; \
    original_function_SSL_read = true; \
    original_function_SSL_get_shutdown = true; \
    original_function_SSL_get_peer_certificate = true; \
    original_function_X509_get_pubkey = true; \
    original_function_EVP_PKEY_type = true; \
    original_function_X509_verify = true; \
    original_function_HavePublicKey = true; \
    original_function_EVP_PKEY_cmp = true; \
    SSL_write_result = -1; \
    SSL_read_result = -1; \
    SSL_get_shutdown_result = -1; \
    X509_get_pubkey_result = NULL; \
    EVP_PKEY_type_result = -1; \
    X509_verify_result = -1; \
    HavePublicKey_result = NULL; \
    EVP_PKEY_cmp_result = -1;
/*
 * These macros are used to control each function separatedly.
 */
#define USE_ORIGINAL(f) \
    original_function_ ## f = true
#define USE_MOCK(f) \
    original_function_ ## f = false
#define USING(f) \
    original_function_ ## f
#define SSL_WRITE_RETURN(x) \
    SSL_write_result = x
#define SSL_GET_SHUTDOWN_RETURN(x) \
    SSL_get_shutdown_result = x
#define SSL_READ_RETURN(x) \
    SSL_read_result = x
#define SSL_READ_USE_BUFFER(x) \
    SSL_read_buffer = xstrdup(x)
#define SSL_READ_REMOVE_BUFFER \
    if (SSL_read_buffer) \
    {  \
        free (SSL_read_buffer); \
        SSL_read_buffer = NULL; \
    }
#define SSL_GET_PEER_CERTIFICATE_RETURN(x) \
    SSL_get_peer_certificate_result = x
#define X509_GET_PUBKEY_RETURN(x) \
    X509_get_pubkey_result = x
#define EVP_PKEY_TYPE_RETURN(x) \
    EVP_PKEY_type_result = x
#define X509_VERIFY_RETURN(x) \
    X509_verify_result = x
#define HAVEPUBLICKEY_RETURN(x) \
    HavePublicKey_result = x
#define EVP_PKEY_CMP_RETURN(x) \
    EVP_PKEY_cmp_result = x
/*
 * We keep a copy of the original functions, so we can simulate
 * the real behavior if needed.
 * This is risky since we need to keep these functions up to date
 * in case OpenSSL decides to change them.
 */
int original_SSL_write(SSL *s, const void *buf, int num)
{
    if (s->handshake_func == 0)
        {
        SSLerr(SSL_F_SSL_WRITE, SSL_R_UNINITIALIZED);
        return -1;
        }

    if (s->shutdown & SSL_SENT_SHUTDOWN)
        {
        s->rwstate=SSL_NOTHING;
        SSLerr(SSL_F_SSL_WRITE,SSL_R_PROTOCOL_IS_SHUTDOWN);
        return(-1);
        }
    return(s->method->ssl_write(s,buf,num));
}

int original_SSL_read(SSL *s, void *buf, int num)
{
    if (s->handshake_func == 0)
        {
        SSLerr(SSL_F_SSL_READ, SSL_R_UNINITIALIZED);
        return -1;
        }

    if (s->shutdown & SSL_RECEIVED_SHUTDOWN)
        {
        s->rwstate=SSL_NOTHING;
        return(0);
        }
    return(s->method->ssl_read(s,buf,num));
}

int original_SSL_get_shutdown(const SSL *ssl)
{
    return(ssl->shutdown);
}
X509 *original_SSL_get_peer_certificate(const SSL *ssl)
{
    X509 *r;

    if ((ssl == NULL) || (ssl->session == NULL))
        r=NULL;
    else
        r=ssl->session->peer;

    if (r == NULL) return(r);

    CRYPTO_add(&r->references,1,CRYPTO_LOCK_X509);

    return(r);
}
EVP_PKEY *original_X509_get_pubkey(X509 *x)
{
    if ((x == NULL) || (x->cert_info == NULL))
        return(NULL);
    return(X509_PUBKEY_get(x->cert_info->key));
}
int original_EVP_PKEY_type(int type)
{
    switch (type)
    {
    case EVP_PKEY_RSA:
    case EVP_PKEY_RSA2:
        return(EVP_PKEY_RSA);
    case EVP_PKEY_DSA:
    case EVP_PKEY_DSA1:
    case EVP_PKEY_DSA2:
    case EVP_PKEY_DSA3:
    case EVP_PKEY_DSA4:
        return(EVP_PKEY_DSA);
    case EVP_PKEY_DH:
        return(EVP_PKEY_DH);
    case EVP_PKEY_EC:
        return(EVP_PKEY_EC);
    default:
        return(NID_undef);
    }
    return(NID_undef);
}
int original_X509_verify(X509 *a, EVP_PKEY *r)
{
    return(ASN1_item_verify(ASN1_ITEM_rptr(X509_CINF),a->sig_alg,
        a->signature,a->cert_info,r));
}
RSA *original_HavePublicKey(const char *username, const char *ipaddress, const char *digest)
{
    char keyname[CF_MAXVARSIZE], newname[CF_BUFSIZE], oldname[CF_BUFSIZE];
    struct stat statbuf;
    static char *passphrase = "public";
    unsigned long err;
    FILE *fp;
    RSA *newkey = NULL;

    snprintf(keyname, CF_MAXVARSIZE, "%s-%s", username, digest);

    snprintf(newname, CF_BUFSIZE, "%s/ppkeys/%s.pub", CFWORKDIR, keyname);
    MapName(newname);

    if (stat(newname, &statbuf) == -1)
    {
        Log(LOG_LEVEL_VERBOSE, "Did not find new key format '%s'", newname);
        snprintf(oldname, CF_BUFSIZE, "%s/ppkeys/%s-%s.pub", CFWORKDIR, username, ipaddress);
        MapName(oldname);

        Log(LOG_LEVEL_VERBOSE, "Trying old style '%s'", oldname);

        if (stat(oldname, &statbuf) == -1)
        {
            Log(LOG_LEVEL_DEBUG, "Did not have old-style key '%s'", oldname);
            return NULL;
        }

        if (strlen(digest) > 0)
        {
            Log(LOG_LEVEL_INFO, "Renaming old key from '%s' to '%s'", oldname, newname);

            if (rename(oldname, newname) != 0)
            {
                Log(LOG_LEVEL_ERR, "Could not rename from old key format '%s' to new '%s'. (rename: %s)", oldname, newname, GetErrorStr());
            }
        }
        else
        {
            /* We don't know the digest (e.g. because we are a client and have
               no lastseen-map yet), so we're using old file format
               (root-IP.pub). */
            Log(LOG_LEVEL_VERBOSE,
                "We have no digest yet, using old keyfile name: %s",
                oldname);
            snprintf(newname, sizeof(newname), "%s", oldname);
        }
    }

    if ((fp = fopen(newname, "r")) == NULL)
    {
        Log(LOG_LEVEL_ERR, "Couldn't find a public key '%s'. (fopen: %s)", newname, GetErrorStr());
        return NULL;
    }

    if ((newkey = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL)
    {
        err = ERR_get_error();
        Log(LOG_LEVEL_ERR, "Error reading public key. (PEM_read_RSAPublicKey: %s)", ERR_reason_error_string(err));
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    if ((BN_num_bits(newkey->e) < 2) || (!BN_is_odd(newkey->e)))
    {
        Log(LOG_LEVEL_ERR, "RSA Exponent too small or not odd");
        RSA_free(newkey);
        return NULL;
    }

    return newkey;
}
int original_EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (a->type != b->type)
        return -1;

    if (EVP_PKEY_cmp_parameters(a, b) == 0)
        return 0;

    switch (a->type)
    {
    case EVP_PKEY_RSA:
        if (BN_cmp(b->pkey.rsa->n,a->pkey.rsa->n) != 0
            || BN_cmp(b->pkey.rsa->e,a->pkey.rsa->e) != 0)
            return 0;
        break;
    default:
        return -2;
    }

    return 1;
}

/*
 * Mock'ed functions
 */
int SSL_write(SSL *ssl, const void *buf, int num)
{
    if (USING(SSL_write))
        return original_SSL_write(ssl, buf, num);
    return (SSL_write_result > num) ? num : SSL_write_result;
}

int SSL_read(SSL *ssl, void *buf, int num)
{
    if (USING(SSL_read))
        return original_SSL_read(ssl, buf, num);
    if (SSL_read_buffer)
    {
        char *temp = buf;
        int i = 0;
        for (i = 0; SSL_read_buffer[i] != '\0'; ++i)
            temp[i] = SSL_read_buffer[i];
        return i;
    }
    return (SSL_read_result > num) ? num : SSL_read_result;
}
int SSL_get_shutdown(const SSL *ssl)
{
    if (USING(SSL_get_shutdown))
        return original_SSL_get_shutdown(ssl);
    return SSL_get_shutdown_result;
}
X509 *SSL_get_peer_certificate(const SSL *ssl)
{
    if (USING(SSL_get_peer_certificate))
    {
        return original_SSL_get_peer_certificate(ssl);
    }
    return SSL_get_peer_certificate_result;
}
EVP_PKEY *X509_get_pubkey(X509 *cert)
{
    if (USING(X509_get_pubkey))
    {
        return original_X509_get_pubkey(cert);
    }
    return X509_get_pubkey_result;
}
int EVP_PKEY_type(int type)
{
    if (USING(EVP_PKEY_type))
    {
        return original_EVP_PKEY_type(type);
    }
    return EVP_PKEY_type_result;
}
int X509_verify(X509 *a, EVP_PKEY *r)
{
    if (USING(X509_verify))
    {
        return original_X509_verify(a, r);
    }
    return X509_verify_result;
}
RSA *HavePublicKey(const char *username, const char *ipaddress, const char *digest)
{
    if (USING(HavePublicKey))
    {
        return original_HavePublicKey(username, ipaddress, digest);
    }
    return HavePublicKey_result;
}
int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
    if (USING(EVP_PKEY_cmp))
    {
        return original_EVP_PKEY_cmp(a, b);
    }
    return EVP_PKEY_cmp_result;
}

/*
 * Functions to test:
 * int TLSVerifyCallback(X509_STORE_CTX *ctx ARG_UNUSED, void *arg ARG_UNUSED)
 * int TLSVerifyPeer(ConnectionInfo *conn_info, const char *remoteip, const char *username);
 * int TLSSend(SSL *ssl, const char *buffer, int length);
 * int TLSRecv(SSL *ssl, char *buffer, int length);
 * int TLSRecvLine(SSL *ssl, char *buf, size_t buf_size);
 */
#define ASSERT_IF_NOT_INITIALIZED assert_true(correctly_initialized)
static void test_TLSVerifyCallback(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    /*
     * This function always returns 1.
     * We do this in order to be able to avoid the locking.
     * This test exists as a reminder for the future, in case somebody changes the return value
     * this test will fail. Update the logic in the TLS negotiation if you change the verification
     * callback.
     */
    RESET_STATUS;
    X509_STORE_CTX ctx;
    char test[16];
    assert_int_equal(1, TLSVerifyCallback(&ctx, test));
    RESET_STATUS;
}

#define REREAD_CERTIFICATE(f, c) \
    rewind(f); \
    c = PEM_read_X509(f, (X509 **)NULL, NULL, NULL)
#define REREAD_PUBLIC_KEY(f, k, e) \
    rewind(f); \
    k = PEM_read_RSAPublicKey(f, (RSA **)NULL, NULL, NULL); \
    e = EVP_PKEY_new(); \
    EVP_PKEY_assign_RSA(e, k)
static void test_TLSVerifyPeer(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    RESET_STATUS;

    SSL *ssl = NULL;
    ConnectionInfo *conn_info = NULL;

    /*
     * Open a socket and establish a tcp connection.
     */
    struct sockaddr_in server_addr;
    int server = 0;
    int result = 0;

    conn_info = ConnectionInfoNew();

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server = socket(AF_INET, SOCK_STREAM, 0);
    assert_int_not_equal(-1, server);
    server_addr.sin_family = AF_INET;
    ConnectionInfoSetSocket(conn_info, server);
    /* We should not use inet_addr, but it is easier for this particular case. */
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8035);
    /*
     * Connect
     */
    result = connect(server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    assert_int_not_equal(-1, result);
    /*
     * Create a SSL instance
     */
    ssl = SSL_new(SSLCLIENTCONTEXT);
    assert_true(ssl != NULL);
    SSL_set_fd(ssl, server);
    /*
     * Establish the TLS connection over the socket.
     */
    result = SSL_connect(ssl);
    assert_int_not_equal(-1, result);
    /*
     * Fill the remaining fields on ConnectionInfo
     */
    ConnectionInfoSetProtocolVersion(conn_info, CF_PROTOCOL_TLS);
    ConnectionInfoSetSSL(conn_info, ssl);
    /*
     * Fill in the structures we need for testing.
     */
    X509 *certificate = NULL;
    FILE *certificate_stream = fopen(server_certificate_template_public, "r");
    certificate = PEM_read_X509(certificate_stream, (X509 **)NULL, NULL, NULL);
    assert_true(certificate != NULL);

    /*
     * Start testing
     */
    USE_MOCK(SSL_get_peer_certificate);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);

    USE_MOCK(X509_get_pubkey);
    X509_GET_PUBKEY_RETURN(NULL);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));

    /*
     * Due to the cleaning up we do after failing, we need to re read the certificate after
     * very failure. The same is true for the public key.
     */
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    EVP_PKEY *server_pubkey = NULL;
    FILE *stream = NULL;
    stream = fopen(server_name_template_public, "r");
    RSA *pubkey = PEM_read_RSAPublicKey(stream, (RSA **)NULL, NULL, NULL);
    server_pubkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(server_pubkey, pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);

    USE_MOCK(EVP_PKEY_type);
    EVP_PKEY_TYPE_RETURN(EVP_PKEY_DSA);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    EVP_PKEY_TYPE_RETURN(EVP_PKEY_RSA);

    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    USE_MOCK(X509_verify);
    X509_VERIFY_RETURN(-1);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    X509_VERIFY_RETURN(0);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    X509_VERIFY_RETURN(1);

    USE_MOCK(HavePublicKey);
    HAVEPUBLICKEY_RETURN(NULL);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    assert_int_equal(0, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));

    USE_MOCK(EVP_PKEY_cmp);
    EVP_PKEY_CMP_RETURN(-1);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    HAVEPUBLICKEY_RETURN(pubkey);
    assert_int_equal(0, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    EVP_PKEY_CMP_RETURN(0);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    HAVEPUBLICKEY_RETURN(pubkey);
    assert_int_equal(0, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    EVP_PKEY_CMP_RETURN(-2);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    HAVEPUBLICKEY_RETURN(pubkey);
    assert_int_equal(-1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));
    EVP_PKEY_CMP_RETURN(1);
    REREAD_CERTIFICATE(certificate_stream, certificate);
    SSL_GET_PEER_CERTIFICATE_RETURN(certificate);
    REREAD_PUBLIC_KEY(stream, pubkey, server_pubkey);
    X509_GET_PUBKEY_RETURN(server_pubkey);
    HAVEPUBLICKEY_RETURN(pubkey);
    assert_int_equal(1, TLSVerifyPeer(conn_info, "127.0.0.1", "root"));

    /*
     * Shutting down is not as easy as it seems.
     */
    do {
        result = SSL_shutdown(ssl);
        assert_int_not_equal(-1, result);
    } while (result != 1);
    ConnectionInfoDestroy(&conn_info);
    RESET_STATUS;
}

/*
 * This test checks for the three basic operations:
 * - TLSSend
 * - TLSRecv
 * - TLSRecvLine
 * It is difficult to test each one separatedly, so we test all at once.
 * The test consists on establishing a connection to our child process and then
 * sending and receiving data. We switch between the original functions and the
 * mock functions.
 * We do not test SSL_new, SSL_accept and such because those will be covered by either
 * the client or server tests.
 */
static void test_TLSBasicIO(void)
{
    ASSERT_IF_NOT_INITIALIZED;
    RESET_STATUS;
    SSL *ssl = NULL;
    char output_buffer[] = "this is a buffer";
    int output_buffer_length = strlen(output_buffer);
    char input_buffer[4096];
    int result = 0;

    /*
     * Open a socket and establish a tcp connection.
     */
    struct sockaddr_in server_addr;
    int server = 0;

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server = socket(AF_INET, SOCK_STREAM, 0);
    assert_int_not_equal(-1, server);
    server_addr.sin_family = AF_INET;
    /* We should not use inet_addr, but it is easier for this particular case. */
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8035);

    /*
     * Connect
     */
    result = connect(server, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    assert_int_not_equal(-1, result);
    /*
     * Create a SSL instance
     */
    ssl = SSL_new(SSLCLIENTCONTEXT);
    assert_true(ssl != NULL);
    SSL_set_fd(ssl, server);
    /*
     * Establish the TLS connection over the socket.
     */
    result = SSL_connect(ssl);
    assert_int_not_equal(-1, result);
    /*
     * Start testing. The first obvious thing to test is to send data.
     */
    result = TLSSend(ssl, output_buffer, output_buffer_length);
    assert_int_equal(result, output_buffer_length);
    /*
     * Good we sent data and the data was sent. Let's check what we get back
     * by using TLSRecv.
     */
    result = TLSRecv(ssl, input_buffer, output_buffer_length);
    assert_int_equal(output_buffer_length, result);

    input_buffer[output_buffer_length] = '\0';
    assert_string_equal(output_buffer, input_buffer);
    /*
     * Brilliant! We transmitted and received data using simple communication.
     * Let's try the line sending.
     */
    char output_line_buffer[] = "hello\ngoodbye\n";
    int output_line_buffer_length = strlen(output_line_buffer);
    char output_just_hello[] = "hello";
    int output_just_hello_length = strlen(output_just_hello);

    result = TLSSend(ssl, output_line_buffer, output_line_buffer_length);
    assert_int_equal(result, output_line_buffer_length);

    result = TLSRecvLine(ssl, input_buffer, output_line_buffer_length);
    /* The reply should be up to the first hello */
    assert_int_equal(result, output_just_hello_length);
    assert_string_equal(input_buffer, output_just_hello);

    /*
     * Basic check
     */
    USE_MOCK(SSL_write);
    USE_MOCK(SSL_read);
    assert_int_equal(0, TLSSend(ssl, output_buffer, 0));
    assert_int_equal(-1, TLSSend(ssl, output_buffer, output_buffer_length));
    assert_int_equal(-1, TLSRecv(ssl, input_buffer, output_buffer_length));
    RESET_STATUS;

    /*
     * Start replacing the functions inside to check that the logic works
     * We start by testing TLSSend, then TLSRead and at last TLSRecvLine.
     */
    USE_MOCK(SSL_write);
    SSL_WRITE_RETURN(0);
    assert_int_equal(0, TLSSend(ssl, output_buffer, output_buffer_length));
    USE_MOCK(SSL_get_shutdown);
    SSL_GET_SHUTDOWN_RETURN(1);
    assert_int_equal(0, TLSSend(ssl, output_buffer, output_buffer_length));
    SSL_WRITE_RETURN(-1);
    assert_int_equal(-1, TLSSend(ssl, output_buffer, output_buffer_length));

    USE_MOCK(SSL_read);
    SSL_READ_RETURN(0);
    SSL_GET_SHUTDOWN_RETURN(0);
    assert_int_equal(0, TLSRecv(ssl, input_buffer, output_buffer_length));
    SSL_GET_SHUTDOWN_RETURN(1);
    assert_int_equal(0, TLSRecv(ssl, input_buffer, output_buffer_length));
    SSL_READ_RETURN(-1);
    assert_int_equal(-1, TLSRecv(ssl, input_buffer, output_buffer_length));

    USE_ORIGINAL(SSL_write);
    SSL_READ_RETURN(0);
    assert_int_equal(-1, TLSRecvLine(ssl, input_buffer, output_buffer_length));
    SSL_READ_RETURN(-1);
    assert_int_equal(-1, TLSRecvLine(ssl, input_buffer, output_buffer_length));
    SSL_READ_RETURN(5);
    assert_int_equal(-1, TLSRecvLine(ssl, input_buffer, 10));
    SSL_READ_USE_BUFFER(output_line_buffer);
    assert_int_equal(5, TLSRecvLine(ssl, input_buffer, output_line_buffer_length));
    assert_string_equal(output_just_hello, input_buffer);

    result = SSL_shutdown(ssl);
    if (ssl)
    {
        SSL_free(ssl);
    }
    RESET_STATUS;
}

int main()
{
    PRINT_TEST_BANNER();
    tests_setup();

    const UnitTest tests[] =
    {
        unit_test(test_TLSVerifyCallback),
        unit_test(test_TLSVerifyPeer),
        unit_test(test_TLSBasicIO)
    };

    int result = run_tests(tests);
    tests_teardown();
    return result;
}

