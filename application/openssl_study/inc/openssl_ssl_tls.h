#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/*
"SSL/TLS --- 为了更安全的通信"
SSL(Secure Socket Layer) 加密套接字协议层; SSL3.0
TLS(Transport Layer Security)安全网络传输协议, TLS1.0相当于SSL3.1/TLS1.1/TLS1.2/TLS1.3

A和B安全通信:
对A来说:
1. 通信过程不能被监听;
    1.1 可以使用对称加密加密通信.由于对称加密算法的密钥不能被攻击者预测,因此我们使用伪随机数生成器来生成密钥.
    1.2 若要将对称加密的密钥发送给通信对象,可以使用非对称加密算法完成密钥交换.
2. 通信过程不能被篡改;使用消息认证码.
3. 通信对方是真实存在的B; 使用证书;

*/

/*
要正确使用数字签名,有一个大前提,那是用于验证签名的公钥必须属于真正的发送者.即便数字签名算法再强大,如果你得到的公钥是伪造的,那么数字签名也会完全失效.

"证书 -- 为公钥加上数字签名" 就是将公钥当作一条消息,由一个可信的第三方对其签名后所得到的公钥.
公钥证书(Public-Key Certificate,PKC)其实和驾照很相似,里面记有姓名,组织,邮箱地址等个人信息,以及属于此人的公钥,并由认证机构(Certification Authority,Certifying Authority, CA)施加数字签名.
只要看到公钥证书,我们就可以知道认证机构认定该公钥的确属于此人.公钥证书也简称为证书(certificate).

认证机构就是能够认定 “公钥确实属于此人”,并能够生成数字签名的个人或者组织.认证机构中有国际性组织和政府所设立的组织,也有通过提供认证服务来盈利的一般企业,此外个人也可以成立认证机构.

证书标准规范X.509
在一份证书中,必须证明公钥及其所有者的姓名是一致的.
对X.509证书来说,认证者总是CA或由CA指定的人,一份X.509证书是一些标准字段的集合,这些字段包含有关用户或设备及其相应公钥的信息.
X.509标准定义了证书中应该包含哪些信息,并描述了这些信息是如何编码的(即数据格式)
一般来说,一个数字证书内容可能包括基本数据(版本,序列号) ,所签名对象信息( 签名算法类型,签发者信息,有效期,被签发人,签发的公开密钥),CA的数字签名,等等.

前使用最广泛的标准为ITU和ISO联合制定的X.509的 v3版本规范 (RFC5280), 其中定义了如下证书信息域:
1. 版本号(Version Number):规范的版本号,目前为版本3,值为0x2;
2. 序列号(Serial Number):由CA维护的为它所发的每个证书分配的一的列号,用来追踪和撤销证书.只要拥有签发者信息和序列号,就可以唯一标识一个证书,最大不能过20个字节;
3. 签名算法(Signature Algorithm):数字签名所采用的算法,如:
    sha256-with-RSA-Encryption
    ccdsa-with-SHA2S6;
4. 颁发者(Issuer):发证书单位的标识信息,如 ” C=CN,ST=Beijing, L=Beijing, O=org.example.com,CN=ca.org.example.com ”;
5. 有效期(Validity): 证书的有效期很,包括起止时间.
6. 主体(Subject) : 证书拥有者的标识信息(Distinguished Name),如:” C=CN,ST=Beijing, L=Beijing, CN=person.org.example.com”;
7. 主体的公钥信息(SubJect Public Key Info):所保护的公钥相关的信息:
    1. 公钥算法 (Public Key Algorithm)公钥采用的算法;
    2. 主体公钥(Subject Unique Identifier):公钥的内容.
8. 颁发者唯一号(Issuer Unique Identifier):代表颁发者的唯一信息,仅2,3版本支持,可选;
9. 主体唯一号(Subject Unique Identifier):代表拥有证书实体的唯一信息,仅2,3版本支持,可选:
10. 扩展(Extensions,可选): 可选的一些扩展.中可能包括:
    Subject Key Identifier:实体的秘钥标识符,区分实体的多对秘钥;
    Basic Constraints:一指明是否属于CA;
    Authority Key Identifier:证书颁发者的公钥标识符;
    CRL Distribution Points: 撤销文件的颁发地址;
    Key Usage:证书的用途或功能信息.

此外,证书的颁发者还需要对证书内容利用自己的私钥添加签名, 以防止别人对证书的内容进行篡改.

X.509规范中一般推荐使用PEM(Privacy Enhanced Mail)格式来存储证书相关的文件.证书文件的文件名后缀一般为 .crt 或 .cer .对应私钥文件的文件名后缀一般为 .key.证书请求文件的文件名后綴为 .csr .有时候也统一用pem作为文件名后缀.
PEM格式采用文本方式进行存储.一般包括首尾标记和内容块,内容块采用Base64进行编码.
编码格式总结:
1. X.509 DER(Distinguished Encoding Rules)编码,后缀为:.der .cer .crt
2. X.509 BASE64编码(PEM格式),后缀为:.pem .cer .crt


一个PEM格式(base64编码)的示例证书文件内容:
-----BEGIN CERTIFICATE-----
MIIDyjCCArKgAwIBAgIQdZfkKrISoINLporOrZLXPTANBgkqhkiG9w0BAQsFADBn
MSswKQYDVQQLDCJDcmVhdGVkIGJ5IGh0dHA6Ly93d3cuZmlkZGxlcjIuY29tMRUw
EwYDVQQKDAxET19OT1RfVFJVU1QxITAfBgNVBAMMGERPX05PVF9UUlVTVF9GaWRk
bGVyUm9vdDAeFw0xNzA0MTExNjQ4MzhaFw0yMzA0MTExNjQ4MzhaMFoxKzApBgNV
BAsMIkNyZWF0ZWQgYnkgaHR0cDovL3d3dy5maWRkbGVyMi5jb20xFTATBgNVBAoM
DERPX05PVF9UUlVTVDEUMBIGA1UEAwwLKi5iYWlkdS5jb20wggEiMA0GCSqGSIb3
DQEBAQUAA4IBDwAwggEKAoIBAQDX0AM198jxwRoKgwWsd9oj5vI0and9v9SB9Chl
gZEu6G9ZA0C7BucsBzJ2bl0Mf6qq0Iee1DfeydfEKyTmBKTafgb2DoQE3OHZjy0B
QTJrsOdf5s636W5gJp4f7CUYYA/3e1nxr/+AuG44Idlsi17TWodVKjsQhjzH+bK6
8ukQZyel1SgBeQOivzxXe0rhXzrocoeKZFmUxLkUpm+/mX1syDTdaCmQ6LT4KYYi
soKe4f+r2tLbUzPKxtk2F1v3ZLOjiRdzCOA27e5n88zdAFrCmMB4teG/azCSAH3g
Yb6vaAGaOnKyDLGunW51sSesWBpHceJnMfrhwxCjiv707JZtAgMBAAGjfzB9MA4G
A1UdDwEB/wQEAwIEsDATBgNVHSUEDDAKBggrBgEFBQcDATAWBgNVHREEDzANggsq
LmJhaWR1LmNvbTAfBgNVHSMEGDAWgBQ9UIffUQSuwWGOm+o74JffZJNadjAdBgNV
HQ4EFgQUQh8IksZqcMVmKrIibTHLbAgLRGgwDQYJKoZIhvcNAQELBQADggEBAC5Y
JndwXpm0W+9SUlQhAUSE9LZh+DzcSmlCWtBk+SKBwmAegbfNSf6CgCh0VY6iIhbn
GlszqgAOAqVMxAEDlR/YJTOlAUXFw8KICsWdvE01xtHqhk1tCK154Otci60Wu+tz
1t8999GPbJskecbRDGRDSA/gQGZJuL0rnmIuz3macSVn6tH7NwdoNeN68Uj3Qyt5
orYv1IFm8t55224ga8ac1y90hK4R5HcvN71aIjMKrikgynK0E+g45QypHRIe/z0S
/1W/6rqTgfN6OWc0c15hPeJbTtkntB5Fqd0sfsnKkW6jPsKQ+z/+vZ5XqzdlFupQ
29F14ei8ZHl9aLIHP5s=
-----END CERTIFICATE-----

证书中的解析出来的内容:
Certificate:
    Data:
        Version: 3 (0x2)
        Serial Number:
            10:e6:fc:62:b7:41:8a:d5:00:5e:45:b6
    Signature Algorithm: sha256WithRSAEncryption
        Issuer: C=BE, O=GlobalSign nv-sa, CN=GlobalSign Organization Validation CA-SHA256-G2
        Validity
            Not Before: Nov 21 08:00:00 2016 GMT
            Not After : Nov 22 07:59:59 2017 GMT
        Subject: C=US, ST=California, L=San Francisco, O=Wikimedia Foundation, Inc., CN=*.wikipedia.org
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub:
                    04:c9:22:69:31:8a:d6:6c:ea:da:c3:7f:2c:ac:a5:
                    af:c0:02:ea:81:cb:65:b9:fd:0c:6d:46:5b:c9:1e:
                    ed:b2:ac:2a:1b:4a:ec:80:7b:e7:1a:51:e0:df:f7:
                    c7:4a:20:7b:91:4b:20:07:21:ce:cf:68:65:8c:c6:
                    9d:3b:ef:d5:c1
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        X509v3 extensions:
            X509v3 Key Usage: critical
                Digital Signature, Key Agreement
            Authority Information Access:
                CA Issuers - URI:http://secure.globalsign.com/cacert/gsorganizationvalsha2g2r1.crt
                OCSP - URI:http://ocsp2.globalsign.com/gsorganizationvalsha2g2

            X509v3 Certificate Policies:
                Policy: 1.3.6.1.4.1.4146.1.20
                  CPS: https://www.globalsign.com/repository/
                Policy: 2.23.140.1.2.2

            X509v3 Basic Constraints:
                CA:FALSE
            X509v3 CRL Distribution Points:

                Full Name:
                  URI:http://crl.globalsign.com/gs/gsorganizationvalsha2g2.crl

            X509v3 Subject Alternative Name:
                DNS:*.wikipedia.org, DNS:*.m.mediawiki.org, DNS:*.m.wikibooks.org, DNS:*.m.wikidata.org, DNS:*.m.wikimedia.org, DNS:*.m.wikimediafoundation.org, DNS:*.m.wikinews.org, DNS:*.m.wikipedia.org, DNS:*.m.wikiquote.org, DNS:*.m.wikisource.org, DNS:*.m.wikiversity.org, DNS:*.m.wikivoyage.org, DNS:*.m.wiktionary.org, DNS:*.mediawiki.org, DNS:*.planet.wikimedia.org, DNS:*.wikibooks.org, DNS:*.wikidata.org, DNS:*.wikimedia.org, DNS:*.wikimediafoundation.org, DNS:*.wikinews.org, DNS:*.wikiquote.org, DNS:*.wikisource.org, DNS:*.wikiversity.org, DNS:*.wikivoyage.org, DNS:*.wiktionary.org, DNS:*.wmfusercontent.org, DNS:*.zero.wikipedia.org, DNS:mediawiki.org, DNS:w.wiki, DNS:wikibooks.org, DNS:wikidata.org, DNS:wikimedia.org, DNS:wikimediafoundation.org, DNS:wikinews.org, DNS:wikiquote.org, DNS:wikisource.org, DNS:wikiversity.org, DNS:wikivoyage.org, DNS:wiktionary.org, DNS:wmfusercontent.org, DNS:wikipedia.org
            X509v3 Extended Key Usage:
                TLS Web Server Authentication, TLS Web Client Authentication
            X509v3 Subject Key Identifier:
                28:2A:26:2A:57:8B:3B:CE:B4:D6:AB:54:EF:D7:38:21:2C:49:5C:36
            X509v3 Authority Key Identifier:
                keyid:96:DE:61:F1:BD:1C:16:29:53:1C:C0:CC:7D:3B:83:00:40:E6:1A:7C

    Signature Algorithm: sha256WithRSAEncryption
         8b:c3:ed:d1:9d:39:6f:af:40:72:bd:1e:18:5e:30:54:23:35:
         ...

CA是Certificate Authority的缩写,也叫“证书授权中心”.
它是负责管理和签发证书的第三方机构, 好比一个可信任的中介公司.
一般来说,CA必须是所有行业和所有公众都信任的,认可的.因此它必须具有足够的权威性.就好比A,B两公司都必须信任C公司,才会找 C 公司作为公章的中介.

证书直接是可以有信任关系的, 通过一个证书可以证明另一个证书也是真实可信的. 实际上,证书之间的信任关系,是可以嵌套的.
比如,C 信任 A1,A1 信任 A2,A2 信任 A3...这个叫做证书的信任链.只要你信任链上的头一个证书,那后续的证书,都是可以信任滴.

假设 C 证书信任 A 和 B;然后 A 信任 A1 和 A2;B 信任 B1 和 B2.则它们之间,构成如下的一个树形关系(一个倒立的树).

处于最顶上的树根位置的那个证书,就是“根证书”.除了根证书,其它证书都要依靠上一级的证书,来证明自己.那谁来证明“根证书”可靠捏?
实际上,根证书自己证明自己是可靠滴(或者换句话说,根证书是不需要被证明滴).
聪明的同学此刻应该意识到了:根证书是整个证书体系安全的根本.所以,如果某个证书体系中,根证书出了问题(不再可信了),那么所有被根证书所信任的其它证书,也就不再可信了.

PKI只是一个总称,而并非指某一个单独的规范或规格.
例如,RSA公司所制定的PKCS(Public-Key Cryptography Standards,公钥密码标准)系列规范也是PKI的一种,而互联网规格RFC(Requestfor Comments)中也有很多与PKI相关的文档.
此外,X.509这样的规范也是PKI的一种.
在开发PKI程序时所使用的由各个公司编写的API(Application Programming Interface, 应用程序编程接口)和规格设计书也可以算是PKI的相关规格.
*/

void tcp_tls_server(const char *crt_file, const char *key_file);
void tcp_tls_client(const char *ca_file);

#ifdef __cplusplus
}
#endif
