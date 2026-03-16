#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/*
非对称加密算法
通过数学函数生成一对密钥(公钥与私钥), 实现加密与解密的分离;

1. 密钥生成: 公钥(可公开)和私钥(需保密).公钥用于加密或验证签名, 私钥用于解密或生成签名.
2. 加密与解密:
    加密: 发送方使用接收方的公钥加密信息, 生成密文.
    解密: 接收方使用私钥解密, 恢复明文.由于私钥唯一, 仅接收方可解密.

3. 数字签名
    签名生成: 发送方用私钥对消息哈希值加密, 生成数字签名.
    验证签名: 接收方用公钥解密签名并与消息哈希值比对, 验证消息来源和完整性.

4. SSL/TLS协议
握手阶段: 服务器用私钥签名证书, 客户端用公钥验证身份; 非对称加密协商对称密钥(如AES).
数据传输: 对称加密处理高速数据流, 非对称加密保护密钥交换.

5. 区块链与加密货币
地址生成: 比特币使用ECC生成公钥哈希作为地址.
交易签名: 私钥签名交易, 全网节点用公钥验证.

数学基础:安全性依赖大数分解(RSA),离散对数(Diffie-Hellman)或椭圆曲线离散对数(ECC)等数学难题.
常用于身份验证

// RSA填充方式
// PKCS#1 v1.5
// (EB = 00 || BT || PS || 00 || D)这是整个填充后的数据结构,称为 Encryption Block(EB),长度等于密钥字节长度(k 字节)
部分	含义
00	第一个字节,确保整个块小于模数 N(防止高位溢出)
BT	Block Type,一个字节,表示用途, 公钥02, 私钥00/01
PS	Padding String,填充数据
    PS由k-3-||D||这么多个字节构成
    k为密钥字节长度
    ||D||明文数据D的字节长度
    BT为00,PS为00,BT为01,PS为FF(私钥解密)
    BT为02,PS随机非0(公钥加密)
00	分隔符,必须为 0
D	原始明文数据
所有部分拼接成一个 k 字节的整数,再进行 m^e mod n 加密.

// PKCS#1: n - 11
// OAEP(SHA1): n - 42

// RSA
// RSA的加密:RSA的密文是对代表明文的数字的E次方求mod N的结果.换句话说,就是将明文自己做E次乘法,然后将其结果除以N求余数,这个余数就是密文.
因此只要知道E和N这两个数,任何人都可以完成加密的运算.所以说,E和N是RSA加密的密钥,也就是说,E和N的组合就是公钥;

// RSA的解密:对表示密文的数字的D次方求modN就可以得到明文.换句话说,将密文自己做D次乘法,再对其结果除以N求余数,就可以得到明文.
数D和数N组合起来就是RSA的解密密钥,因此D和N的组合就是私钥.

// 公钥指数e: 可指定随机值(默认为)
// 生成 私钥指数d 和 模数n(p * q 随机大质数)

// 明文 ^ e % N = 密文
// 密文 ^ e % N = 铭文

// ECC
// ECC椭圆曲线(相对存储空间更小)
https://www.cnblogs.com/Yumeka/p/7392505.html
https://blog.csdn.net/taifei/article/details/73277247

// openssl ecparam -list_curves

ECC通过椭圆曲线方程式的性质产生密钥
通常将Fp上的一条椭圆曲线描述为T=(p,a,b,G,n,h)p,a,b确定一条椭圆曲线(p为质数,(mod p)运算)G为基点,n为点G的阶,h是椭圆曲线上所有点的个数m与n相除的商的整数部分
参量选择要求:
1. p越大安全性越好,但会导致计算速度变慢
2. 200-bit左右可满足一般安全要求
3. n应为质数
4. h≤4;p≠n×h ;pt≠1(mod n) (1≤t＜20)
5. 4a3＋27b2≠0 (mod p)

