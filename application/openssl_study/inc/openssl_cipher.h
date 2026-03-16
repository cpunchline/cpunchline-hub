#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/*
加密(Encrypt)是用密钥(Key)从明文生成密文的步骤;
解密(Decrypt)是用密钥(Key)从密文还原成明文的步骤;
*/

// 对称加密又称为共享密钥加密, 其最大的缺点是, 对称加密的安全性依赖于密钥, 一旦泄露, 就意味着任何人都能解密消息.
/*
DES:一种分组密码(Block Cipher, 或者叫块加密), 即将明文按64比特进行分组加密, 每组生成64位比特的密文.它的密钥长度为56比特(从规格上来说, 密钥长度是64比特, 但由于每隔7比特会设置一个用于错误检查的比特, 因此实际长度为56比特).
3DES:一种分组密码(Block Cipher, 或者叫块加密), DES的增强版, 对每组数据应用了三次DES算法; 在每次应用DES时, 使用不同的密钥, 所以有三把独立密钥.这三把密钥组成一起, 是一个长度为168(56 + 56 + 56)比特的密钥, 所以3DES算法的密钥总长度为168比特.
    如果密钥1和密钥3使用相同的密钥,而密钥2使用不同的密钥(也就是只使用两个DES密钥),这种三重DES就称为DES-EDE2.密钥1,密钥2,密钥3全部使用不同的比特序列的三重DES称为DES-EDE3.EDE表示的是加密(Encryption) –>解密(Decryption)–>加密(Encryption)这个流程.
    3DES的解密过程和加密正好相反, 是以密钥3,密钥2,密钥1的顺序, 进行解密→加密→解密的操作;
AES:一种分组密码(Block Cipher, 或者叫块加密), 它的分组长度为128比特, 密钥长度可以为128比特,192比特或256比特;
国密SM4

<>分组密码的工作模式

若将所有的明文分组合并起来就是完整的明文(先忽略填充), 将所以的密文分组合并起来就是完整的密文.
分组密码只能加密固定长度的明文.如果需要加密更长的明文, 就需要对分组密码进行迭代, 而分组密码的迭代方法称为分组密码的模式(Model).简而一句话:分组密码的模式, 就是分组密码的迭代方式.
在分组密码中, 我们称每组的明文为明文分组, 每组生成的密文称为密文分组.
    ECB(Electronic CodeBook)模式, 即电子密码本模式.该模式是将明文分组, 加密后直接成为密文分组, 分组之间没有关系.无需IV;(不要使用)
    CBC(Cipher Block Chaining)模式, 即密码分组链接模式.该模式首先将明文分组与前一个密文分组进行XOR运算, 然后再进行加密.当加密第一个明文分组时,由于不存在“前一个密文分组”,因此需要事先准备一个长度为一个分组的比特序列来代替“前一个密文分组“,这个比特序列称为初始化向量(initialization vector)
    CFB(Cipher FeedBack)模式, 即密文反馈模式.该模式首先将前一个密文分组进行加密, 再与当前明文分组进行XOR运算, 来生成密文分组.同样CFB模式也需要一个IV.
    OFB(Output FeedBack)模式, 即输出反馈模式.该模式会产生一个密钥流, 即将密码算法的前一个输出值, 做为当前密码算法的输入值.该输入值再与明文分组进行XOR运行, 计算得出密文分组.该模式需要一个IV, 进行加密后做为第一个分组的输入.
    CTR(CounTeR)模式, 即计数器模式.该模式也会产生一个密钥流, 它通过递增一个计数器来产生连续的密钥流.对该计数器进行加密, 再与明文分组进行XOR运算, 计算得出密文分组.
    # define         EVP_CIPH_ECB_MODE               0x1
    # define         EVP_CIPH_CBC_MODE               0x2
    # define         EVP_CIPH_CFB_MODE               0x3
    # define         EVP_CIPH_OFB_MODE               0x4
    # define         EVP_CIPH_CTR_MODE               0x5
    # define         EVP_CIPH_GCM_MODE               0x6 // 需要iv,但名字叫nonce
    # define         EVP_CIPH_CCM_MODE               0x7
    # define         EVP_CIPH_XTS_MODE               0x10001
    # define         EVP_CIPH_WRAP_MODE              0x10002
    # define         EVP_CIPH_OCB_MODE               0x10003
    # define         EVP_CIPH_SIV_MODE               0x10004
    # define         EVP_CIPH_GCM_SIV_MODE           0x10005
<> 分组密码的填充

在分组密码中, 当数据长度不符合分组长度时, 需要按一定的方式, 将尾部明文分组进行填充, 这种将尾部分组数据填满的方法称为填充(Padding).
    No Padding: 即不填充, 要求明文的长度, 必须是加密算法分组长度的整数倍.
    ANSI X9.23: 在填充字节序列中, 最后一个字节填充为需要填充的字节长度, 其余字节填充0.
    ISO 10126: 在填充字节序列中, 最后一个字节填充为需要填充的字节长度, 其余字节填充随机数.
    PKCS#5和PKCS#7: 在填充字节序列中, 每个字节填充为需要填充的字节长度.
    ISO/IEC 7816-4: 在填充字节序列中, 第一个字节填充固定值80, 其余字节填充0.若只需填充一个字节, 则直接填充80.
    Zero Padding: 在填充字节序列中, 每个字节填充为0.
    // Padding modes
    // 0 is not padding
    #define EVP_PADDING_PKCS7     1(默认)
    #define EVP_PADDING_ISO7816_4 2
    #define EVP_PADDING_ANSI923   3
    #define EVP_PADDING_ISO10126  4
    #define EVP_PADDING_ZERO      5
*/

