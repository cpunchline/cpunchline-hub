#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// 单向散列(Hash)函数
// 强抗碰撞性,是指要找到散列值相同的两条不同的消息是非常困难的这一性质.在这里,散列值可以是任意值.密码技术中的单向散列函数必须具备强抗碰撞性.
// 单向散列函数必须具备单向性(one-way).单向性指的是无法通过散列值反算出消息的性质.根据消息计算散列值可以很容易,但这条单行路是无法反过来走的.

// 用途:
// 1. 完整性校验(验证文件是否被篡改)
// 2. 消息认证码:消息认证码是将“发送者和接收者之间的共享密钥”和“消息,进行混合后计算出的散列值.使用消息认证码可以检测并防止通信过程中的错误,篡改以及伪装.
// 3. 进行数字签名时也会使用单向散列函数.
// 4. 使用单向散列函数可以构造伪随机数生成器.密码技术中所使用的随机数需要具备“事实上不可能根据过去的随机数列预测未来的随机数列”这样的性质.为了保证不可预测性,可以利用单向散列函数的单向性.
// 5. 一次性口令经常被用于服务器对客户端的合法性认证.在这种方式中,通过使用单向散列函数可以保证口令只在通信链路上传送一次(one-time),因此即使窃听者窃取了口令,也无法使用.

/*
常用Hash算法:
MD5 - 16 bytes(128 bits) hashvalue (RFC 1321)-disuse
SHA1 - 20 bytes(160 bits) 散列值 H0 H1 H2 H3 H4
SHA2(SHA-256 - 32 bytes(256 bits); SHA-512 - 64 bytes(512 bits);)
SHA3 Keccak256
国密SM3
*/

#include <stddef.h>

// 文件完整性校验实现思路: 哈希算法可根据需要指定
// 1. 流式方式:         文件分块流式计算哈希;
// 2. list方式:         文件分块计算每一块的哈希, 最后将所有块的哈希合并后再次计算哈希;
// 3. Merkle可信树方式: 文件分块计算每一块的哈希, 作为Merkle可信树的叶子节点集合(大小为count), 反复此过程(遍历从[0, count / 2)用来计算父节点的哈希(相邻两两[i * 2][i * 2 + 1]合并计算哈希, 将其更新到集合对应的父节点索引[i]中, 遍历完成后, 删除(区间[count / 2, count])无用的叶子节点)), 直到计算到根节点;
/*
https://www.less-bug.com/posts/design-and-implementation-of-merkle-tree-and-its-algorithm/
Merkle可信树, 又叫哈希树.它能快速地(准确来说, 在对数级别的时间复杂度内)验证数据块是否存在于一个更大的数据集合中, 甚至还能找到出它的位置(即);
        H(ABCD)
        /    \
      H(AB)  H(CD)
     / \     /  \
  H(A) H(B) H(C) H(D)
假设有一个列表 l, 有 4 个元素:Alice Bob Caro David
我们很容易计算出各自的哈希, 记作 H(A), H(B), H(C), H(D).把它们作为叶子结点.
然后计算 H(AB)=H(H(A)`H(B)), H(CD)=H(H(C)`H(D)), 我们就得到两个哈希值.把它们作为倒数第二层节点.
最后我们计算出 H(ABCD)=H(H(AB)`H(CD)) 就得到根节点.
*/

// max_size = EVP_MD_get_size(digest);
// block_size = EVP_MD_get_block_size(digest);
int openssl_provider_digest(const char *digest_name, const void *_src, size_t src_len, void *dest, size_t *dest_len);
int openssl_provider_xof_digest(const char *xof_digest_name, const void *_src, size_t src_len, void *dest, size_t dest_len);
int openssl_provider_digest_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len);
int openssl_provider_xof_digest_file(const char *xof_digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t dest_len);
int openssl_provider_digest_list_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len);
int openssl_provider_digest_merkle_tree_file(const char *digest_name, size_t each_handle_block_size, char *file_path, void *dest, size_t *dest_len);

// openssl list -digest-algorithms
// printf "buffer" | openssl dgst -<digest>
// openssl dgst -<digest> [file...]

#ifdef __cplusplus
}
#endif