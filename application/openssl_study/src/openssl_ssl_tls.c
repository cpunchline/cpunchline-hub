#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/epoll.h>

#include "openssl/ssl.h"
#include "openssl_print.h"
#include "openssl_ssl_tls.h"

#define SERVER_IP   "127.0.0.1" // 本地测试 回环地址 或 INADDR_ANY
#define SERVER_PORT 10086
#define TREESIZE    10000

#define LISTERN_QUEUE_LEN 20 // 监听队列长度
#define BUFFER_SIZE       1024

static int server_load_cert_from_memory(SSL_CTX *ctx, const char *cert_pem, size_t cert_len)
{
    if (NULL == ctx || NULL == cert_pem || 0 == cert_len)
    {
        return -1;
    }

    int ret = 0;
    BIO *crtb = NULL;
    X509 *cert = NULL;

    do
    {
        crtb = BIO_new_mem_buf(cert_pem, (int)cert_len);
        if (NULL == crtb)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        cert = PEM_read_bio_X509(crtb, NULL, NULL, NULL);
        if (NULL == cert)
        {
            LOG_PRINT_ERROR("PEM_read_bio_X509 fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = SSL_CTX_use_certificate(ctx, cert);
        if (1 != ret)
        {
            LOG_PRINT_ERROR("SSL_CTX_use_certificate fail");
            print_openssl_err();
            ret = -1;
            break;
        }
        ret = 0;
    } while (0);

    if (NULL != cert)
    {
        X509_free(cert);
    }

    if (NULL != crtb)
    {
        BIO_free(crtb);
    }

    return ret;
}

static int server_load_key_from_memory(SSL_CTX *ctx, const char *key_data, size_t key_len)
{
    if (NULL == key_data || 0 == key_len)
    {
        return -1;
    }

    int ret = 0;
    BIO *keyb = NULL;
    EVP_PKEY *pkey = NULL;

    do
    {
        keyb = BIO_new_mem_buf(key_data, (int)key_len);
        if (NULL == keyb)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        pkey = PEM_read_bio_PrivateKey(keyb, NULL, NULL, NULL);
        if (NULL == pkey)
        {
            LOG_PRINT_ERROR("PEM_read_bio_PrivateKey fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        ret = SSL_CTX_use_PrivateKey(ctx, pkey);
        if (1 != ret)
        {
            LOG_PRINT_ERROR("SSL_CTX_use_PrivateKey fail");
            print_openssl_err();
            ret = -1;
            break;
        }
        ret = 0;
    } while (0);

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != keyb)
    {
        BIO_free(keyb);
    }

    return ret;
}

static int server_load_cert_and_key_from_memory(SSL_CTX *ctx, const char *cert_pem, size_t cert_len, const char *key_data, size_t key_len)
{
    if (NULL == ctx || NULL == cert_pem || 0 == cert_len || NULL == key_data || 0 == key_len)
    {
        return -1;
    }

    int ret = 0;
    BIO *crtb = NULL;
    BIO *keyb = NULL;
    X509 *cert = NULL;
    EVP_PKEY *pkey = NULL;

    do
    {
        crtb = BIO_new_mem_buf(cert_pem, (int)cert_len);
        if (NULL == crtb)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        keyb = BIO_new_mem_buf(key_data, (int)key_len);
        if (NULL == keyb)
        {
            LOG_PRINT_ERROR("BIO_new_mem_buf fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        cert = PEM_read_bio_X509(crtb, NULL, NULL, NULL);
        if (NULL == cert)
        {
            LOG_PRINT_ERROR("PEM_read_bio_X509 fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        pkey = PEM_read_bio_PrivateKey(keyb, NULL, NULL, NULL);
        if (NULL == pkey)
        {
            LOG_PRINT_ERROR("PEM_read_bio_PrivateKey fail");
            print_openssl_err();
            ret = -1;
            break;
        }

        if (1 != SSL_CTX_use_cert_and_key(ctx, cert, pkey, NULL, 1))
        {
            LOG_PRINT_ERROR("SSL_CTX_use_cert_and_key fail");
            print_openssl_err();
            ret = -1;
            break;
        }
        ret = 0;
    } while (0);

    if (NULL != pkey)
    {
        EVP_PKEY_free(pkey);
    }

    if (NULL != cert)
    {
        X509_free(cert);
    }

    if (NULL != keyb)
    {
        BIO_free(keyb);
    }

    if (NULL != crtb)
    {
        BIO_free(crtb);
    }

    return ret;
}

static int client_config_ca_chain_from_memory(SSL_CTX *ctx, const char *certs, size_t certs_len, const char *crl, size_t crl_len)
{
    if (NULL == ctx || NULL == certs || 0 == certs_len)
    {
        return -1;
    }

    X509_STORE *cert_store = X509_STORE_new();
    if (cert_store == NULL)
    {
        print_openssl_err();
        return -1;
    }

    BIO *crtb = BIO_new_mem_buf(certs, (int)certs_len);
    if (crtb == NULL)
    {
        print_openssl_err();
        return -1;
    }

    // certificates first
    X509 *cert;
    while ((cert = PEM_read_bio_X509(crtb, NULL, 0, NULL)) != NULL)
    {
        X509_STORE_add_cert(cert_store, cert);
        X509_free(cert); // X509_STORE_add_cert takes a reference
    }
    BIO_free(crtb);

    if (crl != NULL)
    {
        // 证书信任列表Certificate Trust List
        BIO *crlb = BIO_new_mem_buf(crl, (int)crl_len);
        X509_CRL *xcrl = PEM_read_bio_X509_CRL(crlb, NULL, NULL, NULL);
        if (xcrl != NULL)
        {
            X509_STORE_add_crl(cert_store, xcrl);
        }
        BIO_free(crlb);
    }
    SSL_CTX_set_cert_store(ctx, cert_store);

    return 0;
}

// ET + 非阻塞
void tcp_tls_server(const char *crt_file, const char *key_file)
{
    int serverFd = -1;
    static int clientFd = -1;
    int epoll_Fd = -1;
    int flag;
    int ret = -1;
    int i = -1;
    char buf[1024] = {0};
    struct epoll_event ev;
    struct epoll_event evArr[100] = {0}; // 准备就绪fd数组
    // 定义Internet协议地址类型变量
    struct sockaddr_in serverAddr = {};
    struct sockaddr_in clientAddr = {};
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL; // only one client

    ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == NULL)
    {
        LOG_PRINT_ERROR("server SSL_CTX_new fail!");
        print_openssl_err();
        goto tcp_tls_server_end;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
    // SSL_CTX_set_ex_data(ctx, 1, usedata); // set userdata
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    // server_load_cert_from_memory
    if (0 == SSL_CTX_use_certificate_file(ctx, crt_file, SSL_FILETYPE_PEM))
    {
        LOG_PRINT_ERROR("server SSL_CTX_use_certificate_file fail!");
        print_openssl_err();
        goto tcp_tls_server_end;
    }

    if (0 == SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM))
    {
        LOG_PRINT_ERROR("server SSL_CTX_use_PrivateKey_file fail!");
        print_openssl_err();
        goto tcp_tls_server_end;
    }

    if (0 == SSL_CTX_check_private_key(ctx))
    {
        LOG_PRINT_ERROR("SSL_CTX_check_private_key fail!");
        print_openssl_err();
        goto tcp_tls_server_end;
    }

    // 1.socket创建套接字
    serverFd = socket(PF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        LOG_PRINT_ERROR("server socket fail, errno[%d](%s)!", errno, strerror(errno));
        goto tcp_tls_server_end;
    }
    else
    {
        LOG_PRINT_INFO("server socket ok!");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));        // bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = PF_INET;                   // 地址族
    serverAddr.sin_port = htons(SERVER_PORT);          // 端口:主机字节序→网络字节序
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // IP地址:主机字节序→网络字节序

    int on = 1; // 为1表示可以复用  为0相反
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(int)) < 0)
    {
        LOG_PRINT_ERROR("server setsockopt fail, errno[%d](%s)!", errno, strerror(errno));
        goto tcp_tls_server_end;
    }

    // 2.bind绑定服务器地址信息
    ret = bind(serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (ret < 0)
    {
        LOG_PRINT_ERROR("server bind fail, errno[%d](%s)!", errno, strerror(errno));
        goto tcp_tls_server_end;
    }
    LOG_PRINT_INFO("server bind ok!");

    // 3.创建监听队列
    ret = listen(serverFd, 10);
    if (ret < 0)
    {
        LOG_PRINT_ERROR("server listen fail, errno[%d](%s)!", errno, strerror(errno));
        goto tcp_tls_server_end;
    }
    LOG_PRINT_INFO("server listening...");

    // 1.创建epoll实例(红黑树根节点),类似于之前的创建rfds
    epoll_Fd = epoll_create(TREESIZE);
    if (epoll_Fd < 0)
    {
        LOG_PRINT_ERROR("server epoll_create fail, errno[%d](%s)!", errno, strerror(errno));
        goto tcp_tls_server_end;
    }

    // 修改connfd为非阻塞读
    flag = fcntl(serverFd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(serverFd, F_SETFL, flag);

    // 2.将serverFd加入到红黑树中,类似于FD_SET
    ev.events = EPOLLIN | EPOLLET; // 设置serverFd要监控的事件是读事件 ET边沿触发
    ev.data.fd = serverFd;

    // 插入红黑树
    ret = epoll_ctl(epoll_Fd, EPOLL_CTL_ADD, serverFd, &ev);
    if (ret < 0)
    {
        LOG_PRINT_ERROR("server epoll_ctl fd[%d] fail, errno[%d](%s)!", serverFd, errno, strerror(errno));
        goto tcp_tls_server_end;
    }

    while (1)
    {
        memset(&clientAddr, 0, sizeof(struct sockaddr_in));
        socklen_t len = sizeof(clientAddr);
        memset(evArr, 0, sizeof(evArr)); // 清空就绪数组

        // 调用一次,检测一次
        //  3.epoll_wait会将内核中的准备就绪fd全部拷贝给就绪fd数组   返回就绪fd的个数
        int n = epoll_wait(epoll_Fd, evArr, 100, 2000); // 不设置timout则最后一个参数为-1
        if (n < 0)
        {
            LOG_PRINT_ERROR("server epoll_wait fail, errno[%d](%s)!", errno, strerror(errno));
            continue;
        }
        else if (n == 0)
        {
            LOG_PRINT_DEBUG("2000 Timeout!");
        }
        else // 返回值n>0即准备就绪的文件描述符的个数,在传出参数evArr存储
        {
            LOG_PRINT_DEBUG("n = %d", n);
            //  epoll_wait结束后,evArr中放的就是准备就绪的文件描述符,返回值是个数
            // 我们只需要将evArr中的拿出来,看是谁,直接操作就好
            for (i = 0; i < n; i++)
            {
                if (evArr[i].data.fd == serverFd)
                {
                    // 这个示例只是为了验证TLS,所以限制只能有一个客户端
                    if (clientFd >= 0)
                    {
                        LOG_PRINT_ERROR("server only support one client!");
                        int temp_fd = accept(serverFd, NULL, NULL);
                        if (temp_fd >= 0)
                        {
                            LOG_PRINT_WARN("Only one client allowed. Rejecting new connection.");
                            close(temp_fd);
                        }
                        continue;
                    }
                    clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &len);
                    if (clientFd < 0)
                    {
                        LOG_PRINT_ERROR("server accept fail, errno[%d](%s)!", errno, strerror(errno));
                        continue;
                    }
                    else
                    {
                        LOG_PRINT_DEBUG("accept ok, IP=%s, PORT=%d!", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                        ssl = SSL_new(ctx);
                        if (NULL == ssl)
                        {
                            LOG_PRINT_ERROR("server SSL_new fail!");
                            print_openssl_err();
                            goto tcp_tls_server_end;
                        }
                        SSL_set_fd(ssl, clientFd);

                        if (0 == SSL_accept(ssl))
                        {
                            LOG_PRINT_ERROR("SSL_accept fail!");
                            print_openssl_err();
                            goto tcp_tls_server_end;
                        }
                        LOG_PRINT_INFO("recv client Cipher: %s", SSL_get_cipher_name(ssl));
                    }
                    /*
                    // 将监听套接字设置为非阻塞模式,用 while 循环抱住 accept 调用,处理完 TCP 就绪队列中的所有连接后再退出循环.
                    // 如何知道是否处理完就绪队列中的所有连接呢? accept 返回 -1 并且 errno 设置为 EAGAIN 就表示所有连接都处理完
                    while ((clientFd = accept(serverFd, (struct sockaddr *) &remote, (size_t *)&addrlen)) > 0) {
                        // todo
                    }
                    if (clientFd == -1)
                    {
                        if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR)
                        {
                            LOG_PRINT_ERROR("server accept fail, errno[%d](%s)!", errno, strerror(errno));
                        }
                    }
                    */
                    // 设置clientFd为非阻塞读
                    flag = fcntl(clientFd, F_GETFL);
                    flag |= O_NONBLOCK;
                    fcntl(clientFd, F_SETFL, flag);

                    // 将clientFd加入epoll中
                    memset(&ev, 0, sizeof(struct epoll_event));
                    ev.events = EPOLLIN | EPOLLET; // 设置clientFd要监控的事件是读事件 ET边沿触发
                    ev.data.fd = clientFd;
                    ret = epoll_ctl(epoll_Fd, EPOLL_CTL_ADD, clientFd, &ev);
                    if (ret < 0)
                    {
                        LOG_PRINT_ERROR("server epoll_ctl fd[%d] fail, errno[%d](%s)!", clientFd, errno, strerror(errno));
                        goto tcp_tls_server_end;
                    }
                }
                else
                {
                    if (NULL == ssl)
                    {
                        epoll_ctl(epoll_Fd, EPOLL_CTL_DEL, evArr[i].data.fd, NULL);
                        close(evArr[i].data.fd);
                        continue;
                    }

                    memset(buf, 0, sizeof(buf));
                    // 这块是ET且socket非阻塞必须这么做
                    /*
                    这里的意思是,对于ET模式,相当于我们要自己重写read和write,使其像”原子操作“一样,保证一次read 或 write能够完整的读完缓冲区的数据或者写完要写入缓冲区的数据.
                    因此,实现为用while包住read和write即可.
                    */
                    while (1)
                    {
                        ret = SSL_read(ssl, buf, sizeof(buf));
                        if (ret < 0)
                        {
                            int ssl_err = SSL_get_error(ssl, ret);
                            if (ssl_err == SSL_ERROR_WANT_READ)
                            {
                                LOG_PRINT_WARN("SSL_read from clientFd = %d data read end!", evArr[i].data.fd);
                                break;
                            }
                            else if (ssl_err == SSL_ERROR_ZERO_RETURN)
                            {
                                LOG_PRINT_WARN("clientFd = %d client quit!", evArr[i].data.fd);
                                // 如果客户端退出,就需要将对应的clientFd,此时就是i,从集合中清理出去
                                epoll_ctl(epoll_Fd, EPOLL_CTL_DEL, evArr[i].data.fd, NULL);
                                close(evArr[i].data.fd);
                                break;
                            }
                            else
                            {
                                LOG_PRINT_ERROR("server SSL_read clientFd[%d] fail, errno[%d](%s)!", evArr[i].data.fd, errno, strerror(errno));
                                print_openssl_err();
                                goto tcp_tls_server_end;
                            }
                        }
                        else if (ret == 0)
                        {
                            LOG_PRINT_WARN("clientFd = %d client quit!", evArr[i].data.fd);
                            // 如果客户端退出,就需要将对应的clientFd,此时就是i,从集合中清理出去
                            epoll_ctl(epoll_Fd, EPOLL_CTL_DEL, evArr[i].data.fd, NULL);
                            close(evArr[i].data.fd);
                            // break;
                            LOG_PRINT_WARN("server exit!");
                            goto tcp_tls_server_end;
                        }
                        else
                        {
                            LOG_PRINT_DEBUG("SSL_read from clientFd[%d] client:[%s]", evArr[i].data.fd, buf);
                            if (buf[0] == '9')
                            {
                                memset(buf, 0, sizeof(buf));
                                strcpy(buf, "quit");
                            }
                            // 回传数据
                            int ret1 = SSL_write(ssl, buf, (int)strlen(buf)); // write
                            if (ret1 < 0)
                            {
                                int ssl_err = SSL_get_error(ssl, ret1);
                                LOG_PRINT_ERROR("server SSL_write fail, ssl_err[%d]", ssl_err);
                                close(evArr[i].data.fd);
                                goto tcp_tls_server_end;
                            }

                            ret1 = SSL_pending(ssl);
                            if (ret1 <= 0)
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

tcp_tls_server_end:
    if (NULL != ssl)
    {
        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    if (NULL != ctx)
    {
        SSL_CTX_free(ctx);
    }

    if (serverFd > 0)
    {
        close(serverFd);
    }
}

static int client_SSL_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
    // SSL_VERIFY_NONE 不验证对端证书

    // SSL_VERIFY_PEER
    // 1. 没收到证书 → 继续握手(但验证失败);
    // 2. 收到证书才执行此cb, 验证证书的合法性, 若发了无效证书 → 验证失败, 终止握手(若回调返回 0)

    // SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT
    // 1. 如果对端没发证书 → 立即终止握手;
    // 2. 收到证书才执行此cb, 验证证书的合法性, 若发了无效证书 → 验证失败, 终止握手(若回调返回 0)

    if (0 == preverify_ok)
    {
        int err = X509_STORE_CTX_get_error(x509_ctx);
        LOG_PRINT_ERROR("SSL_verify_cb fail, err[%d]", err);
    }
    else
    {
        LOG_PRINT_DEBUG("SSL_verify_cb ok!");
    }

    return preverify_ok;
}

void tcp_tls_client(const char *ca_file)
{
    int cFd = -1; // 通信套接字(客户端)
    int ret = -1;
    char buffer[BUFFER_SIZE] = {};
    struct sockaddr_in serverAddr = {};
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL; // only one client
    X509 *peer_cert = NULL;
    X509_NAME *subject_name = NULL;
    X509_NAME *issuer_name = NULL;

    memset(&serverAddr, 0x00, sizeof(serverAddr));
    serverAddr.sin_family = PF_INET;                   // 地址族
    serverAddr.sin_port = htons(SERVER_PORT);          // 端口:主机字节序→网络字节序
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP); // IP地址:主机字节序→网络字节序

    ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL)
    {
        LOG_PRINT_ERROR("server SSL_CTX_new fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // client_config_ca_chain_from_memory
    if (0 == SSL_CTX_load_verify_locations(ctx, ca_file, NULL))
    {
        LOG_PRINT_WARN("SSL_CTX_load_verify_locations fail");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    // SSL_CTX_set_ex_data(ctx, 1, usedata); // set userdata
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, client_SSL_verify_cb);

    // 创建套接字
    cFd = socket(PF_INET, SOCK_STREAM, 0);
    if (cFd < 0)
    {
        LOG_PRINT_ERROR("client socket fail, errno[%d](%s)!", errno, strerror(errno));
        return;
    }
    // 绑定client地址信息<可选>

    // 客户端发出连接请求
    ret = connect(cFd, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (ret < 0)
    {
        LOG_PRINT_ERROR("client connect fail, errno[%d](%s)!", errno, strerror(errno));
        return;
    }

    ssl = SSL_new(ctx);
    if (NULL == ssl)
    {
        LOG_PRINT_ERROR("SSL_new fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    SSL_set_fd(ssl, cFd);

    LOG_PRINT_INFO("Client Initiating TLS handshake...");

    int retry = 3;
    while (retry-- > 0)
    {
        ret = SSL_connect(ssl);
        if (0 == ret)
        {
            int ssl_err = SSL_get_error(ssl, ret);
            if (ssl_err == SSL_ERROR_WANT_READ && retry > 0)
            {
                usleep(500 * 1000);
                continue;
            }
            LOG_PRINT_ERROR("SSL_connect fail, retry[%d], error code: %d", retry, ssl_err);
            print_openssl_err();
            goto tcp_tls_client_end;
        }
        else
        {
            break;
        }
    }

    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK)
    {
        LOG_PRINT_ERROR("SSL_get_verify_result fail, verify_result[%ld]", verify_result);
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    LOG_PRINT_DEBUG("verify_result: %ld", verify_result);

    LOG_PRINT_INFO("TLS handshake successful! Cipher: %s", SSL_get_cipher_name(ssl));

    // print peer cert info
    peer_cert = SSL_get1_peer_certificate(ssl);
    if (NULL == peer_cert)
    {
        LOG_PRINT_ERROR("SSL_get1_peer_certificate fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }

    long version = X509_get_version(peer_cert);
    LOG_PRINT_INFO("server version: %ld", version);

    subject_name = X509_get_subject_name(peer_cert);
    if (NULL == subject_name)
    {
        LOG_PRINT_ERROR("X509_get_subject_name fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }

    char name_buffer[1024] = {};
    char *ptr_buffer = X509_NAME_oneline(subject_name, name_buffer, sizeof(name_buffer));
    if (NULL == ptr_buffer)
    {
        LOG_PRINT_ERROR("X509_NAME_oneline fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    LOG_PRINT_INFO("server subject: %s", ptr_buffer);

    memset(name_buffer, 0x00, sizeof(name_buffer));
    ptr_buffer = NULL;
    issuer_name = X509_get_issuer_name(peer_cert);
    if (NULL == issuer_name)
    {
        LOG_PRINT_ERROR("X509_get_issuer_name fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    ptr_buffer = X509_NAME_oneline(issuer_name, name_buffer, sizeof(name_buffer));
    if (NULL == ptr_buffer)
    {
        LOG_PRINT_ERROR("X509_NAME_oneline fail!");
        print_openssl_err();
        goto tcp_tls_client_end;
    }
    LOG_PRINT_INFO("server issuer: %s", ptr_buffer);

    // 收发数据
    char num = '0';
    while (1)
    {
        sleep(1);
        num++;
        memset(buffer, 0x00, sizeof(buffer));
        buffer[0] = num;

        ret = SSL_write(ssl, buffer, BUFFER_SIZE); // write
        if (ret < 0)
        {
            int ssl_err = SSL_get_error(ssl, ret);
            LOG_PRINT_ERROR("SSL_write failed, error: %d", ssl_err);
            print_openssl_err();
            break;
        }

        // 接收回传消息
        memset(buffer, 0, sizeof(buffer));
        ret = SSL_read(ssl, buffer, sizeof(buffer)); // read
        if (ret < 0)
        {
            int ssl_err = SSL_get_error(ssl, ret);
            if (ssl_err == SSL_ERROR_ZERO_RETURN)
            {
                LOG_PRINT_WARN("server over!");
                break;
            }
            LOG_PRINT_ERROR("SSL_read failed, error: %d", ssl_err);
            print_openssl_err();
            break;
        }
        else if (0 == ret)
        {
            LOG_PRINT_WARN("server over!");
            break;
        }
        LOG_PRINT_DEBUG("client recv: %s", buffer);
        if (0 == strncmp(buffer, "quit", 4))
        {
            break;
        }
    }

    LOG_PRINT_WARN("client exit!");
tcp_tls_client_end:
    if (NULL != peer_cert)
    {
        // subject_name issuer_name will free by peer_cert
        X509_free(peer_cert);
    }

    if (NULL != ssl)
    {
        SSL_shutdown(ssl);

        SSL_free(ssl);
    }

    if (NULL != ctx)
    {
        SSL_CTX_free(ctx);
    }

    if (cFd > 0)
    {
        close(cFd);
    }
}