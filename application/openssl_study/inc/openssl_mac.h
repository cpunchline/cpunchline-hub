#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// "消息认证码 --- 消息被正确传送了吗?"
// 消息认证码HMAC: 基于哈希函数(如SHA-256,SHA-512等)的消息认证码;
// 消息认证码的输入包括任意长度的消息和一个发送者与接收者之间共享的密钥,它可以输出固定长度的数据,这个数据称为MAC值.

/*
消息的完整性(integrity), 指的是“消息没有被篡改”这一性质,完整性也叫一致性.如果能够确认汇款请求的内容与Alice银行所发出的内容完全一致,就相当于是确认了消息的完整性,也就意味着消息没有被篡改.
消息的认证(authentication)指的是“消息来自正确的发送者”这一性质.如果能够确认汇款请求确实来自Alice银行,就相当于对消息进行了认证,也就意味着消息不是其他人伪装成发送者所发出的.
*/
/*
HMAC的基本工作原理如下:
生成HMAC值:
    发送方使用一个密钥和一个哈希函数对消息进行处理, 生成一个HMAC值.
    将消息和HMAC值一起发送给接收方.

验证HMAC值:
    接收方使用相同的密钥和哈希函数对收到的消息进行处理, 生成一个新的HMAC值.
    将生成的HMAC值与接收到的HMAC值进行比较.

如果两个HMAC值相同, 则认为消息未被篡改且确实来自预期的发送方.


消息认证码中,由于发送者和接收者共享相同的密钥,因此会产生无法对第三方证明以及无法防止否认等问题.
能够解决这些问题的数字签名.
*/

#include <stddef.h>
#include <stdint.h>

// max_size = EVP_MAC_CTX_get_mac_size(ctx); // (use it after ctx set digest); same as max_size = EVP_MD_get_size(digest);
int openssl_provider_hmac(const char *digest_name, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);
// key_len = EVP_CIPHER_get_key_length(cipher);
int openssl_provider_cmac(const char *cipher_name, uint8_t *key, size_t key_len, const void *_src, size_t src_len, void *dest, size_t *dest_len);

// openssl list -mac-algorithms
// printf "buffer" | openssl dgst -<digest> -hmac "key"
// openssl dgst -<digest> -hmac "key" [file...]

// printf "buffer" | openssl mac -macopt key:<key> -cipher <cipher> CMAC
// openssl mac -macopt key:<key> -cipher <cipher> CMAC <file>

#ifdef __cplusplus
}
#endif