ECC 164位的密钥产生的一个安全级相当于RSA 1024位密钥提供的保密强度,而且计算量较小,处理速度更快,存储空间和传输带宽占用较少.目前我国居民二代身份证正在使用 256 位的椭圆曲线密码,虚拟货币比特币也选择ECC作为加密算法.
椭圆曲线依赖的数学难题是: 考虑K=kG ,其中K,G为椭圆曲线Ep(a,b)上的点,n为G的阶(nG=O∞ ),k为小于n的整数.则给定k和G,根据加法法则,计算K很容易但反过来,给定K和G,求k就非常困难.
因为实际使用中的ECC原则上把p取得相当大,n也相当大,要把n个解点逐一算出来列成上表是不可能的.这就是椭圆曲线加密算法的数学依据
点G称为基点(base point)
k(k<n)为私有密钥(privte key)
K = kG 为公开密钥(public key)

R = 生成随机整数r*G
M 明文m映射至椭圆曲线上一点M, 带入x进行计算
C 公钥加密(M + r*k*G)
M' 私钥解密(C-k*R)
*/
#include <stddef.h>

// 模数n长度 = 密钥位数 / 8 = 256 bytes
// 密文长度 = EVP_PKEY_get_size(pkey)
// RSA_PKCS1_PADDING      最大明文长度 = 密文长度 − 11 = 245 bytes;
// RSA_PKCS1_OAEP_PADDING 最大明文长度 = 密文长度 − 42 = 214 bytes;
// RSA_NO_PADDING         最大明文长度 = 密文长度 bytes;
// 若超过最大明文长度, 则考虑混合加密, 例如生成随机 AES-256 密钥(32 字节)用 AES-GCM 加密你的大数据用 RSA 公钥加密这个 AES 密钥(32 < 245,一次搞定)传输:{AES密文, RSA加密的AES密钥}

int openssl_provider_RSA_public_encrypt(int padding, char *public_key, size_t public_key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);
int openssl_provider_RSA_private_decrypt(int padding, char *private_key, size_t private_key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);

/*
"数字签名 --- 消息到底是谁写的"
*/
// 生成签名:就是根据消息内容计算数字签名的值,这个行为意味着 “我认可该消息的内容”
// 验证签名:这一行为一般是由消息的接收者来完成的,但也可以由需要验证消息的第三方来完成.验证签名就是检查该消息的签名是否真的属于发送者;
// 签名者生成签名时使用私钥,验证者验证签名时使用公钥;

// 数字签名的方法
// 1. 直接对消息签名的方法
// 2. 对消息的散列值签名的方法