#include <stddef.h>
#include <stdint.h>

// mode = EVP_CIPHER_get_mode(cipher);
// key_len = EVP_CIPHER_get_key_length(cipher);
// iv_len/nonce_len = EVP_CIPHER_get_iv_length(cipher);
// block_size = EVP_CIPHER_get_block_size(cipher);
// max encrypt size = plain_len + block_size;
// max decrypt size = block_size;
// 12 < tag_len < 16

// cipher_name = cipher-mode
// not support GCM/CCM(AEAD mode)
int openssl_provider_cipher_encrypt(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);
int openssl_provider_cipher_decrypt(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);
int openssl_provider_cipher_aead_encrypt(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len, uint8_t *tag, size_t tag_len);
int openssl_provider_cipher_aead_decrypt(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len, uint8_t *tag, size_t tag_len);
int openssl_provider_cipher_encrypt_file(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path);
int openssl_provider_cipher_decrypt_file(const char *cipher_name, int padding, uint8_t *iv, size_t iv_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path);
int openssl_provider_cipher_aead_encrypt_file(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path, uint8_t *tag, size_t tag_len);
int openssl_provider_cipher_aead_decrypt_file(const char *cipher_name, const uint8_t *nonce, size_t nonce_len, uint8_t *aad, size_t aad_len, uint8_t *key, size_t key_len, size_t each_handle_block_size, char *src_file_path, char *dest_file_path, uint8_t *tag, size_t tag_len);
// openssl list -cipher-algorithms
/*
// encrypt
openssl enc -<cipher> \
    -K <hex_key> \          # 大写 K, 十六进制密钥(无 0x) -K  $(echo -n "ASCII_key" | xxd -p)
    -iv <hex_iv> \          # 十六进制 IV                -iv $(echo -n "ASCII_iv" | xxd -p)
    [-nopad] \              # 可选:禁用 PKCS#7 填充
    -in input -out output

// encrypt with password
openssl enc -<cipher> \
    -pass pass:<xxx> \      # 用 xxx 当明文密码, OpenSSL使用默认的KDF(密钥派生函数) PBKDF2 + HMAC-SHA256 + 随机 salt(8 字节)将该密码派生出密钥(Key)和初始化向量(IV); 会自动加no salt;
    [-nopad] \              # 可选:禁用 PKCS#7 填充
    -nosalt \               # 必须加!否则会尝试读 salt, 每次用相同密码加密, 结果完全相同
    -in input -out output


// decrypt
openssl enc -<cipher> -d \
    -K <hex_key> \          # 大写 K, 十六进制密钥(无 0x) -K  $(echo -n "ASCII_key" | xxd -p)
    -iv <hex_iv> \          # 十六进制 IV                -iv $(echo -n "ASCII_iv" | xxd -p)
    [-nopad] \              # 可选:禁用 PKCS#7 填充
    -in input -out output

// decrypt with password
openssl enc -<cipher> -d \
    -pass pass:<xxx> \      # 用 xxx 当明文密码, OpenSSL使用默认的KDF(密钥派生函数) PBKDF2 + HMAC-SHA256 + 随机 salt(8 字节)将该密码派生出密钥(Key)和初始化向量(IV); 会自动加no salt;
    [-nopad] \              # 可选:禁用 PKCS#7 填充
    -nosalt \               # 必须加!否则会尝试读 salt, 每次用相同密码加密, 结果完全相同
    -in input -out output

// 打印使用密码加解密时, 实际使用的的salt/Key/IV
// echo "plaintext" | openssl -<cipher> -pass pass:<xxx> -p
*/

#ifdef __cplusplus
}
#endif