/*
PKCS 目前共发布过 15 个标准:

PKCS#1:RSA加密标准.==PKCS#1定义了RSA公钥函数的基本格式标准,特别是数字签名==.它定义了数字签名如何计算,包括待签名数据和签名本身的格式;它也定义了PSA公/私钥的语法.
PKCS#2:涉及了RSA的消息摘要加密,这已被并入PKCS#1中.
PKCS#3:Diffie-Hellman密钥协议标准.PKCS#3描述了一种实现Diffie- Hellman密钥协议的方法.
PKCS#4:最初是规定RSA密钥语法的,现已经被包含进PKCS#1中.
PKCS#5:基于口令的加密标准.PKCS#5描述了使用由口令生成的密钥来加密8位位组串并产生一个加密的8位位组串的方法.PKCS#5可以用于加密私钥,以便于密钥的安全传输(这在PKCS#8中描述).
PKCS#6:扩展证书语法标准.PKCS#6定义了提供附加实体信息的X.509证书属性扩展的语法(当PKCS#6第一次发布时,X.509还不支持扩展.这些扩展因此被包括在X.509中).
PKCS#7:密码消息语法标准.PKCS#7为使用密码算法的数据规定了通用语法,比如数字签名和数字信封.PKCS#7提供了许多格式选项,包括未加密或签名的格式化消息,已封装(加密)消息,已签名消息和既经过签名又经过加密的消息.
PKCS#8:私钥信息语法标准.PKCS#8定义了私钥信息语法和加密私钥语法,其中私钥加密使用了PKCS#5标准.
PKCS#9:可选属性类型.PKCS#9定义了PKCS#6扩展证书,PKCS#7数字签名消息,PKCS#8私钥信息和PKCS#10证书签名请求中要用到的可选属性类型.已定义的证书属性包括E-mail地址,无格式姓名,内容类型,消息摘要,签名时间,签名副本(counter signature),质询口令字和扩展证书属性.
PKCS#10:证书请求语法标准.PKCS#10定义了证书请求的语法.证书请求包含了一个唯一识别名,公钥和可选的一组属性,它们一起被请求证书的实体签名(证书管理协议中的PKIX证书请求消息就是一个PKCS#10).
PKCS#11:密码令牌接口标准.PKCS#11或“Cryptoki”为拥有密码信息(如加密密钥和证书)和执行密码学函数的单用户设备定义了一个应用程序接口(API).智能卡就是实现Cryptoki的典型设备.注意:Cryptoki定义了密码函数接口,但并未指明设备具体如何实现这些函数.而且Cryptoki只说明了密码接口,并未定义对设备来说可能有用的其他接口,如访问设备的文件系统接口.
PKCS#12:个人信息交换语法标准.PKCS#12定义了个人身份信息(包括私钥,证书,各种秘密和扩展字段)的格式.PKCS#12有助于传输证书及对应的私钥,于是用户可以在不同设备间移动他们的个人身份信息.
PDCS#13:椭圆曲线密码标准.PKCS#13标准当前正在完善之中.它包括椭圆曲线参数的生成和验证,密钥生成和验证,数字签名和公钥加密,还有密钥协定,以及参数,密钥和方案标识的ASN.1语法.
PKCS#14:伪随机数产生标准.PKCS#14标准当前正在完善之中.为什么随机数生成也需要建立自己的标准呢?PKI中用到的许多基本的密码学函数,如密钥生成和Diffie-Hellman共享密钥协商,都需要使用随机数.然而,如果“随机数”不是随机的,而是取自一个可预测的取值集合,那么密码学函数就不再是绝对安全了,因为它的取值被限于一个缩小了的值域中.因此,安全伪随机数的生成对于PKI的安全极为关键.
PKCS#15:密码令牌信息语法标准.PKCS#15通过定义令牌上存储的密码对象的通用格式来增进密码令牌的互操作性.在实现PKCS#15的设备上存储的数据对于使用该设备的所有应用程序来说都是一样的,尽管实际上在内部实现时可能所用的格式不同.PKCS#15的实现扮演了翻译家的角色,它在卡的内部格式与应用程序支持的数据格式间进行转换.

*/

int openssl_provider_RSA_sign(const char *digest_name, char *private_key, size_t private_key_len, void *_src, size_t src_len, void *sign, size_t *sign_len);
int openssl_provider_RSA_verify_sign(const char *digest_name, char *public_key, size_t public_key_len, void *_src, size_t src_len, void *sign, size_t sign_len);

int openssl_provider_ECDSA_sign(const char *digest_name, char *private_key, size_t private_key_len, void *_src, size_t src_len, void *sign, size_t *sign_len);
int openssl_provider_ECDSA_verify_sign(const char *digest_name, char *public_key, size_t public_key_len, void *_src, size_t src_len, void *sign, size_t sign_len);

/*
密钥交换
可以在的不安全的通信场景下进行AES密钥协商,即使有第三者监听到了所有的密钥交换信息,也无法获知最终计算出的AES密钥,防止MITM(Man-in-the-middle attack 中间人攻击)
ECDH 算法就很简单了:

Alice 生成一个随机 ECC 密钥对: {alicePrivKey, alicePubKey = alicePrivKey * G}
Bob 生成一个随机 ECC 密钥对:{bobPrivKey, bobPubKey = bobPrivKey * G}
Alice 和 Bob 通过不可靠信道(比如互联网)交换各自的公钥
Alice 计算共享密钥 sharedKey = bobPubKey * alicePrivKey
Bob 计算共享密钥 sharedKey = alicePubKey * bobPrivKey
Alice 和 Bob 即拥有相同的共享密钥 sharedKey == bobPubKey * alicePrivKey == alicePubKey * bobPrivKey
*/
int openssl_provider_ECDH_exchange_key(char *private_key, size_t private_key_len, char *peer_public_key, size_t peer_public_key_len, void *dest, size_t *dest_len);

#ifdef __cplusplus
}
#endif